
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <new>
#include <cstring>
#include <atomic>
#include <density/density_common.h>
#include <density/detail/wf_page_stack.h>
#include <density/detail/singleton_ptr.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

namespace density
{
    namespace detail
    {
        enum class page_allocation_type
        {
            uninitialized,
            zeroed,
        };

        /** \internal

        */
        class alignas(concurrent_alignment) PageAllocatorSlot
        {
        private:

            PageAllocatorSlot(PageAllocatorSlot * i_next_slot) noexcept
                : m_next_slot(i_next_slot)
            {
            }

            ~PageAllocatorSlot() noexcept = default;

        public:

            WF_PageStack m_page_stack;
            WF_PageStack m_zeroed_page_stack;
            std::atomic<PageAllocatorSlot*> m_next_slot;

            WF_PageStack & get_stack(page_allocation_type const i_allocation_type) noexcept
            {
                DENSITY_ASSERT_INTERNAL(i_allocation_type == page_allocation_type::uninitialized || i_allocation_type == page_allocation_type::zeroed);
                return i_allocation_type == page_allocation_type::zeroed ? m_zeroed_page_stack : m_page_stack;
            }

            /** Creates a new thread slot.
                May throw an std::bad_alloc. */
            static PageAllocatorSlot * create(PageAllocatorSlot * const i_next_slot)
            {
                auto const block = aligned_allocate(sizeof(PageAllocatorSlot), alignof(PageAllocatorSlot));
                DENSITY_ASSERT_INTERNAL(block != nullptr);
                return new(block) PageAllocatorSlot(i_next_slot);
            }

            /** Destroy a thread slot */
            static void destroy(PageAllocatorSlot * const i_slot) noexcept
            {
                DENSITY_ASSERT_INTERNAL(i_slot != nullptr);
                i_slot->~PageAllocatorSlot();
                return aligned_deallocate(i_slot, sizeof(PageAllocatorSlot), alignof(PageAllocatorSlot));
            }
        };

        template <typename SYSTYEM_PAGE_MANAGER>
            class GlobalState
        {
        private:
            SYSTYEM_PAGE_MANAGER m_sys_page_manager;
            std::atomic<PageAllocatorSlot*> m_last_assigned{ nullptr };
            std::atomic<PageAllocatorSlot*> m_first_slot{ nullptr };

        public:

            GlobalState(const GlobalState &) = delete;
            GlobalState & operator = (const GlobalState &) = delete;

            PageAllocatorSlot * assign_slot() noexcept
            {
                auto result = m_last_assigned.load();
                m_last_assigned.store(result->m_next_slot.load());
                return result;
            }

            SYSTYEM_PAGE_MANAGER & sys_page_manager() noexcept
            {
                return m_sys_page_manager;
            }

        private:

            friend class SingletonPtr<GlobalState>;

            GlobalState()
            {
                auto const first = PageAllocatorSlot::create(nullptr);
                auto prev = first;
                for (int i = 0; i < 7; i++)
                {
                    auto const curr = PageAllocatorSlot::create(nullptr);
                    prev->m_next_slot.store(curr);
                    prev = curr;
                }
                prev->m_next_slot.store(first);
                m_first_slot.store(first);
                m_last_assigned = first;
            }

            ~GlobalState()
            {
                auto const first = m_first_slot.load();
                auto curr = first;
                do {
                    auto const next = curr->m_next_slot.load();
                    PageAllocatorSlot::destroy(curr);
                    curr = next;
                } while (curr != first);
            }
        };

        template <typename SYSTYEM_PAGE_MANAGER>
            class PageAllocator
        {
        private:
            PageAllocatorSlot * m_current_slot, * m_victim_slot;
            PageStack m_private_page_stack, m_private_zeroed_page_stack;
            SingletonPtr<GlobalState<SYSTYEM_PAGE_MANAGER>> m_global_state;
            PageStack m_pages_to_unpin;

            static thread_local PageAllocator t_instance;

        public:

            /** Alignment guaranteed for the pages */
            static constexpr size_t page_alignment = SYSTYEM_PAGE_MANAGER::page_alignment_and_size;

            /** Usable size of the pages, in bytes. */
            static constexpr size_t page_size = SYSTYEM_PAGE_MANAGER::page_alignment_and_size - sizeof(PageFooter);

            static PageAllocator & thread_local_instance()
            {
                return t_instance;
            }

            template <page_allocation_type ALLOCATION_TYPE>
                void * try_allocate_page(progress_guarantee i_progress_guarantee) noexcept
            {
                // try from the private stack...
                auto * new_page = get_private_stack(ALLOCATION_TYPE).pop_unpinned();
                if (new_page == nullptr)
                {
                    // ...then from the current slot...
                    new_page = m_current_slot->get_stack(ALLOCATION_TYPE).try_pop_unpinned();
                    if (new_page == nullptr)
                    {
                        // ...else try to steal all the pages from the victim slot...
                        new_page = try_steal_and_allocate(ALLOCATION_TYPE);

                        // if still do not have a page, we go to the next 2nd level
                        if (new_page == nullptr)
                        {
                            new_page = allocate_page_slow_path(ALLOCATION_TYPE, i_progress_guarantee);
                        }
                    }
                }
                return address_lower_align(new_page, page_alignment);
            }

            template <page_allocation_type ALLOCATION_TYPE>
                void deallocate_page(void * i_page) noexcept
            {
                auto const page = get_footer(i_page);

                // try to push the page once on every slot
                auto * const original_slot = m_current_slot;
                bool done = false;
                do {

                    if (m_current_slot->get_stack(ALLOCATION_TYPE).try_push(page))
                    {
                        done = true;
                        break;
                    }

                    m_current_slot = m_current_slot->m_next_slot.load();
                } while (m_current_slot != original_slot);

                // this is unlikely, but it may happen
                if (!done)
                {
                    get_private_stack(ALLOCATION_TYPE).push(page);
                }
            }

            size_t try_reserve_lockfree_memory(progress_guarantee const i_progress_guarantee, size_t i_size) noexcept
            {
                return m_global_state->sys_page_manager().try_reserve_region_memory(i_progress_guarantee, i_size);
            }

            static void pin_page(void * const i_address) noexcept
            {
                auto const footer = get_footer(i_address);
                footer->m_pin_count.fetch_add(1, detail::mem_relaxed);
            }
            
            static bool try_pin_page(progress_guarantee i_progress_guarantee, void * const i_address) noexcept
            {
                auto const footer = get_footer(i_address);
                if(i_progress_guarantee <= progress_guarantee::progress_lock_free)
                {
                    footer->m_pin_count.fetch_add(1, detail::mem_relaxed);
                    return true;
                }
                else
                {
                    auto curr_value = footer->m_pin_count.fetch_load(detail::mem_relaxed);
                    return footer->m_pin_count.compare_exchange_weak(curr_value, detail::mem_acquire);
                }
            }

            static void unpin_page(void * const i_address) noexcept
            {
                auto const footer = get_footer(i_address);
                auto const prev_pins = footer->m_pin_count.fetch_sub(1, detail::mem_acq_rel);
                DENSITY_ASSERT(prev_pins > 0);
                (void)prev_pins;
            }

            static void unpin_page(progress_guarantee i_progress_guarantee, void * const i_address) noexcept
            {
                if (i_progress_guarantee <= progress_guarantee::progress_lock_free)
                {
                    unpin_page(i_address);
                    return true;
                }
                else
                {
                    auto curr_value = footer->m_pin_count.fetch_load(detail::mem_relaxed);
                    DENSITY_ASSERT(curr_value > 0);
                    if (!footer->m_pin_count.compare_exchange_weak(curr_value, detail::mem_acquire))
                    {
                        // failed due to contention, we must retry later
                        t_instance.m_pages_to_unpin.push(footer);
                    }
                }
            }

            static uintptr_t get_pin_count(const void * const i_address) noexcept
            {
                return get_footer(i_address)->m_pin_count.load(detail::mem_relaxed);
            }

        private:

            PageAllocator() noexcept
            {
                m_current_slot = m_global_state->assign_slot();
                m_victim_slot = m_current_slot->m_next_slot.load();
            }

            ~PageAllocator()
            {
                dump_private_stack(page_allocation_type::uninitialized);
                dump_private_stack(page_allocation_type::zeroed);
            }

            static PageFooter * get_footer(void * const i_address) noexcept
            {
                auto const page = address_lower_align(i_address, page_alignment);
                return static_cast<PageFooter*>(address_add(page, page_size));
            }

            static const PageFooter * get_footer(const void * const i_address) noexcept
            {
                auto const page = address_lower_align(i_address, page_alignment);
                return static_cast<const PageFooter*>(address_add(page, page_size));
            }

            PageFooter * try_steal_and_allocate(page_allocation_type const i_allocation_type) noexcept
            {
                PageStack stolen_pages = m_victim_slot->get_stack(i_allocation_type).try_remove_all();

                auto new_page = stolen_pages.pop_unpinned();

                // try to push the stolen stack to the current slot
                if (!stolen_pages.empty() && !m_current_slot->get_stack(i_allocation_type).try_push(stolen_pages))
                {
                    /* try_push may fail in case of concurrency. Anyway we have a stack of page, and we
                        have to push it somewhere.
                        The following function will loop m_current_slot around all the slot, trying to push
                        the stack. As last resort it will prepend stolen_pages to the private stack */
                    disccard_page_stack(i_allocation_type, stolen_pages);
                }

                return new_page;
            }

            static PageFooter * initialize_page(page_allocation_type const i_allocation_type, void * const i_page_mem) noexcept
            {
                DENSITY_ASSERT_ALIGNED(i_page_mem, page_alignment);
                auto const new_page = new(get_footer(i_page_mem)) PageFooter();

                bool should_zero = !SYSTYEM_PAGE_MANAGER::pages_are_zeroed && i_allocation_type == page_allocation_type::zeroed;
                if (should_zero)
                {
                    std::memset(i_page_mem, 0, page_size); // the page footer is not touched
                }

                return new_page;
            }

            DENSITY_NO_INLINE PageFooter * allocate_page_slow_path(page_allocation_type const i_allocation_type, progress_guarantee const i_progress_guarantee) noexcept
            {
                PageFooter * new_page = nullptr;

                // First we try to use the memory already allocated from the system...
                void * new_page_mem = m_global_state->sys_page_manager().try_allocate_page(progress_wait_free);
                if (new_page_mem != nullptr)
                {
                    new_page = initialize_page(i_allocation_type, new_page_mem);
                }
                else
                {
                    // ...then try to steal from m_victim_slot, looping all the slots...
                    auto const starting_victim_slot = m_victim_slot;
                    do {

                        new_page = try_steal_and_allocate(i_allocation_type);
                        if (new_page != nullptr)
                            break;

                        m_victim_slot = m_victim_slot->m_next_slot.load();

                    } while (m_victim_slot != starting_victim_slot);

                    if (new_page == nullptr && i_progress_guarantee == progress_blocking)
                    {
                        // ...last chance, try possibly allocating new memory from the system
                        new_page_mem = m_global_state->sys_page_manager().try_allocate_page(progress_blocking);
                        if (new_page_mem != nullptr)
                        {
                            new_page = initialize_page(i_allocation_type, new_page_mem);
                        }
                    }
                }
                return new_page;
            }

            void dump_private_stack(page_allocation_type const i_allocation_type)
            {
                auto & private_stack = get_private_stack(i_allocation_type);
                if (!private_stack.empty())
                {
                    while (!m_current_slot->get_stack(i_allocation_type).try_push(private_stack))
                    {
                        m_current_slot = m_current_slot->m_next_slot.load();
                    }
                }
            }

            PageStack & get_private_stack(page_allocation_type const i_allocation_type) noexcept
            {
                DENSITY_ASSERT_INTERNAL(i_allocation_type == page_allocation_type::uninitialized || i_allocation_type == page_allocation_type::zeroed);
                return i_allocation_type == page_allocation_type::zeroed ? m_private_zeroed_page_stack : m_private_page_stack;
            }

            void disccard_page_stack(page_allocation_type const i_allocation_type, PageStack & i_page_stack) noexcept
            {
                DENSITY_ASSERT(!i_page_stack.empty());

                // try to push the page once on every slot
                auto * const original_slot = m_current_slot;
                bool done = false;
                do {

                    if (m_current_slot->get_stack(i_allocation_type).try_push(i_page_stack))
                    {
                        done = true;
                        break;
                    }

                    m_current_slot = m_current_slot->m_next_slot.load();
                } while (m_current_slot != original_slot);

                // this is unlikely, but it may happen
                if (!done)
                {
                    get_private_stack(i_allocation_type).push(i_page_stack);
                }
            }
        };

        template <typename SYSTYEM_PAGE_MANAGER>
            thread_local PageAllocator<SYSTYEM_PAGE_MANAGER>
                PageAllocator<SYSTYEM_PAGE_MANAGER>::t_instance;

    } // namespace detail

} // namespace density

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
