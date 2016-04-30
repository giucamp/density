
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
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
		/** This internal class template implements an heterogeneous fifo sequence that allocates the elements on an externally-owned 
		   memory buffer. QueueImpl is moveable but not copyable.
		   A null-QueueImpl is a QueueImpl with no associated memory buffer. A default constructed QueueImpl is a null-QueueImpl. The
		   source of a move-costructor or move-assignment becomes a null-QueueImpl. The only way for null-QueueImpl to become a non-
		   null-QueueImpl is being the destination of a move-assigment with a non-null source.A null-QueueImpl is always empty, and a
		   try_push on it results in undefined behaviour.
		   Implementation: the layout of the buffer is composed by a linear-alocated sequence of Control-element pairs. This squence
		   wraps to the boundaries of the memory buffer. Control is a private struct that contains:
		      - the RUNTIME_TYPE associated to the element. If RUNTIME_TYPE is an empty struct, no storage is wasted (Control exploits
			    the empty base optimization).
			  - a pointer to the element. This pointer does not point to the end of the Control, as:
					* the storage of element is properly aligned
					* the pointer may point to a subobject of the element, in case of typed containers (that if the public container has
					  a non-void type). Note: the address of a subobject (the base class "part") is not equal to the address of the
					  complete type (that is, a static-casting a pointer is not a no-operation).
		            * this pointer may wrap to the beginning of the buffer, when there is not enough space in the buffer after the Control.
			  - a pointer to the Control of the next element. This pointer is corretcly aligned, but its content is undefined if this
			    element is the last one. Usualy this points to the end of the element, upper-aligned according to the alignment requirement
				of Control. This pointer may wrap to the beginning of the memory buffer. */
		template <typename RUNTIME_TYPE>
			class QueueImpl final
		{
			struct Control;

		public:

			struct IteratorImpl
			{
				/** Construct a IteratorImpl with undefined content. */
				IteratorImpl() DENSITY_NOEXCEPT {}

				IteratorImpl(Control * i_curr_control) DENSITY_NOEXCEPT
					: m_curr_control(i_curr_control) { }

				void operator ++ () DENSITY_NOEXCEPT
				{
					m_curr_control = m_curr_control->m_next;
				}

				bool operator == (const IteratorImpl & i_source) const DENSITY_NOEXCEPT
				{
					return i_source.m_curr_control == i_source.m_curr_control;
				}

				bool operator != (const IteratorImpl & i_source) const DENSITY_NOEXCEPT
				{
					return i_source.m_curr_control != i_source.m_curr_control;
				}

				void * element() const DENSITY_NOEXCEPT
				{
					return m_curr_control->m_element;
				}

				const RUNTIME_TYPE * type() const DENSITY_NOEXCEPT
				{
					return m_curr_control;
				}

			private:
				Control * m_curr_control;
			};

			/** Construct a null-QueueImpl. */
			QueueImpl() DENSITY_NOEXCEPT
				: m_head(nullptr), m_tail(nullptr), m_element_max_alignment(alignof(RUNTIME_TYPE)),
				  m_buffer_start(nullptr), m_buffer_end(nullptr)
			{
			}

			/** Construct a QueueImpl provinding a memory buffer. 
				Precondition: i_buffer_address can't be null,
				*/
			QueueImpl(void * i_buffer_address, size_t i_buffer_byte_capacity) DENSITY_NOEXCEPT
			{
				assert(i_buffer_address != nullptr);
				m_buffer_start = i_buffer_address;
				m_buffer_end = address_add(m_buffer_start, i_buffer_byte_capacity);
				m_head = static_cast<Control*>( address_upper_align(m_buffer_start, alignof(Control)) );
				m_tail = m_head;
				m_element_max_alignment = alignof(Control);
			}

			QueueImpl(QueueImpl && i_source) DENSITY_NOEXCEPT
				: m_head(i_source.m_head), m_tail(i_source.m_tail), m_element_max_alignment(i_source.m_element_max_alignment),
				  m_buffer_start(i_source.m_buffer_start), m_buffer_end(i_source.m_buffer_end)
			{
				i_source.m_tail = i_source.m_head = nullptr;
				i_source.m_buffer_end = i_source.m_buffer_start = nullptr;
				i_source.m_element_max_alignment = alignof(Control);
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
				const IteratorImpl end_it = i_source.end();
				while( it != end_it )
				{
					auto const type = it.type();
					auto const source_element = it.element();
					++it;

					bool result = try_push(*type,
						typename detail::QueueImpl<RUNTIME_TYPE>::MoveConstruct(source_element));
					assert(result);

					type->destroy(source_element);
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				}
				// set the source as empty
				i_source.m_tail = i_source.m_head = static_cast<Control*>(address_upper_align(i_source.m_buffer_start, alignof(Control)));
			}

			/* Copies the elements from i_source to this queue. This queue must have enough space to 
				allocate space for all the elements of i_source, otherwise the behaviour is undefined. */
			void copy_elements_from(const QueueImpl & i_source) DENSITY_NOEXCEPT
			{
				IteratorImpl it = i_source.begin();
				const IteratorImpl end_it = i_source.end();
				while (it != end_it)
				{
					auto const type = it.type();
					auto const source_element = it.element();
					++it;

					bool result = try_push(*type,
						typename detail::QueueImpl<RUNTIME_TYPE>::CopyConstruct(source_element));
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
				return IteratorImpl(m_head);			
			}

			IteratorImpl end() const DENSITY_NOEXCEPT
			{
				return IteratorImpl(m_tail);
			}
			
			struct CopyConstruct
			{
				const void * const m_source;

				CopyConstruct(const void * i_source) DENSITY_NOEXCEPT
					: m_source(i_source) { }

				void * operator () (const RUNTIME_TYPE & i_element_type, void * i_dest)
				{
					return i_element_type.copy_construct(i_dest, m_source);
				}
			};

			struct MoveConstruct
			{
				void * const m_source;

				MoveConstruct(void * i_source) DENSITY_NOEXCEPT
					: m_source(i_source) { }

				void * operator () (const RUNTIME_TYPE & i_element_type, void * i_dest) DENSITY_NOEXCEPT
				{
					return i_element_type.move_construct_nothrow(i_dest, m_source);
				}
			};

			/** Tries to insert a new element in the queue. If this is a null-QueueImpl, the behaviour is undefined.
				@param i_source_type type of the element to insert
				@param i_constructor function object that is used to construct the new element. The expected signature 
					of the function is:
						void * (const RUNTIME_TYPE & i_source_type, void * i_new_element_place)
				@return true if the element was successfully inserted, false otherwise */
			template <typename CONSTRUCTOR>
				bool try_push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
					DENSITY_NOEXCEPT_V(DENSITY_NOEXCEPT_V(i_constructor(std::declval<RUNTIME_TYPE>(), std::declval<void*>())))
			{
				// test preconditions
				assert(m_buffer_start != nullptr);

				// test internal preconditions
				assert(m_buffer_start != nullptr);

				const auto element_aligment = i_source_type.alignment();

				Control * curr_control = m_tail;
				void * new_tail = curr_control + 1;
				void * curr_element = single_push(&new_tail, i_source_type.size(), element_aligment);
				void * next_control = single_push(&new_tail, sizeof(Control), alignof(Control) );
				if (curr_element == nullptr || next_control == nullptr)
				{
					return false;
				}

				new(curr_control) Control(std::move(i_source_type), static_cast<Control*>(next_control));
				curr_control->m_element = i_constructor(i_source_type, curr_element);
				
				m_tail = static_cast<Control *>(next_control);
				m_element_max_alignment = size_max(m_element_max_alignment, element_aligment);
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
				assert(!empty()); // the queue must not be empty

				Control * first_control = m_head;
				const RUNTIME_TYPE & type = *first_control;
				void * const element_ptr = first_control->m_element;
				i_operation(type, element_ptr);
				m_head = first_control->m_next;
				type.destroy(element_ptr);
				type.RUNTIME_TYPE::~RUNTIME_TYPE();
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
				IteratorImpl const it_end = end();
				for (IteratorImpl it = begin(); it != it_end; )
				{
					auto const type = it.type();
					auto const element = it.element();
					++it;

					type->destroy(element);
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				}
				// restart from m_buffer_start, with an empty queue
				m_tail = m_head = static_cast<Control*>(address_upper_align(m_buffer_start, alignof(Control)));
			}

			size_t element_max_alignment() const DENSITY_NOEXCEPT { return m_element_max_alignment; }

		private:

			struct Control : RUNTIME_TYPE
			{
				void * m_element;
				Control * m_next;

				Control(const RUNTIME_TYPE & i_type, Control * i_next)
					: RUNTIME_TYPE(i_type), m_element(nullptr), m_next(i_next) {}
			};

			/* Allocates an object on the queue. The return value is the address of the new object.
			   This function is used to push the Control and the element. If the required size with the
			   required alignment does not fit in the queue the return value is nullptr. 
			   Precoditions: *io_tail can't be null, or the behaviour is undefined.
			   */
			void * single_push(void * * io_tail, size_t i_size, size_t i_alignment) DENSITY_NOEXCEPT
			{
				auto const prev_tail = *io_tail;
				assert(prev_tail != nullptr);
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
			Control * m_head; /**< points to the first RUNTIME_TYPE. If m_tail==m_head the queue is empty, otherwise this member points
										to a valid type. */
			Control * m_tail; /**< points to the end of the last element. When pushing, m_tail is first aligned, and then a RUNTIME_TYPE is
								constructed on it. After the push m_tail points to the end of the last element. */
			size_t m_element_max_alignment; /**< The maximum between alignof(RUNTIME_TYPE) and the maximum alignment of the alignment of the
											elements in the container. This field has a specific getter (element_max_alignment), and it
											is required to allocate a memory buffer big enough to contain all the elements. */
			void * m_buffer_start, * m_buffer_end; /**< range of the memory buffer */
		}; // class template QueueImpl

	} // namespace detail

} // namespace density
