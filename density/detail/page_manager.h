
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <new>
#include <cstring>
#include <atomic>
#include <density/density_common.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

namespace density
{
	namespace detail
	{
		/** \internal Structure allocated at the end of every page. 
			This struct is the reason why page_manager::page_size is less than SYSTYEM_PAGE_MANAGER::page_size. */
		struct alignas(concurrent_alignment) PageFooter
		{
			/** Pointer to the next page when the page is inside a stack, undefined otherwise. */
			PageFooter * m_next_page{ nullptr };

			/* Number of times the page has been pinned. The allocator can't modify the content 
				of a page while the pin count is greater than zero. */
			std::atomic<uintptr_t> m_pin_count{ 0 };
		};

		#if 0 // currently not used

		/** \internal This class implements a lock-free stack of free pages. This is not a general purpose
			stack, rather it is designed and specialized to be used by the page manager.
			The push inserts a page always on the top. The pop removes the first unpinned page, which may
			not be the page on the top.

			PageStack allows a limited concurrency: only one thread at time is allowed to 
			call the functions push/pop/steal_all_and_pop. Anyway a stack can be used as argument for 
			steal_all_and_pop by any thread in any moment within the lifetime of the object. 
			Implementation note: the reason for this limited concurrency are the stores on m_first,
			which are assuming that no other thread is pushing pages. */
		class PageStack
		{
		private:
			std::atomic<PageFooter*> m_first{ nullptr }; /**< Top of the stack. */

		public:

			/* Pushes a (possibly still pinned) page on the stack. 
				@param i_page The page to push. The member initial value of m_next_page is ignored
			*/
			void push(PageFooter * i_page) noexcept
			{
				auto first = m_first.load(detail::mem_relaxed);
				do {
					i_page->m_next_page = first;

					/* Here a thread may set m_first to nullptr while executing steal_all_and_pop
					   using this stack as victim (the parameter). */

				} while (!m_first.compare_exchange_weak(first, i_page, detail::mem_release, detail::mem_acquire));
			}

			/** Removes from the stack the first unpinned page. 
				As first operation, a pop temporary steals the whole stack. So it can safely walk and analyze
				the pages, and can edit the stack without incurring in the ABA problem. In the meanwhile,
				the stack is empty. After finishing the work, the stack is restored (possibly with one 
				less page). Another benefit of this mechanism is that PageFooter::m_next does not need to be
				an atomic.
				This function is optimized for the execution path in which a page is found.
			
				@return The page removed from the stack, or nullptr in case of failure.
			*/
			PageFooter * pop() noexcept
			{
				auto stack = m_first.exchange(nullptr, detail::mem_acquire);
				if (stack != nullptr)
				{
					auto const result = stack != nullptr ? remove_unpinned(stack) : nullptr;

					// assuming that m_first is still nullptr...
					DENSITY_ASSERT_INTERNAL(m_first.load(detail::mem_relaxed) == nullptr);
					m_first.store(stack, detail::mem_release);
					return result;
				}
				else
				{
					return nullptr;
				}
			}

			/** Tries to steal all the page from the victim stack. In case of success all the pages
				are transfered to this stack. Then tries to pop an unpinned page (if any). 
				@param i_victim Stack to steal from. After the call the victim is always empty.

				@return The page removed from the stack, or nullptr in case of failure.
			*/
			PageFooter * steal_all_and_pop(PageStack & i_victim) noexcept
			{
				auto pages = i_victim.m_first.exchange(nullptr, detail::mem_acquire);
				if (pages == nullptr)
				{
					return nullptr;
				}
				else
				{
					auto const result = remove_unpinned(pages);

					// assuming that m_first is still nullptr...
					DENSITY_ASSERT_INTERNAL(m_first.load(detail::mem_relaxed) == nullptr);
					m_first.store(pages, detail::mem_release);
					return result;
				}
			}

		private:
				
			/** Search for a page with pin-count == 0 in the stack of pages starting from i_first.
				If such page is found, it is removed, possibly modifying i_first. No exception is ever thrown.
				@param i_first reference to the pointer to the first page of the stack
				@return the page removed from the list, or nullptr */
			static PageFooter * remove_unpinned(PageFooter * & i_first) noexcept
			{
				DENSITY_ASSERT_INTERNAL(i_first != nullptr);
				PageFooter * curr = i_first;
				PageFooter * prev = nullptr;
				do {
					DENSITY_ASSERT_INTERNAL((prev == nullptr) == (curr == i_first));

					if (curr->m_pin_count.load(detail::mem_relaxed) == 0)
					{
						if (prev != nullptr)
							prev->m_next_page = curr->m_next_page;
						else
							i_first = curr->m_next_page;
						return curr;
					}

					prev = curr;
					curr = curr->m_next_page;
				} while (curr != nullptr);

				return nullptr;
			}
		};
		#endif // #if 0


		/** \internal This class implements a lock-free stack of free pages. This is not a general purpose
			stack, rather it is designed and specialized to be used by the page manager.
			While a thread is doing a pop, the other threads may observe an empty stack. */
		class PageStack1
		{
		private:
			std::atomic<PageFooter*> m_first{ nullptr }; /**< Top of the stack. */

		public:

			/* Pushes a (possibly still pinned) single page on the stack. 
				@param i_page The page to push. The initial value of i_page->m_next_page is ignored. 
					This parameter can't be nullptr.
					If this page is already in the list the behavior is undefined. 
			*/
			void push(PageFooter * i_page) noexcept
			{
				DENSITY_ASSERT_INTERNAL(i_page != nullptr);

				auto first = m_first.load(detail::mem_relaxed);
				do {
					i_page->m_next_page = first;

					/* The ABA problem may happen, but here is harmless: even if m_first has been changed 
						to B and then back to first, the push can be safely committed. */

				} while (!m_first.compare_exchange_weak(first, i_page, detail::mem_release, detail::mem_acquire));
			}

			/* Pushes a list of (possibly still pinned) pages on the stack. 
				@param i_first Null-terminated list of pages to push. If any page in this list is 
					already in the list the behavior is undefined. 
					This parameter can be nullptr, in which case the function has no effects.
			*/
			void push_list(PageFooter * i_list_first) noexcept
			{
				if (i_list_first != nullptr)
				{
					PageFooter * list_last = nullptr;

					auto first = m_first.load(detail::mem_relaxed);
					do {

						/* we have to set the value we have read from m_first as the next of the last of the
							input list. Anyway, if m_first was null, we avoid an expensive linear scan, as
							the input list is null-terminated. */
						if (first != nullptr)
						{
							if (list_last == nullptr) /* we have to do the scan only once, otherwise we would
								would reach first (because we append it to the input list). */
							{
								list_last = i_list_first;
								while (list_last->m_next_page != nullptr)
								{
									list_last = list_last->m_next_page;
								}
							}
							list_last->m_next_page = first;
						}
						else if (list_last != nullptr)
						{
							/* m_first was null, but we already have scanned the input list in a previous
								iteration, so we have set list_last->m_next_page to something else. So we need
								to set is to null. */
							list_last->m_next_page = nullptr;
						}

						/* The ABA problem may happen, but here it is harmless: even if m_first has been changed
							to B and then back to first, the push can be safely committed. */

					} while (!m_first.compare_exchange_weak(first, i_list_first, detail::mem_release, detail::mem_acquire));
				}
			}

			/* Pushes a list of (possibly still pinned) pages on the stack. Then tries to pop an unpinned page
				searching among those just pushed.
				On an empty stack, this function is equivalent to a call to push_list followed by a call to pop.
				@param i_first Null-terminated list of pages to push. If any page in this list is 
					already in the list the behavior is undefined. 
					This parameter can be nullptr, in which case the function has no effects, and the return value is nullptr.
				@return Unpinned page, if any, or nullptr.
			*/
			PageFooter * push_list_and_pop_one(PageFooter * i_list_first) noexcept
			{
				PageFooter * unpinned = nullptr;
				if (i_list_first != nullptr)
				{
					unpinned = remove_unpinned(i_list_first);
					push_list(i_list_first);
				}
				return unpinned;
			}

			/** Removes from the stack the first unpinned page. 
				As first operation, a pop temporary steals the whole stack. So it can safely walk and analyze
				the pages, and can edit the stack without incurring in the ABA problem. In the meanwhile,
				any other thread will observe the stack as empty. After finishing the work, the stack is restored 
				(possibly with one less page). Another benefit of this mechanism is that PageFooter::m_next does 
				not need to be an atomic.
				This function is optimized for the execution path in which a page is found.
			
				@return The page removed from the stack, or nullptr in case of failure.
			*/
			PageFooter * pop() noexcept
			{
				auto stack = m_first.exchange(nullptr, detail::mem_acquire);
				if (stack != nullptr)
				{
					auto const page = stack != nullptr ? remove_unpinned(stack) : nullptr;

					// now we have to restore the stack
					auto const list = m_first.exchange(stack, detail::mem_acq_rel);
					if (list != nullptr)
					{
						/* Another thread has pushed pages since we did the first exchange. So
							with the second exchange we have removed those page, and we are 
							going to push them again on the stack. */
						push_list(list);
					}

					return page;
				}
				else
				{
					return nullptr;
				}
			}

			/** Empties the stack, removing all the pages. A null-terminated list of the removed pages is returned. 
				This function is optimized for the execution path in which at least a page was present.
				@return The pages removed, or nullptr in the case the stack was already empty.
			*/
			PageFooter * remove_all_optimistic() noexcept
			{
				return m_first.exchange(nullptr, detail::mem_acquire);
			}

			/** Empties the stack, removing all the pages. A null-terminated list of the removed pages is returned. 
				This function is optimized for the execution path in which the stack was empty.

				@return The pages removed, or nullptr in the case the stack was already empty.
			*/
			PageFooter * remove_all_pessimistic() noexcept
			{
				if (m_first.load(detail::mem_relaxed) == nullptr)
				{
					return nullptr;
				}
				else
				{
					return remove_all_optimistic();
				}
			}

		private:
				
			/** Search for a page with pin-count == 0 in the stack of pages starting from i_first.
				If such page is found, it is removed, possibly modifying i_first. No exception is ever thrown.
				@param i_first reference to the pointer to the first page of the stack
				@return the page removed from the list, or nullptr */
			static PageFooter * remove_unpinned(PageFooter * & i_first) noexcept
			{
				DENSITY_ASSERT_INTERNAL(i_first != nullptr);
				PageFooter * curr = i_first;
				PageFooter * prev = nullptr;
				do {
					DENSITY_ASSERT_INTERNAL((prev == nullptr) == (curr == i_first));

					if (curr->m_pin_count.load(detail::mem_relaxed) == 0)
					{
						if (prev != nullptr)
							prev->m_next_page = curr->m_next_page;
						else
							i_first = curr->m_next_page;
						return curr;
					}

					prev = curr;
					curr = curr->m_next_page;
				} while (curr != nullptr);

				return nullptr;
			}
		};

		/** \internal 
			
		*/
		struct alignas(concurrent_alignment) FreePageStore
		{
		private:

			FreePageStore(FreePageStore * i_next_slot) noexcept
				: m_next_slot(i_next_slot)
			{
			}

			~FreePageStore() noexcept = default;

		public:

			PageStack1 m_page_stack;
			PageStack1 m_zeroed_page_stack;
			std::atomic<FreePageStore*> m_next_slot;

			/** Creates a new thread slot.
				May throw an std::bad_alloc. */
			static FreePageStore * create(FreePageStore * i_next_slot)
			{
				auto const block = aligned_allocate(sizeof(FreePageStore), alignof(FreePageStore));
				DENSITY_ASSERT_INTERNAL(block != nullptr);
				return new(block) FreePageStore(i_next_slot);
			}

			/** Destroy a thread slot */
			static void destroy(FreePageStore * i_slot) noexcept
			{
				DENSITY_ASSERT_INTERNAL(i_slot != nullptr);
				i_slot->~FreePageStore();
				return aligned_deallocate(i_slot, sizeof(FreePageStore), alignof(FreePageStore));
			}
		};

		enum class page_allocation_type
		{
			uninitialized,
			zeroed,
		};

		/** \internal 
			Class template providing page based memory management.
			page_manager allows allocation and deallocation of pages.

			Implementation notes:
			page_manager keeps a list of slots. Every slot has a free-list of pages and a free-list of
			zeroed pages. Every thread has a pointer to its current slot. When a thread fails to take a page
			from a slot, or when it finds out to have contention with another thread on the same slot, it 
			may move to another slot, that may already exist or may be a new one. */
		template <typename SYSTYEM_PAGE_MANAGER>
			class page_manager
		{
		private:

			struct ThreadEntry
			{
				FreePageStore * m_current_slot, * m_victim_slot;

				ThreadEntry()
				{
					auto & global_data = get_global_data();
					m_current_slot = global_data.assign_one();
					m_victim_slot = m_current_slot->m_next_slot.load();
				}
			};
			
			static thread_local ThreadEntry t_thread_entry;

			struct GlobalData
			{
				SYSTYEM_PAGE_MANAGER m_sys_page_manager;

				std::atomic<FreePageStore*> m_last_assigned{nullptr};
				std::atomic<FreePageStore*> m_first_slot{nullptr};
				
				GlobalData()
				{
					auto const first = FreePageStore::create(nullptr);
					auto prev = first;
					for (int i = 0; i < 7; i++)
					{
						auto const curr = FreePageStore::create(nullptr);
						prev->m_next_slot.store(curr);
						prev = curr;
					}
					prev->m_next_slot.store(first);
					m_first_slot.store(first);
					m_last_assigned = first;
				}

				~GlobalData()
				{
					auto curr = m_first_slot.load();
					while (curr != nullptr)
					{
						auto const next = curr->m_next_slot.load();
						FreePageStore::destroy(curr);
						curr = next;
					}
				}

				FreePageStore * assign_one() noexcept
				{
					auto result = m_last_assigned.load();
					m_last_assigned.store(result->m_next_slot.load());
					return m_last_assigned;
				}
			};

				
			static GlobalData & get_global_data()
			{
				static GlobalData s_global_data;
				return s_global_data;
			}

			static PageFooter * get_footer(void * i_address) noexcept
			{
				auto const page = address_lower_align(i_address, page_alignment);
				return static_cast<PageFooter*>(address_add(page, page_size));
			}

			static const PageFooter * get_footer(const void * i_address) noexcept
			{
				auto const page = address_lower_align(i_address, page_alignment);
				return static_cast<const PageFooter*>(address_add(page, page_size));
			}

			template <page_allocation_type allocation_type>
				static PageFooter * allocate_page_slow_path()
			{
				auto constexpr list_memb = allocation_type == page_allocation_type::zeroed ? &FreePageStore::m_zeroed_page_stack : &FreePageStore::m_page_stack;

				PageFooter * new_page = nullptr;

				// First we try to use the memory already allocated from the system...
				GlobalData & global_data = get_global_data();
				void * new_page_mem = global_data.m_sys_page_manager.allocate_page(allocate_page_opt::only_available);
				if (new_page_mem != nullptr)
				{
					DENSITY_ASSERT_INTERNAL(address_is_aligned(new_page_mem, page_alignment));
					new_page = new(get_footer(new_page_mem)) PageFooter();
					bool should_zero = allocation_type == page_allocation_type::zeroed && !SYSTYEM_PAGE_MANAGER::pages_are_zeroed;
					if (should_zero)
					{
						std::memset(new_page_mem, 0, page_size);
					}
					return new_page;
				}
				else
				{
					// ...then try to steal from m_victim_slot, looping all the slots...
					auto const starting_victim_slot = t_thread_entry.m_victim_slot;
					do {

						auto const stolen_list = (t_thread_entry.m_victim_slot->*list_memb).remove_all_pessimistic();
						new_page = (t_thread_entry.m_current_slot->*list_memb).push_list_and_pop_one(stolen_list);
						if (new_page != nullptr)
						{
							break;
						}

						t_thread_entry.m_victim_slot = t_thread_entry.m_victim_slot->m_next_slot.load();

					} while (t_thread_entry.m_victim_slot != starting_victim_slot);

					if (new_page == nullptr)
					{
						// ...last chance, try possibly allocating new memory from the system
						new_page_mem = global_data.m_sys_page_manager.allocate_page(allocate_page_opt::allow_system_alloc);
						if (new_page_mem != nullptr)
						{
							DENSITY_ASSERT_INTERNAL(address_is_aligned(new_page_mem, page_alignment));
							new_page = new(get_footer(new_page_mem)) PageFooter();
							bool should_zero = allocation_type == page_allocation_type::zeroed && !SYSTYEM_PAGE_MANAGER::pages_are_zeroed;
							if (should_zero)
							{
								std::memset(new_page_mem, 0, page_size);
							}
						}
						else
						{
							throw std::bad_alloc();
						}
					}

					return new_page;
				}
			}

		public:

			/** Alignment guaranteed for the pages */
			static constexpr size_t page_alignment = SYSTYEM_PAGE_MANAGER::page_alignment;

			/** Usable size of the pages. */
			static constexpr size_t page_size = SYSTYEM_PAGE_MANAGER::page_size - sizeof(PageFooter);

			template <page_allocation_type allocation_type>
				static void * allocate_page()
			{
				static_assert(allocation_type == page_allocation_type::uninitialized || allocation_type == page_allocation_type::zeroed, "");
				auto constexpr list_memb = allocation_type == page_allocation_type::zeroed ? &FreePageStore::m_zeroed_page_stack : &FreePageStore::m_page_stack;

				// First try to pop from the current slot...
				PageFooter * new_page = (t_thread_entry.m_current_slot->*list_memb).pop();
				if (new_page == nullptr)
				{
					// ...else try to steal all the pages from the victim slot					
					auto const all_stolen_pages = (t_thread_entry.m_victim_slot->*list_memb).remove_all_optimistic();

					// push the list to the current slot, possibly getting an unpinned page
					new_page = (t_thread_entry.m_current_slot->*list_memb).push_list_and_pop_one(all_stolen_pages);					
					if (new_page == nullptr)
					{
						new_page = allocate_page_slow_path<allocation_type>();
					}
				}

				// new_page is a PageFooter, we have to return the address of the first byte of the page
				DENSITY_ASSERT_INTERNAL(get_footer(new_page) == new_page);
				return address_lower_align(new_page, page_alignment);
			}

			template <page_allocation_type allocation_type>
				static void deallocate_page(void * i_page) noexcept
			{
				static_assert(allocation_type == page_allocation_type::uninitialized || allocation_type == page_allocation_type::zeroed, "");
				auto constexpr list_memb = allocation_type == page_allocation_type::zeroed ? &FreePageStore::m_zeroed_page_stack : &FreePageStore::m_page_stack;
				
				auto const footer = get_footer(i_page);
				(t_thread_entry.m_current_slot->*list_memb).push(footer);
			}

			static void pin_page(void * i_address) noexcept
			{
				auto const footer = get_footer(i_address);
				footer->m_pin_count.fetch_add(1, detail::mem_relaxed);
			}

			static void unpin_page(void * i_address) noexcept
			{
				auto const footer = get_footer(i_address);
				#if DENSITY_DEBUG
					auto const prev_pins = footer->m_pin_count.fetch_sub(1, detail::mem_acq_rel);
					DENSITY_ASSERT(prev_pins > 0);
				#else
					footer->m_pin_count.fetch_sub(1, detail::mem_acq_rel);
				#endif
			}

			static uintptr_t get_pin_count(const void * i_address) noexcept
			{
				return get_footer(i_address)->m_pin_count.load(detail::mem_relaxed);
			}

		private:


		};

		template <typename SYSTYEM_PAGE_MANAGER>
			thread_local typename page_manager<SYSTYEM_PAGE_MANAGER>::ThreadEntry 
				page_manager<SYSTYEM_PAGE_MANAGER>::t_thread_entry;

	
	} // namespace detail

} // namespace density

#ifdef _MSC_VER
	#pragma warning(pop)
#endif