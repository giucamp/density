
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cstdlib> // for std::max_align_t
#include <density/density_common.h>
#include <density/detail/system_page_manager.h>
#include <density/detail/page_manager.h>

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

        basic_void_allocator meets the requirements of UntypedAllocator concept.
    */



    /*! \page PagedAllocator_concept PagedAllocator concept
        The PagedAllocator concept encapsulates a page-based untyped memory allocation service. All pages allocated by a
        PagedAllocator have the same size and alignment requirements.

        <table>
        <caption id="multi_row">PagedAllocator Requirements</caption>
        <tr><th style="width:500px">Requirement                      </th><th>Semantic</th></tr>

        <tr><td>Static constexpr member variable: @code static size_t page_size; @endcode </td></tr>
        <td>Specifies the size of a page in bytes, that is always less than or equal to the alignment.\n Note: there is no
			constraint on the page size. Accessing memory past the end of a page is undefined behaviour. </td> </tr>

        <tr><td>Static constexpr member variable: @code static const size_t page_alignment; @endcode </td></tr>
        <td>Specifies the minimum alignment of a page in bytes, that is always greater than zero and an integer power of 2. </td> </tr>

        <tr><td>Non-static member function: @code void * allocate_page(); @endcode </td></tr>
            <td>Allocates a memory page large at least \em page_size bytes. The first byte of the page is aligned at
            least to \em page_alignment. On failure this function should throw a
            <a href="http://en.cppreference.com/w/cpp/memory/new/bad_alloc">std::bad_alloc</a>. \n
        The return value is a pointer to the first byte of the memory page. The content of the page is undefined. \n
        </td></tr>

        <tr><td>Non-static member function: @code void deallocate_page(void * i_page) noexcept; @endcode </td></tr>
        <td>Deallocates a memory page. </td></tr>

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

        basic_void_allocator meets the requirements of PagedAllocator.
    */

	template <size_t PAGE_CAPACITY_AND_ALIGNMENT>
		class basic_void_allocator;

	/** Specialization of basic_void_allocator that uses the default page capacity. */
	using void_allocator = basic_void_allocator<default_page_capacity>;

    /** This class encapsulates an untyped memory allocation service, modeling both the \ref UntypedAllocator_concept "UntypedAllocator"
        and \ref PagedAllocator_concept "PagedAllocator" concepts.

        basic_void_allocator is stateless. Any instance of basic_void_allocator compares equal to any instance of basic_void_allocator. This implies that
        blocks and pages can be deallocated by any instance of basic_void_allocator.

        \section Implementation
        basic_void_allocator redirects block allocations to the language built-in operator new. Whenever the requested alignment
        is greater than alignof(std::max_align_t), basic_void_allocator allocates an overhead whose size is the maximum between
        the requested alignment and sizeof(void*). \n

        Every thread is associated to a free-page cache. When a page is deallocated, if the cache has less than 4 pages,
        the page to deallocate is pushed in this cache. When a page allocation is requested, a page from the cache is returned (if any).
        Pushing/peeking to\from the cache is very fast, and does not require thread synchronization. \n
        Note: on win32 basic_void_allocator is not redirecting page allocations to VirtualAlloc, since from several tests the latter resulted
        around ten times slower than the operator new.
    */
	template <size_t PAGE_CAPACITY_AND_ALIGNMENT = default_page_capacity>
		class basic_void_allocator
    {
		using page_manager = typename detail::page_manager<typename detail::system_page_manager<PAGE_CAPACITY_AND_ALIGNMENT>>;
    
	public:

        /** Size (in bytes) of a memory page. */
        static constexpr size_t page_size = page_manager::page_size;

        /** Alignment (in bytes) of a memory page. */
        static constexpr size_t page_alignment = page_manager::page_alignment;

        basic_void_allocator() noexcept = default;
        basic_void_allocator(const basic_void_allocator&) noexcept = default;
        basic_void_allocator(basic_void_allocator&&) noexcept = default;
        basic_void_allocator & operator = (const basic_void_allocator&) noexcept = default;
        basic_void_allocator & operator = (basic_void_allocator&&) noexcept = default;

		/** Allocates a memory block with at least the specified size and alignment.
				@param i_size size of the requested memory block, in bytes
				@param i_alignment alignment of the requested memory block, in bytes
				@param i_alignment_offset offset of the block to be aligned, in bytes. The alignment is guaranteed only at i_alignment_offset
					from the beginning of the block.
				@return address of the new memory block

			\pre i_alignment is > 0 and is an integer power of 2
			\pre i_alignment_offset is <= i_size

			A violation of any precondition results in undefined behavior.

			\exception std::bad_alloc if the allocation fails

			The content of the newly allocated block is undefined. */
		void * allocate(size_t i_size,
			size_t i_alignment = alignof(std::max_align_t),
			size_t i_alignment_offset = 0) noexcept
		{
			return density::aligned_allocate(i_size, i_alignment, i_alignment_offset);
		}

		/** Deallocates a memory block. After the call accessing the memory block results in undefined behavior.
				@param i_block block to deallocate, or nullptr.
				@param i_size size of the block to deallocate, in bytes.
				@param i_alignment alignment of the memory block.
				@param i_alignment_offset

			\pre i_block is a memory block allocated with any instance of basic_void_allocator, or nullptr
			\pre i_size and i_alignment are the same specified when allocating the block

			\exception never throws

			If i_block is nullptr, the call has no effect. */
		void deallocate(void * i_block,
			size_t i_size, 
			size_t i_alignment = alignof(std::max_align_t), 
			size_t i_alignment_offset = 0) noexcept
		{
			density::aligned_deallocate(i_block, i_size, i_alignment, i_alignment_offset);
		}

        /** Allocates a memory page.
            @return address of the new memory page

            \exception std::bad_alloc if the allocation fails

            All the pages have the same size and alignment requirement (see page_size and page_alignment).
            The content of the newly allocated page is undefined. */
        void * allocate_page()
        {
            return page_manager::allocate_page<detail::page_allocation_type::uninitialized>();
        }

		void * allocate_page_zeroed()
		{
			return page_manager::allocate_page<detail::page_allocation_type::zeroed>();
		}

        /** Deallocates a memory page. After the call accessing the page results in undefined behavior.
            @param i_page page to deallocate. Cannot be nullptr.

            \pre i_page is a memory page allocated with any instance of basic_void_allocator

            \exception never throws */
        void deallocate_page(void * i_page) noexcept
        {
			page_manager::deallocate_page<detail::page_allocation_type::uninitialized>(i_page);
        }

		// the page may be not still zeroed, if it is pinned
		void deallocate_page_zeroed(void * i_page) noexcept
		{
			page_manager::deallocate_page<detail::page_allocation_type::zeroed>(i_page);
		}

		/** Pins the page containing the specified address.
			The owner page is obtained from the address as address_lower_align(i_address, page_alignment).
			If the owner page is currently allocated, the return value is a non-empty scoped_pin.
			If the owner page was allocated from this allocator instance, but was the deallocated, the
			behaviour is implementation defined (no undefined behaviour): the scoped_pin may be empty or not.
			The user is supposded to detect such cases in some way and discard the returned scoped_pin.
			
			While a page has a scoped_pin asdsociated with it, if the page gets deallocated, the allocator
			will not alter its content in any way, and will not allocate a page in the samer address. */
		void pin_page(void * i_address) noexcept
		{
			page_manager::pin_page(i_address);
		}

		/** @return the number of pins before the modification */
		void unpin_page(void * i_address) noexcept
		{
			page_manager::unpin_page(i_address);
		}

		uintptr_t get_pin_count(const void * i_address) noexcept
		{
			return page_manager::get_pin_count(i_address);
		}
		
        /** Returns whether the right-side allocator can be used to deallocate block and pages allocated by this allocator.
            @return always true. */
        bool operator == (const basic_void_allocator &) const noexcept
            { return true; }

        /** Returns whether the right-side allocator cannot be used to deallocate block and pages allocated by this allocator.
            @return always false. */
        bool operator != (const basic_void_allocator &) const noexcept
            { return false; }

        /** Allocates and constructs an object. The alignment of the object is always respected.
                @param i_construction_params argument list to be forwarded to the constructor of the object.
                @return a pointer to the new object

            Objects created by new_object must be deleted by delete_object. Using the language builtin delete on an object
            created by new_object causes undefined behavior. Since all basic_void_allocator objects compares equal, an instance of
            basic_void_allocator can delete an object created by another instance.
        */
        template <typename TYPE, typename... CONSTRUCTION_PARAMS>
            TYPE * new_object(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            return new(allocate(sizeof(TYPE), alignof(TYPE))) TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
        }

        /** Deletes an object created with new_object*/
        template <typename TYPE>
            void delete_object(TYPE * i_object) noexcept
        {
            if (i_object != nullptr)
            {
                i_object->~TYPE();
                deallocate(i_object, sizeof(TYPE), alignof(TYPE));
            }
        }
    };

} // namespace density
