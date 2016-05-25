
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "detail\dense_list_impl.h"

namespace density
{
    /** A dense-list is a polymorphic sequence container optimized to be compact in both heap memory and inline storage.
        Elements is a dense_list are allocated respecting their alignment requirements.
        In a polymorphic container every element can have a different complete type, provided that this type is covariant to the type ELEMENT.
        All the elements of a dense_list are arranged in the same memory block of the heap.
        Insertions\removals of a non-zero number of elements and clear() always reallocate the memory blocks and invalidate existing iterators.
        The inline storage of dense_list is the same of a pointer. An empty dense_list does not use heap memory.
        All the functions of dense_list gives at least the strong exception guarantee. */
    template <typename ELEMENT, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class dense_list final
    {
    private:

        using ListImpl = detail::DenseListImpl<ALLOCATOR, RUNTIME_TYPE>;
        using IteratorImpl = typename ListImpl::IteratorBaseImpl;

    public:

        using allocator_type = ALLOCATOR;
        using difference_type = ptrdiff_t;
        using size_type = size_t;
        using value_type = ELEMENT;
        using reference = ELEMENT &;
        using const_reference = const ELEMENT &;
        using pointer = typename std::allocator_traits<allocator_type>::pointer;
        using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

        /** Alias for the template arguments */
        using runtime_type = RUNTIME_TYPE;

        /** Creates a dense_list containing all the elements specified in the parameter list.
            For each object of the parameter pack, an element is added to the list by copy-construction or move-construction.
                @param i_args elements to add to the list.
                @return the new dense_list
            Example:
                const auto list = dense_list<int>::make(1, 2, 3);
                const auto list1 = dense_list<ListImpl>::make(Derived1(), Derived2(), Derived1()); */
        template <typename... TYPES>
            inline static dense_list make(TYPES &&... i_args)
        {
            static_assert(AllCovariant<ELEMENT, TYPES...>::value, "All the paraneter types must be covariant to ELEMENT" );
            dense_list new_list;
            ListImpl::template make_impl<ELEMENT>(new_list.m_impl, std::forward<TYPES>(i_args)...);
            return std::move(new_list);
        }

        /** Creates a dense_list containing all the elements specified in the parameter list. The allocator of the new dense_list is copy-constructed from the provided one.
            For each object of the parameter pack, an element is added to the list by copy-construction or move-construction.
                @param i_args elements to add to the list.
                @return the new dense_list
            Example:
                MyAlloc<int> my_alloc;
                MyAlloc<ListImpl> my_alloc1;
                const auto list = dense_list<int>::make_with_alloc(my_alloc, 1, 2, 3);
                const auto list1 = dense_list<ListImpl>::make_with_alloc(my_alloc1, Derived1(), Derived2(), Derived1()); */
        template <typename... TYPES>
            inline static dense_list make_with_alloc(const ALLOCATOR & i_allocator, TYPES &&... i_args)
        {
            static_assert(AllCovariant<ELEMENT, TYPES...>::value, "Al the paraneter types must be covariant to ELEMENT");
            dense_list new_list;
            ListImpl::template make_impl<ELEMENT>(new_list.m_impl, std::forward<TYPES>(i_args)...);
            return std::move(new_list);
        }


        size_t size() const DENSITY_NOEXCEPT
        {
            return m_impl.size();
        }

        bool empty() const DENSITY_NOEXCEPT
        {
            return m_impl.empty();
        }

        class iterator;
        class const_iterator;

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

            iterator(const IteratorImpl & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source) {  }

            value_type & operator * () const DENSITY_NOEXCEPT { return *element(); }
            value_type * operator -> () const DENSITY_NOEXCEPT { return element(); }
            value_type * element() const DENSITY_NOEXCEPT
                { return static_cast<value_type *>(m_impl.element()); }

            iterator & operator ++ () DENSITY_NOEXCEPT
            {
                m_impl.move_next();
                return *this;
            }

            iterator operator++ (int) DENSITY_NOEXCEPT
            {
                iterator copy(*this);
                m_impl.move_next();
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

			const RUNTIME_TYPE & complete_type() const DENSITY_NOEXCEPT { return m_impl.complete_type(); }
		
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
            using reference = const ELEMENT &;
            using const_reference = const ELEMENT &;
            using pointer = typename std::allocator_traits<allocator_type>::pointer;
            using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

            const_iterator(const IteratorImpl & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source) {  }

            const_iterator(const iterator & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source.m_impl) {  }

            value_type & operator * () const DENSITY_NOEXCEPT { return *element(); }
            value_type * operator -> () const DENSITY_NOEXCEPT { return element(); }
            value_type * element() const DENSITY_NOEXCEPT
                { return static_cast<value_type *>(m_impl.element()); }

            const_iterator & operator ++ () DENSITY_NOEXCEPT
            {
                m_impl.move_next();
                return *this;
            }

            const_iterator operator++ (int) DENSITY_NOEXCEPT
            {
				const_iterator copy(*this);
                m_impl.move_next();
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

			const RUNTIME_TYPE & complete_type() const DENSITY_NOEXCEPT { return m_impl.complete_type(); }

			friend class dense_list; // this allows dense_list to access m_impl
			            
        private:
            IteratorImpl m_impl;
        }; // class const_iterator

        iterator begin() DENSITY_NOEXCEPT { return iterator(m_impl.begin()); }
        iterator end() DENSITY_NOEXCEPT { return iterator(m_impl.end()); }

        const_iterator begin() const DENSITY_NOEXCEPT { return const_iterator(m_impl.begin()); }
        const_iterator end() const DENSITY_NOEXCEPT{ return const_iterator(m_impl.end()); }

        const_iterator cbegin() const DENSITY_NOEXCEPT { return const_iterator(m_impl.begin()); }
        const_iterator cend() const DENSITY_NOEXCEPT { return const_iterator(m_impl.end()); }

        template <typename ELEMENT_COMPLETE_TYPE>
            void push_back(const ELEMENT_COMPLETE_TYPE & i_source)
                DENSITY_NOEXCEPT_IF(std::is_nothrow_copy_constructible<ELEMENT_COMPLETE_TYPE>::value)
        {
            m_impl.insert_impl(m_impl.get_control_blocks() + m_impl.size(),
                runtime_type::template make<ELEMENT_COMPLETE_TYPE>(),
                CopyConstruct(&i_source) );
        }

        template <typename ELEMENT_COMPLETE_TYPE>
            void push_front(const ELEMENT_COMPLETE_TYPE & i_source)
                DENSITY_NOEXCEPT_IF(std::is_nothrow_copy_constructible<ELEMENT_COMPLETE_TYPE>::value)
        {
            m_impl.insert_impl(m_impl.get_control_blocks(),
                runtime_type::template make<ELEMENT_COMPLETE_TYPE>(),
                CopyConstruct(&i_source) );
        }

        template <typename ELEMENT_COMPLETE_TYPE>
            void push_back(ELEMENT_COMPLETE_TYPE && i_source)
                DENSITY_NOEXCEPT_IF(std::is_nothrow_copy_constructible<ELEMENT_COMPLETE_TYPE>::value)
        {
            m_impl.insert_impl(m_impl.get_control_blocks() + m_impl.size(),
                runtime_type::template make<ELEMENT_COMPLETE_TYPE>(),
                MoveConstruct(&i_source) );
        }

        template <typename ELEMENT_COMPLETE_TYPE>
            void push_front(ELEMENT_COMPLETE_TYPE && i_source)
        {
            m_impl.insert_impl(m_impl.get_control_blocks(),
                runtime_type::template make<ELEMENT_COMPLETE_TYPE>(),
                MoveConstruct(&i_source) );
        }

        void pop_front()
        {
            auto const at = m_impl.get_control_blocks();
            m_impl.erase_impl(at, at + 1);
        }

        void pop_back()
        {
			auto const at = m_impl.get_control_blocks() + m_impl.get_size_not_empty();
            m_impl.erase_impl(at - 1, at);
        }

        template <typename ELEMENT_COMPLETE_TYPE>
            iterator insert(const_iterator i_position, const ELEMENT_COMPLETE_TYPE & i_source)
        {
            return m_impl.insert_impl(i_position.m_impl.control(),
                runtime_type::template make<ELEMENT_COMPLETE_TYPE>(),
                CopyConstruct(&i_source) );
        }

        template <typename ELEMENT_COMPLETE_TYPE>
            iterator insert(const_iterator i_position, size_t i_count, const ELEMENT_COMPLETE_TYPE & i_source)
        {
            if (i_count > 0)
            {
                return m_impl.insert_n_impl(i_position.m_impl.control(), i_count,
                    runtime_type::template make<ELEMENT_COMPLETE_TYPE>(),
                    CopyConstruct(&i_source) );
            }
            else
            {
                // inserting 0 elements
                return iterator(i_position.m_impl);
            }
        }

        iterator erase(const_iterator i_position)
        {
            return m_impl.erase_impl(i_position.m_impl.m_curr_type, i_position.m_impl.m_curr_type + 1);
        }

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

        void swap(dense_list & i_other) DENSITY_NOEXCEPT
        {
            std::swap(m_impl.edit_control_blocks(), m_impl.i_other.edit_control_blocks());
        }

                    /////////////////////////

        /* to do, & WARNING!: this function is slicing-comparing. Fix or delete. */
        bool equal_to(const dense_list & i_source) const
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
                    if (*it_1 != *it_2)
                    {
                        return false;
                    }
                }
                return true;
            }
        }

        bool operator == (const dense_list & i_source) const { return equal_to(i_source); }
        bool operator != (const dense_list & i_source) const { return !equal_to(i_source); }

    private:

        struct CopyConstruct
        {
            const ELEMENT * const m_source;

            CopyConstruct(const ELEMENT * i_source)
                : m_source(i_source) { }

            void * operator () (typename ListImpl::ListBuilder & i_builder, const RUNTIME_TYPE & i_element_type)
            {
                return i_builder.add_by_copy(i_element_type, m_source);
            }
        };

        struct MoveConstruct
        {
            ELEMENT * const m_source;

            MoveConstruct(ELEMENT * i_source)
                : m_source(i_source) { }

            void * operator () (typename ListImpl::ListBuilder & i_builder, const RUNTIME_TYPE & i_element_type) DENSITY_NOEXCEPT
            {
                return i_builder.add_by_move(i_element_type, m_source);
            }
        };

    private:
        detail::DenseListImpl<ALLOCATOR, RUNTIME_TYPE> m_impl;
    }; // class dense_list;

    template <typename ELEMENT, typename... TYPES>
        inline dense_list<ELEMENT> make_dense_list(TYPES &&... i_args)
    {
        return dense_list<ELEMENT>::make(std::forward<TYPES>(i_args)...);
    }

} // namespace density
