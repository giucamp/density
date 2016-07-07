
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <vector>
#include "density_common.h"
#include "void_allocator.h"
#include "detail\queue_impl.h"

namespace density
{
    /** \brief Class template that implements an heterogeneous FIFO container with dynamic size.
        heterogeneous_queue is a queue: elements can be added only at the end of the container, and only the first element
        can be removed.
        A heterogeneous_queue allocates a set of pages from the provided page allocator. The \b head page is the
        the least recently allocated page, while the \b tail page is the most recently allocated one.
        heterogeneous_queue never reallocates or move its elements: new elements are allocated to the head page.
        When there is not enough space in the head page, a new page is allocated, which becomes the new head page.
        After an element is removed, if the page that contained it becomes empty, the page is immediately deallocated.
        An iterator is invalidated only when the pointed element is deleted (including when the queue is destroyed).

        \n\b Thread safeness: None. The user is responsible to avoid race conditions.
        \n<b>Exception safeness</b>: Any function of heterogeneous_queue is noexcept or provides the strong exception guarantee.
            @param ELEMENT Base type of the elements of the queue. The queue enforces the compile-time
                constraint that the type of each element is covariant to ELEMENT.
                If ELEMENT is void, elements of any complete type can be added to the container. In this
                case, the methods of heterogeneous_queue (and its iterators) that returns a pointer to an element
                will return a void* to a complete object, while the methods that returns a reference to
                an element will return void. Use iterators and pointer semantic to write generic code
                that works with any heterogeneous_queue.
                If ELEMENT decays to void but it is not a plain void, a compile time error is issued.
                Note: if ELEMENT is to be a built-in type, a pointer, or a final type, then the complete
                type of all elements will always be ELEMENT (that is, the container will not be heterogeneous). In
                this case a standard container (like std::queue) instead of heterogeneous_queue is a better choice.
                If ELEMENT is not void, it must be noexcept move constructible.
            @param ALLOCATOR Allocator to be used to allocate the memory buffer. The queue may rebind
                this allocator to a different type, eventually unrelated to ELEMENT.
            @param RUNTIME_TYPE Type to be used to represent the actual complete type of each element.
                This type must meet the requirements of RuntimeType.
        heterogeneous_queue provides only forward iteration. Only the first element is accessible in constant time (with
        the functions: heterogeneous_queue::front, heterogeneous_queue::begin). The iterator provides access to both the ELEMENT (with
        the function element) and the RUNTIME_TYPE (with the function type).
        There is not constant time function that gives the number of elements in a heterogeneous_queue in constant time,
        but std::distance will do (in linear time). Anyway heterogeneous_queue::empty work in constant time.
        Limitations: when an element of COMPETE_ELEMENT is pushed to the queue, the current implementation needs
            sometimes to downcast from ELEMENT to COMPETE_ELEMENT.
                - If no virtual inheritance is involved, static_cast is used
                - If virtual inheritance is involved, dynamic_cast is used. Anyway, in this case, ELEMENT must be
                    a polymorphic type, otherwise there is no way to perform the downcast (in this case a compile-
                    time error is issued). */
    template < typename ELEMENT = void, typename ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class heterogeneous_queue final : private ALLOCATOR
    {
        struct PageHeader;

    public:

        static_assert(std::is_same< typename std::decay<ELEMENT>::type, void >::value ? std::is_same<ELEMENT, void>::value : true,
            "If ELEMENT decays to void, it must be void (i.e. use plain 'void', not cv or ref qualified voids, like 'void&' or 'const void' )");

        using allocator_type = ALLOCATOR;
        using runtime_type = RUNTIME_TYPE;
        using value_type = ELEMENT;
        using reference = typename std::add_lvalue_reference< ELEMENT >::type;
        using const_reference = typename std::add_lvalue_reference< const ELEMENT>::type;
        using difference_type = ptrdiff_t;
        using size_type = size_t;
        class iterator;
        class const_iterator;

        /** Default and reserving constructor. It is unspecified whether the default constructor allocates any page (that is, if a
            default-constructed queue consumes memory). The page allocator inside the queue is default-constructed.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).*/
        heterogeneous_queue()
            : m_first_page(nullptr), m_last_page(nullptr)
        {
            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif
        }

        /** Constructs a queue. It is unspecified whether the default constructor allocates any page (that is, if a
            default-constructed queue consumes heap memory). The allocator inside the queue is default-constructed.
                @param i_allocator source to use to copy-construct the allocator

            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).*/
        heterogeneous_queue(const allocator_type & i_allocator)
            : allocator_type(i_allocator), m_first_page(nullptr), m_last_page(nullptr)
        {
        }

        /** Move constructs the queue, transferring the content from another queue.
                @param i_source queue to move from. After the call the source queue will be empty.

            \n\b Requires: the move-constructor of the allocator must be noexcept
            \n\b Throws: nothing
            \n\b Complexity: constant */
        heterogeneous_queue(heterogeneous_queue && i_source) noexcept
            : allocator_type(std::move(static_cast<allocator_type&>(i_source))),
            m_first_page(i_source.m_first_page), m_last_page(i_source.m_last_page)
        {
            static_assert(noexcept( allocator_type(std::move(std::declval<allocator_type>()))),
                "The move constructor of the allocator is required to be noexcept");
            i_source.m_first_page = nullptr;
            i_source.m_last_page = nullptr;
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
        heterogeneous_queue & operator = (heterogeneous_queue && i_source) noexcept
        {
            static_assert(noexcept(std::declval<allocator_type>() = std::move(std::declval<allocator_type>())),
                "The move assignment of the allocator is required to be noexcept");

            DENSITY_ASSERT(this != &i_source);
            delete_all();
            static_cast<allocator_type&>(*this) = std::move( static_cast<allocator_type&>(i_source) );
            m_first_page = i_source.m_first_page;
            m_last_page = i_source.m_last_page;
            i_source.m_first_page = nullptr;
            i_source.m_last_page = nullptr;
            return *this;
        }

        /** Copy constructor. Copies the content of the source queue (deep copy).
            \n\b Throws: anything that the allocator or the copy-constructor of the element throws.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: linear in the size of the source. */
        heterogeneous_queue(const heterogeneous_queue & i_source)
            : allocator_type(static_cast<const allocator_type&>(i_source)), m_first_page(nullptr), m_last_page(nullptr)
        {
            try
            {
                for (auto it = i_source.begin(); it != i_source.end(); it++) // this loop may be optimized
                {
                    push_by_copy(it.complete_type(), it.element());
                }
            }
            catch (...)
            {
                /* If an exception is thrown inside a constructor, the destructor is not called. */
                clear();
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
        heterogeneous_queue & operator = (const heterogeneous_queue & i_source)
        {
            DENSITY_ASSERT(this != &i_source);
            heterogeneous_queue tmp(i_source); // the copy may throw, with this queue still being unmodified
            // from now on nothing can throw
            *this = std::move(tmp);
            return *this;
        }

        /** Destructor.

            \n<b>Effects on iterators</b>: all iterators are invalidated.
            \n\b Throws: nothing
            \n\b Complexity: linear in the size of this queue. */
        ~heterogeneous_queue()
        {
            delete_all();
        }

        /** Adds an element at the end of the queue. The operation may require the allocation of a new page.
            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed (and the source object is left unchanged).
                - If this argument is an r-value, the new element move-constructed (and the source object will have an undefined but valid content).

            \n\b Requires:
                - the type ELEMENT_COMPLETE_TYPE must copy constructible (in case of l-value) or move constructible (in case of r-value)
                - an ELEMENT_COMPLETE_TYPE * must be implicitly convertible to ELEMENT *
                - an ELEMENT * must be convertible to an ELEMENT_COMPLETE_TYPE * with a static_cast or a dynamic_cast
                    (this requirement is not met for example if ELEMENT is a non-polymorphic (direct or indirect) virtual
                    base of ELEMENT_COMPLETE_TYPE).

            \n<b> Effects on iterators </b>: no iterator is invalidated
            \n\b Throws: anything that the constructor of the new element throws
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: constant */
        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            static_assert(std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
                "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT, or ELEMENT must be void");
            push_impl(std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
        }

        /** Adds an element at the end of the queue, constructing it inplace. The operation may require the allocation of a new page.
            \n Note: the template argument ELEMENT_COMPLETE_TYPE must be explicit (it can't be deduced from the parameter).
                @i_parameters construction params for the new element

            \n\b Requires:
                - the ELEMENT_COMPLETE_TYPE must be constructible with the specified parameter list
                - an ELEMENT_COMPLETE_TYPE * must be implicitly convertible to ELEMENT *
                - an ELEMENT * must be convertible to an ELEMENT_COMPLETE_TYPE * with a static_cast or a dynamic_cast
                    (this requirement is not met for example if ELEMENT is a non-polymorphic (direct or indirect) virtual
                    base of ELEMENT_COMPLETE_TYPE).

            \n<b> Effects on iterators </b>: no iterator is invalidated
            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: constant */
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
                typename detail::QueueImpl<RUNTIME_TYPE>::copy_construct(i_source));
        }

        void push_by_move(const RUNTIME_TYPE & i_type, ELEMENT * i_source) noexcept
        {
            insert_back_impl(i_type,
                typename detail::QueueImpl<RUNTIME_TYPE>::move_construct(i_source));
        }
		
        /** Deletes the first element of the queue (the oldest one).
            \pre The queue must be non-empty (otherwise the behavior is undefined).

            \n\b Effects on iterators: only iterators and references to the first element are invalidated
            \n\b Throws: nothing
            \n\b Complexity: constant */
        void pop() noexcept
        {
            DENSITY_ASSERT(!empty());

            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif

            m_first_page->m_queue.pop();
            if (m_first_page->m_queue.empty())
            {
                auto const next = m_first_page->m_next_page;
                delete_page(m_first_page);
                m_first_page = next;
                if (next == nullptr)
                {
                    m_last_page = nullptr;
                }
            }

            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif
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
                noexcept(noexcept((i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()))))
                    -> decltype(i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()))
        {
            DENSITY_ASSERT(!empty());

            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif

            using ReturnType = decltype(i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()));
            return manual_consume_impl(i_operation, std::is_same<ReturnType, void>());
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
            using reference = typename heterogeneous_queue::reference;
            using const_reference = typename heterogeneous_queue::const_reference;

            /** Constructs an iterator which is not dereferenceable
                \n\b Throws: nothing */
            iterator() noexcept
                : m_impl(nullptr), m_curr_page(nullptr) { }

            /** Initializing constructor. Provided for internal use only, do not use. */
            iterator(const typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl & i_impl, PageHeader * i_curr_page ) noexcept
                : m_impl(i_impl), m_curr_page(i_curr_page) { }

            /** Returns a reference to the subobject of type ELEMENT of current element. If ELEMENT is void this function is useless, because the return type is void. */
            reference operator * () const noexcept { return detail::DeferenceVoidPtr<value_type>::apply(m_impl.element()); }

            /** Returns a pointer to the subobject of type ELEMENT of current element. If ELEMENT is void this function is useless, because the return type is void *. */
            pointer operator -> () const noexcept { return static_cast<value_type *>(m_impl.element()); }

            /** Returns a pointer to the subobject of type ELEMENT of current element. If ELEMENT is void, then the return type is void *. */
            pointer element() const noexcept { return static_cast<value_type *>(m_impl.element()); }

            /** Returns the RUNTIME_TYPE associated to this element. The user may use the function type_info of RUNTIME_TYPE
                (whenever supported) to obtain a const-reference to a std::type_info. */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                return m_impl.complete_type();
            }

            iterator & operator ++ () noexcept
            {
                DENSITY_ASSERT(m_impl != m_curr_page->m_queue.end());

                ++m_impl;
                if (m_impl == m_curr_page->m_queue.end())
                {
                    m_curr_page = m_curr_page->m_next_page;
                    if (m_curr_page != nullptr)
                    {
                        m_impl = m_curr_page->m_queue.begin();
                    }
                }

                return *this;
            }

            iterator operator++ (int) noexcept
            {
                const auto copy(*this);
                ++*this;
                return copy;
            }

            bool operator == (const iterator & i_other) const noexcept
            {
                return m_impl == i_other.m_impl;
            }

            bool operator != (const iterator & i_other) const noexcept
            {
                return m_impl != i_other.m_impl;
            }

            bool operator == (const const_iterator & i_other) const noexcept
            {
                return m_impl == i_other.m_impl;
            }

            bool operator != (const const_iterator & i_other) const noexcept
            {
                return m_impl != i_other.m_impl;
            }

        private:
            typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl m_impl;
            PageHeader * m_curr_page;
        }; // class iterator

        class const_iterator final
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = ptrdiff_t;
            using size_type = size_t;
            using value_type = const ELEMENT;
            using pointer = const ELEMENT *;
            using reference = typename heterogeneous_queue::const_reference;
            using const_reference = typename heterogeneous_queue::const_reference;

            /** Constructs an iterator which is not dereferenceable
                \n\b Throws: nothing */
            const_iterator() noexcept
                : m_impl(nullptr), m_curr_page(nullptr) {}

            /** Initializing constructor. Provided for internal use only, do not use. */
            const_iterator(const typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl & i_impl, PageHeader * i_curr_page ) noexcept
                : m_impl(i_impl), m_curr_page(i_curr_page) { }

            /** Copy-like constructor from an iterator. Makes an exact copy of the iterator.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            const_iterator(const iterator & i_source) noexcept
                : m_impl(i_source.m_impl) {  }

            /** Copy-like assignment from an iterator. Makes an exact copy of the iterator.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            const_iterator & operator = (const iterator & i_source) noexcept
            {
                m_impl = i_source.m_impl;
                return *this;
            }

            /** Returns a const reference to the subobject of type ELEMENT of current element. If ELEMENT is void this function is useless, because the return type is const void. */
            reference operator * () const noexcept { return detail::DeferenceVoidPtr<value_type>::apply(m_impl.element()); }

            /** Returns a const pointer to the subobject of type ELEMENT of current element. If ELEMENT is void this function is useless, because the return type is const void *. */
            pointer operator -> () const noexcept { return static_cast<value_type *>(m_impl.element()); }

            /** Returns a const pointer to the subobject of type ELEMENT of current element. If ELEMENT is void, then the return type is const void *. */
            pointer element() const noexcept { return static_cast<value_type *>(m_impl.element()); }

            /** Returns the RUNTIME_TYPE associated to this element. The user may use the function type_info of RUNTIME_TYPE
                (whenever supported) to obtain a const-reference to a std::type_info. */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                return m_impl.complete_type();
            }

            const_iterator & operator ++ () noexcept
            {
                ++m_impl;
                if (m_impl == m_curr_page->m_queue.end())
                {
                    m_curr_page = m_curr_page->m_next_page;
                    if (m_curr_page != nullptr)
                    {
                        m_impl = m_curr_page->m_queue.begin();
                    }
                }
                return *this;
            }

            const_iterator operator ++ (int) noexcept
            {
                const auto copy(*this);
                ++*this;
                return copy;
            }

            bool operator == (const iterator & i_other) const noexcept
            {
                return m_impl == i_other.m_impl;
            }

            bool operator != (const iterator & i_other) const noexcept
            {
                return m_impl != i_other.m_impl;
            }

            bool operator == (const const_iterator & i_other) const noexcept
            {
                return m_impl == i_other.m_impl;
            }

            bool operator != (const const_iterator & i_other) const noexcept
            {
                return m_impl != i_other.m_impl;
            }

        private:
            typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl m_impl;
            PageHeader * m_curr_page;
        }; // class const_iterator

        iterator begin() noexcept
        {
            if (m_first_page != nullptr)
            {
                return iterator(m_first_page->m_queue.begin(), m_first_page);
            }
            else
            {
                return iterator();
            }
        }

        const_iterator begin() const noexcept
        {
            if (m_first_page != nullptr)
            {
                return const_iterator(m_first_page->m_queue.begin(), m_first_page);
            }
            else
            {
                return const_iterator();
            }
        }

        const_iterator cbegin() const noexcept
        {
            if (m_first_page != nullptr)
            {
                return const_iterator(m_first_page->m_queue.begin(), m_first_page);
            }
            else
            {
                return const_iterator();
            }
        }

        iterator end() noexcept
        {
            if (m_first_page != nullptr)
            {
                return iterator(m_last_page->m_queue.end(), m_last_page);
            }
            else
            {
                return iterator();
            }
        }

        const_iterator end() const noexcept
        {
            if (m_first_page != nullptr)
            {
                return const_iterator(m_last_page->m_queue.end(), m_last_page);
            }
            else
            {
                return const_iterator();
            }
        }

        const_iterator cend() const noexcept
        {
            if (m_first_page != nullptr)
            {
                return const_iterator(m_last_page->m_queue.end(), m_last_page);
            }
            else
            {
                return const_iterator();
            }
        }

        /** Returns true if this queue contains no elements.
            \n\b Throws: nothing
            \n\b Complexity: constant */
        bool empty() const noexcept { return m_first_page == nullptr; }

        /** Deletes all the elements from this queue.
            \n\b Throws: nothing
            \n\b Complexity: linear */
        void clear() noexcept
        {
            delete_all();
            m_first_page = nullptr;
            m_last_page = nullptr;

            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif
        }

        /** Returns a reference to the first element of this queue. If ELEMENT is void,
            the this function returns void.
            \pre The queue must be non-empty (otherwise the behavior is undefined)

            \n\b Throws: nothing
            \n\b Complexity: constant */
        reference front() noexcept
        {
            DENSITY_ASSERT(!empty());
            const auto it = m_first_page->m_queue.begin();
            return detail::DeferenceVoidPtr<ELEMENT>::apply(it.element());
        }

        /** Returns a const reference to the first element of this queue. If ELEMENT is void,
            the this function returns void.
            \pre The queue must be non-empty (otherwise the behavior is undefined)

            \n\b Throws: nothing
            \n\b Complexity: constant */
        const_reference front() const noexcept
        {
            DENSITY_ASSERT(!empty());
            const auto it = m_first_page->m_queue.begin();
            return detail::DeferenceVoidPtr<ELEMENT>::apply(it.element());
        }

        /** Returns the capacity in bytes of this queue, that is the total usable spacer in the allocated pages.
            \n Note: the capacity is not the memory consumed by the queue, as there is a constant overhead per-page.
            \remark There is no way to predict if the next push\emplace will cause a reallocation.

            \n\b Throws: nothing
            \n\b Complexity: constant */
        size_t mem_capacity() const noexcept
        {
            size_t result(0);
            for (PageHeader * page = m_first_page; page != nullptr; page = page->m_next_page)
            {
                result += page->m_queue.mem_capacity();
            }
            return result;
        }

        /** Returns the used size in bytes. This size is dependant, in an implementation defined way, to the count, the type and
            the order of the elements present in the queue. The return value is zero if and only if the queue is empty. It is highly
            recommended to use the function heterogeneous_queue::empty to check if the queue is empty.
            \remark There is no way to predict if the next push\emplace will cause a reallocation.

            \n\b Throws: nothing
            \n\b Complexity: constant */
        size_t mem_size() const noexcept
        {
            size_t result(0);
            for (PageHeader * page = m_first_page; page != nullptr; page = page->m_next_page)
            {
                result += page->m_queue.mem_size();
            }
            return result;
        }

        /** Returns the free size in bytes. This is equivalent to heterogeneous_queue::mem_capacity decreased by heterogeneous_queue::mem_size.
            \remark There is no way to predict if the next push\emplace will cause a reallocation.

            \n\b Throws: nothing
            \n\b Complexity: constant */
        size_t mem_free() const noexcept
        {
            size_t result(0);
            for (PageHeader * page = m_first_page; page != nullptr; page = page->m_next_page)
            {
                result += page->m_queue.mem_free();
            }
            return result;
        }

        /** Returns a copy of the allocator instance owned by the queue.
            \n\b Throws: nothing
            \n\b Complexity: constant */
        allocator_type get_allocator() const noexcept
        {
            return *this;
        }

        /** Returns a reference to the allocator instance owned by the queue.
            \n\b Throws: nothing
            \n\b Complexity: constant */
        allocator_type & get_allocator_ref() noexcept
        {
            return *this;
        }

        /** Returns a const reference to the allocator instance owned by the queue.
            \n\b Throws: nothing
            \n\b Complexity: constant */
        const allocator_type & get_allocator_ref() const noexcept
        {
            return *this;
        }

    private:

        struct PageHeader
        {
            detail::QueueImpl<RUNTIME_TYPE> m_queue;
            PageHeader * m_next_page;

            PageHeader(void * i_buffer_address, size_t i_buffer_byte_capacity) noexcept
                : m_queue(i_buffer_address, i_buffer_byte_capacity), m_next_page(nullptr) { }
        };

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
            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif

            try
            {
                while (m_last_page == nullptr || !m_last_page->m_queue.try_push(i_source_type, std::forward<CONSTRUCTOR>(i_constructor)))
                {
                    make_space(i_source_type.size(), i_source_type.alignment());
                }
            }
            catch (...)
            {
                /* if a new page is allocated and the constructor of the element throws, the queue is left
                    with an empty page (the last). This is a violation of the class invariants, so it must
                    be fixed in the exception flow. */
                if (m_last_page != nullptr && m_last_page->m_queue.empty())
                {
                    auto const page_to_delete = m_last_page;

                    if (m_first_page == page_to_delete)
                    {
                        m_last_page = m_first_page = nullptr;
                    }
                    else
                    {
                        // we have to find the previous page
                        auto page = m_first_page;
                        for (; page->m_next_page != page_to_delete; page = page->m_next_page)
                        {
                        }
                        page->m_next_page = nullptr;
                        m_last_page = page;
                    }
                    delete_page(page_to_delete);
                }
                throw;
                #if DENSITY_DEBUG_INTERNAL
                    check_invariants();
                #endif
            }

            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif
        }

        DENSITY_NO_INLINE void make_space(size_t i_required_size, size_t i_required_alignment)
        {
            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif

            const size_t min_page_size = detail::size_max(i_required_alignment, alignof(PageHeader)) + i_required_size + sizeof(PageHeader);

            PageHeader * alloc;
            size_t actual_page_size;
            const size_t allocator_page_size = ALLOCATOR::page_size();
            if (min_page_size <= allocator_page_size)
            {
                actual_page_size = allocator_page_size;
                alloc = static_cast<PageHeader*>( get_allocator_ref().allocate_page() );
            }
            else
            {
                actual_page_size = min_page_size;
                alloc = static_cast<PageHeader*>(get_allocator_ref().allocate(actual_page_size));
            }

            auto new_page = new(alloc) PageHeader(alloc + 1, actual_page_size - sizeof(PageHeader));
            if (m_last_page != nullptr)
            {
                m_last_page->m_next_page = new_page;
            }
            else
            {
                m_first_page = new_page;
            }
            m_last_page = new_page;
        }

        void delete_page(PageHeader * i_page) noexcept
        {
            const size_t allocator_page_size = ALLOCATOR::page_size();

            // assuming that the destructor of an empty QueueImpl is trivial
            DENSITY_ASSERT(i_page->m_queue.empty());

            size_t actual_page_size = i_page->m_queue.mem_capacity();
            DENSITY_ASSERT(actual_page_size >= sizeof(PageHeader));
            actual_page_size += sizeof(PageHeader);
            DENSITY_ASSERT(actual_page_size >= allocator_page_size);
            if (actual_page_size <= allocator_page_size)
            {
                get_allocator_ref().deallocate_page(i_page);
            }
            else
            {
                char * block = reinterpret_cast<char*>(i_page);
                get_allocator_ref().deallocate(block, actual_page_size);
            }
        }

        void delete_all() noexcept
        {
            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif

            for (PageHeader * page = m_first_page; page != nullptr;)
            {
                const auto next = page->m_next_page;
                page->m_queue.delete_all();

                delete_page(page);

                page = next;
            }
        }

        // overload used if the return type is to not void
        template <typename OPERATION>
            auto manual_consume_impl(OPERATION && i_operation, std::false_type)
                noexcept(noexcept((i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()))))
                    -> decltype(i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()))
        {
            auto const result = m_first_page->m_queue.manual_consume([&i_operation](const RUNTIME_TYPE & i_type, void * i_element) {
                return i_operation(i_type, static_cast<ELEMENT*>(i_element));
            });
            if (m_first_page->m_queue.empty())
            {
                auto const next = m_first_page->m_next_page;
                delete_page(m_first_page);
                m_first_page = next;
                if (next == nullptr)
                {
                    m_last_page = nullptr;
                }
            }

            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif

            return std::move(result);
        }

        // overload used if the return type is to void
        template <typename OPERATION>
            void manual_consume_impl(OPERATION && i_operation, std::true_type)
                noexcept(noexcept((i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()))))
        {
            m_first_page->m_queue.manual_consume([&i_operation](const RUNTIME_TYPE & i_type, void * i_element) {
                i_operation(i_type, static_cast<ELEMENT*>(i_element));
            });
            if (m_first_page->m_queue.empty())
            {
                auto const next = m_first_page->m_next_page;
                delete_page(m_first_page);
                m_first_page = next;
                if (next == nullptr)
                {
                    m_last_page = nullptr;
                }
            }

            #if DENSITY_DEBUG_INTERNAL
                check_invariants();
            #endif
        }

        #if DENSITY_DEBUG_INTERNAL
            void check_invariants() const
            {
                DENSITY_ASSERT( (m_first_page == nullptr) == (m_last_page == nullptr) );

                for (PageHeader * page = m_first_page; page != nullptr; page = page->m_next_page)
                {
                    DENSITY_ASSERT(!page->m_queue.empty());
                    DENSITY_ASSERT( (page->m_next_page == nullptr) == (page == m_last_page) );
                }
            }
        #endif

    private:
        PageHeader * m_first_page, * m_last_page;
    }; // class heterogeneous_queue

} // namespace density

