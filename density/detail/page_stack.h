
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>

namespace density
{
    namespace detail
    {
        /** \internal Structure allocated at the end of every page. The page manager allocates an instance of
            this struct at the end of every page. For this reason the size of pages is less than the alignment. */
        struct PageFooter
        {
            /** Pointer to the next page when the page is inside a stack, undefined otherwise. */
            PageFooter * m_next_page{ nullptr };

            /* Number of times the page has been pinned. The allocator can't modify the content
                of a page while the pin count is greater than zero. */
            std::atomic<uintptr_t> m_pin_count{ 0 };
        };

        /** \internal Non-concurrent stack of pages. This is not a general purpose
            stack, rather it is designed and specialized to be used by the page manager.
            \n PageStack is not a strict stack: pop_unpinned removes the first unpinned page, if any. */
        class PageStack
        {
        private:
            PageFooter * m_first{ nullptr }; /**< root of the null-terminated linked list. */
            PageFooter * m_cached_last{ nullptr }; /**< pointer to the last page, if known, or nullptr. This variable is redundant, and
                                                    is used just to optimize the function find_last. */
        
        public:

            /** Constructs a stack from a null-terminated linked-list of pages. If the argument is nullptr the stack is empty */
            PageStack(PageFooter * i_first = nullptr) noexcept
                : m_first(i_first), m_cached_last(nullptr)
            {
            }

            /** Copy construction not allowed */
            PageStack(const PageStack &) = delete;
            
            /** Copy assignment not allowed */
            PageStack & operator = (const PageStack &) = delete;

            /** Move constructor. A moved-from PageStack can only be destroyed. Using it in any other way causes undefined behavior. */
            PageStack(PageStack &&) noexcept = default;

            /** Move assignment. A moved-from PageStack can only be destroyed. Using it in any other way causes undefined behavior. */
            PageStack & operator = (PageStack &&) noexcept = default;

            /** Prepends a page to this stack.

                \pre The behavior is undefined if either:
                    - the argument is nullptr
                    - the page is already present in any stack */
            void push(PageFooter * i_page) noexcept
            {
                DENSITY_ASSERT_INTERNAL(i_page != nullptr);
                i_page->m_next_page = m_first;
                m_first = i_page;
            }

            /** Prepends another PageStack to this stack. The argument may be modified because find_last is called on it.

                \pre The behavior is undefined if either:
                    - the argument is empty
                    - any page in the argument is already present in any stack */
            void push(PageStack & i_stack) noexcept
            {
                DENSITY_ASSERT_INTERNAL(!i_stack.empty());
                i_stack.find_last()->m_next_page = m_first;
                m_first = i_stack.first();
            }

            /** Returns the top of the stack */
            PageFooter * first() const noexcept
            {
                return m_first;
            }

            /** Returns whether the stack is empty */
            bool empty() const noexcept
            {
                return m_first == nullptr;
            }

            /** Returns the page at the bottom of the stack. This function uses a cache pointer to avoid the linear scan from the 2nd invocation on. 
                
                \pre The behavior is undefined if either:
                    - this stack is empty */
            PageFooter * find_last() noexcept
            {
                DENSITY_ASSERT_INTERNAL(!empty());
                if (m_cached_last == nullptr)
                {
                    m_cached_last = m_first;
                    while (m_cached_last->m_next_page != nullptr)
                    {
                        m_cached_last = m_cached_last->m_next_page;
                    }
                }
                return m_cached_last;
            }

            /** Search for a page with pin-count == 0, and removes it, if any.
                @param i_first reference to the pointer to the first page of the stack
                @return the page removed from the stack, or nullptr */
            PageFooter * pop_unpinned() noexcept
            {
                if (m_first != nullptr)
                {
                    // we may remove the last, so we reset m_cached_last
                    m_cached_last = nullptr;

                    PageFooter * curr = m_first;
                    PageFooter * prev = nullptr;
                    do {
                        DENSITY_ASSERT_INTERNAL((prev == nullptr) == (curr == m_first));

                        // note: probably this may be a non-atomic read
                        if (curr->m_pin_count.load(detail::mem_relaxed) == 0)
                        {
                            if (prev != nullptr)
                                prev->m_next_page = curr->m_next_page;
                            else
                                m_first = curr->m_next_page;
                            return curr;
                        }

                        prev = curr;
                        curr = curr->m_next_page;
                    } while (curr != nullptr);

                    // the search has failed, but we can set m_cached_last
                    m_cached_last = prev;
                }
                return nullptr;
            }
        };

    } // namespace detail

} // namespace density
