
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <vector>
#include "density_common.h"
#include "detail\queue_impl.h"
#include <list>

namespace density
{
    namespace detail
    {
        template <typename ALLOCATOR, typename RUNTIME_TYPE>
            class PagedQueueImpl final : private ALLOCATOR
        {
        public:

            using FixedQueue = QueueImpl<RUNTIME_TYPE>;

            struct PageHeader
            {
                FixedQueue m_fifo_allocator;
                PageHeader * m_next_page;

                PageHeader(void * i_buffer_address, size_t i_buffer_byte_capacity) DENSITY_NOEXCEPT
                    : m_fifo_allocator(i_buffer_address, i_buffer_byte_capacity), m_next_page(nullptr) { }
            };

            static const size_t s_min_page_size = sizeof(PageHeader) * 4 + alignof(PageHeader);

            PagedQueueImpl(size_t i_min_page_size)
            {
                m_last_page = m_first_page = m_peek_page = m_put_page 
                    = new_page(i_min_page_size);
                m_first_page->m_next_page = m_first_page; // the linked ist is circular
                m_page_size = i_min_page_size;
            }
            
            /** Creates a pages that has at least the specified size, (but does not adds it to the linked list) */
            PageHeader * new_page(size_t i_min_size)
            {
                size_t size = size_max(i_min_size, s_min_page_size);
                typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(
                    *static_cast<ALLOCATOR*>(this) );

                PageHeader * header = reinterpret_cast< PageHeader * >( char_alloc.allocate(i_min_size) );
                ::new(header) PageHeader(header + 1, size - sizeof(PageHeader));
                header->m_next_page = nullptr;
                return header;
            }

            /** Removes a page from the linked list */
            void remove_page(PageHeader * i_page)
            {
                DENSITY_ASSERT(i_page != nullptr);
                DENSITY_ASSERT(m_first_page != nullptr);

                PageHeader * curr_page = m_first_page;
                while (curr_page->m_next_page != i_page)
                {
                    curr_page = curr_page->m_next_page;
                }

                curr_page->m_next_page = i_page->m_next_page;
                if (i_page == m_last_page)
                    m_last_page = curr_page;
                if (i_page == m_first_page)
                    m_first_page = i_page->m_next_page;
                if (i_page == m_put_page)
                    m_put_page = i_page->m_next_page;
            }

            /** Deletes a page (it should be removed from the inked list first) */
            void delete_page(PageHeader * i_page)
            {
                i_page->~PageHeader();

                typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(
                    static_cast<ALLOCATOR*>(this));
                char_alloc.deallocate(i_page);
            }

            template <typename CONSTRUCTOR>
                void impl_push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
            {
                // try to allocate in m_put_page
                bool result = m_put_page->m_fifo_allocator.try_push(i_source_type, i_constructor);
                if (!result)
                {
                    // move m_put_page to the next page
                    PageHeader * next_page = m_put_page->m_next_page;
                    if (next_page == m_peek_page)
                    {
                        // m_put_page is reaching reached m_peek_page, create a new page
                        const size_t requiredSize = (i_source_type.size() + i_source_type.alignment() + s_min_page_size + 1 + sizeof(PageHeader)) & ~(i_source_type.alignment() - 1);
                        next_page = new_page(size_max(m_page_size, requiredSize));

                        // insert the new page in the linked list
                        next_page->m_next_page = m_put_page->m_next_page;
                        m_put_page->m_next_page = next_page;
                        if (m_put_page == m_last_page)
                            m_last_page = next_page;
                    }
                    m_put_page = next_page;

                    // retry to allocate
                    result = m_put_page->m_fifo_allocator.try_push(i_source_type, i_constructor);
                    DENSITY_ASSERT(result); // page_size should be enough to allocate the block
                }
            }

        private:
            PageHeader * m_put_page, * m_peek_page; // head and tail of the queue
            PageHeader * m_first_page, *m_last_page; // linked list of all pages
            size_t m_page_size;
        };

    } // namespace detail

    template < typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class PagedQueue final : private ALLOCATOR
    {
        using PagedQueueImpl = detail::PagedQueueImpl<ALLOCATOR, RUNTIME_TYPE>;
    public:

        using runtime_type = RUNTIME_TYPE;
        using allocator_type = ALLOCATOR;
        using value_type = ELEMENT;
        using reference = ELEMENT &;
        using const_reference = const ELEMENT &;
        using pointer = typename std::allocator_traits<allocator_type>::pointer;
        using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
        using difference_type = ptrdiff_t;
        using size_type = size_t;
        class iterator;
        class const_iterator;

        PagedQueue(size_t i_min_page_size = 1024 * 128)
            : m_impl(i_min_page_size) {}

        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            push_impl(std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
        }

        template <typename OPERATION>
            void consume(OPERATION && i_operation)
                DENSITY_NOEXCEPT_IF(DENSITY_NOEXCEPT_IF((i_operation( std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT>() ))))
        {
            m_impl.consume([&i_operation](const RUNTIME_TYPE & i_type, void * i_element) {
                i_operation(i_type, *static_cast<ELEMENT*>(i_element));
            });
        }

    private:

        // overload used if i_source is an rvalue
        template <typename ELEMENT_COMPLETE_TYPE>
            void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::true_type)
                DENSITY_NOEXCEPT_IF((std::is_nothrow_move_constructible<ELEMENT_COMPLETE_TYPE>::value))
        {
            m_impl.impl_push(runtime_type::template make<typename std::decay<ELEMENT_COMPLETE_TYPE>::type>(),
                typename detail::QueueImpl<RUNTIME_TYPE>::MoveConstruct(&i_source));
        }

        // overload used if i_source is an lvalue
        template <typename ELEMENT_COMPLETE_TYPE>
            void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::false_type)
                DENSITY_NOEXCEPT_IF((std::is_nothrow_copy_constructible<ELEMENT_COMPLETE_TYPE>::value))
        {
            m_impl.impl_push(runtime_type::template make<typename std::decay<ELEMENT_COMPLETE_TYPE>::type>(),
                typename detail::QueueImpl<RUNTIME_TYPE>::CopyConstruct(&i_source));
        }

    private:
        PagedQueueImpl m_impl;
    }; // class PagedQueue

} // namespace density

