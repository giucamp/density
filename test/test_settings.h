
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct TestSettings
{
    uint32_t m_rand_seed               = 0;
    bool     m_exceptions              = true;
    bool     m_spare_one_cpu           = true;
    bool     m_test_allocators         = true;
    bool     m_print_progress          = true;
    size_t   m_queue_tests_cardinality = 2000;
    bool     m_allocator_stress_test{true};

    std::vector<std::string> m_run_only;
    std::vector<std::string> m_exclude;

    bool should_run(const char * i_test_name) const;
};

std::shared_ptr<const TestSettings> parse_settings(int argc, char ** argv);
