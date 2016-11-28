
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace density
{
    namespace detail
    {
        /** Lock-free concurrent stack of free pages. */
        class FreePageStack
        {
            struct Entry
            {
                uintptr_t m_next;
            };

        public:

            static constexpr size_t min_page_size = sizeof(Entry);

            FreePageStack(const FreePageStack &) = delete;

            FreePageStack & operator = (const FreePageStack &) = delete;

            FreePageStack()
                : m_first(0)
            {
            }

            /** Pushes a page. The page must be large at least like an uintptr_t, or the behavior is undefined */
            void push(void * i_page) noexcept
            {
                auto const new_entry = static_cast<Entry*>(i_page);

                uintptr_t first;
                do {
                    first = m_first.fetch_or(1, sync::hint_memory_order_acquire);
                } while (first & 1);

                new_entry->m_next = first;

                m_first.store(reinterpret_cast<uintptr_t>(new_entry), sync::hint_memory_order_release);
            }

            /** Retrieves a page from the top, or nullptr. */
            void * pop() noexcept
            {
                uintptr_t first;
                do {
                    first = m_first.fetch_or(1, sync::hint_memory_order_acquire);
                } while (first & 1);

                auto new_page = reinterpret_cast<Entry*>(first);
                if (new_page != nullptr)
                {
                    m_first.store(new_page->m_next, sync::hint_memory_order_release);
                }
                else
                {
                    m_first.store(0, sync::hint_memory_order_release);
                }

                return new_page;
            }

        private:
            sync::atomic<uintptr_t> m_first;
        };

        template <size_t PAGE_SIZE>
            class PageRegion
        {
        public:

            static_assert(PAGE_SIZE > sizeof(void*) * 4 && is_power_of_2(PAGE_SIZE), "PAGE_SIZE too low or not a power of 2");

            static constexpr size_t page_size = PAGE_SIZE;
            static constexpr size_t min_region_size = page_size * 8;

            PageRegion(const PageRegion &) = delete;
            PageRegion & operator = (const PageRegion &) = delete;

            PageRegion(size_t i_region_size)
                : m_next(nullptr)
            {
                size_t region_size = i_region_size;

                void * region = nullptr;
                while (region == nullptr)
                {
                    region_size = size_max(region_size, min_region_size);

                    region = std::malloc(region_size);
                    if (region == nullptr)
                    {
                        if (region_size == min_region_size)
                        {
                            throw std::bad_alloc;
                        }
                        region_size /= 2;
                    }
                }

                m_start = region;
                m_curr = address_upper_align(region, PAGE_SIZE);
                m_end = reinterpret_cast<uintptr_t>(address_lower_align(address_add(m_start, region_size), PAGE_SIZE));
            }

            void * alloc_page() noexcept
            {
                uintptr_t curr, next;
                curr = m_curr.load(sync::hint_memory_order_relaxed);
                do {
                    next = curr + PAGE_SIZE;
                    if (next > m_end)
                    {
                        return nullptr;
                    }
                } while (!m_curr.compare_exchange_weak(curr, next, sync::hint_memory_order_relaxed, sync::hint_memory_order_relaxed));
                return reinterpret_cast<void*>(curr);
            }

            ~PageRegion()
            {
                if (m_start != nullptr)
                {
                    std::free(m_start);
                }
            }

        private:
            uintptr_t m_end;
            sync::atomic<uintptr_t> m_curr;
            void * m_start;
            sync::atomic<PageRegion*> m_next;
        };

        template <size_t PAGE_SIZE>
            class PageAllocator
        {
        public:

            PageAllocator()
            {

            }

            void * alloc_page() noexcept
            {
                auto page = m_free_stack.pop();
                if (page != nullptr)
                {
                    return page;
                }

                std::lock_guard<sync::mutex> lock(m_ragion_mutex);

                page = m_last_region.load()->alloc_page();
                if (page != nullptr)
                {
                    return page;
                }

                return nullptr;
            }

        private:
            FreePageStack m_free_stack;

            sync::mutex m_ragion_mutex;
            PageRegion* m_last_region, m_first_region;
        };

    } // namespace detail

} // namespace density

