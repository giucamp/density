
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#include "bench_framework/test_tree.h"
#include "bench_framework/test_session.h"
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

    const char * out_file = nullptr;
    if (argc >= 2)
    {
        out_file = argv[1];
        if (!touch_file(out_file))
        {
            std::cerr << "can't open for write the file " << out_file << std::endl;
            return -1;
        }
    }   

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

    if (out_file != nullptr)
    {
        result.save_to(out_file);
    }

    result.print_summary(std::cout);
}
