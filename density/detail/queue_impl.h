
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "..\density_common.h"
#include "..\runtime_type.h"

namespace density
{
	namespace detail
	{
		template < typename RUNTIME_TYPE, size_t DEFAULT_ELEMENT_ALIGNMENT, bool RUNTIME_TYPE_HAS_ALIGNMENT = true >
			struct AlignmentForElement;
		template < typename RUNTIME_TYPE, size_t DEFAULT_ELEMENT_ALIGNMENT>
			struct AlignmentForElement<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT, true>
		{
			static size_t value(const RUNTIME_TYPE & i_type) DENSITY_NOEXCEPT
			{
				return i_type.alignment();
			}
		};

		/* This internal class template implements an heterogeneous fifo sequence that allocates the elements on an externally-owned 
		   memory buffer. QueueImpl is moveable but not copyable. After a move construction or assignment, the source QueueImpl 
		   loses the reference to the memory buffer, and it is left defaut constructed. */
		template < typename RUNTIME_TYPE, size_t DEFAULT_ELEMENT_ALIGNMENT > class QueueImpl
		{
		private:

			static size_t alignment_for(const RUNTIME_TYPE & i_type) DENSITY_NOEXCEPT
			{
				return AlignmentForElement<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>::value(i_type);
			}

		public:

			/** Iterator like class. Create on with QueueImpl::begin, move t the next with IteratorImpl::move_next. The
				end of the iteration cab be tested comparing to QueueImpl::end, or using IteratorImpl::is_end. */
			struct IteratorImpl
			{
				/** Construct a IteratorImpl with undefined content. */
				IteratorImpl() DENSITY_NOEXCEPT {}

				/** Constructor used by QueueImpl::begin. Internal only. */
				IteratorImpl(const QueueImpl * i_queue, RUNTIME_TYPE * i_type, void * i_element) DENSITY_NOEXCEPT
					: m_curr_type(i_type), m_curr_element(i_element), m_queue(i_queue) { }

				/** Constructor used by QueueImpl::end. Internal only. */
				IteratorImpl(const QueueImpl * i_queue, RUNTIME_TYPE * i_type) DENSITY_NOEXCEPT
					: m_curr_type(i_type), m_queue(i_queue) { }

				/** Moves to the next element. Precondition: !is_end() */
				void move_next() DENSITY_NOEXCEPT
				{
					assert(!is_end());

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
						const auto element_alignment = alignment_for(*m_curr_type);
						m_curr_element = address_upper_align(m_curr_type + 1, element_alignment);
						auto end_of_element = address_add(m_curr_element, m_curr_type->size());
						if (end_of_element > m_queue->m_buffer_end)
						{
							m_curr_element = address_upper_align(m_queue->m_buffer_start, element_alignment);
							end_of_element = address_add(m_curr_element, m_curr_type->size());
						}
					}
				}

				bool operator == (const IteratorImpl & i_source) DENSITY_NOEXCEPT
				{
					return i_source.m_curr_type == i_source.m_curr_type;
				}

				bool is_end() const DENSITY_NOEXCEPT
				{
					return m_curr_type == m_queue->m_tail;
				}

				RUNTIME_TYPE * m_curr_type;
				void * m_curr_element;
				const QueueImpl * m_queue;
			};

			QueueImpl() DENSITY_NOEXCEPT
				: m_head(nullptr), m_tail(nullptr), m_element_max_alignment(alignof(RUNTIME_TYPE)),
				  m_buffer_start(nullptr), m_buffer_end(nullptr)
			{
			}

			QueueImpl(void * i_buffer_address, size_t i_buffer_byte_capacity) DENSITY_NOEXCEPT
			{
				m_buffer_start = i_buffer_address;
				m_buffer_end = address_add(m_buffer_start, i_buffer_byte_capacity);
				m_tail = m_head = static_cast<RUNTIME_TYPE *>(m_buffer_start);
				m_element_max_alignment = alignof(RUNTIME_TYPE);
			}

			QueueImpl(QueueImpl && i_source) DENSITY_NOEXCEPT
				: m_head(i_source.m_head), m_tail(i_source.m_tail), m_element_max_alignment(i_source.m_element_max_alignment),
				  m_buffer_start(i_source.m_buffer_start), m_buffer_end(i_source.m_buffer_end)
			{
				i_source.m_tail = i_source.m_head = nullptr;
				i_source.m_buffer_end = i_source.m_buffer_start = nullptr;
				i_source.m_element_max_alignment = alignof(RUNTIME_TYPE);
			}

			QueueImpl & operator = (QueueImpl && i_source) DENSITY_NOEXCEPT
			{
				m_head = i_source.m_head;
				m_tail = i_source.m_tail;
				m_element_max_alignment = i_source.m_element_max_alignment;
				m_buffer_start = i_source.m_buffer_start;
				m_buffer_end = i_source.m_buffer_end;				

				i_source.m_tail = i_source.m_head = nullptr;
				i_source.m_buffer_end = i_source.m_buffer_start = nullptr;
				i_source.m_element_max_alignment = alignof(RUNTIME_TYPE);

				return *this;
			}

			QueueImpl(const QueueImpl & i_source) = delete;

			QueueImpl & operator = (const QueueImpl & i_source) = delete;
						
			/* Moves the elements from i_source to this queue. After the call, i_source will be empty.
				This queue must have enough space to allocate space for all the elements of i_source,
				otherwise the behaviour is undefined. */
			void move_elements_from(QueueImpl & i_source) DENSITY_NOEXCEPT
			{
				IteratorImpl it = i_source.begin();
				while( !it.is_end() )
				{
					auto const type = it.m_curr_type;
					auto const source_element = it.m_curr_element;
					it.move_next();

					bool result = push(*type,
						typename detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>::MoveConstruct(source_element));
					assert(result);

					type->destroy(source_element);
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				}
				// set the source as empty
				i_source.m_head = static_cast<RUNTIME_TYPE*>(i_source.m_tail);
			}

			/* Copies the elements from i_source to this queue. This queue must have enough space to 
				allocate space for all the elements of i_source, otherwise the behaviour is undefined. */
			void copy_elements_from(const QueueImpl & i_source) DENSITY_NOEXCEPT
			{
				IteratorImpl it = i_source.begin();
				while( !it.is_end() )
				{
					auto const type = it.m_curr_type;
					auto const source_element = it.m_curr_element;
					it.move_next();

					bool result = push(*type,
						typename detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>::CopyConstruct(source_element));
					assert(result);
				}
			}

			/** Returns whether the queue is empty. Same to begin()==end() */
			bool empty() const DENSITY_NOEXCEPT
			{
				return m_head == m_tail;
			}

			IteratorImpl begin() const DENSITY_NOEXCEPT
			{
				if (m_head == m_tail)
				{
					return end();
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
					const auto element_alignment = alignment_for(*type_ptr);

					auto element_end = type_end;
					auto element_ptr = linear_alloc(&element_end, element_size, element_alignment);
					if (element_end > m_buffer_end)
					{
						element_end = m_buffer_start;
						element_ptr = linear_alloc(&element_end, element_size, element_alignment);
					}

					return IteratorImpl(this, type_ptr, element_ptr);
				}
			}

			IteratorImpl end() const DENSITY_NOEXCEPT
			{
				return IteratorImpl(this, static_cast<RUNTIME_TYPE*>(m_tail));
			}
			
			struct CopyConstruct
			{
				const void * const m_source;

				CopyConstruct(const void * i_source) DENSITY_NOEXCEPT
					: m_source(i_source) { }

				void operator () (const RUNTIME_TYPE & i_element_type, void * i_dest)
				{
					i_element_type.copy_construct(i_dest, m_source);
				}
			};

			struct MoveConstruct
			{
				void * const m_source;

				MoveConstruct(void * i_source) DENSITY_NOEXCEPT
					: m_source(i_source) { }

				void operator () (const RUNTIME_TYPE & i_element_type, void * i_dest) DENSITY_NOEXCEPT
				{
					i_element_type.move_construct_nothrow(i_dest, m_source);
				}
			};

			/** Tries to insert a new element in the queue.
				@param i_source_type type of the element to insert
				@param i_constructor function object that is used to construct the new element. The expected signature 
					of the function is:
						void (const RUNTIME_TYPE & i_source_type, void * i_new_element_place)
				@return true if the element was successfully inserted, false otherwise */
			template <typename CONSTRUCTOR>
				bool push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
					DENSITY_NOEXCEPT_V(DENSITY_NOEXCEPT_V(i_constructor(std::declval<RUNTIME_TYPE>(), std::declval<void*>())))
			{
				const auto element_aligment = alignment_for(i_source_type);

				auto tail = static_cast<void*>(m_tail);
				const auto type_block = single_push(&tail, sizeof(RUNTIME_TYPE), alignof(RUNTIME_TYPE) );
				const auto element_block = single_push(&tail, i_source_type.size(), element_aligment);
				if (element_block == nullptr || type_block == nullptr)
				{
					return false;
				}

				// commit the push
				m_element_max_alignment = size_max(m_element_max_alignment, element_aligment);
				i_constructor(i_source_type, element_block);
				new(type_block) RUNTIME_TYPE(i_source_type);				
				m_tail = static_cast<RUNTIME_TYPE*>(tail);
				assert(tail == address_add(element_block, i_source_type.size()));
				return true;
			}

			/** Calls the specified function object on the first element (the oldest one), and then
				deletes it from the queue. The queue must be non emty, otherwise the behaviour is undefined. 
				The expected signature of i_operation is:
						void (const RUNTIME_TYPE & i_type, void * i_element) */
			template <typename OPERATION>
				void consume(OPERATION && i_operation)
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
				auto const element_alignment = alignment_for(*type_ptr);

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

			/** Returns a pointer to the beginning of the memory buffer. Note: this is not like a data() method, as
				the data deoes not start here (it starts where m_head points to). */
			void * buffer() DENSITY_NOEXCEPT { return m_buffer_start; }

			/** Returns the size of the memory buffer assigned to the queue */
			MemSize<size_t> mem_capacity() const DENSITY_NOEXCEPT
			{
				return MemSize<size_t>(address_diff(m_buffer_end, m_buffer_start));
			}

			/** Returns how much of the memory buffer is used. */
			MemSize<size_t> mem_size() const DENSITY_NOEXCEPT
			{
				if (m_head <= m_tail)
				{
					return MemSize<size_t>( address_diff(m_tail, m_head) );
				}
				else
				{
					return MemSize<size_t>(address_diff(m_buffer_end, m_head) + address_diff(m_tail, m_buffer_start));
				}				
			}
			
			/** Deletes al the element from the queue. After this call the memory buffer is still
				associated to the queue, but it is empty. */
			void delete_all() DENSITY_NOEXCEPT
			{
				for (IteratorImpl it = begin(); !it.is_end();)
				{
					auto const type = it.m_curr_type;
					auto const element = it.m_curr_element;
					it.move_next();

					type->destroy(element);
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				}
				m_head = static_cast<RUNTIME_TYPE*>(m_tail);
			}

			size_t element_max_alignment() const DENSITY_NOEXCEPT { return m_element_max_alignment; }

		private:

			/* Allocates an object on the queue. The return value is the address of the new object.
			   This function is used to push the RUNTIME_TYPE and the element. If the required size with the
			   required alignment does not fit in the queue the return value is nullptr. */
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

		private: // data members
			RUNTIME_TYPE * m_head; /**< points to the first RUNTIME_TYPE. If m_tail==m_head the queue is empty, otherwise this member points
										to a valid type. */
			void * m_tail; /**< points to the end of the last element. When pushing, m_tail is first aligned, and then a RUNTIME_TYPE is 
								constructed on it. After the push m_tail points to the end of the last element. */
			size_t m_element_max_alignment; /**< The maximum between alignof(RUNTIME_TYPE) and the maximum alignment of the alignment of the
											elements in the container. This field has a specific getter (element_max_alignment), and it
											is required to allocate a memory buffer big enough to contain all the elements. */
			void * m_buffer_start, * m_buffer_end; /**< range of the memory buffer */
		}; // class template QueueImpl

	} // namespace detail

} // namespace density
