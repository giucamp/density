
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/void_allocator.h>
#include <testity/testity_common.h>
#include <testity/test_allocator.h>

namespace density_tests
{
    using namespace testity;
    using namespace density;

    /* This class meets the requirements of both \ref UntypedAllocator_concept "UntypedAllocator"
        and \ref PagedAllocator_concept "PagedAllocator".
        It has a state, and uses a SharedBlockRegistry to detect invalid deallocations and leaks. */
    class TestVoidAllocator
    {
    public:

        void * allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0)
        {
            auto block = m_underlying_allocator.allocate(i_size, i_alignment, i_alignment_offset);
            m_registry.register_block(s_default_category, block, i_size, i_alignment, i_alignment_offset);
            return block;
        }

        void deallocate(void * i_block, size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0) noexcept
        {
            m_registry.unregister_block(s_default_category, i_block, i_size, i_alignment, i_alignment_offset);
            m_underlying_allocator.deallocate(i_block, i_size, i_alignment, i_alignment_offset);
        }

        static constexpr size_t page_size = void_allocator::page_size;

        static constexpr size_t page_alignment = void_allocator::page_alignment;

        static const size_t free_page_cache_size = void_allocator::free_page_cache_size;

        void * allocate_page()
        {
            auto page = m_underlying_allocator.allocate_page();
            m_registry.register_block(s_page_category, page, page_size, page_alignment, 0);
            return page;
        }

        void deallocate_page(void * i_page) noexcept
        {
            m_registry.unregister_block(s_page_category, i_page, page_size, page_alignment, 0);
            m_underlying_allocator.deallocate_page(i_page);
        }

        bool operator == (const TestVoidAllocator & i_other) const
        {
            return m_registry == i_other.m_registry;
        }

        bool operator != (const TestVoidAllocator & i_other) const
        {
            return m_registry != i_other.m_registry;
        }

    private:
		constexpr static int s_default_category = 2;
		constexpr static int s_page_category = 4;
        SharedBlockRegistry m_registry;
		void_allocator m_underlying_allocator;
    };

} // density_tests

