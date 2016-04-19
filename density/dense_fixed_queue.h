
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <memory> // std::allocator
#include <utility> // std::forward
#include <type_traits> // std::is_constructible, ...
#include "runtime_type.h"

namespace density
{
	namespace detail
	{
		template < typename ALLOCATOR, typename RUNTIME_TYPE >
			class DenseFixedQueueImpl : private ALLOCATOR
		{
		public:

			struct IteratorBaseImpl
			{
				IteratorBaseImpl() DENSITY_NOEXCEPT {}

				IteratorBaseImpl(RUNTIME_TYPE * i_type) DENSITY_NOEXCEPT // used to construct end
					: m_curr_type(i_type) { }

				IteratorBaseImpl(const DenseFixedQueueImpl * i_queue, RUNTIME_TYPE * i_type, void * i_element) DENSITY_NOEXCEPT
					: m_curr_type(i_type), m_curr_element(i_element), m_queue(i_queue) { }

				void move_next() DENSITY_NOEXCEPT
				{
					// advance m_curr_type
					m_curr_type = static_cast<RUNTIME_TYPE*>(address_add(m_curr_element, m_curr_type->size()));
					if (m_curr_type != m_queue->m_tail)
					{
						m_curr_type = static_cast<RUNTIME_TYPE*>(address_upper_align(m_curr_type, alignof(RUNTIME_TYPE)));
						if (m_curr_type + 1 > m_queue->m_buffer_end)
						{
							m_curr_type = static_cast<RUNTIME_TYPE*>(address_upper_align(m_queue->m_buffer_start, alignof(RUNTIME_TYPE)));
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

				bool operator == (const IteratorBaseImpl & i_source) DENSITY_NOEXCEPT
				{
					return i_source.m_curr_type == i_source.m_curr_type;
				}

				bool is_end() const DENSITY_NOEXCEPT
				{
					return m_curr_type == m_queue->m_tail;
				}

				RUNTIME_TYPE * m_curr_type;
				void * m_curr_element;
				const DenseFixedQueueImpl * m_queue;
			};

			DenseFixedQueueImpl(size_t i_buffer_byte_capacity)
			{
				impl_init(i_buffer_byte_capacity);
			}

			DenseFixedQueueImpl(DenseFixedQueueImpl && i_source) DENSITY_NOEXCEPT
				: m_head(i_source.m_head), m_tail(i_source.m_tail), m_buffer_start(i_source.m_buffer_start), m_buffer_end(i_source.m_buffer_end)
			{
				i_source.m_tail = i_source.m_head = nullptr;
				i_source.m_buffer_end = i_source.m_buffer_start = nullptr;
			}

			DenseFixedQueueImpl(const DenseFixedQueueImpl & i_source)
				: ALLOCATOR(i_source)
			{
				impl_assign(i_source);
			}

			DenseFixedQueueImpl & operator = (const DenseFixedQueueImpl & i_source)
			{
				impl_destroy();
				m_buffer_start = nullptr; // if impl_assign throws, the destructor must not get dirt data
				*static_cast<ALLOCATOR*>(this) = i_source;
				impl_assign(i_source);
				return *this;
			}

			DenseFixedQueueImpl & operator = (DenseFixedQueueImpl && i_source) DENSITY_NOEXCEPT
			{
				impl_destroy();

				m_head = i_source.m_head;
				m_tail = i_source.m_tail;
				m_buffer_start = i_source.m_buffer_start;
				m_buffer_end = i_source.m_buffer_end;

				i_source.m_tail = i_source.m_head = nullptr;
				i_source.m_buffer_end = i_source.m_buffer_start = nullptr;

				return *this;
			}

			~DenseFixedQueueImpl() DENSITY_NOEXCEPT
			{
				impl_destroy();
			}

			void impl_init(size_t i_buffer_byte_capacity)
			{
				typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(
					*static_cast<ALLOCATOR*>(this));

				m_buffer_start = char_alloc.allocate(i_buffer_byte_capacity);
				m_buffer_end = address_add(m_buffer_start, i_buffer_byte_capacity);
				m_tail = m_head = static_cast<RUNTIME_TYPE *>(m_buffer_start);
			}

			void impl_destroy()
			{
				impl_clear();

				typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(
					*static_cast<ALLOCATOR*>(this));

				char_alloc.deallocate(static_cast<char*>(m_buffer_start),
					address_diff(m_buffer_end, m_buffer_start));
			}

			void impl_assign(const DenseFixedQueueImpl & i_source)
			{
				impl_init(i_source.impl_mem_capacity());

				/* now the queue is empty, and m_tail = m_head = m_buffer_start. Anyway
					we offset m_tail and m_head like they are in the source, in order to make
					an exact copy, in which the free space is the same */
				m_tail = m_head = static_cast<RUNTIME_TYPE*>(address_add(m_buffer_start,
					address_diff(i_source.m_head, i_source.m_buffer_start) ) );

				for (auto source_it = i_source.impl_begin();
					source_it.m_curr_type != static_cast<RUNTIME_TYPE*>(i_source.m_tail); source_it.move_next())
				{
					impl_push(*source_it.m_curr_type, CopyConstruct(source_it.m_curr_element));
				}
			}

			bool impl_empty() const DENSITY_NOEXCEPT
			{
				return m_head == m_tail;
			}

			IteratorBaseImpl impl_begin() const DENSITY_NOEXCEPT
			{
				if (m_head == m_tail)
				{
					return IteratorBaseImpl(static_cast<RUNTIME_TYPE*>(m_tail));
				}
				else
				{
					auto type_ptr = static_cast<RUNTIME_TYPE*>(address_upper_align(m_head, alignof(RUNTIME_TYPE)));
					auto type_end = static_cast<void*>(type_ptr + 1);
					if (type_end > m_buffer_end)
					{
						type_ptr = static_cast<RUNTIME_TYPE*>(address_upper_align(m_buffer_start, alignof(RUNTIME_TYPE)));
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

					return IteratorBaseImpl(this, type_ptr, element_ptr);
				}
			}

			IteratorBaseImpl impl_end() const DENSITY_NOEXCEPT
			{
				return IteratorBaseImpl(static_cast<RUNTIME_TYPE*>(m_tail));
			}
			
			struct CopyConstruct
			{
				const void * const m_source;

				CopyConstruct(const void * i_source)
					: m_source(i_source) { }

				void operator () (void * i_dest, const RUNTIME_TYPE & i_element_type)
				{
					i_element_type.copy_construct(i_dest, m_source);
				}
			};

			struct MoveConstruct
			{
				void * const m_source;

				MoveConstruct(void * i_source)
					: m_source(i_source) { }

				void operator () (void * i_dest, const RUNTIME_TYPE & i_element_type) DENSITY_NOEXCEPT
				{
					i_element_type.move_construct(i_dest, m_source);
				}
			};

			/* Inserts an object on the queue. The return value is the address of the new object */
			void * single_push(void * * io_tail, size_t i_size, size_t i_alignment) DENSITY_NOEXCEPT
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
				bool impl_push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
			{
				auto tail = static_cast<void*>(m_tail);
				const auto type_block = single_push(&tail, sizeof(RUNTIME_TYPE), alignof(RUNTIME_TYPE) );
				const auto element_block = single_push(&tail, i_source_type.size(), i_source_type.alignment());
				if (element_block == nullptr || type_block == nullptr)
				{
					return false;
				}

				// commit the push
				i_constructor(element_block, i_source_type);
				new(type_block) RUNTIME_TYPE(i_source_type);				
				m_tail = static_cast<RUNTIME_TYPE*>(tail);

				assert(tail == address_add(element_block, i_source_type.size()));

				return true;
			}

			template <typename OPERATION>
				void impl_consume(OPERATION && i_operation)
					DENSITY_NOEXCEPT_V(DENSITY_NOEXCEPT_V(i_operation(std::declval<RUNTIME_TYPE>(), std::declval<void*>())))
			{
				assert(m_head != m_tail); // the queue must not be empty
		
				auto type_ptr = static_cast<RUNTIME_TYPE*>(address_upper_align(m_head, alignof(RUNTIME_TYPE)));
				auto type_end = static_cast<void*>(type_ptr + 1);
				if (type_end > m_buffer_end)
				{
					type_ptr = static_cast<RUNTIME_TYPE*>(address_upper_align(m_buffer_start, alignof(RUNTIME_TYPE)));
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
				type_ptr->destroy(element_ptr);
				type_ptr->RUNTIME_TYPE::~RUNTIME_TYPE();
				m_head = static_cast<RUNTIME_TYPE*>(element_end);
			}

			size_t impl_mem_capacity() const DENSITY_NOEXCEPT
			{
				return address_diff(m_buffer_end, m_buffer_start);
			}

			size_t impl_mem_size() const DENSITY_NOEXCEPT
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
			
			void impl_clear() DENSITY_NOEXCEPT
			{
				IteratorBaseImpl it = impl_begin();
				while (it.m_curr_type != m_tail)
				{
					auto const type = it.m_curr_type;
					auto const element = it.m_curr_element;
					it.move_next();

					type->destroy(element);
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				}
				m_head = static_cast<RUNTIME_TYPE*>(m_tail);
			}

		private:
			RUNTIME_TYPE * m_head; // poinnts to the first RUNTIME_TYPE
			void * m_tail; // points to the end of the last element - if = m_tail the queue is empty
			void * m_buffer_start;
			void * m_buffer_end;
		}; // class DenseFixedQueueImpl

	} // namespace detail

	template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = RuntimeType<ELEMENT> >
		class DenseFixedQueue final
	{
	public:

		using RuntimeType = RUNTIME_TYPE;
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

			iterator() DENSITY_NOEXCEPT {}

			iterator(const typename detail::DenseFixedQueueImpl<ALLOCATOR, RUNTIME_TYPE>::IteratorBaseImpl & i_source) DENSITY_NOEXCEPT
				: m_impl(i_source) {  }

			value_type & operator * () const DENSITY_NOEXCEPT { return *static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * curr_element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }

			iterator & operator ++ () DENSITY_NOEXCEPT
			{
				m_impl.move_next();
				return *this;
			}

			iterator operator++ (int) DENSITY_NOEXCEPT
			{
				const iterator copy(*this);
				m_impl.move_next();
				return copy;
			}

			bool operator == (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type == i_other.curr_type();
			}

			bool operator != (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type != i_other.curr_type();
			}

			bool operator == (const const_iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type == i_other.curr_type();
			}

			bool operator != (const const_iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type != i_other.curr_type();
			}

			const RUNTIME_TYPE * curr_type() const DENSITY_NOEXCEPT { return m_impl.m_curr_type; }

			bool is_end() const DENSITY_NOEXCEPT
			{
				return m_impl.is_end();
			}

		private:
			typename detail::DenseFixedQueueImpl<ALLOCATOR, RUNTIME_TYPE>::IteratorBaseImpl m_impl;

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

			const_iterator() DENSITY_NOEXCEPT {}

			const_iterator(const typename detail::DenseFixedQueueImpl<ALLOCATOR, RUNTIME_TYPE>::IteratorBaseImpl & i_source) DENSITY_NOEXCEPT
				: m_impl(i_source) {  }

			const_iterator(const iterator & i_source) DENSITY_NOEXCEPT
				: m_impl(i_source.m_impl) {  }

			value_type & operator * () const DENSITY_NOEXCEPT { return *static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * curr_element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }

			const_iterator & operator ++ () DENSITY_NOEXCEPT
			{
				m_impl.move_next();
				return *this;
			}

			const_iterator operator++ (int) DENSITY_NOEXCEPT
			{
				const iterator copy(*this);
				m_impl.move_next();
				return copy;
			}

			bool operator == (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_curr_type == i_other.curr_type();
			}

			bool operator != (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_curr_type != i_other.curr_type();
			}

			bool operator == (const const_iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type == i_other.curr_type();
			}

			bool operator != (const const_iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type != i_other.curr_type();
			}

			const RUNTIME_TYPE * curr_type() const DENSITY_NOEXCEPT { return m_impl.m_curr_type; }

			bool is_end() const DENSITY_NOEXCEPT
			{
				return m_impl.is_end();
			}

		private:
			typename detail::DenseFixedQueueImpl<ALLOCATOR, RUNTIME_TYPE>::IteratorBaseImpl m_impl;
		}; // class const_iterator

		iterator begin() DENSITY_NOEXCEPT { return iterator(m_impl.impl_begin()); }
		iterator end() DENSITY_NOEXCEPT { return iterator(m_impl.impl_end()); }

		const_iterator begin() const DENSITY_NOEXCEPT { return const_iterator(m_impl.impl_begin()); }
		const_iterator end() const DENSITY_NOEXCEPT { return const_iterator(m_impl.impl_end()); }

		const_iterator cbegin() const DENSITY_NOEXCEPT { return const_iterator(m_impl.impl_begin()); }
		const_iterator cend() const DENSITY_NOEXCEPT { return const_iterator(m_impl.impl_end()); }

		bool empty() const DENSITY_NOEXCEPT { return m_impl.impl_empty(); }

		void clear() DENSITY_NOEXCEPT { m_impl.impl_clear(); }

		template <typename ELEMENT_COMPLETE_TYPE>
			bool try_push(ELEMENT_COMPLETE_TYPE && i_source)				
		{
			return try_push_impl(std::forward<ELEMENT_COMPLETE_TYPE>(i_source), typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type() );
		}

		template <typename ELEMENT_COMPLETE_TYPE, typename... PARAMETERS>
			bool try_emplace(PARAMETERS && ... i_arguments)
				DENSITY_NOEXCEPT_V((std::is_nothrow_constructible<ELEMENT_COMPLETE_TYPE, PARAMETERS...>::value))
		{
			return m_impl.impl_push(RuntimeType::template make<ELEMENT_COMPLETE_TYPE>(),
				[&i_arguments...]( void * i_dest, const RuntimeType & ) {
					new(i_dest) ELEMENT_COMPLETE_TYPE(std::forward<PARAMETERS>(i_arguments)...);
			});
		}

		bool try_copy_push(const RUNTIME_TYPE & i_type, const ELEMENT * i_source )
		{
			return m_impl.impl_push(i_type, 
				typename detail::DenseFixedQueueImpl<ALLOCATOR, RUNTIME_TYPE>::CopyConstruct(i_source) );
		}

		bool try_move_push(const RUNTIME_TYPE & i_type, ELEMENT * i_source) DENSITY_NOEXCEPT
		{
			return m_impl.impl_push(i_type,
				typename detail::DenseFixedQueueImpl<ALLOCATOR, RUNTIME_TYPE>::MoveConstruct(i_source));
		}

		template <typename OPERATION>
			void consume(OPERATION && i_operation)
				DENSITY_NOEXCEPT_V(DENSITY_NOEXCEPT_V((i_operation( std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT>() ))))
		{
			m_impl.impl_consume([&i_operation](const RUNTIME_TYPE & i_type, void * i_element) {
				i_operation(i_type, *static_cast<ELEMENT*>(i_element));
			});
		}

		void pop() DENSITY_NOEXCEPT
		{
			m_impl.impl_consume([](const RUNTIME_TYPE &, void *) {});
		}

		const ELEMENT & front() DENSITY_NOEXCEPT
		{
			assert(!empty());
			const auto it = m_impl.impl_begin();
			return *static_cast<value_type *>(it.m_curr_element);
		}

		size_t mem_capacity() const DENSITY_NOEXCEPT
		{
			return m_impl.impl_mem_capacity();
		}

		size_t mem_size() const DENSITY_NOEXCEPT
		{
			return m_impl.impl_mem_size();
		}

		size_t mem_free() const DENSITY_NOEXCEPT
		{
			return m_impl.impl_mem_capacity() - m_impl.impl_mem_size();
		}

	private:

		// overload used if i_source is an rvalue
		template <typename ELEMENT_COMPLETE_TYPE>
			bool try_push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::true_type)
				DENSITY_NOEXCEPT_V((std::is_nothrow_move_constructible<ELEMENT_COMPLETE_TYPE>::value))
		{
			return m_impl.impl_push(RuntimeType::template make<typename detail::RemoveRefsAndConst<ELEMENT_COMPLETE_TYPE>::type>(),
				typename detail::DenseFixedQueueImpl<ALLOCATOR, RUNTIME_TYPE>::MoveConstruct(&i_source));
		}

		// overload used if i_source is an lvalue
		template <typename ELEMENT_COMPLETE_TYPE>
			bool try_push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::false_type)
				DENSITY_NOEXCEPT_V((std::is_nothrow_copy_constructible<ELEMENT_COMPLETE_TYPE>::value))
		{
			return m_impl.impl_push(RuntimeType::template make<typename detail::RemoveRefsAndConst<ELEMENT_COMPLETE_TYPE>::type>(),
				typename detail::DenseFixedQueueImpl<ALLOCATOR, RUNTIME_TYPE>::CopyConstruct(&i_source));
		}

	private:
		detail::DenseFixedQueueImpl<ALLOCATOR, RUNTIME_TYPE> m_impl;

	}; // class DenseFixedQueue
}
