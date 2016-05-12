
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
        /** This internal class template implements an heterogeneous FIFO container that allocates the elements on an externally-owned
           memory buffer. QueueImpl is movable but not copyable.
           A null-QueueImpl is a QueueImpl with no associated memory buffer. A default constructed QueueImpl is a null-QueueImpl. The
           source of a move-constructor or move-assignment becomes a null-QueueImpl. The only way for null-QueueImpl to become a non-
           null-QueueImpl is being the destination of a move-assignment with a non-null source. A null-QueueImpl is always empty, and a
           try_push on it results in undefined behavior.
           Implementation: the layout of the buffer is composed by a linear-allocated sequence of Control-element pairs. This sequence
           wraps to the boundaries of the memory buffer. Control is a private struct that contains:
              - the RUNTIME_TYPE associated to the element. If RUNTIME_TYPE is an empty struct, no storage is wasted (Control exploits
                the empty base optimization).
              - a pointer to the element. This pointer does always not point to the end of the Control, as:
                    * the storage of each element is aligned according to its type
                    * this pointer may wrap to the beginning of the buffer, when there is not enough space in the buffer after the Control.
                    * this pointer may point to a subobject of the element, in case of typed containers (that if the public container has
                      a non-void type). Note: the address of a subobject (the base class "part") is not equal to the address of the
                      complete type (that is, a static-casting a pointer is not a no-operation).
              - a pointer to the Control of the next element. The content of the pointed memory is undefined if this
                element is the last one. Usually this points to the end of the element, upper-aligned according to the alignment requirement
                of Control. This pointer may wrap to the beginning of the memory buffer. */
        template <typename RUNTIME_TYPE>
            class QueueImpl final
        {
            // this causes RuntimeTypeConceptCheck<RUNTIME_TYPE> to be specialized, and eventually compilation to fail
            static_assert(sizeof(RuntimeTypeConceptCheck<RUNTIME_TYPE>)>0, "");

            struct Control : RUNTIME_TYPE
            {
                void * const m_element;
                Control * const m_next;

                Control(const RUNTIME_TYPE & i_type, void * i_element, Control * i_next) DENSITY_NOEXCEPT
                    : RUNTIME_TYPE(i_type), m_element(i_element), m_next(i_next)
                {
                }
            };

        public:

            /** Minimum size of a memory buffer. This requirement avoids the need of handling the special case of very small buffers. */
            static const size_t s_minimum_buffer_size = sizeof(Control) * 4;

            /** Minimum alignment of a memory buffer */
            static const size_t s_minimum_buffer_alignment = alignof(Control);

            /** Iterator class, similar to an stl iterator */
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
                    return m_curr_control == i_source.m_curr_control;
                }

                bool operator != (const IteratorImpl & i_source) const DENSITY_NOEXCEPT
                {
                    return m_curr_control != i_source.m_curr_control;
                }

                void * element() const DENSITY_NOEXCEPT
                {
                    return m_curr_control->m_element;
                }

                const RUNTIME_TYPE & complete_type() const DENSITY_NOEXCEPT
                {
                    return *m_curr_control;
                }

                Control * control() const DENSITY_NOEXCEPT
                {
                    return m_curr_control;
                }

            private:
                Control * m_curr_control;
            };

            /** Construct a null-QueueImpl. */
            QueueImpl() DENSITY_NOEXCEPT
                : m_head(nullptr), m_tail(nullptr), m_element_max_alignment(alignof(Control)),
                  m_buffer_start(nullptr), m_buffer_end(nullptr)
            {
            }

            /** Construct a QueueImpl providing a memory buffer.
                Preconditions:
                 * i_buffer_address can't be null
                 * the whole memory buffer must be readable and writable.
                 * i_buffer_byte_capacity must be >= s_minimum_buffer_size
                 * i_buffer_alignment must be >= s_minimum_buffer_alignment */
            QueueImpl(void * i_buffer_address, size_t i_buffer_byte_capacity, size_t i_buffer_alignment = s_minimum_buffer_alignment) DENSITY_NOEXCEPT
            {
                DENSITY_ASSERT(i_buffer_address != nullptr &&
                    i_buffer_byte_capacity >= s_minimum_buffer_size &&
                    i_buffer_alignment >= s_minimum_buffer_alignment); // preconditions

                m_buffer_start = i_buffer_address;
                m_buffer_end = address_add(m_buffer_start, i_buffer_byte_capacity);
                m_head = static_cast<Control*>( address_upper_align(m_buffer_start, i_buffer_alignment) );
                m_tail = m_head;
                m_element_max_alignment = i_buffer_alignment;

                DENSITY_ASSERT(m_head + 1 <= m_buffer_end); // postcondition
            }

            /** Move construct a QueueImpl. The source becomes a null-QueueImpl. */
            QueueImpl(QueueImpl && i_source) DENSITY_NOEXCEPT
                : m_head(i_source.m_head), m_tail(i_source.m_tail), m_element_max_alignment(i_source.m_element_max_alignment),
                  m_buffer_start(i_source.m_buffer_start), m_buffer_end(i_source.m_buffer_end)
            {
                i_source.m_tail = i_source.m_head = nullptr;
                i_source.m_buffer_end = i_source.m_buffer_start = nullptr;
                i_source.m_element_max_alignment = alignof(Control);
            }

            /** Move assigns a QueueImpl. The source becomes a null-QueueImpl. */
            QueueImpl & operator = (QueueImpl && i_source) DENSITY_NOEXCEPT
            {
                m_head = i_source.m_head;
                m_tail = i_source.m_tail;
                m_element_max_alignment = i_source.m_element_max_alignment;
                m_buffer_start = i_source.m_buffer_start;
                m_buffer_end = i_source.m_buffer_end;

                i_source.m_tail = i_source.m_head = nullptr;
                i_source.m_buffer_end = i_source.m_buffer_start = nullptr;
                i_source.m_element_max_alignment = alignof(Control);

                return *this;
            }

            QueueImpl(const QueueImpl & i_source) = delete;
            QueueImpl & operator = (const QueueImpl & i_source) = delete;

            /** Moves the elements from i_source to this queue, move constructing them to this QueueImpl, and destroying
                them from the source. After the call, i_source will be empty.
                This queue must have enough space to allocate space for all the elements of i_source,
                otherwise the behavior is undefined. If you assign to this QueueImpl a memory buffer with the same size
                of the source, but with (at least) the aligned to i_source.element_max_alignment(), the space will always
                be enough.
                Precondition: this queue must be empty, and must have enough space to contain all the element of the source.
                This function never throws. */
            void move_elements_from(QueueImpl & i_source) DENSITY_NOEXCEPT
            {
                DENSITY_ASSERT(empty());
                auto it = i_source.begin();
                const auto end_it = i_source.end();
                while( it != end_it )
                {
                    auto const control = it.control();
                    auto const source_element = it.element(); // get_complete_type( it.control() );
                    ++it;

                    // to do: a slightly optimized nofail_push may be used here
                    bool result = try_push(*control,
                        typename detail::QueueImpl<RUNTIME_TYPE>::MoveConstruct(source_element));
                    DENSITY_ASSERT(result);
                    DENSITY_UNUSED(result);

                    control->destroy(source_element);
                    control->Control::~Control();
                }
                // set the source as empty
                i_source.m_tail = i_source.m_head = static_cast<Control*>(address_upper_align(i_source.m_buffer_start, alignof(Control)));
                i_source.m_element_max_alignment = alignof(Control);
            }

            /** Copies the elements from i_source to this queue. This queue must have enough space to
                allocate space for all the elements of i_source, otherwise the behavior is undefined.
                If you assign to this QueueImpl a memory buffer with the same size of the source, but
                with (at least) the aligned to i_source.element_max_alignment(), the space will always be enough.
                Precondition: this queue must be empty, and must have enough space to contain all the element of the source.
                Can throw: anything that the copy constructor of the elements can throw
                Exception guarantee: strong (if an exception is raised during this function, this object is left unchanged). */
            void copy_elements_from(const QueueImpl & i_source)
            {
                DENSITY_ASSERT(empty());
                try
                {
                    IteratorImpl it = i_source.begin();
                    const IteratorImpl end_it = i_source.end();
                    while (it != end_it)
                    {
                        auto const & type = it.complete_type();
                        auto const source_element = it.element(); // get_complete_type(it.control());
                        ++it;

                        // to do: a slightly optimized nofail_push may be used here
                        bool result = try_push(type,
                            typename detail::QueueImpl<RUNTIME_TYPE>::CopyConstruct(source_element));
                        DENSITY_ASSERT(result);
                        DENSITY_UNUSED(result);
                    }
                }
                catch(...)
                {
                    delete_all(); // <- this call is noexcept
                    throw;
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

            /** Tries to insert a new element in the queue. If this is a null-QueueImpl, the behavior is undefined.
                @param i_source_type type of the element to insert
                @param i_constructor function object to which the construction of the new element is delegated. The
                    expected signature of the function is:
                        void * (const RUNTIME_TYPE & i_source_type, void * i_new_element_place)
                    The return value is a pointer to the constructed element. QueueImpl is not aware of the value-type
                    of the overlying container, but i_constructor may be, and may return a pointer to a sub-object of
                    the complete object. If the value-type is void or a standard-layout type, i_constructor should return
                    i_new_element_place.
                @return true if the element was successfully inserted, false in case of insufficient space.
                Preconditions: this is not a null-QueueImpl. */
            template <typename CONSTRUCTOR>
                bool try_push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
                    /* This function may throw as consequences of a pointer overflow. It would be better to make the function just return false.
                        DENSITY_NOEXCEPT_IF(DENSITY_NOEXCEPT_IF(i_constructor(std::declval<const RUNTIME_TYPE>(), std::declval<void*>()))) */
            {
                DENSITY_ASSERT(m_buffer_start != nullptr);
                DENSITY_ASSERT(m_tail + 1 <= m_buffer_end);

                const auto element_aligment = i_source_type.alignment();

                Control * curr_control = m_tail;
                void * new_tail = curr_control + 1;
                void * element = single_push(new_tail, i_source_type.size(), element_aligment);
                void * next_control = single_push(new_tail, sizeof(Control), alignof(Control) );
                if (element == nullptr || next_control == nullptr)
                {
                    return false;
                }

                void * new_element = i_constructor(i_source_type, element);

                // from now on, no exception can be raised
                new(curr_control) Control(i_source_type, new_element, static_cast<Control*>(next_control));
                m_tail = static_cast<Control *>(next_control);
                m_element_max_alignment = size_max(m_element_max_alignment, element_aligment);
                return true;
            }

            /** Calls the specified function object on the first element (the oldest one), and then
                removes it from the queue without calling its destructor.
                @param i_operation function object with a signature compatible with:
                \code
                void (const RUNTIME_TYPE & i_complete_type, void * i_element_base_ptr)
                \endcode
                \n to be called for the first element. This function object is responsible of synchronously
                destroying the element.

                \pre The queue must be non-empty (otherwise the behavior is undefined).
            */
            template <typename OPERATION>
                auto manual_consume(OPERATION && i_operation) -> decltype(i_operation(std::declval<RUNTIME_TYPE>(), std::declval<void*>()))
//                    DENSITY_NOEXCEPT_IF(DENSITY_NOEXCEPT_IF(i_operation(std::declval<RUNTIME_TYPE>(), std::declval<void*>())))
            {
				using ReturnType = decltype(i_operation(std::declval<RUNTIME_TYPE>(), std::declval<void*>()));
				return manual_consume(std::forward<OPERATION>(i_operation), std::is_same<ReturnType, void>());
            }

            /** Deletes the first element of the queue (the oldest one).
                \pre The queue must be non-empty (otherwise the behavior is undefined).
                \n\b Throws: nothing
                \n\b Complexity: constant */
            void pop() DENSITY_NOEXCEPT
            {
                DENSITY_ASSERT(!empty()); // the queue must not be empty

                Control * first_control = m_head;
                void * const element_ptr = first_control->m_element;
                m_head = first_control->m_next;
                first_control->destroy(element_ptr);
                first_control->Control::~Control();
            }

            /** Returns a pointer to the beginning of the memory buffer. Note: this is not like a data() method, as
                the data does not start here (it starts where m_head points to). */
            void * buffer() DENSITY_NOEXCEPT { return m_buffer_start; }

            /** Returns the size of the memory buffer assigned to the queue */
            MemSize mem_capacity() const DENSITY_NOEXCEPT
            {
                return MemSize(address_diff(m_buffer_end, m_buffer_start));
            }

            /** Returns how much of the memory buffer is used. */
            MemSize mem_size() const DENSITY_NOEXCEPT
            {
                if (m_head <= m_tail)
                {
                    return MemSize( address_diff(m_tail, m_head) );
                }
                else
                {
                    return MemSize(address_diff(m_buffer_end, m_head) + address_diff(m_tail, m_buffer_start));
                }
            }

            /** Deletes al the element from the queue. After this call the memory buffer is still
                associated to the queue, but it is empty. */
            void delete_all() DENSITY_NOEXCEPT
            {
                IteratorImpl const it_end = end();
                for (IteratorImpl it = begin(); it != it_end; )
                {
                    auto const control = it.control();
                    auto const element = it.element(); // get_complete_type(it.control());
                    ++it;

                    control->destroy(element);
                    control->Control::~Control();
                }
                // restart from m_buffer_start, with an empty queue
                m_tail = m_head = static_cast<Control*>(address_upper_align(m_buffer_start, alignof(Control)));
            }

            size_t element_max_alignment() const DENSITY_NOEXCEPT { return m_element_max_alignment; }

        private:

            /*void * get_complete_type(Control * i_control) const DENSITY_NOEXCEPT
            {
                void * new_tail = i_control + 1;
                void * element = single_push(new_tail, i_control->size(), i_control->alignment());
                return element;
            }*/

			template <typename OPERATION>
                auto manual_consume(OPERATION && i_operation, std::false_type) -> decltype(i_operation(std::declval<RUNTIME_TYPE>(), std::declval<void*>()))
                   //DENSITY_NOEXCEPT_IF(DENSITY_NOEXCEPT_IF((i_operation(std::declval<RUNTIME_TYPE>(), std::declval<void*>()))))
            {
				DENSITY_ASSERT(!empty()); // the queue must not be empty

                Control * first_control = m_head;
                void * const element_ptr = first_control->m_element;
				decltype(auto) result = i_operation(static_cast<const RUNTIME_TYPE &>(*first_control), element_ptr);
                m_head = first_control->m_next;
                first_control->Control::~Control();
				return result;
            }

			template <typename OPERATION>
                void manual_consume(OPERATION && i_operation, std::true_type)
                   DENSITY_NOEXCEPT_IF(DENSITY_NOEXCEPT_IF((i_operation(std::declval<RUNTIME_TYPE>(), std::declval<void*>()))))
            {
				DENSITY_ASSERT(!empty()); // the queue must not be empty

                Control * first_control = m_head;
                void * const element_ptr = first_control->m_element;
				i_operation(static_cast<const RUNTIME_TYPE &>(*first_control), element_ptr);
                m_head = first_control->m_next;
                first_control->Control::~Control();
            }

            /* Allocates an object on the queue. The return value is the address of the new object.
               This function is used to push the Control and the element. If the required size with the
               required alignment does not fit in the queue the return value is nullptr.
               Preconditions: *io_tail can't be null, or the behavior is undefined. */
            void * single_push(void * & io_tail, size_t i_size, size_t i_alignment) const DENSITY_NOEXCEPT
            {
                DENSITY_ASSERT(io_tail != nullptr);

                auto const prev_tail = io_tail;

                auto start_of_block = linear_alloc(io_tail, i_size, i_alignment);
                if (io_tail > m_buffer_end)
                {
                    // wrap to the start...
                    io_tail = m_buffer_start;
                    start_of_block = linear_alloc(io_tail, i_size, i_alignment);
                    if (io_tail >= m_head)
                    {
                        // ...not enough space before the head, failed!
                        start_of_block = nullptr;
                    }
                }
                else if ((prev_tail >= m_head) != (io_tail >= m_head))
                {
                    // ...crossed the head, failed!
                    start_of_block = nullptr;
                }
                return start_of_block;
            }

        private: // data members
            Control * m_head; /**< points to the first Control. If m_tail==m_head the queue is empty, otherwise this member points
                                    to a valid Control. */
            Control * m_tail; /**< end marker of the sequence. If another element is successfully added to the sequence, m_tail will
                                    be the address of the associated Control object. */
            size_t m_element_max_alignment; /**< The maximum between alignof(Control) and the maximum alignment of the alignment of the
                                            elements in the container. This field has a specific getter (element_max_alignment), and it
                                            is required to allocate a memory buffer big enough to contain all the elements. */
            void * m_buffer_start, * m_buffer_end; /**< range of the memory buffer */
        }; // class template QueueImpl

    } // namespace detail

} // namespace density
