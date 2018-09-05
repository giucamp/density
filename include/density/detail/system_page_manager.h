
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <atomic>
#include <density/density_common.h>
#include <new>

namespace density
{
    namespace detail
    {
        /** \internal
            Class template the provides thread safe irreversible page allocation from the system.

            SystemPageManager allocates memory regions using the built-in operator new or
            o.s.-specific APIs. Memory regions are deallocated when SystemPageManager is
            destroyed. In some cases of contention between threads a region may be allocated
            and then deallocated before using it to allocate pages for the user. \n
            The user can request a page with the function allocate_page. There is no function
            to deallocate a page. Pages are guaranteed to remain valid until the SystemPageManager
            is destroyed. Therefore, SystemPageManager does not provide an end-user interface,
            but it is suitable as base for a memory management stack.\n

            If allocate_page can allocate a page without requesting a new memory region to the system,
            the allocation is lock-free (assuming that std::atomic<uintptr_t>: is lock free).
            Otherwise the calling thread may block inside the call to the system.
            When a page can be allocated in the current region, allocate_page is wait-free if
            std::atomic<uintptr_t>::fetch_add is wait-free (which usually is not). \n
            Pages are allocated with allocate_page(), and cannot be deallocated.
            Allocated pages are PAGE_CAPACITY_AND_ALIGNMENT bytes big, and are aligned to
            PAGE_CAPACITY_AND_ALIGNMENT. If the page can't be allocated (because the system fails to
            provide a new memory region), nullptr is returned (allocate_page is noexcept).
            The constant pages_are_zeroed can be used to determine if the content of the newly
            allocated pages is undefined or is guaranteed to be zeroed.

            To avoid internal fragmentation, it is recommended to create only one instance of every
            specialization SystemPageManager for every program run. */
        template <size_t PAGE_CAPACITY_AND_ALIGNMENT> class SystemPageManager
        {
          public:
            static_assert(
              PAGE_CAPACITY_AND_ALIGNMENT > sizeof(void *) * 4 &&
                is_power_of_2(PAGE_CAPACITY_AND_ALIGNMENT),
              "PAGE_CAPACITY_AND_ALIGNMENT too small or not a power of 2");

            /** Alignment of all the pages, in bytes. Alignments are always integer power of 2. */
            static constexpr size_t page_alignment_and_size = PAGE_CAPACITY_AND_ALIGNMENT;

            /** If true, the content of pages returned by allocate_page is zeroed. */
            static constexpr bool pages_are_zeroed = false;

            /** Size in bytes of memory region requested to the system, when necessary. If the system fails
                to allocate a region, SystemPageManager may retry iteratively halving the requested size.
                If the requested size reaches region_min_size_bytes, and the system can't still allocate a region,
                the allocation fails. */
            static constexpr size_t region_default_size_bytes = (4 * 1024 * 1024);

            /** Minimum size (in bytes) of a memory region. */
            static constexpr size_t region_min_size_bytes =
              detail::size_min(region_default_size_bytes, 8 * page_alignment_and_size);

            // The first region is always empty, so it will be skipped soon
            constexpr SystemPageManager() noexcept : m_curr_region(&m_first_region) {}

            ~SystemPageManager()
            {
                auto curr = m_first_region.m_next_region.load();
                while (curr != nullptr)
                {
                    auto const next = curr->m_next_region.load();
                    delete_region(curr);
                    curr = next;
                }
            }

            SystemPageManager(const SystemPageManager &) = delete;
            SystemPageManager & operator=(const SystemPageManager &) = delete;

            /** Allocates a new page from the system. This function never throws.
                @param i_progress_guarantee Progress guarantee. If it is progress_blocking, a failure indicates an out of memory.
                @return the allocated page, or nullptr in case of failure. */
            void * try_allocate_page(progress_guarantee i_progress_guarantee) noexcept
            {
                Region * unused_region = nullptr;
                auto     curr_region   = m_curr_region.load(std::memory_order_acquire);

                // Regions that enter the list are destroyed only at destruction time, so deferencing curr_region is always safe
                allocate_result result;

                bool done = false;
                while (!done)
                {
                    // this call may fail for out of space or for contention
                    result = allocate_page_from_region(i_progress_guarantee, curr_region);

                    switch (result.result)
                    {
                    // success or failure for contention, exit
                    case allocate_result::retry:
                    case allocate_result::success:
                        done = true;
                        break;

                    // space in the region exhausted, allocate another one
                    case allocate_result::nomem:
                        curr_region =
                          get_next_region(i_progress_guarantee, curr_region, &unused_region);
                        done = curr_region == nullptr;
                        break;

                    // unexpected result
                    case allocate_result::notset:
                        DENSITY_ASSERT_INTERNAL(false);
                    }
                }

                if (unused_region != nullptr)
                {
                    delete_region(unused_region);
                }

                return result.address;
            }

            /** Tries to reserve the specified memory space as total memory.
                @param i_progress_guarantee Progress guarantee. If it is progress_blocking, a failure indicates an out of memory.
                @param i_size space in bytes to reserve
                @return actual size reserved, that can be less (in case of failure), equal or greater than i_size */
            uintptr_t try_reserve_region_memory(
              progress_guarantee const i_progress_guarantee, uintptr_t const i_size) noexcept
            {
                Region * unused_region = nullptr;

                auto curr_region = m_curr_region.load(std::memory_order_acquire);

                while (i_size > curr_region->m_cumulative_available_memory)
                {
                    auto next_region =
                      get_next_region(i_progress_guarantee, curr_region, &unused_region);
                    if (next_region == nullptr)
                        break;

                    curr_region = next_region;
                }

                if (unused_region != nullptr)
                {
                    delete_region(unused_region);
                }

                return curr_region->m_cumulative_available_memory;
            }

          private:
            struct Region
            {
                /** Address of the next free page in the region. When >= m_end, the region is exhausted. */
                std::atomic<uintptr_t> m_curr{0};

                /** First address after the available memory of the region */
                uintptr_t m_end{0};

                /** Pointer to the next memory region */
                std::atomic<Region *> m_next_region{nullptr};

                /** Address of the first allocable page of the region */
                uintptr_t m_start{0};

                /** Sum of the sizes (in bytes) of all the memory regions in the list up to this one */
                uintptr_t m_cumulative_available_memory{0};

                constexpr Region() noexcept {}
                Region(const Region &) = delete;
                Region & operator=(const Region &) = delete;
            };

            struct allocate_result
            {
                /** Result of the allocation */
                void * address;

                enum result_t
                {
                    success, /**< allocation successful (implies address != nullptr) */
                    retry, /**< allocation failed because of contention, the progress guarantee could not be respacted 
                                (implies address == nullptr) */
                    nomem, /**< allocation failed because no space was available (implies address == nullptr) */
                    notset /**< empty instance (implies address == nullptr) */
                } result;

                allocate_result() noexcept : address(nullptr), result(notset) {}

                allocate_result(void * i_address) noexcept : address(i_address), result(success)
                {
                    DENSITY_ASSUME(i_address != nullptr);
                }

                allocate_result(result_t i_result) noexcept : address(nullptr), result(i_result) {}
            };

            static allocate_result
              allocate_page_from_region(progress_guarantee i_progress_guarantee, Region * i_region)
            {
                if (i_progress_guarantee != progress_wait_free)
                    return allocate_page_from_region_lockfree(i_region);
                else
                    return allocate_page_from_region_waitfree(i_region);
            }

            /** Allocates a page in the specified region. This function is lock-free.
                The case of successful allocation is the fast path. */
            static allocate_result
              allocate_page_from_region_lockfree(Region * const i_region) noexcept
            {
                /* First we blindly allocate the page, then we detect the overflow of m_curr. This is an
                    optimistic method. To do: compare performances with a load-compare-exchange method.
                    We use acquire because any write (done by the os or whatever) must not be moved
                    past this fetch_add. */
                auto page = i_region->m_curr.fetch_add(
                  PAGE_CAPACITY_AND_ALIGNMENT, std::memory_order_acquire);

                /* We want to exploit the full range of uintptr_t to detect overflows of m_curr, so we
                    check also the wraparound of m_curr until m_start.
                    The detection of the overflow will fail if the number of threads trying allocate_from_region_optimistic
                    is in the order of (MAX(uintptr_t) - region-size) / PAGE_CAPACITY_AND_ALIGNMENT. We consider
                    this case very improbable. */
                if (DENSITY_LIKELY(page >= i_region->m_start && page < i_region->m_end))
                {
                    return allocate_result{reinterpret_cast<void *>(page)};
                }
                else
                {
                    i_region->m_curr.fetch_sub(
                      PAGE_CAPACITY_AND_ALIGNMENT, std::memory_order_relaxed);
                    return allocate_result{allocate_result::nomem};
                }
            }

            /** Allocates a page in the specified region. This function is wait-free, but it can fail in case of contention. */
            static allocate_result
              allocate_page_from_region_waitfree(Region * const i_region) noexcept
            {
                auto curr_address = i_region->m_curr.load(std::memory_order_relaxed);
                auto new_address  = curr_address + PAGE_CAPACITY_AND_ALIGNMENT;

                DENSITY_ASSERT_INTERNAL(
                  (curr_address >= i_region->m_end) ==
                  (new_address >
                   i_region->m_end)); // two different way to express the same condition

                if (curr_address >= i_region->m_end)
                {
                    return allocate_result{allocate_result::nomem};
                }

                if (!i_region->m_curr.compare_exchange_weak(
                      curr_address,
                      new_address,
                      std::memory_order_acquire,
                      std::memory_order_relaxed))
                {
                    return allocate_result{allocate_result::retry};
                }

                return allocate_result{reinterpret_cast<void *>(curr_address)};
            }

            /** Returns the region after a given one, possibly creating it.
                @param i_progress_guarantee progress guarantee. If it is progress_blocking, a
                    failure indicates an out of memory.
                @param i_curr_region region whose next is requested
                @param io_new_region contains a new region provided by the caller, or is nullptr. After the call it
                    may point to a memory region that the caller should destroy, or use somehow.
                @return the next region, or null in case of failure */
            Region * get_next_region(
              progress_guarantee i_progress_guarantee,
              Region *           i_curr_region,
              Region **          io_new_region) noexcept
            {
                // we get the pointer to the next_region region, or allocate it
                auto next_region = i_curr_region->m_next_region.load();
                if (next_region == nullptr)
                {
                    // check if the user want to allocate memory from the system
                    if (i_progress_guarantee != progress_blocking)
                    {
                        return nullptr;
                    }

                    // allocate a new region, if we don't have one already
                    if (*io_new_region == nullptr)
                    {
                        *io_new_region = create_region();
                    }

                    if (*io_new_region != nullptr)
                    {
                        // set m_cumulative_available_memory on the new region
                        auto const new_region_size =
                          (*io_new_region)->m_end - (*io_new_region)->m_curr;
                        DENSITY_ASSUME_UINT_ALIGNED(new_region_size, page_alignment_and_size);
                        (*io_new_region)->m_cumulative_available_memory =
                          i_curr_region->m_cumulative_available_memory + new_region_size;

                        // The allocation succeeded, so set the pointer to the next_region region, if it is null
                        if (i_curr_region->m_next_region.compare_exchange_strong(
                              next_region, *io_new_region))
                        {
                            next_region    = *io_new_region;
                            *io_new_region = nullptr;
                        }
                    }
                    else
                    {
                        /* We couldn't allocate a new region, check if someone else could in the meanwhile.
                            If not, we give in. Else we continue the loop. */
                        next_region = i_curr_region->m_next_region.load();
                        if (next_region == nullptr)
                        {
                            // allocation failed
                            return nullptr;
                        }
                    }
                }

                /* try to update m_curr_region (if m_curr_region == i_curr_region, then m_curr_region = next_region).
                    This operation is not mandatory, so we can tolerate spurious failures. */
                DENSITY_ASSUME(next_region != nullptr);
                auto expected = i_curr_region;
                m_curr_region.compare_exchange_weak(
                  expected, next_region, std::memory_order_release, std::memory_order_relaxed);

                // we are done we this region
                i_curr_region = next_region;

                return i_curr_region;
            }

            /** Creates a new memory region big region_default_size_bytes. Tries with smaller sizes on failure.
                After failing with region_min_size_bytes, return nullptr. */
            static Region * create_region() noexcept
            {
                Region * region = new (std::nothrow) Region;
                if (region == nullptr)
                {
                    return nullptr;
                }

                size_t region_size  = region_default_size_bytes;
                void * region_start = nullptr;
                while (region_start == nullptr)
                {
                    region_size = detail::size_max(region_size, region_min_size_bytes);

                    region_start = operator new(region_size, std::nothrow);
                    if (region_start == nullptr)
                    {
                        if (region_size == region_min_size_bytes)
                        {
                            // failed to allocate a region big region_min_size_bytes, give up
                            delete_region(region);
                            return nullptr;
                        }
                        region_size /= 2;
                    }
                }

                auto const curr = address_upper_align(region_start, PAGE_CAPACITY_AND_ALIGNMENT);
                auto const end  = address_lower_align(
                  address_add(region_start, region_size), PAGE_CAPACITY_AND_ALIGNMENT);
                DENSITY_ASSERT_INTERNAL(region_start <= curr && curr < end);

                region->m_start = reinterpret_cast<uintptr_t>(region_start);
                region->m_curr.store(reinterpret_cast<uintptr_t>(curr));
                region->m_end = reinterpret_cast<uintptr_t>(end);
                return region;
            }

            static void delete_region(Region * const i_region) noexcept
            {
                DENSITY_ASSUME(i_region != nullptr);
                operator delete(reinterpret_cast<void *>(i_region->m_start));
                delete i_region;
            }

          private:
            std::atomic<Region *>
                   m_curr_region;  /**< Usually this is a pointer to the last memory region,
                but in case of contention between threads it may be left behind. */
            Region m_first_region; /**< First memory region, always empty */
        };

    } // namespace detail

} // namespace density
