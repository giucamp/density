
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../density/void_page_allocator.h"
#include "../testity/testing_utils.h"

namespace density
{
    namespace tests
    {
        using namespace testity;

        class TestVoidAllocator
        {
        public:

            void * allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0)
            {
                auto block = m_underlying_allocator.allocate(i_size, i_alignment, i_alignment_offset);
                TestAllocatorBase::notify_alloc(block, i_size, i_alignment);
                return block;
            }

            void deallocate(void * i_block, size_t i_size, size_t i_alignment = alignof(std::max_align_t)) noexcept
            {
                TestAllocatorBase::notify_deallocation(i_block, i_size, i_alignment);
                m_underlying_allocator.deallocate(i_block, i_size, i_alignment);
            }

            static size_t page_size() noexcept { return void_page_allocator::page_size(); }

            static size_t page_alignment() noexcept { return void_page_allocator::page_alignment(); }

            static const size_t free_page_cache_size = void_page_allocator::free_page_cache_size;

            void * allocate_page()
            {
                auto page = m_underlying_allocator.allocate_page();
                TestAllocatorBase::notify_alloc(page, page_size(), page_alignment());
                return page;
            }

            void deallocate_page(void * i_page) noexcept
            {
                TestAllocatorBase::notify_deallocation(i_page, page_size(), page_alignment());
                m_underlying_allocator.deallocate_page(i_page);
            }

        private:
            void_page_allocator m_underlying_allocator;
        };
    }
}
