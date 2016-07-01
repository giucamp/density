
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "detail\array_impl.h"
#include "void_allocator.h"

namespace density
{
    /** \brief Class template that implements an heterogeneous sequence container optimized to be compact in both heap memory and inline storage.
        Elements is a heterogeneous_array are allocated tightly in the same dynamic memory block, respecting their alignment requirements.
        To be added to a heterogeneous_array, an element must have a type covariant to the template argument ELEMENT. If ELEMENT is void
        (the default value), any element can be added.
        Unlike std::vector, heterogeneous_array does not provide any extra capacity management. As a consequence, the complexity
        of many methods is linear, in contrast with the constant amortized time of the equivalent methods of std::vector. Insertions
        and removals of a non-zero number of elements and clear() always reallocate the memory blocks and invalidate
        existing iterators
        The sizeof(heterogeneous_array) is the same of a pointer, provided that the template argument VOID_ALLOCATOR is an empty class
        and the compiler supports the empty base class optimization. An empty heterogeneous_array does not own a dynamic memory block.
        \n\b Thread safeness: None. The user is responsible to avoid race conditions.
        \n<b>Exception safeness</b>: Any function of heterogeneous_array is noexcept or provides the strong exception guarantee.
            @param ELEMENT Base type of the elements of the list. The list enforces the compile-time
                constraint that the type of each element is covariant to ELEMENT.
                If ELEMENT is void, elements of any complete type can be added to the container. In this
                case, the methods of heterogeneous_array (and its iterators) that returns a pointer to an element
                will return a void* to a complete object, while the methods that returns a reference to
                an element will return void. Use iterators and pointer semantic to write generic code
                that works with any heterogeneous_array.
                If ELEMENT decays to void but it is not a plain void, a compile time error is issued.
                Note: if ELEMENT is to be a built-in type, a pointer, or a final type, then the complete
                type of all elements will always be ELEMENT (that is, the container will not be heterogeneous). In
                this case a standard container (like std::vector) instead of std::heterogeneous_array is a better choice.
                If ELEMENT is not void, it must be noexcept move constructible.
            @param VOID_ALLOCATOR Allocator to be used to allocate the memory buffer. The list may rebind
                this allocator to a different type, eventually unrelated to ELEMENT.
            @param RUNTIME_TYPE Type to be used to represent the actual complete type of each element.
                This type must meet the requirements of RuntimeType.
        heterogeneous_array provides only forward iteration. Only the first element is accessible in constant time (with
        the functions: heterogeneous_array::front, heterogeneous_array::begin). The iterator provides access to both the ELEMENT (with
        the function element) and the RUNTIME_TYPE (with the function type).
        Limitations: when an element of COMPETE_ELEMENT is pushed to the heterogeneous_array, the current implementation needs
            sometimes to downcast from ELEMENT to COMPETE_ELEMENT.
                - If no virtual inheritance is involved, static_cast is used
                - If virtual inheritance is involved, dynamic_cast is used. Anyway, in this case, ELEMENT must be
                    a polymorphic type, otherwise there is no way to perform the downcast (in this case a compile-
                    time error is issued). */
    template <typename ELEMENT = void, typename VOID_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class heterogeneous_array final
    {
    private:

        using ListImpl = typename detail::ArrayImpl<VOID_ALLOCATOR, RUNTIME_TYPE>;
        using IteratorImpl = typename ListImpl::IteratorBaseImpl;

    public:

        using allocator_type = VOID_ALLOCATOR;
        using runtime_type = RUNTIME_TYPE;
        using value_type = ELEMENT;
        using reference = typename std::add_lvalue_reference< ELEMENT >::type;
        using const_reference = typename std::add_lvalue_reference< const ELEMENT>::type;
        using difference_type = ptrdiff_t;
        using size_type = size_t;
        class iterator;
        class const_iterator;

        /** Creates a heterogeneous_array containing all the elements specified in the parameter list.
            For each object of the parameter pack, an element is added to the list by copy-construction or move-construction.
                @param i_elements elements to add to the list.
                @return the new heterogeneous_array
            Example:
                const auto list = heterogeneous_array<>::make(1, std::string("abc"), 2.5);
                const auto list1 = heterogeneous_array<ListImpl>::make(Derived1(), Derived2(), Derived1()); */
        template <typename... TYPES>
            inline static heterogeneous_array make(TYPES &&... i_elements)
        {
            static_assert(detail::AllCovariant<ELEMENT, TYPES...>::value, "All the parameter types must be covariant to ELEMENT" );
            heterogeneous_array new_list;
            ListImpl::template make_impl<ELEMENT>(new_list.m_impl, std::forward<TYPES>(i_elements)...);
            return std::move(new_list);
        }

        /** Creates a heterogeneous_array containing all the elements specified in the parameter list. The allocator of the new heterogeneous_array is copy-constructed from the provided one.
            For each object of the parameter pack, an element is added to the list by copy-construction or move-construction.
                @param i_args elements to add to the list.
                @return the new heterogeneous_array
            Example:
                MyAlloc<int> my_alloc;
                MyAlloc<ListImpl> my_alloc1;
                const auto list = heterogeneous_array<int>::make_with_alloc(my_alloc, 1, 2, 3);
                const auto list1 = heterogeneous_array<ListImpl>::make_with_alloc(my_alloc1, Derived1(), Derived2(), Derived1()); */
        template <typename... TYPES>
            inline static heterogeneous_array make_with_alloc(const VOID_ALLOCATOR & i_allocator, TYPES &&... i_elements)
        {
            static_assert(detail::AllCovariant<ELEMENT, TYPES...>::value, "All the parameter types must be covariant to ELEMENT");
            heterogeneous_array new_list;
            ListImpl::template make_impl<ELEMENT>(new_list.m_impl, std::forward<TYPES>(i_elements)...);
            return std::move(new_list);
        }


        size_t size() const noexcept
        {
            return m_impl.size();
        }

        bool empty() const noexcept
        {
            return m_impl.empty();
        }

        class iterator final
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = ptrdiff_t;
            using size_type = size_t;
            using value_type = ELEMENT;
            using pointer = ELEMENT *;
            using reference = typename heterogeneous_array::reference;
            using const_reference = typename heterogeneous_array::const_reference;

            iterator(const IteratorImpl & i_source) noexcept
                : m_impl(i_source) {  }

            reference operator * () const noexcept { return *element(); }
            pointer operator -> () const noexcept { return element(); }
            pointer element() const noexcept
                { return static_cast<value_type *>(m_impl.element()); }

            iterator & operator ++ () noexcept
            {
                m_impl.move_next();
                return *this;
            }

            iterator operator++ (int) noexcept
            {
                iterator copy(*this);
                m_impl.move_next();
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

            const RUNTIME_TYPE & complete_type() const noexcept { return m_impl.complete_type(); }

            friend class const_iterator; // this allows const_iterator to access m_impl

        private:
            IteratorImpl m_impl;
        }; // class iterator

        class const_iterator final
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = ptrdiff_t;
            using size_type = size_t;
            using value_type = const ELEMENT;
            using pointer = const ELEMENT *;
            using reference = typename heterogeneous_array::const_reference;
            using const_reference = typename heterogeneous_array::const_reference;

            const_iterator(const iterator & i_source) noexcept
                : m_impl(i_source.m_impl) {  }

            reference operator * () const noexcept { return *element(); }
            pointer operator -> () const noexcept { return element(); }
            pointer element() const noexcept
                { return static_cast<value_type *>(m_impl.element()); }

            const_iterator & operator ++ () noexcept
            {
                m_impl.move_next();
                return *this;
            }

            const_iterator operator ++ (int) noexcept
            {
                const_iterator copy(*this);
                m_impl.move_next();
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

            const RUNTIME_TYPE & complete_type() const noexcept { return m_impl.complete_type(); }

            friend class heterogeneous_array; // this allows heterogeneous_array to access m_impl

        private:
            const_iterator(const IteratorImpl & i_source) noexcept
                : m_impl(i_source) {  }

            IteratorImpl m_impl;
        }; // class const_iterator

        iterator begin() noexcept { return iterator(m_impl.begin()); }
        iterator end() noexcept { return iterator(m_impl.end()); }

        const_iterator begin() const noexcept { return const_iterator(m_impl.begin()); }
        const_iterator end() const noexcept{ return const_iterator(m_impl.end()); }

        const_iterator cbegin() const noexcept { return const_iterator(m_impl.begin()); }
        const_iterator cend() const noexcept { return const_iterator(m_impl.end()); }

        /** Adds an element at the end of the list. This method causes always a reallocation of the list.
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
            \n\b Complexity: linear (a reallocation is always required). */
        template <typename ELEMENT_COMPLETE_TYPE>
            void push_back(ELEMENT_COMPLETE_TYPE && i_source)
        {
            static_assert( std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
                "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT, or ELEMENT must be void" );
            insert_n_impl(m_impl.get_control_blocks() + m_impl.size(), 1,
                std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
        }

        /** Adds an element at the begin of the list. This method causes always a reallocation of the list.
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
            \n\b Complexity: linear (a reallocation is always required). */
        template <typename ELEMENT_COMPLETE_TYPE>
            void push_front(ELEMENT_COMPLETE_TYPE && i_source)
        {
            static_assert( std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
                "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT, or ELEMENT must be void" );
            insert_n_impl(m_impl.get_control_blocks(), 1,
                std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
        }

        /** Adds an element at the specified position of the list. This method causes always a reallocation of the list.
            @param i_at position in which the new element should be inserted. It must be an iterator pointing to a valid element
                of this list, or it can be equal to cend(). Note: a iterator is implicitly converted to a cons_iterator.
            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed (and the source object is left unchanged).
                - If this argument is an r-value, the new element move-constructed (and the source object will have an undefined but valid content).
            @return an iterator that points to the newly inserted element.
            \n\b Requires:
                - the type ELEMENT_COMPLETE_TYPE must copy constructible (in case of l-value) or move constructible (in case of r-value)
                - an ELEMENT_COMPLETE_TYPE * must be implicitly convertible to ELEMENT *
                - an ELEMENT * must be convertible to an ELEMENT_COMPLETE_TYPE * with a static_cast or a dynamic_cast
                    (this requirement is not met for example if ELEMENT is a non-polymorphic (direct or indirect) virtual
                    base of ELEMENT_COMPLETE_TYPE).

            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: linear (a reallocation is always required). */
        template <typename ELEMENT_COMPLETE_TYPE>
            iterator insert(const const_iterator & i_at, ELEMENT_COMPLETE_TYPE && i_source)
        {
            static_assert( std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
                "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT, or ELEMENT must be void" );
            return insert_n_impl(i_at.m_impl.control(), 1,
                std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
        }

        /** Adds new elements at the specified position of the list. This method causes always a reallocation of the list.
            @param i_at position in which the new element should be inserted. It must be an iterator pointing to a valid element
                of this list, or it can be equal to cend(). Note: a iterator is implicitly converted to a cons_iterator.
            @param i_count number of element to insert. Any value is valid (including zero).
            @param i_source object to be used as source to construct of new elements. Unlike the members functions to insert a
                single element, in this overload of insert the sound always binds to an l-value.
            @return an iterator that points to the newly inserted element.
            \n\b Requires:
                - the type ELEMENT_COMPLETE_TYPE must copy constructible.
                - an ELEMENT_COMPLETE_TYPE * must be implicitly convertible to ELEMENT *
                - an ELEMENT * must be convertible to an ELEMENT_COMPLETE_TYPE * with a static_cast or a dynamic_cast
                    (this requirement is not met for example if ELEMENT is a non-polymorphic (direct or indirect) virtual
                    base of ELEMENT_COMPLETE_TYPE).

            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: linear (a reallocation is always required). */
        template <typename ELEMENT_COMPLETE_TYPE>
            iterator insert(const const_iterator & i_at, size_type i_count, const ELEMENT_COMPLETE_TYPE & i_source)
        {
            static_assert( std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
                "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT, or ELEMENT must be void" );
            if (i_count > 0)
            {
                return insert_n_impl(i_at.m_impl.control(), i_count,
                    i_source, std::false_type::type());
            }
            else
            {
                // inserting 0 elements
                return iterator(i_at.m_impl);
            }
        }

        /** Removes and destroys an element at the specified position of the list. This method causes always a reallocation of the list.
            @param i_at position of the element to delete. It must be an iterator pointing to a valid element
                of this list. It can't be equal to cend(). Note: a iterator is implicitly converted to a cons_iterator.
            @return an iterator that points to the location of the erased element, or end() if the list becomes empty.

            Note: this function can throw because is causes a reallocation.

            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: linear (a reallocation is always required). */
        iterator erase(const_iterator i_position)
        {
            return m_impl.erase_impl(i_position.m_impl.control(), i_position.m_impl.control() + 1);
        }

        /** Removes and destroy a range of elements at the specified position of the list. This method causes always a reallocation of the list.
            @param i_from position of the first element to delete. It must be an iterator pointing to a valid element
                of this list. It can be equal to cend() only if i_to is equal to cend(). Note: a iterator is implicitly converted to a cons_iterator.
            @param i_to position of the first element that is not deleted. It must be an iterator pointing to a valid element
                of this list. It can be equal to cend(). Note: a iterator is implicitly converted to a cons_iterator.
            @return an iterator that points to the location of the first erased element, or end() if the list becomes empty.
            Note: this function can throw because is causes a reallocation.
            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            \n\b Complexity: linear (a reallocation is always required). */
        iterator erase(const_iterator i_from, const_iterator i_to)
        {
            auto from_type = i_from.m_impl.control();
            auto to_type = i_to.m_impl.control();
            if (from_type != to_type)
            {
                return m_impl.erase_impl(from_type, to_type);
            }
            else
            {
                // removing 0 elements
                return iterator(i_from.m_impl);
            }
        }

        void swap(heterogeneous_array & i_other) noexcept
        {
            std::swap(m_impl.edit_control_blocks(), m_impl.i_other.edit_control_blocks());
        }

        bool operator == (const heterogeneous_array & i_source) const
        {
            if (m_impl.size() != i_source.size())
            {
                return false;
            }
            else
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
        }

        bool operator != (const heterogeneous_array & i_source) const { return !operator == (i_source); }

    private:

        struct copy_construct
        {
            const ELEMENT * const m_source;

            copy_construct(const ELEMENT * i_source)
                : m_source(i_source) { }

            void * operator () (typename ListImpl::ListBuilder & i_builder, const RUNTIME_TYPE & i_element_type)
            {
                return i_builder.add_by_copy(i_element_type, m_source);
            }
        };

        struct move_construct
        {
            ELEMENT * const m_source;

            move_construct(ELEMENT * i_source)
                : m_source(i_source) { }

            void * operator () (typename ListImpl::ListBuilder & i_builder, const RUNTIME_TYPE & i_element_type) noexcept
            {
                return i_builder.add_by_move(i_element_type, m_source);
            }
        };

        // overload used if i_source is an r-value
        template <typename ELEMENT_COMPLETE_TYPE>
            typename ListImpl::IteratorBaseImpl insert_n_impl(
                const typename ListImpl::ControlBlock * i_position,
                size_t i_count_to_insert,
                ELEMENT_COMPLETE_TYPE && i_source, std::true_type)
        {
            using ElementCompleteType = typename std::decay<ELEMENT_COMPLETE_TYPE>::type;

            /* Note that the specialization of the function template insert_n_impl does not
                depend on ELEMENT_COMPLETE_TYPE. In this way the same machine code will be used
                for this call to push_back_impl, no mater what ELEMENT_COMPLETE_TYPE is. */
            return m_impl.insert_n_impl(i_position, i_count_to_insert,
                runtime_type::template make<ElementCompleteType>(),
                move_construct(&i_source) );
        }

        // overload used if i_source is an l-value
        template <typename ELEMENT_COMPLETE_TYPE>
            typename ListImpl::IteratorBaseImpl insert_n_impl(
                const typename ListImpl::ControlBlock * i_position,
                size_t i_count_to_insert,
                ELEMENT_COMPLETE_TYPE && i_source, std::false_type)
        {
            using ElementCompleteType = typename std::decay<ELEMENT_COMPLETE_TYPE>::type;

            /* Note that the specialization of the function template insert_n_impl does not
                depend on ELEMENT_COMPLETE_TYPE. In this way the same machine code will be used
                for this call to push_back_impl, no mater what ELEMENT_COMPLETE_TYPE is. */
            return m_impl.insert_n_impl(i_position, i_count_to_insert,
                runtime_type::template make<ElementCompleteType>(),
                copy_construct(&i_source) );
        }

    private:
        detail::ArrayImpl<VOID_ALLOCATOR, RUNTIME_TYPE> m_impl;
    }; // class heterogeneous_array;

    template <typename ELEMENT, typename... TYPES>
        inline heterogeneous_array<ELEMENT> make_dense_list(TYPES &&... i_args)
    {
        return heterogeneous_array<ELEMENT>::make(std::forward<TYPES>(i_args)...);
    }

} // namespace density
