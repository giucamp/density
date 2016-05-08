
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "detail\queue_impl.h"
#include <memory>

namespace density
{
    /** Class template that implements an heterogeneous fifo container with dynamic size. 
        A dense_queue allocates one memory buffer with the provided allocator, and sub-allocates inplace
		its elements. The buffer is reallocated accompish push and emplace requests.
		Thread safeness: None. The user is responsible to avoid race conditions.
		Exception safeness: Any function of dense_queue is noexcept or gives the strong exception guarantee.
			@param ELEMENT base type of the elements of the queue. The queue enforces the compile-time
				contraint that the type of each element must be covariant to ELEMENT, that is:
					std::is_convertible<ELEMENT*, COMPLETE_ELEMENT*>::value
				must be true, where COMPLETE_ELEMENT is the compete type of an element. 
				If ELEMENT is void, elements of any compete type can be added to the container. In this
				case, the methods of dense_queue (and its iterators) that returns a pointer to an element
				will return a void* to a complete object, while the methods that returns a reference to
				an element will return void. Use iterators and pointer semantic if you write generic code
				that should work with any dense_queue.
				If ELEMENT decays to void but it is not a plain void, a compile time error is iussued.
				Note: if ELEMENT is to be a buit-in type, a pointer, or a final type, then the complete
				type of al elements will be exactly ELEMENT (that is, the container wil not be heterogeneous). In
				this case a standard container (like std::queue) instead of std::dense_queue is a better choise.
				If ELEMENT is not void, it must be noexcept move constructible.
			@param ALLOCATOR allocator to be used to allocate the memory buffer. The queue may rebind
				this allocator to a different type, eventualy unrelated to ELEMENT
			@param RUNTIME_TYPE type to be used to rapresent the actual complete type of each element. 
				This type must meet the requirements of RuntimeType.
		dense_queue provides only forward iteration. Only the first element is accessibe in constant time (with
		the method dense_queue::front, dense_queue::begin. The iterator provides access to both the ELEMENT (with
		the function element) and the RUNTIME_TYPE (with the function type).
		There is not constant time function that gives the number of elements in a dense_queue in constant time,
		but std::distance wil do (in linear time). Anyway dense_queue::mem_size, dense_queue::mem_capacity and
		dense_queue::empty work in constant time.
        Insertion is allowed only at the end (with the methods dense_queue::push or dense_queue::emplace).
        Removal is allowed ony at the begin (with the methods dense_queue::pop or dense_queue::consume). */
	template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class dense_queue final : private std::allocator_traits<ALLOCATOR>::template rebind_alloc<char>
    {
    public:

		static_assert( std::is_same< typename std::decay<ELEMENT>::type, void >::value ? std::is_same<ELEMENT,void>::value : true,
			"If ELEMENT decays to void, it must be void (i.e. use plain 'void', not cv or ref qualified voids, like 'void&' or 'const void' )" );

		using runtime_type = RUNTIME_TYPE;
        using value_type = ELEMENT;
        using reference = typename std::add_lvalue_reference< ELEMENT >::type;
        using const_reference = typename std::add_lvalue_reference< const ELEMENT>::type;
        using difference_type = ptrdiff_t;
        using size_type = size_t;
        class iterator;
        class const_iterator;

		/** Default and reserving constructor. It is unspecified whether the defaut constructor allocates a memory block (that is, if a default-constructed
			queue has a memory block).
			The allocator is defaut-constructed.
			@param i_initial_reserved_bytes initial capacity to reserve. Any value is legal, but the queue may reserve a bigger capacity.
			@param i_initial_alignment alignment of the initial buffer. Zero or any integer power of 2 is admitted, but the queue may use a stricter alignment.
			Preconditions: 
				* i_initial_alignment must be zero or a power of 2, otherwise the behaviour is undefined
			Throws: ... */
		dense_queue(size_t i_initial_reserved_bytes = 0, size_t i_initial_alignment = 0)
		{
			DENSITY_ASSERT(i_initial_alignment == 0 || MemSize(i_initial_alignment).is_valid_alignment() );
			alloc(std::max(i_initial_reserved_bytes, s_initial_mem_reserve),
				std::max(i_initial_alignment, s_initial_mem_alignment));
		}

		/** Constructs a queue. It is unspecified whether the defaut constructor allocates memory.
			@param i_allocator object to be used as source to copy-construct the allocator.
			@param i_initial_reserved_bytes initial capacity to reserve. Any value is legal, but the queue may reserve a bigger capacity.
			@param i_initial_alignment alignment of the initial buffer. Zero or any integer power of 2 is admitted, but the queue may use a stricter alignment.
			Preconditions:
				* i_initial_alignment must be zero or a power of 2, otherwise the behaviour is undefined
			Throws: ... */
		dense_queue(const ALLOCATOR & i_allocator, size_t i_initial_reserved_bytes = 0, size_t i_initial_alignment = 0)
			: Allocator(i_allocator)
		{
			alloc(std::max(i_initial_reserved_bytes, QueueImp::s_initial_mem_reserve),
				std::max(i_initial_alignment, QueueImp::s_minimum_buffer_alignment));
		}

		/** Move constructor. The move constructor of the allocator is required to be noexcept, otherwise the compilation fails.
			@param i_source queue to move from. After the call this queue will be empty
			Throws: nothing */
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
				* anything that the copy-constructor of the elements throws
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
        
		/** Adds an element at the end of the queue. ELEMENT_COMPLETE_TYPE * must be convertitble to ELEMENT *.
			If the new element doesn't fit in the reserved memory buffer, a reallocation is performed. 
			@param i_source object to be used as source for the construction of the new element.
				If this argument is an l-value, the new element copy-constructed (and the source object 
				is left unchanged).
				If this argument is an rvalue, the new element move-constructed (and the source object
				will have an undefined but valid content).
			Throws: ...
			Complexity: constant amortized (a realocation may be required). */
        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
			static_assert( std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
				"ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT" );
            push_impl(std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
        }

		/** Adds an element at the end of the queue. If the new element doesn't fit in the reserved memory
			buffer, a realocation is performed. 
			@param i_source object to be used as source for the construction of the new element
				If this argument is an l-value, the new element copy-constructed (and the source object 
				is left unchanged).
				If this argument is an rvalue, the new element move-constructed (and the source object
				will have an undefined but valid content).
			Throws: ...
			Complexity: constant amortized (a realocation may be required). */
		template <typename ELEMENT_COMPLETE_TYPE, typename... PARAMETERS>
            void emplace(PARAMETERS && ... i_parameters)
                DENSITY_NOEXCEPT_IF((std::is_nothrow_constructible<ELEMENT_COMPLETE_TYPE, PARAMETERS...>::value))
        {
			static_assert(std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
				"ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT");
            return insert_back_impl(runtime_type::template make<ELEMENT_COMPLETE_TYPE>(),
                [&i_parameters...](const runtime_type &, void * i_dest) {
                    void * const result = new(i_dest) ELEMENT_COMPLETE_TYPE(std::forward<PARAMETERS>(i_parameters)...);
                    return result;
            });
        }

		/**
		
		*/
		void push_by_copy(const RUNTIME_TYPE & i_type, const void * i_source)
        {
            insert_back_impl(i_type,
                typename detail::QueueImpl<RUNTIME_TYPE>::CopyConstruct(i_source));
        }

        void push_by_move(const RUNTIME_TYPE & i_type, void * i_source) DENSITY_NOEXCEPT
        {
            insert_back_impl(i_type,
                typename detail::QueueImpl<RUNTIME_TYPE>::MoveConstruct(i_source));
        }

        template <typename OPERATION>
            void consume(OPERATION && i_operation)
                DENSITY_NOEXCEPT_IF(DENSITY_NOEXCEPT_IF((i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT>()))))
        {
            m_impl.consume([&i_operation](const RUNTIME_TYPE & i_type, void * i_element) {
                i_operation(i_type, detail::DeferenceVoidPtr<ELEMENT>::apply(i_element));
            });
        }

		/** Erases the first element of the queue.
		*/
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
            using reference = typename dense_queue::reference;
            using const_reference = typename dense_queue::const_reference;

            iterator() DENSITY_NOEXCEPT {}

            iterator(const typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source) {  }

			reference operator * () const DENSITY_NOEXCEPT { return detail::DeferenceVoidPtr<value_type>::apply(m_impl.element()); }
			pointer operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }
			pointer element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }

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
				return m_impl.type();
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
			using reference = typename dense_queue::const_reference;
			using const_reference = typename dense_queue::const_reference;
            
            const_iterator() DENSITY_NOEXCEPT {}

            const_iterator(const typename detail::QueueImpl<RUNTIME_TYPE>::IteratorImpl & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source) {  }

            const_iterator(const iterator & i_source) DENSITY_NOEXCEPT
                : m_impl(i_source.m_impl) {  }

			const_reference operator * () const DENSITY_NOEXCEPT { return detail::DeferenceVoidPtr<value_type>::apply(m_impl.element()); }
			pointer operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }
			pointer element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.element()); }

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

		/** Returns true if this queue contains no elements */
        bool empty() const DENSITY_NOEXCEPT { return m_impl.empty(); }

        void clear() DENSITY_NOEXCEPT 
        { 
            m_impl.delete_all(); 
        }
        
		reference front() DENSITY_NOEXCEPT
        {
            DENSITY_ASSERT(!empty());
            const auto it = m_impl.begin();
            return detail::DeferenceVoidPtr<ELEMENT>::apply(it.element());
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
            insert_back_impl(runtime_type::template make<typename std::decay<ELEMENT_COMPLETE_TYPE>::type>(),
                typename detail::QueueImpl<RUNTIME_TYPE>::MoveConstruct(&i_source));
        }

        // overload used if i_source is an lvalue
        template <typename ELEMENT_COMPLETE_TYPE>
            void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::false_type)
        {
            insert_back_impl(runtime_type::template make<typename std::decay<ELEMENT_COMPLETE_TYPE>::type>(),
                typename detail::QueueImpl<RUNTIME_TYPE>::CopyConstruct(&i_source));
        }

    private:
        detail::QueueImpl<RUNTIME_TYPE> m_impl;
    }; // class template dense_queue

} //namespace density
