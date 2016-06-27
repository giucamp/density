
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cstdlib> // for std::max_align_t
#include "density_common.h"
#if DENSITY_DEBUG_INTERNAL
	#include <mutex>
	#include <unordered_set>
	#include <unordered_map>
#endif

namespace density
{
	/*! \page VoidAllocator_concept VoidAllocator concept
		A VoidAllocator is a type that encapsulates an untyped memory allocation service, allowing:
		 - legacy block-based allocations. When the user requests a block, he specifies a size and an alignment. To deallocate
			the block the user must specify (together with the address of the block) the same size and alignment.
		 - fixed-size page allocations. Pages have all the same size and alignment guarantee. Usually allocating/deallocating a
			memory page is more efficient than allocating/deallocating a legacy memory block.
			
		To meet the requirements of VoidAllocator, a type must be comparable with the operators == and !=. The comparison cannot throw
		an exception.
		
		The VoidAllocator concept is unrelated to both the Allocator concept and the interface std::pmr::memory_resource (introduced in C++17).
		
		void_allocator models the VoidAllocator concept.

		\section Blocks Block based memory allocation
		A VoidAllocator must provide these member functions:
		@code
			void * allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0);
			void deallocate(void * i_block, size_t i_size, size_t i_alignment = alignof(std::max_align_t)) noexcept;
		@endcode

		The function allocate allocates a memory block with at least the specified size. The address at the offset i_alignment_offset
		from the beginning of the block is aligned at least to i_alignment. On failure allocate throws std::bad_alloc. \n
		The function deallocate deallocates a block. \n
		The user must guarantee that:
			- The size and alignment passed to deallocate a block must be the same used when the block was allocated.
			- A block is deallocated by the same allocator that allocated it, or by an allocator that compares equal to it.
		
		\section Pages Page based memory allocation
		A VoidAllocator must provide these member functions:
		@code
			static size_t page_size() noexcept;
			static size_t page_alignment() noexcept;
			void * allocate_page();
			void deallocate_page(void * i_page) noexcept;
		@endcode

		allocate_page() and deallocate_page() allocate and deallocate a single page. The size and alignment of the page are the
		values returned by page_size() and page_alignment(). In case of failure allocate_page throws std::bad_alloc. \n
		In the same program execution page_size() and page_alignment() must return the same value on every invocation.
		The user must guarantee that a page is deallocated by the same allocator that allocated it, or by an allocator that compares equal to it.
	*/

	/** This class encapsulates an untyped memory allocation service, modeling the \ref VoidAllocator_concept "VoidAllocator concept".
		
		void_allocator is stateless. Any instance of void_allocator compares equal to any instance of void_allocator. This implies that
		blocks and pages can be deallocated by any instance of void_allocator.

		\section Implementation
		void_allocator redirects block allocations to the language built-in operator new. Whenever the requested alignment 
		is greater than alignof(std::max_align_t), void_allocator allocates an overhead whose size is the maximum between
		alignof(std::max_align_t) and sizeof(void*). \n
		Every thread is associated to a free-page cache. When a page is deallocated, if the cache has less than 4 pages,
		the page to deallocate is pushed in this cache. When a page allocation is requested, a page from the cache is returned (if any).
		Pushing/peeking to\from the cache is very fast, and does not require thread synchronization. \n
		Note: on win32 void_allocator is not redirecting page allocations to VirtualAlloc, since from several tests the latter resulted
		around ten times slower than operator new.
	*/
    class void_allocator
	{
    public:
			
		/** Allocates a memory block with at least the specified size and alignment.
            @param i_size size of the requested memory block, in bytes
            @param i_alignment alignment of the requested memory block, in bytes. Must be >0 and a power of 2
            @param i_alignment_offset offset of the block to be aligned, in bytes. The alignment is guaranteed only at i_alignment_offset
                from the beginning of the block.
            @return address of the new memory block

			\pre i_alignment is > 0 and is an integer power of 2
			\pre i_alignment_offset is <= i_size

			\exception std::bad_alloc if the allocation fails
			
			The content of the newly allocated block is undefined.

			A failure to meet the preconditions result in undefined behavior
		*/
		void * allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0)
        {
			DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));
			DENSITY_ASSERT(i_alignment_offset <= i_size);
			
			// if this function is inlined, and i_alignment is constant, the allocator should simplify much of this function
			void * user_block;
			if (i_alignment <= alignof(std::max_align_t))
			{
				user_block = operator new (i_size);
			}
			else
			{
				// reserve an additional space in the block equal to the max(i_alignment, sizeof(AlignmentHeader) = sizeof(void*) )
				size_t const extra_size = detail::size_max(i_alignment, sizeof(AlignmentHeader));
				size_t const actual_size = i_size + extra_size;
				auto complete_block = operator new (actual_size);
				user_block = address_lower_align(address_add(complete_block, extra_size), i_alignment, i_alignment_offset);
				AlignmentHeader & header = *(static_cast<AlignmentHeader*>(user_block) - 1);
				header.m_block = complete_block;
			}
			#if DENSITY_DEBUG_INTERNAL
				dbg_data().add_block(user_block, i_size, i_alignment);
			#endif
			return user_block;
        }

		
		#if 0 // This function may be available in future versions of the library
			bool resize(void * i_block, size_t i_size, size_t i_alignment = alignof(std::max_align_t))
			{
				DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));

				#if DENSITY_DEBUG_INTERNAL
					dbg_data().check_block(i_block, i_size, i_alignment);
				#endif
				(void)(i_block, i_size, i_alignment);
				return false;
			}
		#endif	

		/** Deallocates a memory block. After the call accessing the memory block results in undefined behavior.
            @param i_block block to deallocate, or nullptr.
            @param i_size size of the block to deallocate, in bytes.
            @param i_alignment alignment of the memory block.
			
			\pre i_block is a memory block allocated with any instance of void_allocator, or nullptr
			\pre i_size and i_alignment are the same specified when allocating the block

			\exception never throws

			If i_block is nullptr, the call has no effect. */
        void deallocate(void * i_block, size_t i_size, size_t i_alignment = alignof(std::max_align_t)) noexcept
        {
			DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));

			#if DENSITY_DEBUG_INTERNAL
				dbg_data().remove_block(i_block, i_size, i_alignment);
			#endif
			if (i_alignment <= alignof(std::max_align_t))
			{
				#if __cplusplus >= 201402L
					operator delete (i_block, i_size); // since C++14
				#else
					(void)i_size;
					operator delete (i_block);
				#endif
			}
			else
			{
				if(i_block != nullptr)
				{
					const auto & header = *( static_cast<AlignmentHeader*>(i_block) - 1 );
					#if __cplusplus >= 201402L // since C++14
						size_t const extra_size = detail::size_max(i_alignment, sizeof(AlignmentHeader));
						size_t const actual_size = i_size + extra_size;
						operator delete (header.m_block, actual_size);
					#else
						(void)i_size;
						operator delete (header.m_block);
					#endif
				}
			}
        }

		/** Returns the size (in bytes) of a memory page. */
		static size_t page_size() noexcept { return 4096; }

		/** Returns the alignment (in bytes) of a memory page. */
		static size_t page_alignment() noexcept { return alignof(std::max_align_t); }

		/** Maximum number of free page that a thread can cache */
		static const size_t free_page_cache_size = 4;
		
		/** Allocates a memory page. 
            @return address of the new memory page

			\exception std::bad_alloc if the allocation fails
			
			All the pages have the same size and alignment requirement (see page_size and page_alignment).
			The content of the newly allocated page is undefined. */
        void * allocate_page()
        {
			auto page = thread_page_store().peek();
			if (page == nullptr)
			{
				page = allocate_page_impl();
			}			
			#if DENSITY_DEBUG_INTERNAL
				dbg_data().add_page(page);
			#endif
			return page;
        }

		/** Deallocates a memory page. After the call accessing the page results in undefined behavior.
            @param i_page page to deallocate. Cannot be nullptr.
            
			\pre i_page is a memory page allocated with any instance of void_allocator

			\exception never throws */
        void deallocate_page(void * i_page) noexcept
        {
			#if DENSITY_DEBUG_INTERNAL
				dbg_data().remove_page(i_page);
			#endif
			auto & page_store = thread_page_store();
			if (page_store.size() < free_page_cache_size)
			{
				page_store.push(i_page);
			}
			else
			{
				deallocate_page_impl(i_page);
			}
        }

		/** Returns whether the right-side allocator can be used to deallocate block and pages allocated by this allocator.
			@return always true. */
		bool operator == (const void_allocator &) const noexcept
			{ return true; }

		/** Returns whether the right-side allocator cannot be used to deallocate block and pages allocated by this allocator.
			@return always false. */
		bool operator != (const void_allocator &) const noexcept
			{ return false; }

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

		struct AlignmentHeader
		{
			void * m_block;
		};

		#if DENSITY_DEBUG_INTERNAL
			class DbgData
			{
			public:

				void add_page(void * i_page)
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					if (m_enable)
					{
						try
						{
							const bool inserted = m_pages.insert(i_page).second;
							DENSITY_ASSERT_INTERNAL(inserted);
						}
						catch (...)
						{
							m_blocks.clear();
							m_pages.clear();
							m_enable = false;
						}
					}
				}

				void remove_page(void * i_page) noexcept
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					auto const removed = m_pages.erase(i_page);
					if (m_enable)
					{
						DENSITY_ASSERT_INTERNAL(removed == 1);
					}
				}

				void add_block(void * i_block, size_t i_size, size_t i_alignment)
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					if (m_enable)
					{
						try
						{
							const bool inserted = m_blocks.insert(std::make_pair(i_block, BlockInfo{i_size, i_alignment})).second;
							DENSITY_ASSERT_INTERNAL(inserted);
						}
						catch (...)
						{
							m_blocks.clear();
							m_pages.clear();
							m_enable = false;
						}
					}
				}

				void remove_block(void * i_block, size_t i_size, size_t i_alignment) noexcept
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					if (m_enable)
					{
						auto const it = m_blocks.find(i_block);
						DENSITY_ASSERT_INTERNAL(it != m_blocks.end());
						DENSITY_ASSERT_INTERNAL(it->second.m_size == i_size && it->second.m_alignment == i_alignment);
						m_blocks.erase(it);
					}
				}

				void check_block(void * i_block, size_t i_size, size_t i_alignment) noexcept
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					if (m_enable)
					{
						auto const it = m_blocks.find(i_block);
						DENSITY_ASSERT_INTERNAL(it != m_blocks.end());
						DENSITY_ASSERT_INTERNAL(it->second.m_size == i_size && it->second.m_alignment == i_alignment);
					}
				}

				~DbgData()
				{
					DENSITY_ASSERT_INTERNAL(m_pages.size() == 0);
					DENSITY_ASSERT_INTERNAL(m_blocks.size() == 0);
				}

			private:

				struct BlockInfo
				{
					size_t m_size;
					size_t m_alignment;
				};

			private:
				std::mutex m_mutex;
				std::unordered_set<void*> m_pages;
				std::unordered_map<void*, BlockInfo> m_blocks;
				bool m_enable = true;
			};

			static DbgData & dbg_data()
			{
				static DbgData s_dbg_data;
				return s_dbg_data;
			}
		#endif // #if DENSITY_DEBUG_INTERNAL

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
