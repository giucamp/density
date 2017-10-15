
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#include "bench_framework/test_tree.h"
#include "bench_framework/test_session.h"
#include <iostream>
#include <chrono>

namespace density_bench
{
    void single_thread_tests(TestTree & i_tree);
    void lifo_tests(TestTree & i_tree);
}


int main()
{
    using namespace density_bench;

    #if !defined(NDEBUG)
        std::cout << "WARNING: this is a debug build!" << std::endl;
    #endif

    std::cout << "density_bench\n" << std::endl;

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
    result.save_to("results.txt");
}
