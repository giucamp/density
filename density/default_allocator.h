
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/detail/page_allocator.h>
#include <density/detail/system_page_manager.h>

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
        <tr><th style="width:500px">Requirement                      </th><th>Semantic</th></tr>
        <tr><td>Non-static member function: @code void * allocate(
                size_t i_size,
                size_t i_alignment,
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
                size_t i_alignment,
                size_t i_alignment_offset = 0) noexcept; @endcode </td></tr>
        <td>Deallocates a memory block. \n

        The user is responsible to ensure that:
        - \em i_block is a block allocated by the same allocator, or by an allocator that compares equal to it.
        - \em i_size, \em i_alignment and \em i_alignment_offset are the same used when the block was allocated.

        otherwise the behavior may be undefined.
        <tr>
            <td>Operators == and !=</td>
            <td>Checks for equality\\inequality.</td>
        </tr>
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

        basic_default_allocator satisfies the requirements of UntypedAllocator concept.
    */


    /*! \page PagedAllocator_concept PagedAllocator concept
        The PagedAllocator concept encapsulates a page-based untyped memory allocation service. All pages allocated by a
        PagedAllocator have the same fixed size and alignment requirements.

        <table>
        <caption id="multi_row">PagedAllocator Requirements</caption>
        <tr><th style="width:700px">Requirement</th><th>Semantic</th></tr>

        <tr><td>Static constexpr member variable: @code static constexpr size_t page_size; @endcode </td></tr>
        <td>Specifies the size of a page in bytes, that is always less than or equal to the alignment.\n Note: there is no
            constraint on the page size. Accessing memory past the end of a page causes undefined behavior. </td> </tr>

        <tr><td>Static constexpr member variable: @code static constexpr size_t page_alignment; @endcode </td></tr>
        <td>Specifies the minimum alignment of a page in bytes, that is always greater than zero and an integer power of 2. </td> </tr>

        <tr><td>Non-static member function: @code void * allocate_page(); @endcode </td></tr>
            <td>Allocates a memory page large at least \em page_size bytes. The first byte of the page is aligned at
            least to \em page_alignment. On failure this function should throw a
            <a href="http://en.cppreference.com/w/cpp/memory/new/bad_alloc">std::bad_alloc</a>. \n
        The return value is a pointer to the first byte of the memory page. The content of the page is undefined. \n
        </td></tr>

        <tr><td>Non-static noexcept member function: @code void * try_allocate_page(progress_guarantee i_progress_guarantee); @endcode </td></tr>
            <td>Tries to allocate a memory page large at least \em page_size bytes with the specified progression guarantee. The first byte of the
            page is aligned at least to \em page_alignment. The return value is a pointer to the first byte of the memory page. The content of the
            page is undefined. Returns null on failure.
        </td></tr>

        <tr><td>Non-static member function: @code void * allocate_page_zeroed(); @endcode </td></tr>
            <td>Allocates a memory page large at least \em page_size bytes. The first byte of the page is aligned at
            least to \em page_alignment. On failure this function should throw a
            <a href="http://en.cppreference.com/w/cpp/memory/new/bad_alloc">std::bad_alloc</a>. \n
        The return value is a pointer to the first byte of the memory page. The content of the page is zeroed. \n
        </td></tr>

        <tr><td>Non-static noexcept member function: @code void * try_allocate_page_zeroed(progress_guarantee i_progress_guarantee); @endcode </td></tr>
            <td>Tries to allocate a memory page large at least \em page_size bytes with the specified progression guarantee. The first byte of the
            page is aligned at least to \em page_alignment. The return value is a pointer to the first byte of the memory page. The content of the
            page is zeroed. Returns null on failure.
        </td></tr>

        <tr><td>Non-static noexcept member function: @code void deallocate_page(void * i_page); @endcode </td></tr>
        <td>Deallocates the memory page containing the provided address. This function must be wait-free. </td></tr>

        <tr><td>Non-static noexcept member function: @code void deallocate_page_zeroed(void * i_page); @endcode </td></tr>
        <td>Deallocates the memory page containing the provided address, assuming that as soon it is not pinned its content is zeroed. This function must be wait-free. </td></tr>

        <tr><td>Non-static noexcept member function: @code void pin_page(void * i_page); @endcode </td></tr>
        <td>Pins the memory page containing the provided address, preventing it from being altered or recycled. The page be already deallocated, in which case the caller will unpin it immediately. This function must be lock-free. </td></tr>

        <tr><td>Non-static noexcept member function: @code void unpin_page(void * i_page); @endcode </td></tr>
        <td>Unpins the memory page containing the provided address, that must have previously been pinned, stopping the effect of the pin. This function must be lock-free. </td></tr>

        <tr><td>Operators == and !=</td></tr>
        <td>Checks for equality\\inequality.
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

        basic_default_allocator satisfies the requirements of PagedAllocator.
    */

    template <size_t PAGE_CAPACITY_AND_ALIGNMENT> class basic_default_allocator;

    /** Specialization of basic_default_allocator that uses density::default_page_capacity as page capacity. */
    using default_allocator = basic_default_allocator<default_page_capacity>;

    /** Class template providing paged and legacy memory allocation. It models both the \ref UntypedAllocator_concept "UntypedAllocator"
        and \ref PagedAllocator_concept "PagedAllocator" concepts.

        basic_default_allocator is stateless, so instances are interchangeable: blocks and pages can be deallocated by any instance of basic_default_allocator. */
    template <size_t PAGE_CAPACITY_AND_ALIGNMENT = default_page_capacity>
    class basic_default_allocator
    {
      private:
        using PageAllocator =
          detail::PageAllocator<typename detail::SystemPageManager<PAGE_CAPACITY_AND_ALIGNMENT>>;

      public:
        /** Usable size (in bytes) of memory pages. */
        static constexpr size_t page_size = PageAllocator::page_size;

        /** Alignment (in bytes) of memory pages. */
        static constexpr size_t page_alignment = PageAllocator::page_alignment;

        /** Allocates a legacy memory block with the specified size and alignment.
                @param i_size size of the requested memory block, in bytes
                @param i_alignment alignment of the requested memory block, in bytes
                @param i_alignment_offset offset of the block to be aligned, in bytes. The alignment is guaranteed only i_alignment_offset
                    bytes from the beginning of the block.
                @return address of the new memory block, always != nullptr

            \pre The behavior is undefined if either:
                - i_alignment is zero or it is not an integer power of 2
                - i_size is not a multiple of i_alignment
                - i_alignment_offset is greater than i_size

            \n <b>Progress guarantee</b>: the same of the built-in operator new, usually blocking
            \n <b>Throws</b>: std::bad_alloc on failure

            The content of the newly allocated block is undefined. */
        void * allocate(size_t i_size, size_t i_alignment, size_t i_alignment_offset = 0)
        {
            return density::aligned_allocate(i_size, i_alignment, i_alignment_offset);
        }

        /** Tries to allocates a legacy memory block with the specified size and alignment.
                @param i_size size of the requested memory block, in bytes
                @param i_alignment alignment of the requested memory block, in bytes
                @param i_alignment_offset offset of the block to be aligned, in bytes. The alignment is guaranteed only i_alignment_offset
                    bytes from the beginning of the block.
                @return address of the new memory block, or nullptr in case of failure

            \pre The behavior is undefined if either:
                - i_alignment is zero or it is not an integer power of 2
                - i_size is not a multiple of i_alignment
                - i_alignment_offset is greater than i_size

            \n <b>Progress guarantee</b>: the same of the built-in operator new, usually blocking
            \n <b>Throws</b>: nothing

            The content of the newly allocated block is undefined. */
        void *
          try_allocate(size_t i_size, size_t i_alignment, size_t i_alignment_offset = 0) noexcept
        {
            return density::try_aligned_allocate(i_size, i_alignment, i_alignment_offset);
        }

        /** Deallocates a legacy memory block. After the call any access to the memory block results in undefined behavior.
                @param i_block block to deallocate, or nullptr.
                @param i_size size of the block to deallocate, in bytes.
                @param i_alignment alignment of the memory block.
                @param i_alignment_offset offset of the alignment


            \pre The behavior is undefined if either:
                - i_block is not a memory block allocated by the function allocate
                - i_size, i_alignment and i_alignment_offset are not the same specified when the block was allocated

            \n <b>Progress guarantee</b>: the same of the built-in operator delete, usually blocking
            \n <b>Throws</b>: nothing

            If i_block is nullptr, the call has no effect. */
        void deallocate(
          void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset = 0) noexcept
        {
            density::aligned_deallocate(i_block, i_size, i_alignment, i_alignment_offset);
        }

        /** Allocates a memory page.
            @return address of the new memory page, always != nullptr

            \n <b>Progress guarantee</b>: blocking
            \n <b>Throws</b>: std::bad_alloc on failure.

            The content of the newly allocated page is undefined. */
        void * allocate_page()
        {
            auto const new_page =
              PageAllocator::thread_local_instance()
                .template try_allocate_page<detail::page_allocation_type::uninitialized>(
                  progress_blocking);
            if (new_page == nullptr)
                throw std::bad_alloc();
            return new_page;
        }

        /** Tries to allocates a memory page.
            @return address of the new memory page, or nullptr if the allocation fails

            \n <b>Progress guarantee</b>: specified by the argument
            \n <b>Throws</b>: nothing

            The content of the newly allocated page is undefined. */
        void * try_allocate_page(progress_guarantee i_progress_guarantee) noexcept
        {
            return PageAllocator::thread_local_instance()
              .template try_allocate_page<detail::page_allocation_type::uninitialized>(
                i_progress_guarantee);
        }

        /** Allocates a memory page.
            @return address of the new memory page, always != nullptr

            \n <b>Progress guarantee</b>: blocking
            \n <b>Throws</b>: std::bad_alloc on failure.

            The content of the newly allocated page is zeroed. */
        void * allocate_page_zeroed()
        {
            auto const new_page =
              PageAllocator::thread_local_instance()
                .template try_allocate_page<detail::page_allocation_type::zeroed>(
                  progress_blocking);
            if (new_page == nullptr)
                throw std::bad_alloc();
            return new_page;
        }

        /** Tries to allocates a memory page.
            @return address of the new memory page, or nullptr if the allocation fails

            \n <b>Progress guarantee</b>: specified by the argument
            \n <b>Throws</b>: nothing

            The content of the newly allocated page is zeroed. */
        void * try_allocate_page_zeroed(progress_guarantee i_progress_guarantee) noexcept
        {
            return PageAllocator::thread_local_instance()
              .template try_allocate_page<detail::page_allocation_type::zeroed>(
                i_progress_guarantee);
        }

        /** Deallocates a memory page. If the page is still pinned by some threads, it is not altered or recycled
                by the allocator until it is unpinned.
            @param i_page pointer to a byte within the page to deallocate. Can't be nullptr.

            \pre The behavior is undefined if either:
                - i_page is not a page allocated by allocate_page, try_allocate_page, allocate_page_zeroed or try_allocate_page_zeroed

            \n <b>Progress guarantee</b>: wait free
            \n <b>Throws</b>: nothing */
        void deallocate_page(void * i_page) noexcept
        {
            DENSITY_ASSERT(i_page != nullptr);
            PageAllocator::thread_local_instance()
              .template deallocate_page<detail::page_allocation_type::uninitialized>(i_page);
        }

        /** Deallocates a memory page. If the page is still pinned by some threads, it is not altered or recycled
                by the allocator until it is unpinned.
            If the page is not pinned, it must be zeroed. Otherwise it must be zeroed when the last pin is removed.
            @param i_page pointer to a byte within the page to deallocate. Can't be nullptr.

            \pre The behavior is undefined if either:
                - i_page is not a page allocated by allocate_page, try_allocate_page, allocate_page_zeroed or try_allocate_page_zeroed
                - when the last pin is removed the page is not completely zeroed

            \n <b>Progress guarantee</b>: wait free
            \n <b>Throws</b>: nothing */
        void deallocate_page_zeroed(void * i_page) noexcept
        {
            PageAllocator::thread_local_instance()
              .template deallocate_page<detail::page_allocation_type::zeroed>(i_page);
        }

        /** Reserves the specified memory size from the system for lock-free page allocation.
            @param i_size the space (in bytes) that the internal page allocator should reserve for page allocation.
            @param o_reserved_size pointer to a size_t that receives the actual space (in bytes) that the allocator has
                allocated from the system, always greater or equal to i_size. This parameter can be null, in which case
                the actual reserved size is not returned.

            The internal page allocator requests memory regions from the system and uses them to allocate pages with a
            lock-free algorithm. Regions are returned to the system only during the destruction of global objects. \n
            This function ensures that the sum of the capacity available in the all the regions is at least the specified
            size. If a new region is necessary in order to reach the specified capacity, but the allocation from the system
            fails, this function throw a std::bad_alloc. \n
            Note: some of this space may be already allocated as pages.

            \n <b>Progress guarantee</b>: blocking
            \n <b>Throws</b>: std::bad_alloc on failure. */
        static void reserve_lockfree_page_memory(size_t i_size, size_t * o_reserved_size = nullptr)
        {
            if (!try_reserve_lockfree_page_memory(progress_blocking, i_size, o_reserved_size))
            {
                throw std::bad_alloc();
            }
        }

        /** Tries to reserve the specified memory size from the system for lock-free page allocation.
            @param i_progress_guarantee minimum progress guarantee of this call. If it is not progress_blocking,
                no region is allocated
            @param i_size the space (in bytes) that the internal page allocator should reserve for page allocation.
            @param o_reserved_size pointer to a size_t that receives the actual space (in bytes) that the allocator has
                allocated from the system, always greater or equal to i_size. This parameter can be null, in which case
                the actual reserved size is not returned
            @return whether the requested size is less than or equal to the actual reserved space

            The internal page allocator requests memory regions from the system and uses them to allocate pages. Regions
            are returned to the system only during the destruction of global objects. \n
            This function verifies that the sum of memory available in the regions is at least the specified size.
            Note: some of this space may be already allocated as pages.

            \n <b>Progress guarantee</b>: specified by the argument
            \n <b>Throws</b>: nothing. */
        static bool try_reserve_lockfree_page_memory(
          progress_guarantee i_progress_guarantee,
          size_t             i_size,
          size_t *           o_reserved_size = nullptr) noexcept
        {
            auto const reserved_size =
              PageAllocator::thread_local_instance().try_reserve_lockfree_memory(
                i_progress_guarantee, i_size);
            if (o_reserved_size != nullptr)
                *o_reserved_size = reserved_size;
            return reserved_size >= i_size;
        }

        /** Pins the page containing the specified address, incrementing an internal page_specific ref-count.

            @param i_page pointer to a byte within the page to deallocate. Can't be nullptr.

            If the page has been already deallocated no undefined behavior occurs: the caller should detect this case
            and unpin the page immediately. Using a deallocated-then-pinned page in any other way other than unpinning
            (including accessing its content) causes undefined behavior. \n
            If the page is still allocated then the pin ensures that, while the page is pinned:
                - the content of the page is not altered by the allocator
                - the page is not returned by a call to allocate_page, try_allocate_page, allocate_page_zeroed or try_allocate_page_zeroed.
                - if the page gets deallocated with a call to allocate_page_zeroed or try_allocate_page_zeroed, the memory
                    may be still not zeroed.

            Every call to pin_page should be matched by a call to unpin_page by the same thread. A thread may pin the same page
            multiple times, provided that it unpins the page the same number of times.


            \pre The behavior is undefined if either:
                - the page containing i_page was never returned by allocate_page, try_allocate_page, allocate_page_zeroed or try_allocate_page_zeroed

            \n <b>Progress guarantee</b>: lock-free
            \n <b>Throws</b>: nothing. */
        void pin_page(void * i_page) noexcept { PageAllocator::pin_page(i_page); }

        /** Removes a pin from the page, decrementing the internal ref-count.

            \pre The behavior is undefined if either:
                - the page containing i_page was never returned by allocate_page, try_allocate_page, allocate_page_zeroed or try_allocate_page_zeroed
                - the page was not previously pinned by this thread

            \n <b>Progress guarantee</b>: lock-free
            \n <b>Throws</b>: nothing. */
        void unpin_page(void * i_address) noexcept { PageAllocator::unpin_page(i_address); }

        /** Tries to pin the page containing the specified address, incrementing an internal page_specific ref-count,
            If the implementation can't complete the action with the specified progress guarantee, the call has no visible effects,
            and the return value is false. Otherwise the return value is true..

            \n <b>Progress guarantee</b>: specified by the argument
            \n <b>Throws</b>: nothing. */
        bool try_pin_page(progress_guarantee i_progress_guarantee, void * i_address) noexcept
        {
            return PageAllocator::try_pin_page(i_progress_guarantee, i_address);
        }

        /** Removes a pin from the page, decrementing the internal ref-count.

            \pre The behavior is undefined if either:
                - the page containing i_page was never returned by allocate_page, try_allocate_page, allocate_page_zeroed or try_allocate_page_zeroed
                - the page was not previously pinned by this thread

            \n <b>Progress guarantee</b>: specified by the argument
            \n <b>Throws</b>: nothing. */
        void unpin_page(progress_guarantee i_progress_guarantee, void * i_address) noexcept
        {
            return PageAllocator::unpin_page(i_progress_guarantee, i_address);
        }

        /** Returns the number of times the specified page has been pinned by any thread. This function is useful only for diagnostic or debugging.

            \pre The behavior is undefined if either:
                - the page containing i_page was never returned by allocate_page, try_allocate_page, allocate_page_zeroed or try_allocate_page_zeroed

            \n <b>Progress guarantee</b>: wait-free
            \n <b>Throws</b>: nothing. */
        uintptr_t get_pin_count(const void * i_address) noexcept
        {
            return PageAllocator::get_pin_count(i_address);
        }

        /** Returns whether the right-side allocator can be used to deallocate block and pages allocated by this allocator.
            @return always true. */
        bool operator==(const basic_default_allocator &) const noexcept { return true; }

        /** Returns whether the right-side allocator cannot be used to deallocate block and pages allocated by this allocator.
            @return always false. */
        bool operator!=(const basic_default_allocator &) const noexcept { return false; }
    };

} // namespace density
