
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

			/** Struct allocated at the end of every page. This struct is the reason why the page sized is
				less than SYSTYEM_PAGE_MANAGER::page_size. */
			struct alignas(concurrent_alignment) PageFooter
			{
				/** Pointer to the next page when the page is inside a stack, undefined otherwise. */
				PageFooter * m_next_page{ nullptr };

				/* Number of times the page has been pinned. The allocator can't invalidate the content 
					of a page while the pint count is greater than zero. */
				std::atomic<uintptr_t> m_pin_count{ 0 };
			};

			struct alignas(concurrent_alignment) Slot
			{
				std::atomic<PageFooter*> m_page_list;
				std::atomic<PageFooter*> m_zeroed_page_list;
				std::atomic<Slot*> m_next_slot;

				Slot() = default;

				Slot(Slot * i_next_slot, PageFooter * i_page_list, PageFooter * i_zeroed_page_list) noexcept
					: m_page_list(i_page_list), m_zeroed_page_list(i_zeroed_page_list), m_next_slot(i_next_slot)
				{
				}
			};

		public:

			/** Alignment guaranteed for the pages */
			static constexpr size_t page_alignment = SYSTYEM_PAGE_MANAGER::page_alignment;

			/** Usable size of the pages. */
			static constexpr size_t page_size = SYSTYEM_PAGE_MANAGER::page_size - sizeof(PageFooter);

			static void * allocate_page()
			{
				auto page = pop_page<false>();
				return page;
			}

			static void * allocate_page_zeroed()
			{
				auto page = pop_page<true>();
				#if DENSITY_DEBUG_INTERNAL
					auto chars = static_cast<const char*>(page);
					for (size_t i = 0; i < page_size; i++)
					{
						DENSITY_ASSERT_INTERNAL(chars[i] == 0);
					}
				#endif
				return page;
			}

			static void deallocate_page(void * i_page) noexcept
			{
				push_page<false>(i_page);
			}

			static void deallocate_page_zeroed(void * i_page) noexcept
			{
				push_page<true>(i_page);
			}

			static void pin_page(void * i_address, uintptr_t i_multeplicity) noexcept
			{
				auto const footer = get_footer(i_address);
				footer->m_pin_count.fetch_add(i_multeplicity, std::memory_order_relaxed);
			}

			static uintptr_t unpin_page(void * i_address, uintptr_t i_multeplicity) noexcept
			{
				auto const footer = get_footer(i_address);
				auto const prev_pins = footer->m_pin_count.fetch_sub(i_multeplicity, std::memory_order_acq_rel);
				DENSITY_ASSERT(prev_pins > 0);
				return prev_pins;
			}

			static uintptr_t get_pin_count(const void * i_address) noexcept
			{
				return get_footer(i_address)->m_pin_count.load(std::memory_order_relaxed);
			}

		private:

			static SYSTYEM_PAGE_MANAGER & system_page_manager()
			{
				static SYSTYEM_PAGE_MANAGER s_instance;
				return s_instance;
			}

			template <bool ZEROED>
				static void * pop_page()
			{
				auto constexpr list_memb = ZEROED ? &Slot::m_zeroed_page_list : &Slot::m_page_list;

				/* loop the slots until we reach the one we started from, searching for
					a free page. Note: the slots are arranged in a circular linked list 
					(i.e. m_current_slot->m_next_slot is always valid). */
				auto const initial_slot = m_current_slot;
				do {

					/* try to acquire the stack of pages of the current slot */
					auto list = (m_current_slot->*list_memb).exchange(nullptr, std::memory_order_acquire);
					if (list != nullptr)
					{
						// list acquired: search for an unpinned page
						auto page = allocate_in_list<ZEROED>(list);
						if (page != nullptr)
						{
							return page;
						}
					}

					m_current_slot = m_current_slot->m_next_slot.load();
				} while (m_current_slot != initial_slot);
				
				// allocate a new one from the system
				auto const new_page = system_page_manager().allocate_page();
				if (new_page == nullptr)
				{
					throw std::bad_alloc();
				}
				DENSITY_ASSERT_INTERNAL(address_is_aligned(new_page, SYSTYEM_PAGE_MANAGER::page_alignment));
				auto const should_memset = ZEROED && !SYSTYEM_PAGE_MANAGER::pages_are_zeroed;
				if (should_memset)
				{
					std::memset(new_page, 0, page_size);
				}
				new(get_footer(new_page)) PageFooter();
				return new_page;
			}

			template <bool ZEROED>
				static void * allocate_in_list(PageFooter * i_list) noexcept
			{
				auto constexpr list_memb = ZEROED ? &Slot::m_zeroed_page_list : &Slot::m_page_list;

				// search for an unpinned page
				auto list = i_list;
				auto page = list;
				PageFooter * prev = nullptr;
				do {
					if (page->m_pin_count.load(std::memory_order_acquire) == 0)
					{
						if (page == list)
						{
							list = page->m_next_page;
						}
						else
						{
							prev->m_next_page = page->m_next_page;
						}
						break;
					}
					prev = page;
					page = prev->m_next_page;
				} while (page != nullptr);

				// release the list
				auto const initial_slot = m_current_slot;
				do {
					PageFooter * expected = nullptr;
					if ((m_current_slot->*list_memb).compare_exchange_strong(expected, list,
						std::memory_order_release, std::memory_order_relaxed))
					{
						break;
					}
					
					auto next_slot = m_current_slot->m_next_slot.load();
					if (next_slot == initial_slot)
					{
						void * new_slot_block = aligned_allocate(sizeof(Slot), alignof(Slot));
						DENSITY_ASSERT_INTERNAL(new_slot_block != nullptr);
						auto new_slot = new(new_slot_block) Slot(next_slot, 
							ZEROED ? nullptr : list, !ZEROED ? nullptr : list);

						if (m_current_slot->m_next_slot.compare_exchange_strong(next_slot, new_slot))
						{
							break;
						}
						else
						{
							delete new_slot;
						}
					}

					m_current_slot = next_slot;
				} while (m_current_slot != initial_slot);

				return address_lower_align(page, page_alignment);
			}

			template <bool ZEROED>
				static void push_page(void * i_address)
			{
				auto constexpr list_memb = ZEROED ? &Slot::m_zeroed_page_list : &Slot::m_page_list;
				auto const page = get_footer(address_lower_align(i_address, page_alignment));
				
				/* loop the slots until we reach the one we started from, searching for
					a free page. Note: the slots are arranged in a circular linked list 
					(i.e. m_current_slot->m_next_slot is always valid). */
				auto const initial_slot = m_current_slot;
				do {

					auto list = (m_current_slot->*list_memb).load();

					page->m_next_page = list;

					if ((m_current_slot->*list_memb).compare_exchange_weak(list, page,
						std::memory_order_release, std::memory_order_relaxed))
					{
						break;
					}

					m_current_slot = m_current_slot->m_next_slot.load();
				} while (m_current_slot != initial_slot);
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

			static Slot * first_slot() noexcept
			{
				static struct FirstSlot : Slot
				{
					FirstSlot()
					{
						std::atomic_init(&this->m_page_list, nullptr);
						std::atomic_init(&this->m_zeroed_page_list, nullptr);
						std::atomic_init(&this->m_next_slot, this);
					}

				} s_first_slot;
				return &s_first_slot;
			}

			static thread_local Slot * m_current_slot;
		};

		template <typename SYSTYEM_PAGE_MANAGER>
			thread_local typename page_manager<SYSTYEM_PAGE_MANAGER>::Slot *
				page_manager<SYSTYEM_PAGE_MANAGER>::m_current_slot = page_manager<SYSTYEM_PAGE_MANAGER>::first_slot();
	
	} // namespace detail

} // namespace density

#ifdef _MSC_VER
	#pragma warning(pop)
#endif