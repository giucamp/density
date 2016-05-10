
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#pragma once
#include <memory>
#include <cstddef>

namespace density
{
    template <typename ALLOCATOR>
        class page_allocator : private std::allocator_traits<ALLOCATOR>::template rebind_alloc<char>
    {
    public:
        static const size_t page_size = 4096;
        static const size_t page_alignment = alignof(std::max_align_t);

        void * alloc_page()
        {
            return ALLOCATOR::allocate(page_size);
        }

        void free_page(void * i_page)
        {
            ALLOCATOR::deallocate(i_page);
        }
    };

} // namespace density
