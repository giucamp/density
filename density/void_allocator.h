#pragma once
#include "density_common.h"

namespace density
{
	template <typename UNDERLYING_ALLOCATOR>
		class void_allocator_adapter : private std::allocator_traits<UNDERLYING_ALLOCATOR>::template rebind_alloc<char>
	{
	public:

		using UnderlyingAllocator = typename std::allocator_traits<UNDERLYING_ALLOCATOR>::template rebind_alloc<char>;

		static const size_t s_min_block_size = 1;
		static const size_t s_min_block_alignment = 1;

		void * allocate(size_t i_size, size_t i_alignment)
		{
			return AllocatorUtils::aligned_allocate(get_underlying_allocator(), i_size, i_alignment, 0);
		}

		void deallocate(void * i_block, size_t i_size, size_t /*i_alignment*/)
		{
			AllocatorUtils::aligned_deallocate(get_underlying_allocator(), i_block, i_size);
		}		

		UnderlyingAllocator & get_underlying_allocator()
		{
			return *this;
		}

		const UnderlyingAllocator & get_underlying_allocator() const
		{
			return *this;
		}
	};

	using void_allocator = void_allocator_adapter<std::allocator<int>>;

} // namespace density
