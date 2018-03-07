
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_test_common.h"
#include <density/density_common.h>
#include <vector>

namespace density_tests
{

    template <typename TYPE> class aligned_allocator
    {
      public:
        using value_type = TYPE;

        aligned_allocator() noexcept = default;

        template <typename OTHER_TYPE>
        aligned_allocator(const aligned_allocator<OTHER_TYPE> &) noexcept
        {
        }

        TYPE * allocate(std::size_t i_count)
        {
            return static_cast<TYPE *>(
              density::aligned_allocate(i_count * sizeof(TYPE), alignof(TYPE)));
        }

        void deallocate(TYPE * i_block, std::size_t i_count) noexcept
        {
            return density::aligned_deallocate(i_block, i_count * sizeof(TYPE), alignof(TYPE));
        }
    };

    template <typename TYPE_1, typename TYPE_2>
    inline bool
      operator==(const aligned_allocator<TYPE_1> &, const aligned_allocator<TYPE_2> &) noexcept
    {
        return true;
    }

    template <typename TYPE_1, typename TYPE_2>
    inline bool
      operator!=(const aligned_allocator<TYPE_1> &, const aligned_allocator<TYPE_2> &) noexcept
    {
        return false;
    }

    template <typename TYPE> using aligned_vector = std::vector<TYPE, aligned_allocator<TYPE>>;

} // namespace density_tests
