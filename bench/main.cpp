
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(_MSC_VER)
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include "bench_framework/test_tree.h"
#include "bench_framework/test_session.h"
#include "bench_framework/performance_test.h"
#include <density/density_common.h>
#include <iostream>
#include <fstream>
#include <chrono>

namespace density_bench
{
    void single_thread_tests(TestTree & i_tree);
    void lifo_tests(TestTree & i_tree);
}

bool touch_file(const char * i_file_name)
{
    return !std::ofstream(i_file_name).fail();
}

int main(int argc, char *argv[])
{
    using namespace density_bench;

    #if !defined(NDEBUG)
        std::cout << "WARNING: this is a debug build!" << std::endl;
    #endif

    std::cout << "density_bench - built on " __DATE__  " at " __TIME__ << std::endl;
    std::cout << "density version: " << density::version << std::endl;
    
    std::string out_file;
    std::string src_dir;

    char string_argument[4096];
    for (int i = 1; i < argc; i++)
    {
        if (sscanf(argv[i], "-out: %4095s", string_argument) == 1)
        {
            out_file = string_argument;
        }
        else if (sscanf(argv[i], "-source: %4095s", string_argument) == 1)
        {
            src_dir = string_argument;
        }
        else
        {
            std::cerr << "unrecognized commandline argument: " << argv[i] << std::endl;
        }
    }

    if (!out_file.empty())
    {
        if (!touch_file(out_file.c_str()))
        {
            std::cerr << "can't open for write the file " << out_file << std::endl;
            return -1;
        }
    }

    PerformanceTestGroup::set_source_dir(src_dir.c_str());

    TestTree root("density");
    single_thread_tests(root);
    lifo_tests(root);

    auto progression = [](const Progression & i_progression) {
        auto const millisecs = std::chrono::duration_cast<std::chrono::milliseconds>(i_progression.m_remaining_time_extimate);
        std::cout << static_cast<int>(i_progression.m_completion_factor * 100.) << "%";
        if(i_progression.m_time_extimate_available)
        {
            std::cout << ", " << static_cast<double>(millisecs.count()) / (1000. * 60.) << " min remaining";
        }
        std::cout << std::endl;
    };

    auto result = run_session(root, TestConfig(), progression);

    if (!out_file.empty())
    {
        result.save_to(out_file.c_str());
    }

    result.print_summary(std::cout);
}

#if defined(_MSC_VER)
    #undef _CRT_SECURE_NO_WARNINGS
#endif
