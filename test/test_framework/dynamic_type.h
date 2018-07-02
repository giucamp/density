
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_test_common.h"
#include "easy_random.h"
#include <density/density_common.h>
#include <limits>

namespace density_tests
{
    class DynamicType
    {
      public:
        static DynamicType make_random(EasyRandom & i_random)
        {
            auto const id        = i_random.get_int<size_t>();
            auto const alignment = size_t(1) << i_random.get_int<size_t>(0, 16);
            auto const size      = alignment * i_random.get_int<size_t>(1, 32);
            return DynamicType(id, size, alignment);
        }

        DynamicType(size_t i_id, size_t i_size, size_t i_alignment)
            : m_id(i_id), m_size(i_size), m_alignment(i_alignment)
        {
            DENSITY_TEST_ASSERT(
              i_alignment > 0 && density::is_power_of_2(i_alignment) && i_size >= i_alignment &&
              (i_size % i_alignment) == 0);
        }

        size_t size() const noexcept { return m_size; }

        size_t alignment() const noexcept { return m_alignment; }

        void * default_construct(void * i_dest) const
        {
            DENSITY_TEST_ASSERT(density::address_is_aligned(i_dest, m_alignment));
            memset(
              i_dest, static_cast<int>(m_id & std::numeric_limits<unsigned char>::max()), m_size);
            auto const result = to_base(i_dest);
            check_content(result);
            return result;
        }

        void * copy_construct(void * i_dest, const void * i_source) const
        {
            check_content(i_source);
            DENSITY_TEST_ASSERT(density::address_is_aligned(i_dest, m_alignment));
            memcpy(i_dest, from_base(i_source), m_size);
            auto const result = to_base(i_dest);
            check_content(result);
            return result;
        }

        void * move_construct(void * i_dest, void * i_source) const noexcept
        {
            return copy_construct(i_dest, i_source);
        }

        void * destroy(void * i_dest) const noexcept
        {
            check_content(i_dest);
            auto const start_address = from_base(i_dest);
            memset(start_address, 99, m_size);
            return start_address;
        }

        bool are_equal(const void * i_first, const void * i_second) const
        {
            check_content(i_first);
            check_content(i_second);
            return memcmp(from_base(i_first), from_base(i_second), m_size) == 0;
        }

        void * to_base(void * i_ptr) const noexcept
        {
            return static_cast<void *>(density::address_add(i_ptr, m_id % m_size));
        }
        void * from_base(void * i_ptr) const noexcept
        {
            return density::address_sub(i_ptr, m_id % m_size);
        }
        const void * to_base(const void * i_ptr) const noexcept
        {
            return static_cast<const void *>(density::address_add(i_ptr, m_id % m_size));
        }
        const void * from_base(const void * i_ptr) const noexcept
        {
            return density::address_sub(i_ptr, m_id % m_size);
        }

        void check_content(const void * i_ptr) const
        {
            auto const chars = static_cast<const unsigned char *>(from_base(i_ptr));
            DENSITY_TEST_ASSERT(density::address_is_aligned(chars, m_alignment));
            for (size_t index = 0; index < m_size; index++)
            {
                DENSITY_TEST_ASSERT(
                  chars[index] == (m_id & std::numeric_limits<unsigned char>::max()));
            }
        }

      private:
        size_t m_id, m_size, m_alignment;
    };
} // namespace density_tests
