
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
    private:
        using Base = typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char>;

    public:
        static const size_t page_size = 4096;
        static const size_t page_alignment = alignof(std::max_align_t);

        void * allocate_page()
        {
            return this->allocate(page_size);
        }

        void deallocate_page(void * i_page)
        {
            return this->deallocate(static_cast<char*>(i_page), page_size);
        }

        void * allocate(size_t i_size)
        {
            return this->Base::allocate(i_size);
        }

        void deallocate(void * i_block, size_t i_size)
        {
            this->Base::deallocate(reinterpret_cast<char*>(i_block), i_size);
        }
    };

} // namespace density
