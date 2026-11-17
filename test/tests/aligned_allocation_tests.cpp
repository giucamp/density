//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2026.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"

//

#include <cstring>
#include <density/default_allocator.h>

namespace density_tests
{
    namespace
    {
        /* Allocates, checks the alignment at the offset, writes the whole block (with the address sanitizer an
           overflow or a corrupted header is reported here, or by the deallocation), and deallocates. */
        void aligned_allocate_test(size_t i_size, size_t i_alignment, size_t i_alignment_offset)
        {
            using namespace density;

            void * block = aligned_allocate(i_size, i_alignment, i_alignment_offset);
            DENSITY_TEST_ASSERT(address_is_aligned(address_add(block, i_alignment_offset), i_alignment));
            std::memset(block, 33, i_size);
            aligned_deallocate(block, i_size, i_alignment, i_alignment_offset);

            void * try_block = try_aligned_allocate(i_size, i_alignment, i_alignment_offset);
            if (try_block != nullptr)
            {
                DENSITY_TEST_ASSERT(
                  address_is_aligned(address_add(try_block, i_alignment_offset), i_alignment));
                std::memset(try_block, 33, i_size);
                aligned_deallocate(try_block, i_size, i_alignment, i_alignment_offset);
            }
        }
    } // namespace

    void aligned_allocation_tests()
    {
        aligned_allocate_test(4, 4, 0);
        aligned_allocate_test(128, 128, 0);
        aligned_allocate_test(128, 128, 8);

        /* An alignment offset smaller than the header of the block (a pointer): the header was written before the
           beginning of the block (heap corruption), or at a misaligned address. The sizes are multiples of the
           alignment (a precondition of aligned_allocate). */
        aligned_allocate_test(16, 8, 4);
        aligned_allocate_test(48, 16, 4);
        aligned_allocate_test(16, 8, 1);
        for (size_t offset = 0; offset <= 64; offset++)
        {
            aligned_allocate_test(65, 1, offset);
            aligned_allocate_test(66, 2, offset);
            aligned_allocate_test(96, 32, offset);
        }
    }

} // namespace density_tests
