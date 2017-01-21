
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>

namespace density
{
	namespace detail
	{
		template<typename COMMON_TYPE> struct QueueControl
		{
			uintptr_t m_next;
			COMMON_TYPE * m_element;
		};

		template<> struct QueueControl<void>
		{
			uintptr_t m_next;
		};
	}

	/** \brief Class template that implements an heterogeneous FIFO container with dynamic size.
		
		\n<b>Thread safeness</b>: None. The user is responsible to avoid race conditions.
        \n<b>Exception safeness</b>: Any function of heterogeneous_queue is noexcept or provides the strong exception guarantee.

			@param COMMON_TYPE common type. An element of type E can be pushed on the queue only if E* is implicitly convertible 
				to COMMON_TYPE*. COMMON_TYPE cannot be cv-qualified, a reference or an array. In general std::decay (http://en.cppreference.com/w/cpp/types/decay) 
				must transform COMMON_TYPE into itself, or a compile time error occurs.
				If COMMON_TYPE is void, elements of any complete type can be added to the container. In this
				case, the methods of heterogeneous_queue (and its iterators) that returns a pointer to an element
				will return a void* to a complete object, while the methods that returns a reference to
				an element will return void. Use iterators and pointer semantic to write generic code
				that works with any heterogeneous_queue.

			@param VOID_ALLOCATOR Allocator to be used to allocate the memory buffer.  This type must 
				meet the requirements of PageAllocator.
			@param RUNTIME_TYPE Type to be used to represent the actual complete type of each element.
				This type must meet the requirements of RuntimeType. */
	template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator >
		class heterogeneous_queue final : private ALLOCATOR_TYPE
	{
	private:

		using ControlBlock = typename detail::QueueControl<COMMON_TYPE>;

		struct PutData
		{
			ControlBlock * m_control_block;
			void * m_element;
		};

		static constexpr auto s_invalid_control_block = ALLOCATOR_TYPE::page_size - 1;
		
	public:

		static_assert(std::is_same<COMMON_TYPE, typename RUNTIME_TYPE::common_type>::value,
			"COMMON_TYPE and RUNTIME_TYPE::common_type must be the same type (did you try to use a type like heter_cont<A,runtime_type<B>>?)");

		static_assert(std::is_same<COMMON_TYPE, typename std::decay<COMMON_TYPE>::type>::value,
			"COMMON_TYPE can't be cv-qualified, an array or a reference");

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

        /** Default constructor. The page allocator inside the queue is default-constructed.
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).*/
		heterogeneous_queue() noexcept
			: m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)), m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
		{
		}

		heterogeneous_queue(heterogeneous_queue && i_source) noexcept
			: m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)), m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
		{
			swap(i_source);
		}

		heterogeneous_queue(const heterogeneous_queue & i_source)
			: allocator_type(static_cast<const allocator_type&>(i_source)),
			  m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)), m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
		{
			for (auto source_it = i_source.cbegin(); source_it != i_source.cend(); source_it++)
			{
				push_by_copy(source_it.complete_type(), source_it.element());
			}
		}

		heterogeneous_queue & operator = (heterogeneous_queue && i_source) noexcept
		{
			swap(i_source);
			return *this;
		}

		heterogeneous_queue & operator = (const heterogeneous_queue & i_source)
		{
			auto copy(i_source);
			swap(copy);
			return *this;
		}

		void swap(heterogeneous_queue & i_other) noexcept
		{
			using std::swap;
			swap(static_cast<ALLOCATOR_TYPE&>(*this), static_cast<ALLOCATOR_TYPE&>(i_other));
			swap(m_head, i_other.m_head);
			swap(m_tail, i_other.m_tail);
		}

		/** Destructor.
			\n <b>Throws</b>: nothing. */
		~heterogeneous_queue()
		{
			destroy();
		}

        /** Returns whether the queue contains no elements.
            \n\b Throws: nothing */
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
				auto const element = transaction.element();
				auto const & type = transaction.complete_type();
				type.destroy(element);
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
				"ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");

			size_t element_size = sizeof(ELEMENT_TYPE);
			size_t element_alignment = alignof(ELEMENT_TYPE);
			if (element_alignment < internal_alignment)
			{
				element_alignment = internal_alignment;
				element_size = uint_upper_align(element_size, internal_alignment);
			}

			auto push_data = begin_put_impl(element_size, element_alignment);

			DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
			new (type_after_control(push_data.m_control_block)) runtime_type(runtime_type::template make<ELEMENT_TYPE>());
			auto const element = new (push_data.m_element) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
			
			return put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
		}

		put_transaction begin_push_by_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
		{
			size_t element_size = i_type.size();
			size_t element_alignment = i_type.alignment();
			if (element_alignment < internal_alignment)
			{
				element_alignment = internal_alignment;
				element_size = uint_upper_align(element_size, internal_alignment);
			}

			auto push_data = begin_put_impl(element_size, element_alignment);

			DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
			new (type_after_control(push_data.m_control_block)) runtime_type(i_type);
			auto const element = i_type.copy_construct(push_data.m_element, i_source);

			return put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
		}

		put_transaction begin_push_by_move(const runtime_type & i_type, COMMON_TYPE * i_source)
		{
			size_t element_size = i_type.size();
			size_t element_alignment = i_type.alignment();
			if (element_alignment < internal_alignment)
			{
				element_alignment = internal_alignment;
				element_size = uint_upper_align(element_size, internal_alignment);
			}

			auto push_data = begin_put_impl(element_size, element_alignment);

			DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
			new (type_after_control(push_data.m_control_block)) runtime_type(i_type);
			auto const element = i_type.move_construct(push_data.m_element, i_source);

			return put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
		}

		void push_by_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
		{
			begin_push_by_copy(i_type, i_source).commit();
		}

		void push_by_move(const runtime_type & i_type, COMMON_TYPE * i_source)
		{
			begin_push_by_move(i_type, i_source).commit();
		}

		class put_transaction
		{
		public:

			put_transaction(heterogeneous_queue * i_queue, PutData i_push_data, std::true_type, void *) noexcept
				: m_queue(i_queue), m_push_data(i_push_data), m_committed(false)
					{ }

			put_transaction(heterogeneous_queue * i_queue, PutData i_push_data, std::false_type, COMMON_TYPE * i_element) noexcept
				: m_queue(i_queue), m_push_data(i_push_data), m_committed(false)
			{
				m_push_data.m_element = i_element;
				m_push_data.m_control_block->m_element = i_element;
			}

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
			heterogeneous_queue * m_queue;
			PutData m_push_data;
			bool m_committed;
		};

		class consume_transaction
		{
		public:

			consume_transaction(heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
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

			const RUNTIME_TYPE & complete_type() noexcept
			{
				return *type_after_control(m_control);
			}

			void * unaligned_element_ptr() noexcept
			{
				return get_unaligned_element(m_control);
			}

			COMMON_TYPE * element() noexcept
			{
				return get_element(m_control);
			}

			explicit operator bool() const noexcept
			{
				return m_control != nullptr;
			}

		private:
			heterogeneous_queue * m_queue;
			ControlBlock * m_control;
		};

		void pop() noexcept
		{
			auto transaction = begin_manual_consume();
			DENSITY_ASSERT((bool)transaction);
			auto const & type = transaction.complete_type();
			auto const element = transaction.element();
			type.destroy(element);
		}

		bool try_pop() noexcept
		{
			auto transaction = begin_manual_consume();
			if (transaction)
			{
				auto const & type = transaction.complete_type();
				auto const element = transaction.element();
				type.destroy(element);
				return true;
			}
			return false;
		}

		/**
			const RUNTIME_TYPE & i_type, COMMON_TYPE * i_element
		*/
		template <typename FUNC>
			void consume(FUNC && i_func) 
				noexcept(noexcept(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>())))
		{
			auto transaction = begin_manual_consume();
			DENSITY_ASSERT(transaction);
			auto const & type = transaction.complete_type();
			auto const element = transaction.element();
			i_func(type, element);
			type.destroy(element);
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
				auto const & type = transaction.complete_type();
				auto const element = transaction.element();
				i_func(type, element);
				type.destroy(element);
				return true;
			}
			return false;
		}

			
		consume_transaction begin_manual_consume() noexcept
		{
			return consume_transaction(this, begin_consume_impl());
		}


					// iterator
			
		class iterator
		{
		public:

			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = COMMON_TYPE;
			using pointer = COMMON_TYPE *;
			using reference = typename heterogeneous_queue::reference;
			using const_reference = typename heterogeneous_queue::const_reference;

			iterator() noexcept = default;
			iterator(const iterator & i_source) noexcept = default;
			iterator & operator = (const iterator & i_source) noexcept = default;

			iterator(heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
				: m_control(i_control), m_queue(i_queue)
					{ }

			/** Returns a reference to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void. */
			reference operator * () const noexcept { return detail::DeferenceVoidPtr<value_type>::apply(get_element(m_control)); }

			/** Returns a pointer to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void *. */
			pointer operator -> () const noexcept { return static_cast<value_type *>(get_element(m_control)); }

			/** Returns a pointer to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void, then the return type is void *. */
			pointer element() const noexcept { return static_cast<value_type *>(get_element(m_control)); }

			/** Returns the RUNTIME_TYPE associated to this element. The user may use the function type_info of RUNTIME_TYPE
			(whenever supported) to obtain a const-reference to a std::type_info. */
			const RUNTIME_TYPE & complete_type() const noexcept
			{
				return *type_after_control(m_control);
			}

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

			bool operator == (const const_iterator & i_other) const noexcept
			{
				return m_control == i_other.m_control;
			}

			bool operator != (const const_iterator & i_other) const noexcept
			{
				return m_control != i_other.m_control;
			}
					
		private:
			ControlBlock * m_control;
			heterogeneous_queue * m_queue;
		};

		iterator begin() noexcept { return iterator(this, first_valid(m_head)); }
		iterator end() noexcept { return iterator(this, m_tail); }

		class const_iterator
		{
		public:

			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = const COMMON_TYPE;
			using pointer = const COMMON_TYPE *;
			using reference = typename heterogeneous_queue::const_reference;
			using const_reference = typename heterogeneous_queue::const_reference;

			const_iterator() noexcept = default;
			const_iterator(const const_iterator & i_source) noexcept = default;
			const_iterator & operator = (const const_iterator & i_source) noexcept = default;

			const_iterator(const heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
				: m_control(i_control), m_queue(i_queue)
					{ }

			/** Returns a reference to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void. */
			reference operator * () const noexcept { return detail::DeferenceVoidPtr<value_type>::apply(get_element(m_control)); }

			/** Returns a pointer to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void *. */
			pointer operator -> () const noexcept { return static_cast<value_type *>(get_element(m_control)); }

			/** Returns a pointer to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void, then the return type is void *. */
			pointer element() const noexcept { return static_cast<value_type *>(get_element(m_control)); }

			/** Returns the RUNTIME_TYPE associated to this element. The user may use the function type_info of RUNTIME_TYPE
				(whenever supported) to obtain a const-reference to a std::type_info. */
			const RUNTIME_TYPE & complete_type() const noexcept
			{
				return *type_after_control(m_control);
			}

			const_iterator & operator ++ () noexcept
			{
				DENSITY_ASSERT(m_queue != nullptr);
				m_control = m_queue->next_valid(m_control);
				return *this;
			}

			const_iterator operator ++ (int) noexcept
			{
				DENSITY_ASSERT(m_queue != nullptr);
				auto const prev = m_control;
				++*this;
				return const_iterator(m_queue, prev);
			}

			bool operator == (const const_iterator & i_other) const noexcept
			{
				return m_control == i_other.m_control;
			}

			bool operator != (const const_iterator & i_other) const noexcept
			{
				return m_control != i_other.m_control;
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
			const heterogeneous_queue * m_queue;
		};

		const_iterator begin() const noexcept { return const_iterator(this, first_valid(m_head)); }
		const_iterator end() const noexcept { return const_iterator(this, m_tail); }

		const_iterator cbegin() const noexcept { return const_iterator(this, first_valid(m_head)); }
		const_iterator cend() const noexcept { return const_iterator(this, m_tail); }

		bool operator == (const heterogeneous_queue & i_source) const
		{
			const auto end_1 = cend();
			for (auto it_1 = cbegin(), it_2 = i_source.cbegin(); it_1 != end_1; ++it_1, ++it_2)
			{
				auto const equal_comparer = it_1.complete_type().template get_feature<type_features::equals>();
				if (it_1.complete_type() != it_2.complete_type() ||
					!equal_comparer(it_1.element(), it_2.element()))
				{
					return false;
				}
			}
			return true;
		}

	private:	
		
		constexpr static size_t internal_alignment = detail::size_max(4, detail::size_max(alignof(ControlBlock), alignof(runtime_type)));
		constexpr static size_t sizeof_ControlBlock = (sizeof(ControlBlock) + (internal_alignment - 1)) & ~(internal_alignment - 1);
		constexpr static size_t sizeof_RuntimeType = (sizeof(runtime_type) + (internal_alignment - 1)) & ~(internal_alignment - 1);

		ControlBlock * first_valid(ControlBlock * i_from) const
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
			return m_tail;
		}

		ControlBlock * next_valid(ControlBlock * i_from) const
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
			return m_tail;
		}

		static runtime_type * type_after_control(ControlBlock * i_control)
		{
			return reinterpret_cast<runtime_type*>(address_add(i_control, sizeof_ControlBlock));
		}

		static void * get_unaligned_element(detail::QueueControl<void> * i_control)
		{
			return address_add(i_control, sizeof_ControlBlock + sizeof_RuntimeType);
		}

		static void * get_element(detail::QueueControl<void> * i_control)
		{
			return address_upper_align(get_unaligned_element(i_control), type_after_control(i_control)->alignment());
		}

		template <typename TYPE>
			static void * get_unaligned_element(detail::QueueControl<TYPE> * i_control)
		{
			return i_control->m_element;
		}

		template <typename TYPE>
			static TYPE * get_element(detail::QueueControl<TYPE> * i_control)
		{
			return i_control->m_element;
		}

		static bool are_in_same_page(void * i_first, void * i_second)
		{
			return (reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) < allocator_type::page_size;
		}

		PutData begin_put_impl(size_t i_size, size_t i_alignment)
		{
			DENSITY_ASSERT_INTERNAL(i_alignment >= internal_alignment && is_power_of_2(i_alignment) && (i_size % i_alignment) == 0);

			for(;;)
			{
				auto const control_block = m_tail;
				void * new_tail = address_add(control_block, sizeof_ControlBlock + sizeof_RuntimeType);

				new_tail = address_upper_align(new_tail, i_alignment);
				void * new_element = new_tail;
				new_tail = address_add(new_tail, i_size);

				if (DENSITY_LIKELY(are_in_same_page(address_add(new_tail, sizeof_ControlBlock), control_block)))
				{
					DENSITY_ASSERT_INTERNAL(control_block != nullptr);
					new(control_block) ControlBlock();

					control_block->m_next = reinterpret_cast<uintptr_t>(new_tail) + 1;
					m_tail = static_cast<ControlBlock *>(new_tail);
					return PutData{ control_block, new_element };
				}
				else
				{
					allocate_new_page();
				}
			}
		}

		DENSITY_NO_INLINE void allocate_new_page()
		{
			// page overflow
			if (DENSITY_LIKELY(m_tail != reinterpret_cast<ControlBlock*>(s_invalid_control_block)))
			{
				auto const control_block = m_tail;

				DENSITY_ASSERT_INTERNAL(control_block != nullptr);
				new(control_block) ControlBlock();

				auto new_page = allocator_type::allocate_page();
				control_block->m_next = reinterpret_cast<uintptr_t>(new_page) + 2;
				m_tail = static_cast<ControlBlock *>(new_page);
			}
			else
			{
				// this happens only on a virgin queue
				m_tail = m_head = static_cast<ControlBlock*>(allocator_type::allocate_page());
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

					auto next = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);

					auto address_xor = reinterpret_cast<uintptr_t>(next) ^ reinterpret_cast<uintptr_t>(curr);
					if (address_xor >= allocator_type::page_size)
					{
						allocator_type::deallocate_page(address_lower_align(curr, allocator_type::page_size));
					}

					curr = next;

				} while (curr != m_tail);

				m_head = curr;
			}
		}

		void destroy() noexcept
		{
			clear();
			DENSITY_ASSERT_INTERNAL(m_tail == m_head);
			allocator_type::deallocate_page(address_lower_align(m_head, allocator_type::page_size));
		}

	private:
		ControlBlock * m_head, * m_tail;
	};
	
	template <typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE>
		inline void swap(heterogeneous_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE> & i_first,
			heterogeneous_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE> & i_second) noexcept
	{
		i_first.swap(i_second);
	}

} // namespace density
