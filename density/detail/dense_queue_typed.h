
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <memory>

namespace density
{
    /** Class template that implements an heterogeneous fifo container with dynamic size. 
        A dense_queue allocates one memory buffer (with the provided allocator), and sub-allocates inplace
		its elements. The buffer is reallocated accompish push and emplace requests.
        dense_queue provides only forward iteration.
        Insertion is allowed only at the end (with the methods dense_queue::push or dense_queue::emplace).
        Removal is allowed ony at the begin (with the methods dense_queue::pop or dense_queue::consume). */
    template <typename ELEMENT, typename ALLOCATOR, typename RUNTIME_TYPE>
        class dense_queue final : private std::allocator_traits<ALLOCATOR>::template rebind_alloc<char>
    {
    public:

        using runtime_type = RUNTIME_TYPE;
        using value_type = ELEMENT;
        using reference = ELEMENT &;
        using const_reference = const ELEMENT &;
        using difference_type = ptrdiff_t;
        using size_type = size_t;
        class iterator;
        class const_iterator;

		/** Constructs a queue.
			@param i_initial_reserved_bytes initial capacity to reserve. The actual reserved capacity may be bigger.
			@param i_initial_alignment alignment of the initial buffer. The actual alignment may be bigger.
			Preconditions: none
			Throws: unspecified */
        dense_queue(size_t i_initial_reserved_bytes = 0, size_t i_initial_alignment = 0)
        {
            alloc( std::max(i_initial_reserved_bytes, s_initial_mem_reserve),
				std::max(i_initial_alignment, s_initial_mem_alignment) );
        }

		/** Constructs a queue.
			@param i_initial_reserved_bytes initial capacity to reserve. The actual reserved capacity may be bigger.
			@param i_initial_alignment alignment of the initial buffer. The actual alignment may be bigger.
			Preconditions: none
			Throws: unspecified */
		/*dense_queue(const ALLOCATOR & i_allocator, size_t i_initial_reserved_bytes = 0, size_t i_initial_alignment = 0)
		{
			alloc(std::max(i_initial_reserved_bytes, QueueImp::s_initial_mem_reserve),
				std::max(i_initial_alignment, QueueImp::s_minimum_buffer_alignment));
		}*/

		/** Move constructor. The move constructor of the allocator is required to be noexcept, otherwise the compilation fails.
			@param i_source queue to move from. After the call this queue will be empty
			Throws: nothing
			Complexity: constant */
        dense_queue(dense_queue && i_source) DENSITY_NOEXCEPT
            : Allocator(std::move(static_cast<Allocator&>(i_source))),
              m_impl(std::move(i_source.m_impl))
        {
			static_assert(DENSITY_NOEXCEPT_IF( Allocator(std::move(std::declval<Allocator>()))),
				"The move constructor of the allocator is required to be noexcept");
        }

		/** Move assigment. The content of this queue is cleared, then the content of the source is moved to this queue.
			@param i_source queue to move from. After the call this queue will be empty
			Preconditions: 
				* the function fails to compile if the move-assignment of the allocator is not noexcept
				* the behaviour is undefined if the source and the destination are same object
			Throws: nothing
			Complexity: linear in the size of destination (its content must be destroyed). */
        dense_queue & operator = (dense_queue && i_source) DENSITY_NOEXCEPT
        {
			static_assert(DENSITY_NOEXCEPT_IF(std::declval<Allocator>() = std::move(std::declval<Allocator>())),
				"The move assignment of the allocator is required to be noexcept");

            DENSITY_ASSERT(this != &i_source);
			m_impl.delete_all();
			free();
			static_cast<Allocator&>(*this) = std::move( static_cast<Allocator&>(i_source) );
            m_impl = std::move(i_source.m_impl);
            return *this;
        }

		/** Copy constructor. Copyes the content of the source queue (deep copy).
			Preconditions: none
			Throws: anything that the allocator or the copy-constructor of the element throws
			Complexity: linear in the size of the source. */
        dense_queue(const dense_queue & i_source)
			: Allocator(static_cast<const Allocator&>(i_source))
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

		/** Copy constructor. Copyes the content of the source queue (deep copy).
			Preconditions: 
				* the behaviour is undefined if the source and the destination are same object
			Throws:
				* anything that the allocator throws
				* anything that the copy-constructor of the element throws
			Complexity: linear in the size of the source. */
        dense_queue & operator = (const dense_queue & i_source)
        {
            DENSITY_ASSERT(this != &i_source);
			dense_queue tmp(i_source); // the copy may throw, with this queue still beeing unmodified
			// from now on nothing can throw
			*this = std::move(tmp);
            return *this;
        }

		/** Destructor.		
			Preconditions: none
			Throws: nothing
			Complexity: linear in the size of this queue. */
        ~dense_queue()
        {
			m_impl.delete_all();
			free();
        }

                    // insertion \ removal
        
		/** Adds an element at the end of the queue. 
			@param i_source object to be used as source for the construction of the new element
				If this argument is an l-value, the new element copy-constructed (and the source object 
				is left unchanged).
				If this argument is an rvalue, the new element move-constructed (and the source object
				will have an undefined but valid content).
			
			*/
        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            push_impl(std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
        }

        template <typename ELEMENT_COMPLETE_TYPE, typename... PARAMETERS>
            void emplace(PARAMETERS && ... i_parameters)
                DENSITY_NOEXCEPT_IF((std::is_nothrow_constructible<ELEMENT_COMPLETE_TYPE, PARAMETERS...>::value))
        {
            return insert_back_impl(runtime_type::template make<ELEMENT_COMPLETE_TYPE>(),
                [&i_parameters...](const runtime_type &, void * i_dest) {
                    void * const result = new(i_dest) ELEMENT_COMPLETE_TYPE(std::forward<PARAMETERS>(i_parameters)...);
                    return result;
            });
        }

        bool copy_push(const RUNTIME_TYPE & i_type, const ELEMENT * i_source)
        {
            return insert_back_impl(i_type,
                typename detail::QueueImpl<RUNTIME_TYPE>::CopyConstruct(i_source));
        }

        bool move_push(const RUNTIME_TYPE & i_type, ELEMENT * i_source) DENSITY_NOEXCEPT
        {
            return insert_back_impl(i_type,
                typename detail::QueueImpl<RUNTIME_TYPE>::MoveConstruct(i_source));
        }

        template <typename OPERATION>
            void consume(OPERATION && i_operation)
                DENSITY_NOEXCEPT_IF(DENSITY_NOEXCEPT_IF((i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT>()))))
        {
            m_impl.consume([&i_operation](const RUNTIME_TYPE & i_type, void * i_element) {
                i_operation(i_type, *static_cast<ELEMENT*>(i_element));
            });
        }

        void pop() DENSITY_NOEXCEPT
        {
            m_impl.consume([](const RUNTIME_TYPE &, void *) {});
        }

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
            using reference = ELEMENT &;
            using const_reference = const ELEMENT &;

            iterator() DENSITY_NOEXCEPT {}

            iterator(const typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source) {  }

            value_type & operator * () const DENSITY_NOEXCEPT { return *static_cast<value_type *>(m_impl.element()); }
            value_type * operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }
            value_type * curr_element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }

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

			const RUNTIME_TYPE & type() const DENSITY_NOEXCEPT
			{
				return m_impl.curr_type();
			}

            bool is_end() const DENSITY_NOEXCEPT
            {
                return m_impl.is_end();
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
            using reference = const ELEMENT &;
            using const_reference = const ELEMENT &;
            
            const_iterator() DENSITY_NOEXCEPT {}

            const_iterator(const typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source) {  }

            const_iterator(const iterator & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source.m_impl) {  }

            value_type & operator * () const DENSITY_NOEXCEPT { return *static_cast<value_type *>(m_impl.element()); }
            value_type * operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }
            value_type * element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }

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

            const RUNTIME_TYPE & type() const DENSITY_NOEXCEPT
			{ 
				return m_impl.type();
			}

            bool is_end() const DENSITY_NOEXCEPT
            {
                return m_impl.is_end();
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

        bool empty() const DENSITY_NOEXCEPT { return m_impl.empty(); }

        void clear() DENSITY_NOEXCEPT 
        { 
            m_impl.delete_all(); 
        }
        
        const ELEMENT & front() DENSITY_NOEXCEPT
        {
            DENSITY_ASSERT(!empty());
            const auto it = m_impl.begin();
            return *static_cast<value_type *>(it.element());
        }

        MemSize mem_capacity() const DENSITY_NOEXCEPT
        {
            return m_impl.mem_capacity();
        }

        MemSize mem_size() const DENSITY_NOEXCEPT
        {
            return m_impl.mem_size();
        }

        MemSize mem_free() const DENSITY_NOEXCEPT
        {
            return m_impl.mem_capacity() - m_impl.mem_size();
        }

    private:

		static const size_t s_initial_mem_reserve = 1024 > detail::QueueImpl<RUNTIME_TYPE>::s_minimum_buffer_size ? 1024 : detail::QueueImpl<RUNTIME_TYPE>::s_minimum_buffer_size;
		static const size_t s_initial_mem_alignment = detail::QueueImpl<RUNTIME_TYPE>::s_minimum_buffer_alignment;

        using Impl = typename detail::QueueImpl<RUNTIME_TYPE>;
        using Allocator = typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char>;

        Allocator & allocator() DENSITY_NOEXCEPT
        {
            return *this;
        }

        void alloc(size_t i_size, size_t i_alignment)
        {
            m_impl = Impl(AllocatorUtils::aligned_allocate(allocator(), i_size, i_alignment, 0), i_size);
        }

        void free() DENSITY_NOEXCEPT
        {
            AllocatorUtils::aligned_deallocate(allocator(), m_impl.buffer(), m_impl.mem_capacity().value());
        }
				
        void mem_realloc_impl(size_t i_mem_size)
        {
            DENSITY_ASSERT(i_mem_size > m_impl.mem_capacity().value());
			
            Impl new_impl(AllocatorUtils::aligned_allocate(allocator(), i_mem_size, m_impl.element_max_alignment(), 0), i_mem_size);
			try
			{
				new_impl.move_elements_from(m_impl);
			}
			catch (...)
			{
				AllocatorUtils::aligned_deallocate(allocator(), new_impl.buffer(), new_impl.mem_capacity().value());
				throw;
			}

			// from now on, nothing can throw
            AllocatorUtils::aligned_deallocate(allocator(), m_impl.buffer(), m_impl.mem_capacity().value());
            m_impl = std::move(new_impl);
        }

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

        // overload used if i_source is an rvalue
        template <typename ELEMENT_COMPLETE_TYPE>
            void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::true_type)
        {
            insert_back_impl(runtime_type::template make<typename detail::RemoveRefsAndConst<ELEMENT_COMPLETE_TYPE>::type>(),
                typename detail::QueueImpl<RUNTIME_TYPE>::MoveConstruct(&i_source));
        }

        // overload used if i_source is an lvalue
        template <typename ELEMENT_COMPLETE_TYPE>
            void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::false_type)
        {
            insert_back_impl(runtime_type::template make<typename detail::RemoveRefsAndConst<ELEMENT_COMPLETE_TYPE>::type>(),
                typename detail::QueueImpl<RUNTIME_TYPE>::CopyConstruct(&i_source));
        }

    private:
        detail::QueueImpl<RUNTIME_TYPE> m_impl;
    }; // class template dense_queue

} //namespace density
