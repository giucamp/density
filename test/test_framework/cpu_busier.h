
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <memory>
#include <chrono>
#include <limits>

namespace density_tests
{
    class cpu_busier
    {
    public:

        struct config
        {
            std::chrono::microseconds m_min_wait = std::chrono::microseconds(1000);
            std::chrono::microseconds m_max_wait = std::chrono::microseconds(5000);
            std::chrono::microseconds m_min_busy = std::chrono::microseconds(1000);
            std::chrono::microseconds m_max_busy = std::chrono::microseconds(5000);
            uint64_t m_num_processors = std::numeric_limits<uint64_t>::max();
        };

        cpu_busier(config i_config = config{});
        ~cpu_busier();

        cpu_busier(const cpu_busier&) = delete;
        cpu_busier & operator = (const cpu_busier&) = delete;

    private:
        struct Impl;
        std::unique_ptr<Impl> const m_impl;
    };
    
} // namespace density_tests
