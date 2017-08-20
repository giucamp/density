
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <new>
#include <atomic>
#include <density/density_common.h>

namespace density
{
	namespace detail
	{
		/** \internal 
			Class template the provides thread safe irreversible page allocation from the system.

			system_page_manager allocates memory regions using the built-in operator new or 
			o.s.-specific APIs. Memory regions are deallocated when system_page_manager is 
			destroyed. In some cases of contention between threads a region may be allocated
			and then deallocated before using it to allocate pages for the user. \n
			The user can request a page with the function allocate_page. There is no function
			to deallocate a page. Pages are guaranteed to remain valid until the system_page_manager
			is destroyed. Therefore, system_page_manager does not provide an end-user interface, 
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
			specialization system_page_manager for every program run. */
		template <size_t PAGE_CAPACITY_AND_ALIGNMENT>
			class system_page_manager
		{
		public:

			static_assert(PAGE_CAPACITY_AND_ALIGNMENT > sizeof(void*) * 4 && is_power_of_2(PAGE_CAPACITY_AND_ALIGNMENT),
				"PAGE_CAPACITY_AND_ALIGNMENT too small or not a power of 2");

			/** Size of all the pages, in bytes. */
			static constexpr size_t page_size = PAGE_CAPACITY_AND_ALIGNMENT;
			
			/** Alignment of all the pages, in bytes. Alignments are always integer power of 2. */
			static constexpr size_t page_alignment = PAGE_CAPACITY_AND_ALIGNMENT;

			/** If true, the content of pages returned by allocate_page is zeroed. */
			static constexpr bool pages_are_zeroed = false;

			/** Size in bytes of memory region requested to the system, when necessary. If the system fails
				to allocate a region, system_page_manager may retry iteratively halving the requested size.
				If the requested size reaches region_min_size_bytes, and the system can't still allocate a region, 
				the allocation fails (and nullptr is returned). */
			static constexpr size_t region_default_size_bytes = (4 * 1024 * 1024);

			/** Minimum size (in bytes) of a memory region. */
			static constexpr size_t region_min_size_bytes = detail::size_min(region_default_size_bytes, 8 * page_size);			

			system_page_manager() noexcept
			{
				/** The first region is always empty, so it will be skipped soon */
				std::atomic_init(&m_curr_region, &m_first_region);
			}

			~system_page_manager()
			{
				auto curr = m_first_region.m_next_region.load();
				while (curr != nullptr)
				{
					auto const next = curr->m_next_region.load();
					delete_region(curr);
					curr = next;
				}
			}

			system_page_manager(const system_page_manager &) = delete;
			system_page_manager & operator = (const system_page_manager &) = delete;

			/** Allocates a new page from the system. This function never throws.
				@param allocate_page_opt. Specifies whether new memory can be allocated from the system. Must be
					either allow_system_alloc or only_available.
				@return the allocated page, or nullptr in case of failure.
			
				This function is wait-free if i_options is allocate_page_opt::only_available, obstruction-free otherwise. */
			void * allocate_page(progress_guarantee i_progress_guarantee) noexcept
			{
				auto new_region = static_cast<Region*>(nullptr);
				auto curr_region = m_curr_region.load(std::memory_order_acquire);

				// Regions that enter the list are destroyed only at destruction time, so the following iteration is always safe
				void * new_page;
				while ( (new_page = allocate_from_region(i_progress_guarantee, curr_region)) == nullptr)
				{
					// we get the pointer to the next_region region, or allocate it
					auto next_region = curr_region->m_next_region.load();
					if (next_region == nullptr)
					{
						// check if the user want to allocate memory from the system
						if (i_progress_guarantee != progress_guarantee::progress_obstruction_free)
						{
							return nullptr;
						}

						// allocate a new region, if we don't have one already
						if (new_region == nullptr)
						{
							new_region = create_region();
						}

						if (new_region != nullptr)
						{
							// The allocation succeeded, so set the pointer to the next_region region, if it is null
							if (curr_region->m_next_region.compare_exchange_strong(next_region, new_region))
							{
								next_region = new_region;
								new_region = nullptr;
							}
						}
						else
						{
							/* We couldn't allocate a new region, check if someone else could in the meanwhile.
								If not, we give in. Else we continue the loop. */
							next_region = curr_region->m_next_region.load();
							if (next_region == nullptr)
							{
								// allocation failed
								return nullptr;
							}
						}
					}

					/* try to update m_curr_region (if m_curr_region == curr_region, then m_curr_region = next_region).
						This operation is not mandatory, so we can tolerate spurious failures. */
					DENSITY_ASSERT_INTERNAL(next_region != nullptr);
					auto expected = curr_region;
					m_curr_region.compare_exchange_weak(expected, next_region,
						std::memory_order_release, std::memory_order_relaxed);

					// we are done we this region
					curr_region = next_region;
				}

				if (new_region != nullptr)
				{
					delete_region(new_region);
				}

				return new_page;
			}

		private:

			struct Region
			{
				/** Address of the next free page in the region. When >= m_end, the region is exhausted. */
				sync::atomic<uintptr_t> m_curr{ 0 };

				/** First address after the available memory of the region */
				uintptr_t m_end{ 0 };

				/** Pointer to the next memory region */
				sync::atomic<Region*> m_next_region{ nullptr };

				/** Address of the first allocable page of the region */
				uintptr_t m_start{ 0 };
			};

			static void * allocate_from_region(progress_guarantee i_progress_guarantee, Region * const i_region)
			{
				if (i_progress_guarantee != progress_wait_free)
					return allocate_from_region_lockfree(i_region);
				else
					return allocate_from_region_waitfree(i_region);
			}

			/** Allocates a page in the specified region. This function is lock-free.
				The case of successful allocation is the fast path. */
			static void * allocate_from_region_lockfree(Region * const i_region) noexcept
			{
				/* First we blindly allocate the page, then we detect the overflow of m_curr. This is an 
					optimistic method. To do: compare performances with a load-compare-exchange method. */
				auto page = i_region->m_curr.fetch_add(PAGE_CAPACITY_AND_ALIGNMENT, std::memory_order_relaxed);
				
				/* We want to exploit the full range of uintptr_t to detect overflows of m_curr, so we
					check also the wraparound of m_curr until m_start.
					The detection of the overflow will fail if the number of threads trying allocate_from_region_optimistic
					is in the order of (MAX(uintptr_t) - region-size) / PAGE_CAPACITY_AND_ALIGNMENT. We consider 
					this case very improbable. */
				if (page >= i_region->m_start && page < i_region->m_end)
				{
					return reinterpret_cast<void*>(page);
				}
				else
				{
					i_region->m_curr.fetch_sub(PAGE_CAPACITY_AND_ALIGNMENT, std::memory_order_relaxed);
					return nullptr;
				}
			}

			/** Allocates a page in the specified region. This function is wait-free, but it can fail in case of contention. */
			static void * allocate_from_region_waitfree(Region * const i_region) noexcept
			{
				auto curr_address = i_region->m_curr.load(std::memory_order_relaxed);
				auto new_address = curr_address + PAGE_CAPACITY_AND_ALIGNMENT;
				DENSITY_ASSERT_INTERNAL( (curr_address >= i_region->m_end) == (new_address > i_region->m_end) ); // two different way to express the condition
				if (curr_address < i_region->m_end)
				{
					if (i_region->m_curr.compare_exchange_weak(curr_address, new_address, std::memory_order_relaxed))
					{
						return reinterpret_cast<void*>(curr_address);
					}
				}
				return nullptr;
			}

			/** Creates a new memory region big region_default_size_bytes. Tries with smaller sizes on failure.
				After failing with region_min_size_bytes, return nullptr. */
			static Region * create_region() noexcept
			{
				Region * region = new(std::nothrow) Region;
				if (region == nullptr)
				{
					return nullptr;
				}

				size_t region_size = region_default_size_bytes;
				void * region_start = nullptr;
				while (region_start == nullptr)
				{
					region_size = detail::size_max(region_size, region_min_size_bytes);

					region_start = operator new (region_size, std::nothrow);
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
				auto const end = address_lower_align(address_add(region_start, region_size), PAGE_CAPACITY_AND_ALIGNMENT);
				DENSITY_ASSERT_INTERNAL(region_start <= curr && curr < end);

				region->m_start = reinterpret_cast<uintptr_t>(region_start);
				region->m_curr.store(reinterpret_cast<uintptr_t>(curr));
				region->m_end = reinterpret_cast<uintptr_t>(end);
				return region;
			}

			static void delete_region(Region * const i_region) noexcept
			{
				DENSITY_ASSERT_INTERNAL(i_region != nullptr);
				operator delete (reinterpret_cast<void*>(i_region->m_start));
				delete i_region;
			}			

		private:
			std::atomic<Region *> m_curr_region; /**< Usually this is a pointer to the last memory region,
				but in case of contention between threads it may be left behind. */
			Region m_first_region; /**< First memory region, statically allocated and always empty */
		};

	} // namespace detail

} // namespace density

