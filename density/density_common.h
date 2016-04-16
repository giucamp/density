
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <assert.h>

#if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below
	#define DENSITY_CONSTEXPR
	#define DENSITY_NOEXCEPT
	#define DENSITY_NOEXCEPT_V(value)
	#define DENSITY_ASSERT_NOEXCEPT(expr)
#else
	#define DENSITY_CONSTEXPR					constexpr
	#define DENSITY_NOEXCEPT						noexcept
	#define DENSITY_NOEXCEPT_V(value)			noexcept(value)
	#define DENSITY_ASSERT_NOEXCEPT(expr)		static_assert(noexcept(expr), "The expression " #expr " is required not be noexcept");
#endif

namespace density
{
				// address functions

	/** Returns true whether the given unsigned integer number is a power of 2 (1, 2, 4, 8, ...)
		@param i_number must be > 0, otherwise the behavior is undefined */
	inline bool is_power_of_2(size_t i_number) DENSITY_NOEXCEPT
	{
		assert(i_number > 0);
		return (i_number & (i_number - 1)) == 0;
	}

	/** Returns true whether the given address has the specified alignment
		@param i_address address to be checked
		@i_alignment must be > 0 and a power of 2 */
	inline bool is_address_aligned(const void * i_address, size_t i_alignment) DENSITY_NOEXCEPT
	{
		assert(i_alignment > 0 && is_power_of_2(i_alignment));
		return (reinterpret_cast<uintptr_t>(i_address) & (i_alignment - 1)) == 0;
	}

	/** Adds an offset to a pointer.
		@param i_address source address
		@param i_offset number to add to the address
		@return i_address plus i_offset */
	inline void * address_add( void * i_address, size_t i_offset ) DENSITY_NOEXCEPT
	{
		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );
		return reinterpret_cast< void * >( uint_pointer + i_offset );
	}
	inline const void * address_add( const void * i_address, size_t i_offset ) DENSITY_NOEXCEPT
	{
		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );
		return reinterpret_cast< void * >( uint_pointer + i_offset );
	}

	/** Subtracts an offset from a pointer
		@param i_address source address
		@param i_offset number to subtract from the address
		@return i_address minus i_offset */
	inline void * address_sub( void * i_address, size_t i_offset ) DENSITY_NOEXCEPT
	{
		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );
		assert( uint_pointer >= i_offset );
		return reinterpret_cast< void * >( uint_pointer - i_offset );
	}
	inline const void * address_sub( const void * i_address, size_t i_offset ) DENSITY_NOEXCEPT
	{
		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );
		assert( uint_pointer >= i_offset );
		return reinterpret_cast< void * >( uint_pointer - i_offset );
	}

	/** Computes the unsigned difference between two pointers. The first must be above or equal to the second.
		@param i_end_address first address
		@param i_start_address second address
		@return i_end_address minus i_start_address	*/
	inline uintptr_t address_diff( const void * i_end_address, const void * i_start_address ) DENSITY_NOEXCEPT
	{
		assert( i_end_address >= i_start_address );

		const uintptr_t end_uint_pointer = reinterpret_cast<uintptr_t>( i_end_address );
		const uintptr_t start_uint_pointer = reinterpret_cast<uintptr_t>( i_start_address );
		
		return end_uint_pointer - start_uint_pointer;
	}

	/** Returns the biggest aligned address lesser than or equal to a given address
		@param i_address address to be aligned
		@param i_alignment alignment required from the pointer. It must be an integer power of 2.
		@return the aligned address */
	inline void * address_lower_align( void * i_address, size_t i_alignment ) DENSITY_NOEXCEPT
	{
		assert( i_alignment > 0 && is_power_of_2( i_alignment ) );

		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

		const size_t mask = i_alignment - 1;

		return reinterpret_cast< void * >( uint_pointer & ~mask );
	}
	inline const void * address_lower_align( const void * i_address, size_t i_alignment ) DENSITY_NOEXCEPT
	{
		assert( i_alignment > 0 && is_power_of_2( i_alignment ) );

		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

		const size_t mask = i_alignment - 1;

		return reinterpret_cast< void * >( uint_pointer & ~mask );
	}

	/** Returns	the biggest address lesser than te first parameter, such that i_address + i_alignment_offset is aligned
		@param i_address address to be aligned
		@param i_alignment alignment required from the pointer. It must be an integer power of 2
		@param i_alignment_offset alignment offset
		@return the result address */
	inline void * address_lower_align( void * i_address, size_t i_alignment, size_t i_alignment_offset ) DENSITY_NOEXCEPT
	{
		void * address = address_add( i_address, i_alignment_offset );

		address = address_lower_align( address, i_alignment );

		address = address_sub( address, i_alignment_offset );
		
		return address;
	}
	inline const void * address_lower_align( const void * i_address, size_t i_alignment, size_t i_alignment_offset ) DENSITY_NOEXCEPT
	{
		const void * address = address_add( i_address, i_alignment_offset );

		address = address_lower_align( address, i_alignment );

		address = address_sub( address, i_alignment_offset );
		
		return address;
	}

	/** Returns the smallest aligned address greater than or equal to a given address
		@param i_address address to be aligned
		@param i_alignment alignment required from the pointer. It must be an integer power of 2.
		@return the aligned address */
	inline void * address_upper_align( void * i_address, size_t i_alignment ) DENSITY_NOEXCEPT
	{
		assert( i_alignment > 0 && is_power_of_2( i_alignment ) );

		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

		const size_t mask = i_alignment - 1;

		return reinterpret_cast< void * >( ( uint_pointer + mask ) & ~mask );
	}
	inline const void * address_upper_align( const void * i_address, size_t i_alignment ) DENSITY_NOEXCEPT
	{
		assert( i_alignment > 0 && is_power_of_2( i_alignment ) );

		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

		const size_t mask = i_alignment - 1;

		return reinterpret_cast< void * >( ( uint_pointer + mask ) & ~mask );
	}

	/** Returns	the smallest address greater than the first parameter, such that i_address + i_alignment_offset is aligned
		@param i_address address to be aligned
		@param i_alignment alignment required from the pointer. It must be an integer power of 2
		@param i_alignment_offset alignment offset
		@return the result address */
	inline void * address_upper_align( void * i_address, size_t i_alignment, size_t i_alignment_offset ) DENSITY_NOEXCEPT
	{
		void * address = address_add( i_address, i_alignment_offset );

		address = address_upper_align( address, i_alignment );

		address = address_sub( address, i_alignment_offset );
		
		return address;
	}
	inline const void * address_upper_align( const void * i_address, size_t i_alignment, size_t i_alignment_offset ) DENSITY_NOEXCEPT
	{
		const void * address = address_add( i_address, i_alignment_offset );

		address = address_upper_align( address, i_alignment );

		address = address_sub( address, i_alignment_offset );
		
		return address;
	}

	/** Returns	wheter two memory ranges overlap */
	inline bool address_overlap( const void * i_first, size_t i_first_size, const void * i_second, size_t i_second_size ) DENSITY_NOEXCEPT
	{
		if( i_first < i_second )
			return address_add( i_first, i_first_size ) > i_second;
		else
			return address_add( i_second, i_second_size ) > i_first;
	}

	/** Returns true whether the given pair of pointers enclose a valid array of objects of the type. This function is intended to validate
			an input array.
		@param i_objects_start inclusive lower bound of the array
		@param i_objects_end exclusive upper bound of the array
		@return true if and only if all the following conditions are true:
			- i_objects_start <= i_objects_end
			- the difference (in bytes) between i_objects_end and i_objects_start is a multiple of the size of TYPE
			- both i_objects_start and i_objects_end respects the alignment for TYPE. */
	template <typename TYPE>
		inline bool is_valid_range(const TYPE * i_objects_start, const TYPE * i_objects_end) DENSITY_NOEXCEPT
	{
		if (i_objects_start > i_objects_end)
		{
			return false;
		}
		if( !is_address_aligned(i_objects_start, std::alignment_of<TYPE>::value) )
		{
			return false;
		}
		if( !is_address_aligned(i_objects_end, std::alignment_of<TYPE>::value))
		{
			return false;
		}
		const uintptr_t diff = reinterpret_cast<uintptr_t>(i_objects_end) -  reinterpret_cast<uintptr_t>(i_objects_start);
		if (diff % sizeof(TYPE) != 0)
		{
			return false;
		}
		return true;
	}

	namespace detail
	{
		struct AlignmentHeader
		{
			void * m_block;
		};
	}
	
	/** Allocates aligned memory using the provided allocator. This function just allocates (no constructors are called). Throws std::bad_alloc if allocation fails.
			@param i_allocator allocator to use
			@param i_size size of the requested memory block, in bytes
			@param i_alignment alignment of the requested memory block, in bytes. Must be >0 and a power of 2
			@param i_alignment_offset offset of the block to be aligned. The alignment is guaranteed only at i_alignment_offset
				from the beginning of the block. 
			@return address of the new memory block */
	template <typename ALLOCATOR>
		void * aligned_alloc(ALLOCATOR & i_allocator, size_t i_size, size_t i_alignment, size_t i_alignment_offset )
	{
		assert(is_power_of_2(i_alignment));

		if (i_alignment <= std::alignment_of<void*>::value)
		{
			typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<void *> other_alloc(i_allocator);
			return other_alloc.allocate((i_size + sizeof(void*) - 1) / sizeof(void*));
		}
		else
		{
			size_t const extra_size = (i_alignment >= sizeof(detail::AlignmentHeader) ? i_alignment : sizeof(detail::AlignmentHeader));
			size_t const actual_size = i_size + extra_size;

			typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(i_allocator);
			void * const complete_block = char_alloc.allocate(actual_size);

			void * const user_block = address_lower_align(address_add(complete_block, extra_size), i_alignment, i_alignment_offset);
			detail::AlignmentHeader & header = *(static_cast<detail::AlignmentHeader*>(user_block) - 1);
			header.m_block = complete_block;

			// done
			return user_block;
		}
	}

	/** Frees an address allocated with aligned_alloc. This function just deallocates (no destructors are called). It never throws.
			@param i_allocator allocator to use. Must be the same passed to aligned_alloc, otherwise the behaviour is undefined.
			@param i_block block to free (returned by aligned_alloc). If it's not a vlid block the behaviour is undefined.
			@param i_size size of the block to free, in bytes. Must be the same passed to aligned_alloc, otherwise the behaviour is undefined.
			@param i_alignment alignment of the memory block. Must be the same passed to aligned_alloc, otherwise the behaviour is undefined.
			@param i_alignment_offset offset of the alignment of the block. Must be the same passed to aligned_alloc, otherwise the behaviour is undefined. */
	template <typename ALLOCATOR>
		void aligned_free(ALLOCATOR & i_allocator, void * i_block, size_t i_size, size_t i_alignment ) DENSITY_NOEXCEPT
	{
		if (i_alignment <= std::alignment_of<void*>::value)
		{
			typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<void *> other_alloc(i_allocator);
			other_alloc.deallocate(static_cast<void**>(i_block), (i_size + sizeof(void*) - 1) / sizeof(void*));
		}
		else if (i_block != nullptr)
		{
			{
				size_t const extra_size = (i_alignment >= sizeof(detail::AlignmentHeader) ? i_alignment : sizeof(detail::AlignmentHeader));
				size_t const actual_size = i_size + extra_size;

				detail::AlignmentHeader * header = static_cast<detail::AlignmentHeader*>(i_block) - 1;

				typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(i_allocator);
				char_alloc.deallocate(static_cast<char*>(header->m_block), actual_size);
			}
		}
	}

	/** Finds the aligned placement for a block with the specified size and alignment, such that it is
			>= *io_top_pointer, and sets *io_top_pointer to the end of the block. The actual pointed memory is not read\written.
		@param io_top_pointer pointer to the current address, which is incremented to make space for the new block. After
			the function exits, *io_top_pointer will point to the first address after the new block.
		@param i_size size (in bytes) of the block to allocate
		@param i_alignment alignment (in bytes) of the block to allocate (must be a positive power of 2).
		@return address of the new block. */
	inline void * linear_alloc(void * * io_top_pointer, size_t i_size, size_t i_alignment)
	{
		assert(is_power_of_2(i_alignment));

		auto top = *io_top_pointer;
		auto new_block = top = address_upper_align(top, i_alignment);
		top = address_add(top, i_size);
		*io_top_pointer = top;
		return new_block;
	}

	/** Finds the aligned placement for a block with the size and alignment of the template parameter TYPE, , such that it is
			>= *io_top_pointer, and sets *io_top_pointer to the end of the block. The actual pointed memory is not read\written.
		@param io_top_pointer pointer to the current address, which is incremented to make space for the new block. After
			the function exits, *io_top_pointer will point to the first address after the new block.
		@param i_size size (in bytes) of the block to allocate
		@param i_alignment alignment (in bytes) of the block to allocate (must be a positive power of 2).
		@return address of the new block. */
	template <typename TYPE>
		inline TYPE * linear_alloc(void * * io_top_pointer)
	{
		return static_cast<TYPE *>(io_top_pointer, sizeof(TYPE), alignof(TYPE) );
	}

	template <typename BASE_CLASS, typename... TYPES>
		struct AllCovariant
	{
		static const bool value = true;
	};
	template <typename BASE_CLASS, typename FIRST_TYPE, typename... OTHER_TYPES>
		struct AllCovariant<BASE_CLASS, FIRST_TYPE, OTHER_TYPES...>
	{
		static const bool value = std::is_base_of<BASE_CLASS, FIRST_TYPE>::value &&
			AllCovariant<BASE_CLASS, OTHER_TYPES...>::value;
	};

} // namespace density
