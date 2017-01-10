
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>
#include <mutex>

namespace density
{
	template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator >
		class concurrent_heterogeneous_queue : private ALLOCATOR_TYPE
	{
	private:
		struct PushData;
		struct ControlBlock;

	public:

		static_assert(std::is_same<COMMON_TYPE, typename RUNTIME_TYPE::common_type>::value,
			"COMMON_TYPE and RUNTIME_TYPE::common_type must be the same type (did you try to use a type like heter_cont<A,runtime_type<B>>?)");

		static_assert(ALLOCATOR_TYPE::page_size > sizeof(void*) * 8 && ALLOCATOR_TYPE::page_alignment == ALLOCATOR_TYPE::page_size,
			"The size and alignment of the pages must be the same (and not too small)");

		using allocator_type = ALLOCATOR_TYPE;
		using runtime_type = RUNTIME_TYPE;
		using common_type = COMMON_TYPE;
		using reference = typename std::add_lvalue_reference< COMMON_TYPE >::type;
		using const_reference = typename std::add_lvalue_reference< const COMMON_TYPE>::type;
		class put_transaction;
		class iterator;
		class const_iterator;

        /** Default constructor. It is unspecified whether the default constructor allocates any page.
				The page allocator inside the queue is default-constructed.
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).*/
		concurrent_heterogeneous_queue()
		{
			m_tail = m_head = static_cast<ControlBlock*>( allocator_type::allocate_page() );

			// post-conditions
			DENSITY_ASSERT_INTERNAL(empty());
		}

		~concurrent_heterogeneous_queue()
		{
			clear();
			DENSITY_ASSERT_INTERNAL(m_tail == m_head);
			allocator_type::deallocate_page( address_lower_align(m_head, allocator_type::page_size) );
		}

        /** Returns true if this queue contains no elements.
            \n\b Throws: nothing
            \n\b Complexity: constant */
		bool empty() const noexcept
		{
			for (auto curr = m_head; curr != m_tail; )
			{
				auto const control_bits = curr->m_next & 3;
				if (control_bits == 0)
				{
					return false;
				}

				curr = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);
			}

			return true;
		}

		/** Deletes all the elements from this queue.
            \n\b Throws: nothing
            \n\b Complexity: linear */
		void clear() noexcept
		{
			for(;;)
			{
				auto transaction = begin_manual_consume();
				if (!transaction)
				{
					break;
				}
				auto const element = transaction.element_ptr();
				auto const type = transaction.type_ptr();
				type->destroy(element);
			}

			DENSITY_ASSERT_INTERNAL(empty());
		}

		template <typename ELEMENT_TYPE>
			void push(ELEMENT_TYPE && i_source)
		{
			return emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
		}

		template <typename ELEMENT_TYPE>
			put_transaction begin_push(ELEMENT_TYPE && i_source)
		{
			return begin_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
		}

		template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
			void emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
		{
			begin_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
		}

		template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
			put_transaction begin_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
		{
			static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
				"ELEMENT_TYPE must be covariant to (i.e. must derive from) COMMON_TYPE, or COMMON_TYPE must be void");

			size_t element_size = sizeof(ELEMENT_TYPE);
			size_t element_alignment = alignof(ELEMENT_TYPE);
			if (element_alignment < internal_alignment)
			{
				element_alignment = internal_alignment;
				element_size = uint_upper_align(element_size, internal_alignment);
			}

			auto push_data = begin_put_impl(element_size, element_alignment);

			new (type_after_control(push_data.m_control_block)) runtime_type(runtime_type::template make<ELEMENT_TYPE>());
			new (push_data.m_element) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);

			return put_transaction(this, push_data);
		}

		class put_transaction
		{
		public:

			put_transaction(concurrent_heterogeneous_queue * i_queue, PushData i_push_data) noexcept
				: m_queue(i_queue), m_push_data(i_push_data), m_committed(false)
					{ }

			put_transaction(const put_transaction &) = delete;
			put_transaction & operator = (const put_transaction &) = delete;

			put_transaction(put_transaction && i_source) noexcept
				: m_queue(i_source.m_queue), m_push_data(i_source.m_push_data), m_committed(i_source.m_committed)
			{
				i_source.m_queue = nullptr;
			}

			put_transaction & operator = (put_transaction && i_source) noexcept
			{
				if (this != &i_source)
				{
					m_queue = i_source.m_queue;
					m_push_data = i_source.m_push_data;
					m_committed = i_source.m_committed;
					i_source.m_queue = nullptr;
				}
				return *this;
			}

			void commit() noexcept
			{
				m_committed = true;
			}

			~put_transaction()
			{
				if (m_queue != nullptr)
				{
					if (m_committed)
						commit_put_impl(m_push_data.m_control_block);
					else
						cancel_put_impl(m_push_data.m_control_block);
				}
			}

		private:
			concurrent_heterogeneous_queue * m_queue;
			PushData m_push_data;
			bool m_committed;
		};

		class consume_transaction
		{
		public:

			consume_transaction(concurrent_heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
				: m_queue(i_queue), m_control(i_control)
			{

			}

			consume_transaction(consume_transaction && i_source) noexcept
				: m_queue(i_source.m_queue), m_control(i_source.m_control)
			{
				i_source.m_control = nullptr;
			}

			consume_transaction & operator = (consume_transaction && i_source) noexcept
			{
				if (this != &i_source)
				{
					m_queue = i_source.m_queue;
					m_control = i_source.m_control;
					i_source.m_control = nullptr;
				}
				return *this;
			}

			consume_transaction(const consume_transaction &) = delete;
			consume_transaction & operator = (const consume_transaction &) = delete;

			~consume_transaction()
			{
				if(m_control != nullptr)
				{
					m_queue->end_consume_impl(m_control);
				}
			}

			const RUNTIME_TYPE * type_ptr() noexcept
			{
				return m_control != nullptr ? type_after_control(m_control) : nullptr;
			}

			void * unaligned_element_ptr() noexcept
			{
				return m_control != nullptr ? address_add(m_control, sizeof_ControlBlock + sizeof_RuntimeType) : nullptr;
			}

			COMMON_TYPE * element_ptr() noexcept
			{
				static_assert(std::is_same<COMMON_TYPE, void>::value, "");
				return address_upper_align(unaligned_element_ptr(), type_ptr()->alignment());
			}

			explicit operator bool() const noexcept
			{
				return m_control != nullptr;
			}

		private:
			concurrent_heterogeneous_queue * m_queue;
			ControlBlock * m_control;
		};

		consume_transaction begin_manual_consume()
		{
			return consume_transaction(this, begin_consume_impl());
		}

		/**
			const RUNTIME_TYPE & i_type, COMMON_TYPE * i_element
		
		*/
		template <typename FUNC>
			bool try_consume(FUNC && i_func) 
				noexcept(noexcept(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>())))
		{
			auto transaction = begin_manual_consume();
			if (transaction)
			{
				auto const type = transaction.type_ptr();
				auto const element = transaction.element_ptr();
				i_func(*type, element);
				type->destroy(element);
			}
			return (bool)transaction;
		}
			
		class iterator
		{
		public:

			iterator() noexcept = default;
			iterator(const iterator & i_source) noexcept = default;
			iterator & operator = (const iterator & i_source) noexcept = default;

			iterator(concurrent_heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
				: m_control(i_control), m_queue(i_queue)
					{ }

			iterator & operator ++ () noexcept
			{
				DENSITY_ASSERT(m_queue != nullptr);
				m_control = m_queue->next_valid(m_control);
				return *this;
			}

			iterator operator ++ (int) noexcept
			{
				DENSITY_ASSERT(m_queue != nullptr);
				auto const prev = m_control;
				++*this;
				return iterator(m_queue, prev);
			}

			bool operator == (const iterator & i_other) const noexcept
			{
				return m_control == i_other.m_control;
			}

			bool operator != (const iterator & i_other) const noexcept
			{
				return m_control != i_other.m_control;
			}
					
		private:
			ControlBlock * m_control;
			concurrent_heterogeneous_queue * m_queue;
		};

		iterator begin() noexcept { return iterator(this, first_valid(m_head)); }
		iterator end() noexcept { return iterator(this, m_tail); }

	private:	
		
		struct ControlBlock
		{
			uintptr_t m_next;
		};

		struct PushData
		{
			ControlBlock * m_control_block;
			void * m_element;
		};

		constexpr static size_t internal_alignment = detail::size_max(4, detail::size_max(alignof(ControlBlock), alignof(runtime_type)));
		constexpr static size_t sizeof_ControlBlock = (sizeof(ControlBlock) + (internal_alignment - 1)) & ~(internal_alignment - 1);
		constexpr static size_t sizeof_RuntimeType = (sizeof(runtime_type) + (internal_alignment - 1)) & ~(internal_alignment - 1);

		ControlBlock * first_valid(ControlBlock * i_from)
		{
			for (auto curr = i_from; curr != m_tail; )
			{
				auto const control_bits = curr->m_next & 3;
				if (control_bits == 0)
				{
					return curr;
				}
				curr = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);
			}
			return nullptr;
		}

		ControlBlock * next_valid(ControlBlock * i_from)
		{
			DENSITY_ASSERT_INTERNAL(i_from != m_tail);
			for (auto curr = reinterpret_cast<ControlBlock*>(i_from->m_next & ~static_cast<uintptr_t>(3)); curr != m_tail; )
			{
				auto const control_bits = curr->m_next & 3;
				if (control_bits == 0)
				{
					return curr;
				}
				curr = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);
			}
			return nullptr;
		}

		static runtime_type * type_after_control(ControlBlock * i_control)
		{
			return reinterpret_cast<runtime_type*>(address_add(i_control, sizeof_ControlBlock));
		}

		static bool are_in_same_page(void * i_first, void * i_second)
		{
			return (reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) < allocator_type::page_size;
		}

		PushData begin_put_impl(size_t i_size, size_t i_alignment)
		{
			DENSITY_ASSERT_INTERNAL(i_alignment >= internal_alignment && is_power_of_2(i_alignment) && (i_size % i_alignment) == 0);

			for(;;)
			{
				auto const control_block = m_tail;
				void * new_tail = address_add(control_block, sizeof_ControlBlock + sizeof_RuntimeType);

				new_tail = address_upper_align(new_tail, i_alignment);
				void * new_element = new_tail;
				new_tail = address_add(new_tail, i_size);

				DENSITY_ASSERT_INTERNAL(control_block != nullptr);
				new(control_block) ControlBlock();				

				if (DENSITY_LIKELY(are_in_same_page(address_add(new_tail, sizeof_ControlBlock), control_block)))
				{
					control_block->m_next = reinterpret_cast<uintptr_t>(new_tail) + 1;
					m_tail = static_cast<ControlBlock *>(new_tail);
					return PushData{ control_block, new_element };
				}
				else
				{
					// page overflow
					auto new_page = allocator_type::allocate_page();
					control_block->m_next = reinterpret_cast<uintptr_t>(new_page) + 2;
					m_tail = static_cast<ControlBlock *>(new_page);
				}
			}
		}

		static void commit_put_impl(ControlBlock * i_control_block)
		{
			DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 1);
			i_control_block->m_next--;
		}

		static void cancel_put_impl(ControlBlock * i_control_block)
		{
			DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 1);
			i_control_block->m_next++;
		}

		ControlBlock * begin_consume_impl() noexcept
		{
			auto curr = m_head;
			while (curr != m_tail)
			{
				auto const control_bits = curr->m_next & 3;
				if (control_bits == 0)
				{
					curr->m_next |= 1;
					return curr;
				}

				curr = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);
			}

			return nullptr;
		}

		void end_consume_impl(ControlBlock * i_control_block) noexcept
		{
			DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 1);
			i_control_block->m_next++;

			if(i_control_block == m_head)
			{
				auto curr = i_control_block;
				DENSITY_ASSERT_INTERNAL(curr != m_tail);
				do {
					auto const control_bits = curr->m_next & 3;
					if (control_bits != 2)
					{
						break;
					}

					curr = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);
				} while (curr != m_tail);

				m_head = curr;
			}
		}

	private:
		std::mutex m_mutex;
		ControlBlock * m_head, * m_tail;
	};
}