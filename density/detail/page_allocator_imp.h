
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace density
{
    namespace detail
    {
        /** Lock-free concurrent stack of free pages */
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

            /** Pushes a page. The page must be large at least min_page_size, or the behavior is undefined */
            void push(void * i_page) noexcept
            {
                auto const new_entry = static_cast<Entry*>(i_page);

                /*auto first = m_first.load(sync::hint_memory_order_relaxed);
                new_entry->m_next = first;
                while (!m_first.compare_exchange_weak(new_entry->m_next, new_entry, sync::hint_memory_order_release, sync::hint_memory_order_relaxed));*/

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

        /** This class allocates a memory region with malloc, and then provides an irreversible allocation service (there is an allocate_page,
            but not a deallocate_page). */
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
                            throw std::bad_alloc();
                        }
                        region_size /= 2;
                    }
                }

                m_start = region;
                m_curr = reinterpret_cast<uintptr_t>(address_upper_align(region, PAGE_SIZE));
                m_end = reinterpret_cast<uintptr_t>(address_lower_align(address_add(m_start, region_size), PAGE_SIZE));
            }

            void * allocate_page() noexcept
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
                std::free(m_start);
            }

            PageRegion * next() noexcept { return m_next; }
            void set_next(PageRegion * i_next) noexcept { m_next = i_next; }

        private:
            uintptr_t m_end;
            sync::atomic<uintptr_t> m_curr;
            void * m_start;
            PageRegion * m_next;
        };

        template <size_t PAGE_SIZE>
            class PageAllocator
        {
        public:

            static constexpr size_t region_size = PAGE_SIZE * 64;

            PageAllocator()
            {
                m_first_region = new PageRegion<PAGE_SIZE>(region_size);
                m_last_region.store(m_first_region);
            }

            void * allocate_page()
            {
                auto page = m_free_stack.pop();
                if (page != nullptr)
                {
                    return page;
                }

                page = m_last_region.load()->allocate_page();
                if (page != nullptr)
                {
                    return page;
                }

                return allocate_slow_path();
            }

            void deallocate_page(void * i_page) noexcept
            {
				if (i_page != nullptr)
				{
					m_free_stack.push(i_page);
				}
            }

            ~PageAllocator()
            {
                DENSITY_ASSERT_INTERNAL(m_last_region.load()->next() == nullptr);

                for( auto curr_region = m_first_region; curr_region != nullptr; )
                {
                    auto const next = curr_region->next();
                    delete curr_region;
                    curr_region = next;
                }
            }

        private:

            void * allocate_slow_path()
            {
                std::lock_guard<sync::mutex> lock(m_ragion_mutex);

                DENSITY_ASSERT_INTERNAL(m_last_region.load()->next() == nullptr);

                auto last_region = m_last_region.load();

                auto page = last_region->allocate_page();
                if (page != nullptr)
                {
                    return page;
                }

                auto new_region = new PageRegion<PAGE_SIZE>(region_size);
                last_region->set_next(new_region);
                page = new_region->allocate_page();
                DENSITY_ASSERT_INTERNAL(page != nullptr);
                m_last_region.store(new_region);

                return page;
            }

        private:
            FreePageStack m_free_stack;
            sync::atomic<PageRegion<PAGE_SIZE>*> m_last_region;
            sync::mutex m_ragion_mutex;
            PageRegion<PAGE_SIZE> * m_first_region;
        };

    } // namespace detail

} // namespace density

