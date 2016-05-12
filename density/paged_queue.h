
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <vector>
#include "density_common.h"
#include "page_allocator.h"
#include "detail\queue_impl.h"

namespace density
{
    template < typename ELEMENT = void, typename PAGE_ALLOCATOR = page_allocator<std::allocator<ELEMENT>>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class paged_queue final : private PAGE_ALLOCATOR
    {
    public:

        static_assert(std::is_same< typename std::decay<ELEMENT>::type, void >::value ? std::is_same<ELEMENT, void>::value : true,
            "If ELEMENT decays to void, it must be void (i.e. use plain 'void', not cv or ref qualified voids, like 'void&' or 'const void' )");

        using allocator_type = PAGE_ALLOCATOR;
        using runtime_type = RUNTIME_TYPE;
        using value_type = ELEMENT;
        using reference = typename std::add_lvalue_reference< ELEMENT >::type;
        using const_reference = typename std::add_lvalue_reference< const ELEMENT>::type;
        using difference_type = ptrdiff_t;
        using size_type = size_t;
        class iterator;
        class const_iterator;

        /** Default and reserving constructor. It is unspecified whether the default constructor allocates memory pages (that is, if a
            default-constructed queue consumes memory). The page allocator inside the queue is default-constructed.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).*/
        paged_queue()
        {
            auto new_page = static_cast<PageHeader*>(get_allocator_ref().allocate_page());
            new(new_page) PageHeader(new_page + 1, PAGE_ALLOCATOR::page_size - sizeof(PageHeader));
            m_first_page = m_last_page = new_page;
            #if DENSITY_DEBUG
                check_invariants();
            #endif
        }

        /** Adds an element at the end of the queue. The operation may require the allocation of a new page.
            \n Note: the template argument ELEMENT_COMPLETE_TYPE must be explicit (it can't be deduced from the parameter).
            @param i_source object to be used as source for the construction of the new element.
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
                "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT");
            push_impl(std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
        }

        /** Deletes the first element of the queue (the oldest one).
            \pre The queue must be non-empty (otherwise the behavior is undefined).

            \n\b Effects on iterators: only iterators and references to the first element are invalidated
            \n\b Throws: nothing
            \n\b Complexity: constant */
        void pop() DENSITY_NOEXCEPT
        {
            // the emptiness-precondition is checked in QueueImpl
            m_first_page->m_queue.pop();
            if (m_first_page->m_queue.empty())
            {
                auto const next = m_first_page->m_next_page;
                if (next != nullptr)
                {
                    // assuming that the destructor of an empty QueueImpl is trivial
                    get_allocator_ref().deallocate_page(m_first_page);
                    m_first_page = next;
                }
            }

            #if DENSITY_DEBUG
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
            auto manual_consume(OPERATION && i_operation) -> decltype(i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()))
//                DENSITY_NOEXCEPT_IF(DENSITY_NOEXCEPT_IF((i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT*>()))))
        {
            // the emptiness-precondition is checked in QueueImpl

            m_first_page->m_queue.manual_consume([&i_operation](const RUNTIME_TYPE & i_type, void * i_element) {
                i_operation(i_type, static_cast<ELEMENT*>(i_element));
            });
            if (m_first_page->m_queue.empty())
            {
                auto const next = m_first_page->m_next_page;
                if (next != nullptr)
                {
                    // assuming that the destructor of an empty QueueImpl is trivial
                    get_allocator_ref().deallocate_page(m_first_page);
                    m_first_page = next;
                }
            }

            #if DENSITY_DEBUG
                check_invariants();
            #endif
        }
			
        /** Returns true if this queue contains no elements.
            \n\b Throws: nothing
            \n\b Complexity: constant */
        bool empty() const DENSITY_NOEXCEPT { return m_first_page == m_last_page && m_first_page->m_queue.empty(); }

        /** Removes all the elements from this queue.
            \n\b Throws: nothing
            \n\b Complexity: linear */
        void clear() DENSITY_NOEXCEPT
        {
			for (PageHeader * page = m_first_page; ; page = page->m_next_page)
			{
				page.m_queue.clear();
				if (page == m_last_page)
				{
					// one page is always allocated (even if empty). This is a class invariant.
					break;
				}

				// assuming that the destructor of an empty QueueImpl is trivial
				get_allocator_ref().deallocate_page(m_first_page);
			}

			#if DENSITY_DEBUG
                check_invariants();
            #endif
        }

        /** Returns a reference to the first element of this queue. If ELEMENT is void,
            the this function returns void.
            \pre The queue must be non-empty (otherwise the behavior is undefined)

            \n\b Throws: nothing
            \n\b Complexity: constant */
        reference front() DENSITY_NOEXCEPT
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
        const_reference front() const DENSITY_NOEXCEPT
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
        MemSize mem_capacity() const DENSITY_NOEXCEPT
        {
			MemSize result(0);
			for (PageHeader * page = m_first_page; page != nullptr; page = page->m_next_page)
			{
				result += page->m_queue.mem_capacity();
			}
			return result;
        }

        /** Returns the used size in bytes. This size is dependant, in an implementation defined way, to the count, the type and
            the order of the elements present in the queue. The return value is zero if and only if the queue is empty. It is highly
			recommended to use the function dense_queue::empty to check if the queue is empty.
            \remark There is no way to predict if the next push\emplace will cause a reallocation.

            \n\b Throws: nothing
            \n\b Complexity: constant */
        MemSize mem_size() const DENSITY_NOEXCEPT
        {
			MemSize result(0);
			for (PageHeader * page = m_first_page; page != nullptr; page = page->m_next_page)
			{
				result += page->m_queue.mem_size();
			}
			return result;
        }

        /** Returns the free size in bytes. This is equivalent to dense_queue::mem_capacity decreased by dense_queue::mem_size.
            \remark There is no way to predict if the next push\emplace will cause a reallocation.

            \n\b Throws: nothing
            \n\b Complexity: constant */
        MemSize mem_free() const DENSITY_NOEXCEPT
        {
			MemSize result(0);
			for (PageHeader * page = m_first_page; page != nullptr; page = page->m_next_page)
			{
				result += page->m_queue.mem_free();
			}
			return result;
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

        struct PageHeader
        {
            detail::QueueImpl<RUNTIME_TYPE> m_queue;
            PageHeader * m_next_page;

            PageHeader(void * i_buffer_address, size_t i_buffer_byte_capacity) DENSITY_NOEXCEPT
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
            while(!m_last_page->m_queue.try_push(i_source_type, std::forward<CONSTRUCTOR>(i_constructor)))
            {
                make_space(i_source_type.size(), i_source_type.alignment());
            }
            #if DENSITY_DEBUG
                check_invariants();
            #endif
        }

        DENSITY_NO_INLINE void make_space(size_t i_required_size, size_t i_required_alignment)
        {
            const size_t min_page_size = detail::size_max(i_required_alignment, alignof(PageHeader)) + i_required_size + sizeof(PageHeader);
            if (min_page_size <= PAGE_ALLOCATOR::page_size)
            {
                auto new_page = static_cast<PageHeader*>( get_allocator_ref().allocate_page() );
                new(new_page) PageHeader(new_page + 1, PAGE_ALLOCATOR::page_size - sizeof(PageHeader));
                m_last_page->m_next_page = new_page;
                m_last_page = new_page;
            }
        }

        #if DENSITY_DEBUG
            void check_invariants()
            {
                DENSITY_ASSERT(m_first_page != nullptr);
                for (PageHeader * page = m_first_page; page != nullptr; page = page->m_next_page)
                {
                    if (m_first_page != m_last_page)
                    {
                        DENSITY_ASSERT(!page->m_queue.empty());
                    }
                    if (page->m_next_page != nullptr)
                    {
                        DENSITY_ASSERT(page != m_last_page);
                    }
                    else
                    {
                        DENSITY_ASSERT(page == m_last_page);
                    }
                }
            }
        #endif

    private:
        PageHeader * m_last_page, * m_first_page;
    }; // class paged_queue

} // namespace density

