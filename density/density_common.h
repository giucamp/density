
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <assert.h>
#include <type_traits>
#include <ostream>
#include <limits>
#include <cstddef>

#define DENSITY_DEBUG		0

#define DENSITY_UNUSED(var)		(void)var;

#define DENSITY_POINTER_OVERFLOW_SAFE		1

#if DENSITY_POINTER_OVERFLOW_SAFE
	#define DENSITY_OVERFLOW_IF(bool_expr)			::density::detail::handle_pointer_overflow((bool_expr))
#else
	#define DENSITY_OVERFLOW_IF(bool_expr)			
#endif

#if DENSITY_DEBUG
	#define DENSITY_ASSERT(bool_expr)		assert((bool_expr))		
#else
	#define DENSITY_ASSERT(bool_expr)
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below
	#define DENSITY_CONSTEXPR
	#define DENSITY_NOEXCEPT
	#define DENSITY_NOEXCEPT_IF(value)
	#define DENSITY_ASSERT_NOEXCEPT(expr)
#else
	#define DENSITY_CONSTEXPR					constexpr
	#define DENSITY_NOEXCEPT						noexcept
	#define DENSITY_NOEXCEPT_IF(value)			noexcept(value)
	#define DENSITY_ASSERT_NOEXCEPT(expr)		static_assert(noexcept(expr), "The expression " #expr " is required not be noexcept");
#endif

#ifdef _MSC_VER
	#define DENSITY_NO_INLINE					__declspec(noinline)
	#define DENSITY_STRONG_INLINE				__forceinline
#else
	#define DENSITY_NO_INLINE
	#define DENSITY_STRONG_INLINE
#endif

namespace density
{
	class Overflow : public std::exception
	{
	public:
		using std::exception::exception;
	};

	namespace detail
	{
		template <typename TYPE> struct RemoveRefsAndConst
		{
			using type = typename std::remove_const<typename std::remove_reference<TYPE>::type>::type;
		};

		inline void handle_pointer_overflow(bool i_overflow)
		{
			if (i_overflow)
			{
				throw Overflow("pointer overflow");
			}
		}

		// size_max: avoid including <algorithm> just to use std::max<size_t>
		DENSITY_CONSTEXPR inline size_t size_max(size_t i_first, size_t i_second) DENSITY_NOEXCEPT
		{
			return i_first > i_second ? i_first : i_second;
		}
	}

				// address functions

	/** Returns true whether the given unsigned integer number is a power of 2 (1, 2, 4, 8, ...)
		@param i_number must be > 0, otherwise the behavior is undefined */
	inline bool is_power_of_2(size_t i_number) DENSITY_NOEXCEPT
	{
		DENSITY_ASSERT(i_number > 0);
		return (i_number & (i_number - 1)) == 0;
	}

	/** Returns true whether the given address has the specified alignment
		@param i_address address to be checked
		@i_alignment must be > 0 and a power of 2 */
	inline bool is_address_aligned(const void * i_address, size_t i_alignment) DENSITY_NOEXCEPT
	{
		DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));
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
		DENSITY_ASSERT( uint_pointer >= i_offset );
		return reinterpret_cast< void * >( uint_pointer - i_offset );
	}
	inline const void * address_sub( const void * i_address, size_t i_offset ) DENSITY_NOEXCEPT
	{
		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );
		DENSITY_ASSERT( uint_pointer >= i_offset );
		return reinterpret_cast< void * >( uint_pointer - i_offset );
	}

	/** Computes the unsigned difference between two pointers. The first must be above or equal to the second.
		@param i_end_address first address
		@param i_start_address second address
		@return i_end_address minus i_start_address	*/
	inline uintptr_t address_diff( const void * i_end_address, const void * i_start_address ) DENSITY_NOEXCEPT
	{
		DENSITY_ASSERT( i_end_address >= i_start_address );

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
		DENSITY_ASSERT( i_alignment > 0 && is_power_of_2( i_alignment ) );

		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

		const size_t mask = i_alignment - 1;

		return reinterpret_cast< void * >( uint_pointer & ~mask );
	}
	inline const void * address_lower_align( const void * i_address, size_t i_alignment ) DENSITY_NOEXCEPT
	{
		DENSITY_ASSERT( i_alignment > 0 && is_power_of_2( i_alignment ) );

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
		DENSITY_ASSERT( i_alignment > 0 && is_power_of_2( i_alignment ) );

		const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

		const size_t mask = i_alignment - 1;

		return reinterpret_cast< void * >( ( uint_pointer + mask ) & ~mask );
	}
	inline const void * address_upper_align( const void * i_address, size_t i_alignment ) DENSITY_NOEXCEPT
	{
		DENSITY_ASSERT( i_alignment > 0 && is_power_of_2( i_alignment ) );

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
		DENSITY_ASSERT(is_power_of_2(i_alignment));

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
			@param i_block block to free (returned by aligned_alloc). If it's not a valid block the behaviour is undefined.
			@param i_size size of the block to free, in bytes. Must be the same passed to aligned_alloc, otherwise the behaviour is undefined.
			@param i_alignment alignment of the memory block. Must be the same passed to aligned_alloc, otherwise the behaviour is undefined. */
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
				size_t const extra_size = detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
				size_t const actual_size = i_size + extra_size;

				detail::AlignmentHeader * header = static_cast<detail::AlignmentHeader*>(i_block) - 1;

				typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(i_allocator);
				char_alloc.deallocate(static_cast<char*>(header->m_block), actual_size);
			}
		}
	}


	class AllocatorUtils
	{
	public:

		template <typename CHAR_ALLOCATOR>
			static void * aligned_allocate(CHAR_ALLOCATOR & i_char_allocator, size_t i_size, size_t i_alignment, size_t i_alignment_offset )
		{
			static_assert(std::is_same<typename CHAR_ALLOCATOR::value_type, char>::value, "The valuetype of the allocator must be char");

			DENSITY_ASSERT(is_power_of_2(i_alignment));

			size_t const extra_size = detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
			size_t const actual_size = i_size + extra_size;

			void * const complete_block = i_char_allocator.allocate(actual_size);

			void * const user_block = address_lower_align(address_add(complete_block, extra_size), i_alignment, i_alignment_offset);
			detail::AlignmentHeader & header = *(static_cast<detail::AlignmentHeader*>(user_block) - 1);
			header.m_block = complete_block;

			return user_block;
		}

		template <typename CHAR_ALLOCATOR>
			static void aligned_deallocate(CHAR_ALLOCATOR & i_char_allocator, void * i_block, size_t i_size ) DENSITY_NOEXCEPT
		{
			static_assert(std::is_same<typename CHAR_ALLOCATOR::value_type, char>::value, "The valuetype of the allocator must be char");

			if (i_block != nullptr)
			{
				detail::AlignmentHeader * header = static_cast<detail::AlignmentHeader*>(i_block) - 1;

				const size_t complete_size = address_diff(address_add(i_block, i_size), header->m_block);

				i_char_allocator.deallocate(static_cast<char*>(header->m_block), complete_size);
			}
		}
	};

	/** Finds the aligned storage for a block with the specified size and alignment, such that it is
			>= *io_top_pointer, and sets *io_top_pointer to the end of the block. The actual pointed memory is not read\written.
		@param io_top_pointer pointer to the current address, which is incremented to make space for the new block. After
			the function exits, *io_top_pointer will point to the first address after the new block.
		@param i_size size (in bytes) of the block to allocate
		@param i_alignment alignment (in bytes) of the block to allocate (must be a positive power of 2).
		@return address of the new block. */
	inline void * linear_alloc(void * * io_top_pointer, size_t i_size, size_t i_alignment)
	{
		DENSITY_ASSERT(is_power_of_2(i_alignment));

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

		
	template <typename UINT>
		class BasicMemSize
	{
	private:
		UINT m_value;

	public:

		static_assert( std::is_integral<UINT>::value && std::is_unsigned<UINT>::value, "UINT must be an unsigned integer" );

		BasicMemSize() DENSITY_NOEXCEPT : m_value(0) { }

		explicit BasicMemSize(UINT i_value) DENSITY_NOEXCEPT : m_value(i_value) { }

		bool operator == (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value == i_source.m_value; }
		bool operator != (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value != i_source.m_value; }
		bool operator > (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value > i_source.m_value; }
		bool operator >= (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value >= i_source.m_value; }
		bool operator < (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value < i_source.m_value; }
		bool operator <= (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value <= i_source.m_value; }

				// arithmetic operations
		
		BasicMemSize operator + (const BasicMemSize & i_source) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			const UINT result = m_value + i_source.m_value;
			DENSITY_OVERFLOW_IF(result < m_value);
			return BasicMemSize(result);
		}

		BasicMemSize operator - (const BasicMemSize & i_source) const	DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{ 
			DENSITY_OVERFLOW_IF(m_value < i_source.m_value);
			return BasicMemSize(m_value - i_source.m_value);
		}

		BasicMemSize operator * (UINT i_source) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			/* see http://stackoverflow.com/questions/1815367/multiplication-of-large-numbers-how-to-catch-overflow 
				using the approch of umull_overflow5, as most times the operands will be small. */
			const auto max_op = ( static_cast<UINT>(1) << (std::numeric_limits<UINT>::digits / 2) ) - 1;
			const auto max_uint = std::numeric_limits<UINT>::max();
			DENSITY_OVERFLOW_IF( ( m_value >= max_op || i_source >= max_op) &&
				i_source != 0 && max_uint / i_source < m_value );
			const UINT result = static_cast<UINT>(m_value * i_source);
			return BasicMemSize<UINT>(result);
		}

		BasicMemSize operator / (UINT i_source) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			DENSITY_ASSERT(i_source != 0);
			DENSITY_OVERFLOW_IF( (m_value % i_source) != 0);
			return BasicMemSize(m_value / i_source);
		}


				// compound assignment

		BasicMemSize operator += (const BasicMemSize & i_source) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			const UINT result = m_value + i_source.m_value;
			DENSITY_OVERFLOW_IF(result < m_value);
			m_value = result;
			return *this;
		}
		BasicMemSize operator -= (const BasicMemSize & i_source) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			DENSITY_OVERFLOW_IF(m_value < i_source.m_value);
			m_value -= i_source.m_value; 
			return *this;
		}
		BasicMemSize operator *= (UINT i_source) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			/* see http://stackoverflow.com/questions/1815367/multiplication-of-large-numbers-how-to-catch-overflow
				using the approch of umull_overflow5, as most times the operands will be small. */
			const auto max_op = (static_cast<UINT>(1) << (std::numeric_limits<UINT>::digits / 2)) - 1;
			const auto max_uint = std::numeric_limits<UINT>::max();
			DENSITY_OVERFLOW_IF((m_value >= max_op || i_source >= max_op) &&
				i_source != 0 && max_uint / i_source < m_value);
			m_value *= i_source;
			return *this;
		}

		BasicMemSize operator /= (UINT i_source) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			DENSITY_ASSERT(i_source != 0);
			DENSITY_OVERFLOW_IF((m_value % i_source) != 0);
			m_value /= i_source;
			return *this; 
		}

		UINT value() const DENSITY_NOEXCEPT { return m_value; }

		bool is_valid_alignment() const DENSITY_NOEXCEPT
		{			
			return m_value > 0 && (m_value & (m_value - 1)) == 0;
		}
	};


	using MemSize = BasicMemSize<size_t>;

	template <typename TYPE, typename UINT_REPRESENTATION>
		class BasicArithmeticPointer;

	template <typename UINT_REPRESENTATION>
		class BasicArithmeticPointer<void, UINT_REPRESENTATION>
	{
	private:
		UINT_REPRESENTATION m_value;

	public:

		BasicArithmeticPointer() DENSITY_NOEXCEPT
			: m_value(0) {}

		explicit BasicArithmeticPointer(nullptr_t) DENSITY_NOEXCEPT
			: m_value(0) {}

		explicit BasicArithmeticPointer(void * i_value) DENSITY_NOEXCEPT_IF((std::is_same<UINT_REPRESENTATION, uintptr_t>::value))
			: m_value(reinterpret_cast<UINT_REPRESENTATION>(i_value))
		{
			auto const is_safe = std::is_same<UINT_REPRESENTATION, uintptr_t>::value;
			if (!is_safe)
			{
				DENSITY_OVERFLOW_IF(reinterpret_cast<void*>(m_value) != i_value);
			}			
		}

		BasicArithmeticPointer & operator = (nullptr_t) DENSITY_NOEXCEPT
		{
			m_value = 0;
			return *this;
		}

		bool operator == (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value == i_source.m_value; }
		bool operator != (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value != i_source.m_value; }
		bool operator > (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value > i_source.m_value; }
		bool operator >= (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value >= i_source.m_value; }
		bool operator < (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value < i_source.m_value; }
		bool operator <= (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value <= i_source.m_value; }

		BasicArithmeticPointer operator + (BasicMemSize<UINT_REPRESENTATION> i_value) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			auto result = *this;
			result += i_value;
			return result;
		}

		BasicArithmeticPointer operator - (BasicMemSize<UINT_REPRESENTATION> i_value) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			auto result = *this;
			result -= i_value;
			return result;
		}

		BasicArithmeticPointer & operator += (BasicMemSize<UINT_REPRESENTATION> i_value) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			auto const result = m_value + i_value.value();
			DENSITY_OVERFLOW_IF(result < m_value);
			m_value = result;
			return *this;
		}

		BasicArithmeticPointer & operator -= (BasicMemSize<UINT_REPRESENTATION> i_value) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			auto const result = m_value - i_value.value();
			DENSITY_OVERFLOW_IF(result > m_value);
			m_value = result;
			return *this;
		}

		BasicMemSize<UINT_REPRESENTATION> operator - (BasicArithmeticPointer i_pointer) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			DENSITY_OVERFLOW_IF(m_value < i_pointer.m_value);
			return BasicMemSize<UINT_REPRESENTATION>(m_value - i_pointer.m_value);
		}

		void * value() const DENSITY_NOEXCEPT
		{
			return reinterpret_cast<void*>(m_value);
		}

		bool is_null() const DENSITY_NOEXCEPT
		{
			return m_value == 0;
		}

		BasicArithmeticPointer lower_align(BasicMemSize<UINT_REPRESENTATION> i_alignment) const DENSITY_NOEXCEPT
		{
			DENSITY_ASSERT(i_alignment.is_valid_alignment());
			
			auto const alignment_mask = i_alignment.value() - 1;

			return BasicArithmeticPointer(reinterpret_cast<void*>(
				m_value & (~alignment_mask) ) );
		}

		BasicArithmeticPointer upper_align(BasicMemSize<UINT_REPRESENTATION> i_alignment) const DENSITY_NOEXCEPT(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			DENSITY_ASSERT(i_alignment.is_valid_alignment());

			auto const alignment_mask = i_alignment.value() - 1;

			return BasicArithmeticPointer(reinterpret_cast<void*>(
				(m_value + alignment_mask) & (~alignment_mask)));
		}			

		BasicArithmeticPointer linear_alloc(
			BasicMemSize<UINT_REPRESENTATION> i_size,
			BasicMemSize<UINT_REPRESENTATION> i_alignment) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			DENSITY_ASSERT(i_alignment.is_valid_alignment());

			auto const result = upper_align(i_alignment);
			*this = result + i_size;
			return result;
		}

		BasicArithmeticPointer linear_alloc(
			BasicMemSize<UINT_REPRESENTATION> i_size,
			BasicMemSize<UINT_REPRESENTATION> i_alignment,
			BasicArithmeticPointer i_end_address) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
		{
			DENSITY_ASSERT(i_alignment.is_valid_alignment() && m_value <= i_end_address.m_value );
			
			BasicArithmeticPointer result = upper_align(i_alignment);
			BasicArithmeticPointer new_ptr = result;
			new_ptr += i_size;
			if (new_ptr <= i_end_address)
			{
				m_value = new_ptr.m_value;
			}
			else
			{
				result.m_value = nullptr;
			}
			return result;
		}
	};

	template <typename TYPE>
		using ArithmeticPointer = BasicArithmeticPointer<TYPE, uintptr_t>;

	template <typename UINT>
		std::ostream & operator << (std::ostream & i_dest, const BasicMemSize<UINT> & i_source)
	{
		const char * suffixes[] = { " KiB", " MiB", " GiB", " TiB" };
		const char * suffixes_p[] = { " KiB(+", " MiB(+", " GiB(+", " TiB(+" };
		const char * suffixes_n[] = { " KiB(-", " MiB(-", " GiB(-", " TiB(-" };
		const double mults[] = { 1024., 1024.*1024., 1024.*1024.*1024., 1024.*1024.*1024.*1024. };

		unsigned prefix_index = 0;
		UINT value = i_source.value();
		while ((value >> 9) != 0 && prefix_index < 3)
		{
			value >>= 10;
			prefix_index++;
		}

		if (prefix_index == 0)
		{
			i_dest << value << "B";
		}
		else
		{
			prefix_index--;
			double d_val = round(i_source.value() / mults[prefix_index] * 100.) / 100.;
			const UINT as_uint = static_cast<UINT>( d_val * mults[prefix_index] );
			if (as_uint == i_source.value())
			{
				i_dest << d_val << suffixes[prefix_index];
			}
			else if (as_uint < i_source.value())
			{
				i_dest << d_val << suffixes_p[prefix_index] << i_source.value() - as_uint << ')';
			}
			else
			{
				i_dest << d_val << suffixes_n[prefix_index] << as_uint - i_source.value() << ')';
			}
		}
		return i_dest;
	}

	class MemStats
	{
	public:

		/** Total memory size requested to the allocator. This is similar to the capacity of an std::vector (except that it is expressed
			in bytes rather than in element count). */
		const MemSize & reserved_capacity() const	{ return m_reserved_capacity; }

		/** Total memory size used to store the elements and the required overhead (like the space for types types) and padding (usualy to respect
			the alignment). The used size is always less than or equal to the reserved_capacity, and it is similar to the size of a vector
			(except that it is expressed in bytes rather than in element count). Adding new elements to the container makes the used size
			increase. If the new used size would exceed the reserved capacity, a reallocation will occur. */
		const MemSize & used_size() const			{ return m_used_size; }
		
		/** Total space used for overhead (headers, footers, types). This size is a part of the used size. */
		const MemSize & overhead() const			{ return m_overhead; }
		
		/** Total space wasted to respect the alignment of elements and overhead data. */
		const MemSize & padding() const				{ return m_padding; }

		MemStats(const MemSize & i_reserved_capacity,
			const MemSize & i_used_size,
			const MemSize & i_overhead,
			const MemSize & i_padding)
			: m_reserved_capacity(i_reserved_capacity), m_used_size(i_used_size), m_overhead(i_overhead), m_padding(i_padding)
		{
		}

		MemStats & operator += (const MemStats & i_source)
		{
			m_reserved_capacity += i_source.m_reserved_capacity;
			m_used_size += i_source.m_used_size;
			m_overhead += i_source.m_overhead;
			m_padding += i_source.m_padding;
			return *this;
		}

		MemStats & operator + (const MemStats & i_source) const
		{
			MemStats result = *this;
			result += i_source;
			return result;
		}

	private:
		MemSize m_reserved_capacity;
		MemSize m_used_size;
		MemSize m_overhead;
		MemSize m_padding;
	};

} // namespace density
