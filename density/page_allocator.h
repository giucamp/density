
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cstdlib> // for std::max_align_t
#include "density_common.h"
#if DENSITY_DEBUG_INTERNAL
	#include <mutex>
	#include <memory>
	#include <unordered_set>
	#include <unordered_map>
#endif

namespace density
{
	/*! \page PageAllocator_concept PageAllocator concept
		A PageAllocator is a type that encapsulates an untyped page-based memory management. Pages have a 
		fixed size and requirement. Being untyped, a PageAllocator is unaware of the type of objects that
		will be constructed in a page (like std::malloc).
		
		A PageAllocator provides a fall-back allocation strategy for large blocks: the user may need to allocate
		blocks larger than a page.

		A PageAllocator must provide the following members:
		
		\section SizeAlign Size and alignment
		@code
		static size_t page_size() noexcept;
		static size_t page_alignment() noexcept;
		@endcode
		All pages have the same size and alignment. These two functions must return the same value for every 
		invocation in the same program execution. 

		\section PageAlloc Page allocation \ deallocation
		@code
		void * allocate_page();
		void deallocate_page(void * i_page) noexcept;
		@endcode
		These functions allocate and deallocate a single page. The size and alignment of the page are the
		values returned by page_size() and page_alignment().

		\section LargeBlockAlloc Large block allocation \ deallocation
		@code
		void * allocate_large_block(size_t i_size);
		void deallocate_large_block(void * i_block, size_t i_size) noexcept;
		@endcode
		These functions allocate and deallocate a single page
	*/


	/** This class provides  */
    class page_allocator
	{
    public:
		
		static size_t page_size() noexcept { return 4096; }
		
		static size_t page_alignment() noexcept { return alignof(std::max_align_t); }
		
		static const size_t thread_store_size = 8;
		
        void * allocate_page()
        {
			auto page = thread_page_store().peek();
			if (page == nullptr)
			{
				page = allocate_page_impl();
			}			
			#if DENSITY_DEBUG_INTERNAL
				m_dbg_data->add_page(page);
			#endif
			return page;
        }

        void deallocate_page(void * i_page) noexcept
        {
			#if DENSITY_DEBUG_INTERNAL
				m_dbg_data->remove_page(i_page);
			#endif
			auto & page_store = thread_page_store();
			if (page_store.size() < thread_store_size)
			{
				page_store.push(i_page);
			}
			else
			{
				deallocate_page_impl(i_page);
			}
        }

        void * allocate_large_block(size_t i_size)
        {
			auto const block = operator new (i_size);
			#if DENSITY_DEBUG_INTERNAL
				m_dbg_data->add_large_block(block, i_size);
			#endif
			return block;
        }

        void deallocate_large_block(void * i_block, size_t i_size) noexcept
        {
			#if DENSITY_DEBUG_INTERNAL
				m_dbg_data->remove_large_block(i_block, i_size);
			#endif
			#if __cplusplus >= 201402L
				operator delete (i_block, i_size); // since C++14
			#else
				(void)i_size;
				operator delete (i_block);
			#endif
        }

	private:
		
		static void * allocate_page_impl()
		{
			return operator new (page_size());
		}

		static void deallocate_page_impl(void * i_page) noexcept
		{
			#if __cplusplus >= 201402L
				operator delete (i_page, page_size()); // since C++14
			#else
				operator delete (i_page);
			#endif
		}

		#if DENSITY_DEBUG_INTERNAL
			class DbgData
			{
			public:

				void add_page(void * i_page)
				{
					std::lock_guard<std::mutex> lock(m_dbg_mutex);
					if (m_dbg_enable)
					{
						try
						{
							const bool inserted = m_dbg_pages.insert(i_page).second;
							DENSITY_ASSERT_INTERNAL(inserted);
						}
						catch (...)
						{
							m_dbg_large_blocks.clear();
							m_dbg_pages.clear();
							m_dbg_enable = false;
						}
					}
				}

				void remove_page(void * i_page) noexcept
				{
					std::lock_guard<std::mutex> lock(m_dbg_mutex);
					auto const removed = m_dbg_pages.erase(i_page);
					if (m_dbg_enable)
					{
						DENSITY_ASSERT_INTERNAL(removed == 1);
					}
				}

				void add_large_block(void * i_block, size_t i_size)
				{
					std::lock_guard<std::mutex> lock(m_dbg_mutex);
					if (m_dbg_enable)
					{
						try
						{
							const bool inserted = m_dbg_large_blocks.insert(std::make_pair(i_block, i_size)).second;
							DENSITY_ASSERT_INTERNAL(inserted);
						}
						catch (...)
						{
							m_dbg_large_blocks.clear();
							m_dbg_pages.clear();
							m_dbg_enable = false;
						}
					}
				}

				void remove_large_block(void * i_block, size_t i_size) noexcept
				{
					std::lock_guard<std::mutex> lock(m_dbg_mutex);
					if (m_dbg_enable)
					{
						auto const it = m_dbg_large_blocks.find(i_block);
						DENSITY_ASSERT_INTERNAL(it != m_dbg_large_blocks.end());
						DENSITY_ASSERT_INTERNAL(it->second == i_size);
					}
				}

				~DbgData()
				{
					DENSITY_ASSERT_INTERNAL(m_dbg_pages.size() == 0);
					DENSITY_ASSERT_INTERNAL(m_dbg_large_blocks.size() == 0);
				}

			private:
				std::mutex m_dbg_mutex;
				std::unordered_set<void*> m_dbg_pages;
				std::unordered_map<void*, size_t> m_dbg_large_blocks;
				bool m_dbg_enable = true;
			};
			std::shared_ptr<DbgData> m_dbg_data = std::make_shared<DbgData>();
		#endif

		class PageList
		{
		public:

			PageList() = default;
			PageList(const PageList &) = delete;
			PageList & operator = (const PageList &) = delete;

			~PageList()
			{
				auto curr = m_first;
				while (curr != nullptr)
				{
					auto next = curr->m_next;
					#if __cplusplus >= 201402L
						operator delete (curr, page_size()); // since C++14
					#else
						operator delete (curr);
					#endif
					curr = next;
				}
			}

			void push(void * i_page) noexcept
			{
				Header * page = static_cast<Header*>(i_page);
				page->m_next = m_first;
				m_first = page;
				m_size++;
			}

			void * peek() noexcept
			{
				auto const result = m_first;
				if (result != nullptr)
				{
					DENSITY_ASSERT_INTERNAL(m_size > 0);
					m_first = result->m_next;
					m_size--;
				}
				return result;
			}

			size_t size() const noexcept { return m_size; }

		private:
			struct Header
			{
				Header * m_next;
			};
			Header * m_first = nullptr;
			size_t m_size = 0;
		};

		static PageList & thread_page_store() noexcept
		{
			static thread_local PageList s_thread_page_store;
			return s_thread_page_store;
		}
    };

} // namespace density
