
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <atomic>
#include <limits>
#include <density/density_common.h>
#include <density/detail/page_stack.h>

namespace density
{
    namespace detail
    {
        /** \internal Wait-free concurrent stack of free pages. This is not a general purpose
            stack, it is rather designed and specialized to be used by the page manager.
            Any non-lifetime function of WF_PageStack is thread-safe and wait-free, but can fail in case of contention. Pop operations
            temporary lock the stack, so that any operation tried by the other threads will fail due to contention. This simplifies
            the implementation and makes the ABA problem harmless.
            \n WF_PageStack is not a strict stack: try_pop_unpinned removes the first unpinned page, if any.*/
        class WF_PageStack
        {
        private:
            std::atomic<PageFooter*> m_first{ nullptr }; /**< Top of the stack. */

            /** Special value set to m_first during a pop operation, that prevents any operation by other threads. */
            static PageFooter * lock_marker() noexcept
            {
                return reinterpret_cast<PageFooter*>(1);
            }

        public:

            /* Pushes a (possibly still pinned) single page on the stack. This function is wait-free and may fail in case of contention.
                @param i_page The page to push. The initial value of i_page->m_next_page is ignored. This parameter can't be nullptr.
                @return whether the push was successful

                \pre The behavior is undefined if either:
                    - the argument is null
                    - the argument is already present in any stack */
            bool try_push(PageFooter * i_page) noexcept
            {
                DENSITY_ASSERT_INTERNAL(i_page != nullptr);

                auto first = m_first.load(detail::mem_relaxed);
                if (first == lock_marker())
                {
                    return false;
                }

                i_page->m_next_page = first;

                /* We use the weak CAS because the strong one may be non wait-free.
                    The ABA problem may happen, but here is harmless: even if m_first has been changed
                    to B and then back to first, the push can be safely committed. */
                return m_first.compare_exchange_weak(first, i_page, detail::mem_release, detail::mem_relaxed);
            }

            /* Pushes a stack of (possibly still pinned) pages on this stack. This function is wait-free and may fail in case of contention.
                @param i_stack stack of page to prepend to this stack
                @return whether the push was successful                 
                
                \pre The behavior is undefined if either:
                    - the argument is empty
                    - any page in the argument is already present in any stack */
            bool try_push(PageStack & i_stack) noexcept
            {
                DENSITY_ASSERT_INTERNAL(!i_stack.empty());

                PageFooter * const range_first = i_stack.first();
                PageFooter * const range_last = i_stack.find_last();

                auto first = m_first.load(detail::mem_relaxed);
                if (first == lock_marker())
                {
                    return false;
                }

                range_last->m_next_page = first;

                /* The ABA problem may happen, but here it is harmless: even if m_first has been changed
                    to B and then back to first, the push can be safely committed. */
                auto result = m_first.compare_exchange_weak(first, range_first, detail::mem_release, detail::mem_relaxed);
                if (!result)
                {
                    range_last->m_next_page = nullptr;
                }
                return result;
            }

            /** Removes from the stack the first unpinned page.
                As first operation, a pop temporary locks the whole stack. So it can safely walk and analyze
                the pages, and can edit the stack without incurring in the ABA problem. In the meanwhile,
                any other thread will observe the stack as empty. After finishing the work, the stack is restored
                (possibly with one less page). Another benefit of this mechanism is that PageFooter::m_next does
                not need to be an atomic.

                @return The page removed from the stack, or nullptr in case of failure. */
            PageFooter * try_pop_unpinned() noexcept
            {
                auto const first = m_first.exchange(lock_marker(), detail::mem_acquire);
                if (first != lock_marker())
                {
                    // try to get a page
                    PageStack range(first);
                    auto const page = range.empty() ? range.pop_unpinned() : nullptr;

                    // now we have to restore the stack
                    m_first.store(range.first(), detail::mem_release);

                    return page;
                }
                else
                {
                    return nullptr;
                }
            }

            /** Empties the stack, removing all the pages. A null-terminated list of the removed pages is returned.
                This function is optimized for the execution path in which at least a page was present.
                @return The pages removed, or nullptr in the case the stack was already empty. */
            PageStack try_remove_all() noexcept
            {
                auto first = m_first.load(detail::mem_acquire);
                if (first != lock_marker())
                {
                    if (m_first.compare_exchange_weak(first, nullptr, mem_release, mem_relaxed))
                    {
                        return first;
                    }
                }
                return nullptr;
            }
        };

    } // namespace detail

} // namespace density
