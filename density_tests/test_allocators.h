
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../density/void_allocator.h"
#include "../testity/testity_common.h"
#include "../testity/test_allocator.h"

namespace density_tests
{
    using namespace testity;
	using namespace density;

	/* This class models both the modeling both the \ref UntypedAllocator_concept "UntypedAllocator"
		and \ref PagedAllocator_concept "PagedAllocator" concepts. 
		It has a state, and uses a SharedBlockRegistry to detect invalid deallocations and leaks. */
    class TestVoidAllocator
    {
    public:

        void * allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0)
        {
            auto block = m_underlying_allocator.allocate(i_size, i_alignment, i_alignment_offset);
			m_registry.add_block(block, i_size, i_alignment);
            return block;
        }

        void deallocate(void * i_block, size_t i_size, size_t i_alignment = alignof(std::max_align_t)) noexcept
        {
			m_registry.remove_block(i_block, i_size, i_alignment);
            m_underlying_allocator.deallocate(i_block, i_size, i_alignment);
        }

        static size_t page_size() noexcept { return void_allocator::page_size(); }

        static size_t page_alignment() noexcept { return void_allocator::page_alignment(); }

        static const size_t free_page_cache_size = void_allocator::free_page_cache_size;

        void * allocate_page()
        {
            auto page = m_underlying_allocator.allocate_page();
			m_registry.add_block(page, page_size(), page_alignment());
            return page;
        }

        void deallocate_page(void * i_page) noexcept
        {
			m_registry.remove_block(i_page, page_size(), page_alignment());
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
		SharedBlockRegistry m_registry;
		void_allocator m_underlying_allocator;
    };

} // density_tests

