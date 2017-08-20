
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <new>
#include <cstring>
#include <atomic>
#include <density/density_common.h>

namespace density
{
	namespace detail
	{
		/** \internal Structure allocated at the end of every page. 
			This struct is the reason why page_manager::page_size is less than SYSTYEM_PAGE_MANAGER::page_size. */
		struct PageFooter
		{
			/** Pointer to the next page when the page is inside a stack, undefined otherwise. */
			PageFooter * m_next_page{ nullptr };

			/* Number of times the page has been pinned. The allocator can't modify the content 
				of a page while the pin count is greater than zero. */
			std::atomic<uintptr_t> m_pin_count{ 0 };
		};

		class PageStack
		{
		public:
			
			PageStack(PageFooter * i_first = nullptr) noexcept
				: m_first(i_first), m_cached_last(nullptr)
			{

			}

			void push(PageFooter * i_page) noexcept
			{
				DENSITY_ASSERT_INTERNAL(i_page != nullptr);

				i_page->m_next_page = m_first;
				m_first = i_page;
			}

			void push(PageStack i_stack) noexcept
			{
				DENSITY_ASSERT_INTERNAL(!i_stack.empty());

				i_stack.find_last()->m_next_page = m_first;
				m_first = i_stack.first();
			}

			PageFooter * first() const noexcept
			{
				return m_first;
			}

			bool empty() const noexcept
			{
				return m_first == nullptr;
			}
			
			PageFooter * find_last() noexcept
			{
				DENSITY_ASSERT_INTERNAL(m_first != nullptr);
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

			/** Search for a page with pin-count == 0 in the stack of pages starting from i_first.
				If such page is found, it is removed, possibly modifying i_first. No exception is ever thrown.
				@param i_first reference to the pointer to the first page of the stack
				@return the page removed from the list, or nullptr */
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

		private:
			PageFooter * m_first = nullptr, * m_cached_last = nullptr;
		};

	} // namespace detail

} // namespace density
