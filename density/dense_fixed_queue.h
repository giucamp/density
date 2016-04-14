
//          Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <memory>
#include "element_type.h"

namespace reflective
{
	namespace details
	{
		template < typename ALLOCATOR, typename ELEMENT_TYPE >
			class DenseFixedQueueBase : private ALLOCATOR
		{
		public:

			struct IteratorBase
			{
				IteratorBase() REFLECTIVE_NOEXCEPT {}

				IteratorBase(ELEMENT_TYPE * i_type) REFLECTIVE_NOEXCEPT // used to construct end
					: m_curr_type(i_type) { }

				IteratorBase(const DenseFixedQueueBase * i_queue, ELEMENT_TYPE * i_type, void * i_element ) REFLECTIVE_NOEXCEPT
					: m_curr_type(i_type), m_curr_element(i_element), m_queue(i_queue) { }

				void move_next() REFLECTIVE_NOEXCEPT
				{					
					// advance m_curr_type
					m_curr_type = static_cast<ELEMENT_TYPE*>(address_add(m_curr_element, m_curr_type->size()));
					if (m_curr_type != m_queue->m_tail)
					{
						m_curr_type = static_cast<ELEMENT_TYPE*>(address_upper_align(m_curr_type, alignof(ELEMENT_TYPE)));
						if (m_curr_type + 1 > m_queue->m_buffer_end)
						{
							m_curr_type = static_cast<ELEMENT_TYPE*>(address_upper_align(m_queue->m_buffer_start, alignof(ELEMENT_TYPE)));
						}

						// advance m_curr_element
						m_curr_element = address_upper_align(m_curr_type + 1, m_curr_type->alignment());
						auto end_of_element = address_add(m_curr_element, m_curr_type->size());
						if (end_of_element > m_queue->m_buffer_end)
						{
							m_curr_element = address_upper_align(m_queue->m_buffer_start, m_curr_type->alignment());
							end_of_element = address_add(m_curr_element, m_curr_type->size());
						}
					}
				}

				bool operator == (const IteratorBase & i_source) REFLECTIVE_NOEXCEPT
				{
					return i_source.m_curr_type == i_source.m_curr_type;
				}

				ELEMENT_TYPE * m_curr_type;
				void * m_curr_element;
				const DenseFixedQueueBase * m_queue;
			};

			DenseFixedQueueBase(size_t i_buffer_byte_capacity)
			{
				typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(
					*static_cast<ALLOCATOR*>(this));

				m_buffer_start = char_alloc.allocate(i_buffer_byte_capacity);
				m_buffer_end = address_add(m_buffer_start, i_buffer_byte_capacity);
				m_tail = m_head = static_cast<ELEMENT_TYPE *>(m_buffer_start);
			}

			DenseFixedQueueBase(DenseFixedQueueBase && i_source) REFLECTIVE_NOEXCEPT
				: m_head(i_source.m_head), m_tail(i_source.m_tail), m_buffer_start(i_source.m_buffer_start), m_buffer_end(i_source.m_buffer_end)
			{
				i_source.m_tail = i_source.m_head = nullptr;
				i_source.m_buffer_end = i_source.m_buffer_start = nullptr;
			}

			DenseFixedQueueBase & operator = (DenseFixedQueueBase && i_source) REFLECTIVE_NOEXCEPT
			{
				impl_clear();

				m_head = i_source.m_head;
				m_tail = i_source.m_tail;
				m_buffer_start = i_source.m_buffer_start;
				m_buffer_end = i_source.m_buffer_end;

				i_source.m_tail = i_source.m_head = nullptr;
				i_source.m_buffer_end = i_source.m_buffer_start = nullptr;

				return *this;
			}

			~DenseFixedQueueBase() REFLECTIVE_NOEXCEPT
			{
				impl_clear();

				typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(
					*static_cast<ALLOCATOR*>(this));
				
				char_alloc.deallocate(static_cast<char*>(m_buffer_start),
					address_diff(m_buffer_end, m_buffer_start) );
			}

			bool impl_empty() const REFLECTIVE_NOEXCEPT
			{
				return m_head == m_tail;
			}

			IteratorBase impl_begin() const REFLECTIVE_NOEXCEPT
			{
				if (m_head == m_tail)
				{
					return IteratorBase(static_cast<ELEMENT_TYPE*>(m_tail));
				}
				else
				{
					auto type_ptr = static_cast<ELEMENT_TYPE*>(address_upper_align(m_head, alignof(ELEMENT_TYPE)));
					auto type_end = static_cast<void*>(type_ptr + 1);
					if (type_end > m_buffer_end)
					{
						type_ptr = static_cast<ELEMENT_TYPE*>(address_upper_align(m_buffer_start, alignof(ELEMENT_TYPE)));
						type_end = static_cast<void*>(type_ptr + 1);
					}

					auto const element_size = type_ptr->size();
					auto const element_alignment = type_ptr->alignment();

					auto element_end = type_end;
					auto element_ptr = linear_alloc(&element_end, element_size, element_alignment);
					if (element_end > m_buffer_end)
					{
						element_end = m_buffer_start;
						element_ptr = linear_alloc(&element_end, element_size, element_alignment);
					}

					return IteratorBase(this, type_ptr, element_ptr);
				}
			}

			IteratorBase impl_end() const REFLECTIVE_NOEXCEPT
			{
				return IteratorBase(static_cast<ELEMENT_TYPE*>(m_tail));
			}
			
			struct CopyConstruct
			{
				const void * const m_source;

				CopyConstruct(const void * i_source)
					: m_source(i_source) { }

				void operator () (void * i_dest, const ELEMENT_TYPE & i_element_type)
				{
					i_element_type.copy_construct(i_dest, m_source);
				}
			};

			struct MoveConstruct
			{
				void * const m_source;

				MoveConstruct(void * i_source)
					: m_source(i_source) { }

				void operator () (void * i_dest, const ELEMENT_TYPE & i_element_type) REFLECTIVE_NOEXCEPT
				{
					i_element_type.move_construct(i_dest, m_source);
				}
			};

			/* Inserts an object on the queue. The return value is the address of the new object */
			void * single_push(void * * io_tail, size_t i_size, size_t i_alignment) REFLECTIVE_NOEXCEPT
			{
				auto const prev_tail = *io_tail;
				auto start_of_block = linear_alloc(io_tail, i_size, i_alignment);
				if (*io_tail > m_buffer_end)
				{
					// wrap to the start...
					*io_tail = m_buffer_start;
					start_of_block = linear_alloc(io_tail, i_size, i_alignment);
					if (*io_tail >= m_head)
					{
						// ...not enough space before the head, failed!
						start_of_block = nullptr;
						*io_tail = prev_tail;
					}
				}
				else if ((prev_tail >= m_head) != (*io_tail >= m_head))
				{
					// ...crossed the head, failed!
					start_of_block = nullptr;
					*io_tail = prev_tail;
				}
				return start_of_block;
			}

			template <typename CONSTRUCTOR>
				bool impl_push(const ELEMENT_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
			{
				auto tail = static_cast<void*>(m_tail);
				const auto type_block = single_push(&tail, sizeof(ELEMENT_TYPE), alignof(ELEMENT_TYPE) );
				const auto element_block = single_push(&tail, i_source_type.size(), i_source_type.alignment());
				if (element_block == nullptr || type_block == nullptr)
				{
					return false;
				}

				// commit the push
				i_constructor(element_block, i_source_type);
				new(type_block) ELEMENT_TYPE(i_source_type);				
				m_tail = static_cast<ELEMENT_TYPE*>(tail);

				assert(tail == address_add(element_block, i_source_type.size()));

				return true;
			}

			template <typename OPERATION>
				void impl_consume(OPERATION && i_operation)
					REFLECTIVE_NOEXCEPT_V(REFLECTIVE_NOEXCEPT_V(i_operation(std::declval<ELEMENT_TYPE>(), std::declval<void*>())))
			{
				assert(m_head != m_tail); // the queue must not be empty
		
				auto type_ptr = static_cast<ELEMENT_TYPE*>(address_upper_align(m_head, alignof(ELEMENT_TYPE)));
				auto type_end = static_cast<void*>(type_ptr + 1);
				if (type_end > m_buffer_end)
				{
					type_ptr = static_cast<ELEMENT_TYPE*>(address_upper_align(m_buffer_start, alignof(ELEMENT_TYPE)));
					type_end = static_cast<void*>(type_ptr + 1);
				}

				auto const element_size = type_ptr->size();
				auto const element_alignment = type_ptr->alignment();

				auto element_end = type_end;
				auto element_ptr = linear_alloc(&element_end, element_size, element_alignment);
				if (element_end > m_buffer_end)
				{
					element_end = m_buffer_start;
					element_ptr = linear_alloc(&element_end, element_size, element_alignment);
				}

				// commit
				i_operation(*type_ptr, element_ptr);
				m_head = static_cast<ELEMENT_TYPE*>(element_end);
			}

			size_t impl_mem_capacity() const REFLECTIVE_NOEXCEPT
			{
				return address_diff(m_buffer_end, m_buffer_start);
			}

			size_t impl_mem_size() const REFLECTIVE_NOEXCEPT
			{
				if (m_head <= m_tail)
				{
					return address_diff(m_tail, m_head);
				}
				else
				{
					return address_diff(m_buffer_end, m_head) + address_diff(m_tail, m_buffer_start);
				}				
			}

		private:
			
			void impl_clear() REFLECTIVE_NOEXCEPT
			{
				IteratorBase it = impl_begin();
				while (it.m_curr_type != m_tail)
				{
					auto const type = it.m_curr_type;
					auto const element = it.m_curr_element;
					it.move_next();

					type->destroy(element);
					type->ELEMENT_TYPE::~ELEMENT_TYPE();
				}
				m_head = static_cast<ELEMENT_TYPE*>(m_tail);
			}

		private:
			ELEMENT_TYPE * m_head; // poinnts to the first ELEMENT_TYPE
			void * m_tail; // points to the end of the last element - if = m_tail the queue is empty
			void * m_buffer_start;
			void * m_buffer_end;
		}; // class DenseFixedQueueBase

	} // namespace details

	template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename ELEMENT_TYPE = ElementType<ELEMENT> >
		class DenseFixedQueue final
	{		
	public:

		using ElementType = ELEMENT_TYPE;
		using allocator_type = ALLOCATOR;
		using value_type = ELEMENT;
		using reference = ELEMENT &;
		using const_reference = const ELEMENT &;
		using pointer = typename std::allocator_traits<allocator_type>::pointer;
		using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
		using difference_type = ptrdiff_t;
		using size_type = size_t;
		class iterator;
		class const_iterator;

		DenseFixedQueue(size_t i_buffer_byte_capacity)
			: m_impl(i_buffer_byte_capacity) { }

		class iterator final
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = ELEMENT;
			using reference = ELEMENT &;
			using const_reference = const ELEMENT &;
			using pointer = typename std::allocator_traits<allocator_type>::pointer;
			using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

			iterator(const typename details::DenseFixedQueueBase<ALLOCATOR, ELEMENT_TYPE>::IteratorBase & i_source) REFLECTIVE_NOEXCEPT
				: m_impl(i_source) {  }

			value_type & operator * () const REFLECTIVE_NOEXCEPT { return *static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * operator -> () const REFLECTIVE_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * curr_element() const REFLECTIVE_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }

			iterator & operator ++ () REFLECTIVE_NOEXCEPT
			{
				m_impl.move_next();
				return *this;
			}

			iterator operator++ (int) REFLECTIVE_NOEXCEPT
			{
				const iterator copy(*this);
				m_impl.move_next();
				return copy;
			}

			bool operator == (const iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_impl.m_curr_type == i_other.curr_type();
			}

			bool operator != (const iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_impl.m_curr_type != i_other.curr_type();
			}

			bool operator == (const const_iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_impl.m_curr_type == i_other.curr_type();
			}

			bool operator != (const const_iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_impl.m_curr_type != i_other.curr_type();
			}

			const ELEMENT_TYPE * curr_type() const REFLECTIVE_NOEXCEPT { return m_impl.m_curr_type; }

		private:
			typename details::DenseFixedQueueBase<ALLOCATOR, ELEMENT_TYPE>::IteratorBase m_impl;

		}; // class iterator

		class const_iterator final
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = const ELEMENT;
			using reference = const ELEMENT &;
			using const_reference = const ELEMENT &;
			using pointer = typename std::allocator_traits<allocator_type>::pointer;
			using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

			const_iterator(const typename details::DenseFixedQueueBase<ALLOCATOR, ELEMENT_TYPE>::IteratorBase & i_source) REFLECTIVE_NOEXCEPT
				: m_impl(i_source) {  }

			const_iterator(const iterator & i_source) REFLECTIVE_NOEXCEPT
				: m_impl(i_source.m_impl) {  }

			value_type & operator * () const REFLECTIVE_NOEXCEPT { return *static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * operator -> () const REFLECTIVE_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * curr_element() const REFLECTIVE_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }

			const_iterator & operator ++ () REFLECTIVE_NOEXCEPT
			{
				m_impl.move_next();
				return *this;
			}

			const_iterator operator++ (int) REFLECTIVE_NOEXCEPT
			{
				const iterator copy(*this);
				m_impl.move_next();
				return copy;
			}

			bool operator == (const iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_curr_type == i_other.curr_type();
			}

			bool operator != (const iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_curr_type != i_other.curr_type();
			}

			bool operator == (const const_iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_impl.m_curr_type == i_other.curr_type();
			}

			bool operator != (const const_iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_impl.m_curr_type != i_other.curr_type();
			}

			const ELEMENT_TYPE * curr_type() const REFLECTIVE_NOEXCEPT { return m_impl.m_curr_type; }

		private:
			typename details::DenseFixedQueueBase<ALLOCATOR, ELEMENT_TYPE>::IteratorBase m_impl;
		}; // class const_iterator

		iterator begin() REFLECTIVE_NOEXCEPT { return iterator(m_impl.impl_begin()); }
		iterator end() REFLECTIVE_NOEXCEPT { return iterator(m_impl.impl_end()); }

		const_iterator begin() const REFLECTIVE_NOEXCEPT { return const_iterator(m_impl.impl_begin()); }
		const_iterator end() const REFLECTIVE_NOEXCEPT { return const_iterator(m_impl.impl_end()); }

		const_iterator cbegin() const REFLECTIVE_NOEXCEPT { return const_iterator(m_impl.impl_begin()); }
		const_iterator cend() const REFLECTIVE_NOEXCEPT { return const_iterator(m_impl.impl_end()); }

		bool empty() const REFLECTIVE_NOEXCEPT { return m_impl.impl_empty(); }

		void clear() REFLECTIVE_NOEXCEPT { m_impl.impl_clear(); }

		template <typename ELEMENT_COMPLETE_TYPE>
			bool try_push(const ELEMENT_COMPLETE_TYPE & i_source)
				// REFLECTIVE_NOEXCEPT_V()
		{
			return m_impl.impl_push(ElementType::template make<ELEMENT_COMPLETE_TYPE>(), 
					typename details::DenseFixedQueueBase<ALLOCATOR, ELEMENT_TYPE>::CopyConstruct(&i_source));
		}

		template <typename OPERATION>
			void consume(OPERATION && i_operation)
		{
			m_impl.impl_consume([&i_operation](const ELEMENT_TYPE & i_type, void * i_element) {
				i_operation(i_type, *static_cast<ELEMENT*>(i_element));
			});
		}

		void pop()
		{
			m_impl.impl_consume_front([](const ELEMENT_TYPE &, void *) {});
		}

		const ELEMENT & front()
		{
			assert(!empty());
			const auto it = m_impl.impl_begin();
			return *static_cast<value_type *>(it.m_curr_element);
		}

		size_t mem_capacity() const REFLECTIVE_NOEXCEPT
		{
			return m_impl.impl_mem_capacity();
		}

		size_t mem_size() const REFLECTIVE_NOEXCEPT
		{
			return m_impl.impl_mem_size();
		}

	private:
		details::DenseFixedQueueBase<ALLOCATOR, ELEMENT_TYPE> m_impl;

	}; // class DenseFixedQueue
}
