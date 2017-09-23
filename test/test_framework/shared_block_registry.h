
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_test_common.h"
#include <memory>
#include <atomic>

namespace density_tests
{
    /** This class can be used to record the allocations performed by an allocator, so that mismatching
        deallocations are detected and reported by a failure of a DENSITY_TEST_ASSERT.
        Every SharedBlockRegistry acts like a std::shared_ptr to a shared state. When a SharedBlockRegistry
        is copied, the copy will share the same registry with the source. When the shared state is destroyed,
        if any allocation has been added but not removed, a DENSITY_TEST_ASSERT fails.

        SharedBlockRegistry is internally thread safe. For a correct use in a concurrent context:
            - unregister_block should be called before the actual deallocation is performed.
            - when doing a reallocation, first unregister the block with unregister_block, the perform the reallocation,
                and then register the new block with register_block. */
    class SharedBlockRegistry
    {
    public:

        SharedBlockRegistry();

        SharedBlockRegistry(const SharedBlockRegistry &);

        SharedBlockRegistry(SharedBlockRegistry &&) noexcept;

        /** Destructor. If this instance is the only one pointing to the shared registry, it must be empty, and it is destroyed. */
        ~SharedBlockRegistry();

        SharedBlockRegistry & operator = (const SharedBlockRegistry &);

        SharedBlockRegistry & operator = (SharedBlockRegistry &&) noexcept;

        /** Registers a block.
                @param i_category user-defined category of the block.
                @param i_block start address of the block.
                @param i_size size in bytes of the block.
                @param i_alignment alignment in bytes of the block. It may be zero, or an integer power of 2.
                @param i_alignment_offset offset in bytes from the beginning of the block where the alignment is satisfied. */
        void register_block(int i_category, void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset);

        /** Unregisters a block. If any of the parameters does not match exactly the value passed to a register_block,
            a DENSITY_TEST_ASSERT fails.
                @param i_category user-defined category of the block.
                @param i_block start address of the block.
                @param i_size size in bytes of the block.
                @param i_alignment alignment in bytes of the block. It may be zero, or an integer power of 2.
                @param i_alignment_offset offset in bytes from the beginning of the block where the alignment is satisfied. */
        void unregister_block(int i_category, void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset);

        /** Returns whether two instances share the same registry. */
        bool operator == (const SharedBlockRegistry & i_other) const
        {
            return m_data == i_other.m_data;
        }

        /** Returns whether two instances don't share the same registry. */
        bool operator != (const SharedBlockRegistry & i_other) const
        {
            return m_data != i_other.m_data;
        }

    private:
        struct AllocationEntry;
        struct Data;

    private:
        std::shared_ptr<Data> m_data;
    };

} // namespace density_tests

