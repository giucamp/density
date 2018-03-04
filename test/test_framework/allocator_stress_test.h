
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <chrono>
#include <limits>
#include <memory>

namespace density_tests
{
    class allocator_stress_test
    {
      public:
        struct config
        {
            std::chrono::microseconds m_min_wait           = std::chrono::microseconds(0);
            std::chrono::microseconds m_max_wait           = std::chrono::microseconds(6000);
            std::chrono::microseconds m_allocation_timeout = std::chrono::microseconds(1000);
            uintptr_t                 m_max_memory_per_cpu = 1024 * 1024 * 4;
            uint64_t                  m_num_processors     = std::numeric_limits<uint64_t>::max();

            config() noexcept {}
        };

        allocator_stress_test(config i_config = config());
        ~allocator_stress_test();

        allocator_stress_test(const allocator_stress_test &) = delete;
        allocator_stress_test & operator=(const allocator_stress_test &) = delete;

      private:
        struct Impl;
        std::unique_ptr<Impl> const m_impl;
    };

} // namespace density_tests
