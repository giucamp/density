
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "detail\queue_impl.h"
#include <memory>

namespace density
{
    /** \brief Class template that implements an heterogeneous FIFO container with dynamic size.
        A dense_queue allocates one monolithic memory buffer with the provided allocator, and sub-allocates
        inplace its elements. This buffer is eventually reallocated to accomplish push and emplace requests.
        The memory management of this container is similar to an std::vector: since all the elements are
        stored in the same memory block, when a reallocation is performed, all the elements have to be moved.
        \n\b Thread safeness: None. The user is responsible to avoid race conditions.
        \n<b>Exception safeness</b>: Any function of dense_queue is noexcept or provides the strong exception guarantee.
            @param ELEMENT Base type of the elements of the queue. The queue enforces the compile-time
                constraint that the type of each element is covariant to ELEMENT.
                If ELEMENT is void, elements of any complete type can be added to the container. In this
                case, the methods of dense_queue (and its iterators) that returns a pointer to an element
                will return a void* to a complete object, while the methods that returns a reference to
                an element will return void. Use iterators and pointer semantic to write generic code
                that works with any dense_queue.
                If ELEMENT decays to void but it is not a plain void, a compile time error is issued.
                Note: if ELEMENT is to be a built-in type, a pointer, or a final type, then the complete
                type of all elements will always be ELEMENT (that is, the container will not be heterogeneous). In
                this case a standard container (like std::queue) instead of std::dense_queue is a better choice.
                If ELEMENT is not void, it must be noexcept move constructible.
            @param ALLOCATOR Allocator to be used to allocate the memory buffer. The queue may rebind
                this allocator to a different type, eventually unrelated to ELEMENT.
            @param RUNTIME_TYPE Type to be used to represent the actual complete type of each element.
                This type must meet the requirements of RuntimeType.
        dense_queue provides only forward iteration. Only the first element is accessible in constant time (with
        the functions: dense_queue::front, dense_queue::begin). The iterator provides access to both the ELEMENT (with
        the function element) and the RUNTIME_TYPE (with the function type).
        There is not constant time function that gives the number of elements in a dense_queue in constant time,
        but std::distance will do (in linear time). Anyway dense_queue::mem_size, dense_queue::mem_capacity and
        dense_queue::empty work in constant time.
        Insertion is allowed only at the end (with the methods dense_queue::push or dense_queue::emplace).
        Removal is allowed only at the begin (with the methods dense_queue::pop or dense_queue::consume).
        Limitations: when an element of COMPETE_ELEMENT is pushed to the queue, the current implementation needs
            sometimes to downcast from ELEMENT to COMPETE_ELEMENT.
                - If no virtual inheritance is involved, static_cast is used
                - If virtual inheritance is involved, dynamic_cast is used. Anyway, in this case, ELEMENT must be
                    a polymorphic type, otherwise there is no way to perform the downcast (in this case a compile-
                    time error is issued). */
    template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class dense_queue final : private std::allocator_traits<ALLOCATOR>::template rebind_alloc<char>
    {
    public:

        static_assert( std::is_same< typename std::decay<ELEMENT>::type, void >::value ? std::is_same<ELEMENT,void>::value : true,
            "If ELEMENT decays to void, it must be void (i.e. use plain 'void', not cv or ref qualified voids, like 'void&' or 'const void' )" );

        using allocator_type = typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char>;
        using runtime_type = RUNTIME_TYPE;
        using value_type = ELEMENT;
        using reference = typename std::add_lvalue_reference< ELEMENT >::type;
        using const_reference = typename std::add_lvalue_reference< const ELEMENT>::type;
        using difference_type = ptrdiff_t;
        using size_type = size_t;
        class iterator;
        class const_iterator;

        /** Default and reserving constructor. It is unspecified whether the default constructor allocates a memory block (that is, if a
            default-constructed queue consumes heap memory). The allocator inside the queue is default-constructed.
                @param i_initial_reserved_bytes initial capacity to reserve. Any value is legal, but the queue may reserve a bigger capacity.
                @param i_initial_alignment alignment of the initial buffer. Zero or any integer power of 2 is admitted, but the queue may use a stricter alignment.
            \pre i_initial_alignment must be zero or a power of 2, otherwise the behavior is undefined

            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).*/
        dense_queue(size_t i_initial_reserved_bytes = 0, size_t i_initial_alignment = 0)
        {
            DENSITY_ASSERT(i_initial_alignment == 0 || MemSize(i_initial_alignment).is_valid_alignment() );
            alloc(detail::size_max(i_initial_reserved_bytes, s_initial_mem_reserve),
                detail::size_max(i_initial_alignment, s_initial_mem_alignment));
        }

        /** Constructs a queue. It is unspecified whether the default constructor allocates a memory block (that is, if a
            default-constructed queue consumes heap memory). The allocator inside the queue is default-constructed.
                @param i_allocator source to use to copy-construct the allocator
                @param i_initial_reserved_bytes initial capacity to reserve. Any value is legal, but the queue may reserve a bigger capacity.
                @param i_initial_alignment alignment of the initial buffer. Zero or any integer power of 2 is admitted, but the queue may use a stricter alignment.
            \pre i_initial_alignment must be zero or a power of 2, otherwise the behavior is undefined

            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).*/
        dense_queue(const ALLOCATOR & i_allocator, size_t i_initial_reserved_bytes = 0, size_t i_initial_alignment = 0)
            : allocator_type(i_allocator)
        {
            alloc(detail::size_max(i_initial_reserved_bytes, QueueImp::s_initial_mem_reserve),
                detail::size_max(i_initial_alignment, QueueImp::s_minimum_buffer_alignment));
        }

        /** Move constructs the queue, transferring the content from another queue.
                @param i_source queue to move from. After the call the source queue will be empty.

            \n\b Requires: the move-constructor of the allocator must be noexcept
            \n\b Throws: nothing
            \n\b Complexity: constant */
        dense_queue(dense_queue && i_source) DENSITY_NOEXCEPT
            : allocator_type(std::move(static_cast<allocator_type&>(i_source))),
              m_impl(std::move(i_source.m_impl))
        {
            static_assert(DENSITY_NOEXCEPT_IF( allocator_type(std::move(std::declval<allocator_type>()))),
                "The move constructor of the allocator is required to be noexcept");
        }

        /** Move assignment. The content of this queue is cleared, then the content of the source is transferred to this queue.
                @param i_source queue to move from. After the call this queue will be empty
            \n\b Requires: the move-assignment of the allocator must be noexcept
            \n\b Effects on iterators:
                - iterators referring to the destination queue are invalidated.
                - iterators referring to the source queue become valid for the destination queue.
            \pre The behavior is undefined if the source and the destination are same object

            \n\b Throws: nothing
            \n\b Complexity: linear in the size of destination (its content must be destroyed). */
        dense_queue & operator = (dense_queue && i_source) DENSITY_NOEXCEPT
        {
            static_assert(DENSITY_NOEXCEPT_IF(std::declval<allocator_type>() = std::move(std::declval<allocator_type>())),
                "The move assignment of the allocator is required to be noexcept");

            DENSITY_ASSERT(this != &i_source);
            m_impl.delete_all();
            free();
            static_cast<allocator_type&>(*this) = std::move( static_cast<allocator_type&>(i_source) );
            m_impl = std::move(i_source.m_impl);
            return *this;
        }

        /** Copy constructor. Copies the content of the source queue (deep copy).
            \n\b Throws: anything that the allocator or the copy-constructor of the element throws.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: linear in the size of the source. */
        dense_queue(const dense_queue & i_source)
            : allocator_type(static_cast<const allocator_type&>(i_source))
        {
            try // to do: would a RAII approach be better than explicit try-catch?
            {
                alloc(i_source.m_impl.mem_capacity().value(), i_source.m_impl.element_max_alignment());
                m_impl.copy_elements_from(i_source.m_impl);
            }
            catch (...)
            {
                free();
                throw;
            }
        }

        /** Copy assignment. Clear the content of this queue, and copies the content of the source queue (deep copy).
            \pre The behavior is undefined if the source and the destination are same object

            \n<b>Effects on iterators</b>: iterators referring to the destination queue are invalidated.
            \n\b Throws:
                - anything that the allocator throws
                - anything that the copy-constructor of the elements throws

            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: linear in the size of the source. */
        dense_queue & operator = (const dense_queue & i_source)
        {
            DENSITY_ASSERT(this != &i_source);
            dense_queue tmp(i_source); // the copy may throw, with this queue still being unmodified
            // from now on nothing can throw
            *this = std::move(tmp);
            return *this;
        }

        /** Destructor.

            \n<b>Effects on iterators</b>: all iterators are invalidated.
            \n\b Throws: nothing
            \n\b Complexity: linear in the size of this queue. */
        ~dense_queue()
        {
            m_impl.delete_all();
            free();
        }


        /** Adds an element at the end of the queue. If the new element doesn't fit in the reserved memory buffer, a reallocation is performed.
            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed (and the source object is left unchanged).
                - If this argument is an r-value, the new element move-constructed (and the source object will have an undefined but valid content).

            \n\b Requires:
                - the type ELEMENT_COMPLETE_TYPE must copy constructible (in case of l-value) or move constructible (in case of r-value)
                - an ELEMENT_COMPLETE_TYPE * must be implicitly convertible to ELEMENT *
                - an ELEMENT * must be convertible to an ELEMENT_COMPLETE_TYPE * with a static_cast or a dynamic_cast
                    (this requirement is not met for example if ELEMENT is a non-polymorphic (direct or indirect) virtual
                    base of ELEMENT_COMPLETE_TYPE).

            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: constant amortized (a reallocation may be required). */
        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            static_assert( std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
                "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT, or ELEMENT must be void" );
            push_impl(std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
        }

        /** Adds an element at the end of the queue, constructing it inplace. If the new element doesn't fit in the reserved memory buffer, a reallocation is performed.
            \n Note: the template argument ELEMENT_COMPLETE_TYPE must be explicit (it can't be deduced from the parameter).
                @i_parameters construction params for the new element

            \n\b Requires:
                - the ELEMENT_COMPLETE_TYPE must be constructible with the specified parameter list
                - an ELEMENT_COMPLETE_TYPE * must be implicitly convertible to ELEMENT *
                - an ELEMENT * must be convertible to an ELEMENT_COMPLETE_TYPE * with a static_cast or a dynamic_cast
                    (this requirement is not met for example if ELEMENT is a non-polymorphic (direct or indirect) virtual
                    base of ELEMENT_COMPLETE_TYPE).

            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: constant amortized (a reallocation may be required). */
        template <typename ELEMENT_COMPLETE_TYPE, typename... PARAMETERS>
            void emplace(PARAMETERS && ... i_parameters)
        {
            static_assert(std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
                "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT, or ELEMENT must be void");
            return insert_back_impl(runtime_type::template make<ELEMENT_COMPLETE_TYPE>(),
                [&i_parameters...](const runtime_type &, void * i_dest) {
                    void * const result = new(i_dest) ELEMENT_COMPLETE_TYPE(std::forward<PARAMETERS>(i_parameters)...);
                    return static_cast<ELEMENT*>(result);
            });
        }

        void push_by_copy(const RUNTIME_TYPE & i_type, const ELEMENT * i_source)
        {
            insert_back_impl(i_type,
                typename detail::QueueImpl<RUNTIME_TYPE>::CopyConstruct(i_source));
        }

        void push_by_move(const RUNTIME_TYPE & i_type, ELEMENT * i_source) DENSITY_NOEXCEPT
        {
            insert_back_impl(i_type,
                typename detail::QueueImpl<RUNTIME_TYPE>::MoveConstruct(i_source));
        }

        /** Deletes the first element of the queue (the oldest one).
            \pre The queue must be non-empty (otherwise the behavior is undefined).

            \n\b Effects on iterators: only iterators and references to the first element are invalidated
            \n\b Throws: nothing
            \n\b Complexity: constant */
        void pop() DENSITY_NOEXCEPT
        {
            m_impl.pop();
        }

        /** Calls the specified function object on the first element (the oldest one), and then
            removes it from the queue without calling its destructor.
            @param i_operation function object with a signature compatible with:
            \code
            void (const RUNTIME_TYPE & i_complete_type, ELEMENT * i_element_base_ptr)
            \endcode
            \n to be called for the first element. This function object is responsible of synchronously
            destroying the element.
                - The first parameter is the complete type of the element.
                - The second parameter is a pointer to a subobject ELEMENT of the element being removed.

            \n Note: a call to pop() is equivalent to a call to manual_consume() with
                \code [] (const RUNTIME_TYPE & i_complete_type, ELEMENT * i_element_base_ptr)
                    { i_complete_type.destroy(i_element_base_ptr); }
                \endcode
            \n This function is to be considered a low-level functionality: use it only for a good reason,
                otherwise use front(), begin() and pop().

            \pre The queue must be non-empty (otherwise the behavior is undefined).

            \n\b Throws: anything that the function object throws
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: constant */
        template <typename OPERATION>
            auto manual_consume(OPERATION && i_operation)
                DENSITY_NOEXCEPT_IF(DENSITY_NOEXCEPT_IF((i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()))))
                    -> decltype(i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()))
        {
            return m_impl.manual_consume([&i_operation](const RUNTIME_TYPE & i_type, void * i_element) {
                return i_operation(i_type, static_cast<ELEMENT*>(i_element));
            });
        }

        /** Reserve the specified space in the queue, reallocating it if necessary.
                @param i_mem_size space to reserve, in bytes

            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: linear in case of reallocation, constant otherwise */
        void mem_reserve(size_t i_mem_size)
        {
            if (i_mem_size > m_impl.mem_capacity())
            {
                mem_realloc_impl(i_mem_size);
            }
        }

                    // iterators

        class iterator final
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = ptrdiff_t;
            using size_type = size_t;
            using value_type = ELEMENT;
            using pointer = ELEMENT *;
            using reference = typename dense_queue::reference;
            using const_reference = typename dense_queue::const_reference;

            /** Constructs an iterator which is not dereferenceable
                \n\b Throws: nothing */
            iterator() DENSITY_NOEXCEPT {}

            /** Initializing constructor. Provided for internal use only, do not use. */
            iterator(const typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source) {  }

            /** Returns a reference to the subobject of type ELEMENT of current element. If ELEMENT is void this function is useless, because the return type is void. */
            reference operator * () const DENSITY_NOEXCEPT { return detail::DeferenceVoidPtr<value_type>::apply(m_impl.element()); }

            /** Returns a pointer to the subobject of type ELEMENT of current element. If ELEMENT is void this function is useless, because the return type is void *. */
            pointer operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }

            /** Returns a pointer to the subobject of type ELEMENT of current element. If ELEMENT is void, then the return type is void *. */
            pointer element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }

            /** Returns the RUNTIME_TYPE associated to this element. The user may use the function type_info of RUNTIME_TYPE
                (whenever supported) to obtain a const-reference to a std::type_info. */
            const RUNTIME_TYPE & complete_type() const DENSITY_NOEXCEPT
            {
                return m_impl.complete_type();
            }

            iterator & operator ++ () DENSITY_NOEXCEPT
            {
                ++m_impl;
                return *this;
            }

            iterator operator++ (int) DENSITY_NOEXCEPT
            {
                const iterator copy(*this);
                ++m_impl;
                return copy;
            }

            bool operator == (const iterator & i_other) const DENSITY_NOEXCEPT
            {
                return m_impl == i_other.m_impl;
            }

            bool operator != (const iterator & i_other) const DENSITY_NOEXCEPT
            {
                return m_impl != i_other.m_impl;
            }

            bool operator == (const const_iterator & i_other) const DENSITY_NOEXCEPT
            {
                return m_impl == i_other.m_impl;
            }

            bool operator != (const const_iterator & i_other) const DENSITY_NOEXCEPT
            {
                return m_impl != i_other.m_impl;
            }

        private:
            typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl m_impl;
        }; // class iterator

        class const_iterator final
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = ptrdiff_t;
            using size_type = size_t;
            using value_type = const ELEMENT;
            using pointer = const ELEMENT *;
            using reference = typename dense_queue::const_reference;
            using const_reference = typename dense_queue::const_reference;

            /** Constructs an iterator which is not dereferenceable
                \n\b Throws: nothing */
            const_iterator() DENSITY_NOEXCEPT { }

            /** Initializing constructor. Provided for internal use only, do not use. */
            const_iterator(const typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source) {  }

            /** Copy-like constructor from an iterator. Makes an exact copy of the iterator.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            const_iterator(const iterator & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source.m_impl) {  }

            /** Copy-like assignment from an iterator. Makes an exact copy of the iterator.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            const_iterator & operator = (const iterator & i_source) DENSITY_NOEXCEPT
            {
                m_impl = i_source.m_impl;
                return *this;
            }

            /** Returns a const reference to the subobject of type ELEMENT of current element. If ELEMENT is void this function is useless, because the return type is const void. */
            reference operator * () const DENSITY_NOEXCEPT { return detail::DeferenceVoidPtr<value_type>::apply(m_impl.element()); }

            /** Returns a const pointer to the subobject of type ELEMENT of current element. If ELEMENT is void this function is useless, because the return type is const void *. */
            pointer operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }

            /** Returns a const pointer to the subobject of type ELEMENT of current element. If ELEMENT is void, then the return type is const void *. */
            pointer element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }

            /** Returns the RUNTIME_TYPE associated to this element. The user may use the function type_info of RUNTIME_TYPE
                (whenever supported) to obtain a const-reference to a std::type_info. */
            const RUNTIME_TYPE & complete_type() const DENSITY_NOEXCEPT
            {
                return m_impl.complete_type();
            }

            const_iterator & operator ++ () DENSITY_NOEXCEPT
            {
                ++m_impl;
                return *this;
            }

            const_iterator operator ++ (int) DENSITY_NOEXCEPT
            {
                const const_iterator copy(*this);
                ++m_impl;
                return copy;
            }

            bool operator == (const iterator & i_other) const DENSITY_NOEXCEPT
            {
                return m_impl == i_other.m_impl;
            }

            bool operator != (const iterator & i_other) const DENSITY_NOEXCEPT
            {
                return m_impl != i_other.m_impl;
            }

            bool operator == (const const_iterator & i_other) const DENSITY_NOEXCEPT
            {
                return m_impl == i_other.m_impl;
            }

            bool operator != (const const_iterator & i_other) const DENSITY_NOEXCEPT
            {
                return m_impl != i_other.m_impl;
            }

        private:
            typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl m_impl;
        }; // class const_iterator

        iterator begin() DENSITY_NOEXCEPT { return iterator(m_impl.begin()); }
        iterator end() DENSITY_NOEXCEPT { return iterator(m_impl.end()); }

        const_iterator begin() const DENSITY_NOEXCEPT { return const_iterator(m_impl.begin()); }
        const_iterator end() const DENSITY_NOEXCEPT { return const_iterator(m_impl.end()); }

        const_iterator cbegin() const DENSITY_NOEXCEPT { return const_iterator(m_impl.begin()); }
        const_iterator cend() const DENSITY_NOEXCEPT { return const_iterator(m_impl.end()); }

        /** Returns true if this queue contains no elements.
            \n\b Throws: nothing
            \n\b Complexity: constant */
        bool empty() const DENSITY_NOEXCEPT { return m_impl.empty(); }

        /** Deletes all the elements from this queue.
            \n\b Throws: nothing
            \n\b Complexity: linear */
        void clear() DENSITY_NOEXCEPT
        {
            m_impl.delete_all();
        }

        /** Returns a reference to the first element of this queue. If ELEMENT is void,
            the this function returns void.
            \pre The queue must be non-empty (otherwise the behavior is undefined)

            \n\b Throws: nothing
            \n\b Complexity: constant */
        reference front() DENSITY_NOEXCEPT
        {
            DENSITY_ASSERT(!empty());
            const auto it = m_impl.begin();
            return detail::DeferenceVoidPtr<ELEMENT>::apply(it.element());
        }

        /** Returns a const reference to the first element of this queue. If ELEMENT is void,
            the this function returns void.
            \pre The queue must be non-empty (otherwise the behavior is undefined)

            \n\b Throws: nothing
            \n\b Complexity: constant */
        const_reference front() const DENSITY_NOEXCEPT
        {
            DENSITY_ASSERT(!empty());
            const auto it = m_impl.begin();
            return detail::DeferenceVoidPtr<ELEMENT>::apply(it.element());
        }

        /** Returns the capacity in bytes of this queue, that is the size of the memory buffer used to store the elements.
            \remark There is no way to predict if the next push\emplace will cause a reallocation.

            \n\b Throws: nothing
            \n\b Complexity: constant */
        MemSize mem_capacity() const DENSITY_NOEXCEPT
        {
            return m_impl.mem_capacity();
        }

        /** Returns the used size in bytes. This size is dependant, in an implementation defined way, to the count, the type and
            the order of the elements present in the queue. The return value is zero if and only if the queue is empty. It is recommended
            to use the function dense_queue::empty to check if the queue is empty.
            \remark There is no way to predict if the next push\emplace will cause a reallocation.

            \n\b Throws: nothing
            \n\b Complexity: constant */
        MemSize mem_size() const DENSITY_NOEXCEPT
        {
            return m_impl.mem_size();
        }

        /** Returns the free size in bytes. This is equivalent to dense_queue::mem_capacity decreased by dense_queue::mem_size.
            \remark There is no way to predict if the next push\emplace will cause a reallocation.

            \n\b Throws: nothing
            \n\b Complexity: constant */
        MemSize mem_free() const DENSITY_NOEXCEPT
        {
            return m_impl.mem_capacity() - m_impl.mem_size();
        }

        /** Returns a copy of the allocator instance owned by the queue.
            \n\b Throws: nothing
            \n\b Complexity: constant */
        allocator_type get_allocator() const DENSITY_NOEXCEPT
        {
            return *this;
        }

        /** Returns a reference to the allocator instance owned by the queue.
            \n\b Throws: nothing
            \n\b Complexity: constant */
        allocator_type & get_allocator_ref() DENSITY_NOEXCEPT
        {
            return *this;
        }

        /** Returns a const reference to the allocator instance owned by the queue.
            \n\b Throws: nothing
            \n\b Complexity: constant */
        const allocator_type & get_allocator_ref() const DENSITY_NOEXCEPT
        {
            return *this;
        }

    private:

        static const size_t s_initial_mem_reserve = 1024 > detail::QueueImpl<RUNTIME_TYPE>::s_minimum_buffer_size ? 1024 : detail::QueueImpl<RUNTIME_TYPE>::s_minimum_buffer_size;
        static const size_t s_initial_mem_alignment = detail::QueueImpl<RUNTIME_TYPE>::s_minimum_buffer_alignment;

        using Impl = typename detail::QueueImpl<RUNTIME_TYPE>;

        void alloc(size_t i_size, size_t i_alignment)
        {
            m_impl = Impl(AllocatorUtils::aligned_allocate(get_allocator_ref(), i_size, i_alignment, 0), i_size);
        }

        void free() DENSITY_NOEXCEPT
        {
            AllocatorUtils::aligned_deallocate(get_allocator_ref(), m_impl.buffer(), m_impl.mem_capacity().value());
        }

        void mem_realloc_impl(size_t i_mem_size)
        {
            DENSITY_ASSERT(i_mem_size > m_impl.mem_capacity().value());

            Impl new_impl(AllocatorUtils::aligned_allocate(get_allocator_ref(), i_mem_size, m_impl.element_max_alignment(), 0), i_mem_size);
            try
            {
                new_impl.move_elements_from(m_impl);
            }
            catch (...)
            {
                AllocatorUtils::aligned_deallocate(get_allocator_ref(), new_impl.buffer(), new_impl.mem_capacity().value());
                throw;
            }

            // from now on, nothing can throw
            AllocatorUtils::aligned_deallocate(get_allocator_ref(), m_impl.buffer(), m_impl.mem_capacity().value());
            m_impl = std::move(new_impl);
        }

        // overload used if i_source is an r-value
        template <typename ELEMENT_COMPLETE_TYPE>
            void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::true_type)
        {
            using ElementCompleteType = typename std::decay<ELEMENT_COMPLETE_TYPE>::type;
            insert_back_impl(runtime_type::template make<ElementCompleteType>(),
                [&i_source](const runtime_type &, void * i_dest) {
                auto const result = new(i_dest) ElementCompleteType(std::move(i_source));
                return static_cast<ELEMENT*>(result);
            });
        }

        // overload used if i_source is an l-value
        template <typename ELEMENT_COMPLETE_TYPE>
            void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::false_type)
        {
            using ElementCompleteType = typename std::decay<ELEMENT_COMPLETE_TYPE>::type;
            insert_back_impl(runtime_type::template make<ElementCompleteType>(),
                [&i_source](const runtime_type &, void * i_dest) {
                auto const result = new(i_dest) ElementCompleteType(i_source);
                return static_cast<ELEMENT*>(result);
            });
        }

        // this function is used by push_back and emplace
        template <typename CONSTRUCTOR>
            void insert_back_impl(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
        {
            while(!m_impl.try_push(i_source_type, std::forward<CONSTRUCTOR>(i_constructor)))
            {
                mem_realloc_impl( detail::size_max(
                    m_impl.mem_capacity().value() * 2,
                    i_source_type.size() * 16 + i_source_type.alignment() ) );
            }
        }

    private:
        detail::QueueImpl<RUNTIME_TYPE> m_impl; /* m_impl manages the memory buffer, but dense_queue owns it */
    }; // class template dense_queue

} // namespace density
