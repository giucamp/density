
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
	   /*! \page UntypedAllocator_concept UntypedAllocator concept
        The UntypedAllocator concept encapsulates an untyped memory allocation service, similar to the 
			<a href="http://en.cppreference.com/w/cpp/concept/Allocator">Allocator concept</a> defined by the standard, but untyped like
			<a href="http://en.cppreference.com/w/cpp/memory/c/malloc">std::malloc</a>. UntypedAllocator supports over-alignment and alignment
			offset. UntypedAllocator is also similar to the class <a href="http://en.cppreference.com/w/cpp/memory/memory_resource">
			std::pmr::memory_resource</a> (introduced in C++17), but is not polymorphic.
			
		<table>
		<caption id="multi_row">UntypedAllocator Requirements</caption>
		<tr><th style="width:500px">Requirement                      <th>Semantic
		<tr><td>Non-static member function: @code void * allocate(
				size_t i_size,
				size_t i_alignment = alignof(std::max_align_t),
				size_t i_alignment_offset = 0); @endcode </td></tr>
			<td>Allocates a memory block large at least \em i_size bytes. The address at the offset \em i_alignment_offset
        from the beginning of the block is aligned at least to \em i_alignment. On failure this function should throw a
		<a href="http://en.cppreference.com/w/cpp/memory/new/bad_alloc">std::bad_alloc</a>. \n
		The return value is a pointer to the first byte of the memory block. The content of the block is undefined. \n
		If \em i_alignment_offset is not zero, in general the allocator has no requirements on the alignment of the first byte of the block.
		Anyway, since the address at \em i_alignment_offset will have \em n low-order zero bits (where <em>i_alignment = 1 << n</em>), if 
		\em i_alignment_offset has \em m low-order zero bits (that is aligned to <em>1 << m</em>), then the address of the first byte 
		of the block is aligned at least to <em>1 << min(n,m)</em>.
				
		The user is responsible to ensure that:
		- \em i_alignment must be greater than zero and an integer power of 2
		- \em i_alignment_offset must be less than or equal to \em i_size

		otherwise the behavior may be undefined.
		</td></tr>

		<tr><td>Non-static member function: @code void deallocate(
				void * i_block,
				size_t i_size,
				size_t i_alignment = alignof(std::max_align_t),
				size_t i_alignment_offset = 0) noexcept; @endcode </td></tr>
		<td>Deallocates a memory block. \n

		The user is responsible to ensure that:
		- \em i_block is a block allocated by the same allocator, or by an allocator that compares equal to it.
		- \em i_size, \em i_alignment and \em i_alignment_offset are the same used when the block was allocated.
		
		otherwise the behavior may be undefined.
		<tr><td>Operators == and !=</td></tr>
		<td>Checks for equality\inequality.
		</td></tr>
		<tr><td>Default constructor and non-throwing destructor</td></tr>
		<td>A default constructed allocator is usable to allocate and deallocate memory block. The destructor must be no-except.
		</td></tr>
		<tr><td>Copy constructor and copy assignment</td></tr>
		<td>A copy-constructed allocator compares equal to the source of the copy. As a consequence, a block allocated by an allocator can be deallocated by an 
			allocator that was copy-constructed from it. Same for assignment.
		</td></tr>
		<tr><td>Non-throwing move constructor and non-throwing move assignment</td></tr>
		<td>A move-constructed allocator may not compare equal to the source of the move. After a move construction or assignment, the source of the move cannot be used to allocate or deallocate any block.
		As a consequence, after an allocator A is used as source for a move construction or assignment to an allocator B, any block allocated
		by A must be deallocated by B. </td></tr>
		</table>
		
		void_allocator models the UntypedAllocator concept.
    */


		  
	/*! \page PagedAllocator_concept PagedAllocator concept
        The PagedAllocator concept encapsulates a page-based untyped memory allocation service. All pages allocated by a 
		PagedAllocator have the same size and alignment requirements.

		<table>
		<caption id="multi_row">PagedAllocator Requirements</caption>
		<tr><th style="width:500px">Requirement                      <th>Semantic

		<tr><td>Static member function: @code static size_t page_size() noexcept; @endcode </td></tr>
		<td>Returns the size of a page in bytes, . In the same program execution this function must return the same value on every invocation. </td> </tr>

		<tr><td>Static member function: @code static size_t page_alignment() noexcept; @endcode </td></tr>
		<td>Returns the minimum alignment of a page in bytes, that is always greater than zero and an integer power of 2. In the same program execution this function must return the same value on every invocation. </td> </tr>

		<tr><td>Non-static member function: @code void * allocate_page(); @endcode </td></tr>
			<td>Allocates a memory page large at least \em page_size() bytes. The first byte of the page is aligned at 
			least to \em page_alignment(). On failure this function should throw a 
			<a href="http://en.cppreference.com/w/cpp/memory/new/bad_alloc">std::bad_alloc</a>. \n
		The return value is a pointer to the first byte of the memory page. The content of the page is undefined. \n
		</td></tr>

		<tr><td>Non-static member function: @code void deallocate_page(void * i_page) noexcept; @endcode </td></tr>
		<td>Deallocates a memory page. </td></tr>

		<tr><td>Operators == and !=</td></tr>
		<td>Checks for equality\inequality.
		</td></tr>
		<tr><td>Default constructor and non-throwing destructor</td></tr>
		<td>A default constructed allocator is usable to allocate and deallocate memory pages. The destructor must be no-except.
		</td></tr>
		<tr><td>Copy constructor and copy assignment</td></tr>
		<td>A copy-constructed allocator compares equal to the source of the copy. As a consequence, a pages allocated by an allocator can be deallocated by an 
			allocator that was copy-constructed from it. Same for assignment.
		</td></tr>
		<tr><td>Non-throwing move constructor and non-throwing move assignment</td></tr>
		<td>A move-constructed allocator may not compare equal to the source of the move. After a move construction or assignment, the source of the move cannot be used to allocate or deallocate any page.
		As a consequence, after an allocator A is used as source for a move construction or assignment to an allocator B, any page allocated
		by A must be deallocated by B. </td></tr>
		</table>
		
		void_allocator models the PagedAllocator concept.
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
            if (i_alignment <= alignof(std::max_align_t) && i_alignment_offset == 0)
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

        /** Deallocates a memory block. After the call accessing the memory block results in undefined behavior.
            @param i_block block to deallocate, or nullptr.
            @param i_size size of the block to deallocate, in bytes.
            @param i_alignment alignment of the memory block.

            \pre i_block is a memory block allocated with any instance of void_allocator, or nullptr
            \pre i_size and i_alignment are the same specified when allocating the block

            \exception never throws

            If i_block is nullptr, the call has no effect. */
        void deallocate(void * i_block, size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0) noexcept
        {
            DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));

            #if DENSITY_DEBUG_INTERNAL
                dbg_data().remove_block(i_block, i_size, i_alignment);
            #endif
            if (i_alignment <= alignof(std::max_align_t) && i_alignment_offset == 0)
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
