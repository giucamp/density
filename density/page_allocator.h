
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cstdlib>

namespace density
{
    class page_allocator
	{
    public:
        static const size_t page_size = 4096;
        static const size_t page_alignment = alignof(std::max_align_t);

        void * allocate_page()
        {
			return operator new (page_size);
        }

        void deallocate_page(void * i_page) noexcept
        {
			long bb = __cplusplus;
			#if __cplusplus >= 201402L
				operator delete (i_page, page_size); // since C++14
			#else
				operator delete (i_page);
			#endif
        }

        void * allocate_large_block(size_t i_size)
        {
			return operator new (i_size);
        }

        void deallocate_large_block(void * i_block, size_t i_size)
        {
			#if __cplusplus >= 201402L
				operator delete (i_block, i_size); // since C++14
			#else
				(void)i_size;
				operator delete (i_block);
			#endif
        }
    };

} // namespace density
