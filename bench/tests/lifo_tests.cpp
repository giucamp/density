
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#include <density/lifo.h>
#include <memory>
#include "bench_framework/test_tree.h"
#if __linux__
    #include <alloca.h>
#endif

namespace density_bench
{
    void lifo_tests_1(TestTree & i_tree)
    {
        PerformanceTestGroup group("lifo_array_b1", "");

        using namespace density;

        group.set_cardinality_start(16);
        group.set_cardinality_end(30000);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            lifo_array<char> chars(i_cardinality);
            volatile char c = 0;
            chars[0] = c;
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            auto chars = std::unique_ptr<char[]>(new char[i_cardinality]);
            volatile char c = 0;
            chars[0] = c;
        }, __LINE__);

        #ifdef _WIN32

            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                auto chars = static_cast<char*>(_alloca(i_cardinality));
                volatile char c = 0;
                chars[0] = c;
            }, __LINE__);

            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                auto chars = static_cast<char*>(_malloca(i_cardinality));
                volatile char c = 0;
                chars[0] = c;
                _freea(chars);
            }, __LINE__);

        #elif defined(__linux__)

            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                auto chars = static_cast<char*>(alloca(i_cardinality));
                volatile char c = 0;
                chars[0] = c;
            }, __LINE__);

        #endif

        i_tree["lifo_tests_1"].add_performance_test(group);
    }

    void lifo_tests_2(TestTree & i_tree)
    {
        PerformanceTestGroup group("lifo_array_b2", "");

        using namespace density;

        group.set_cardinality_start(16);
        group.set_cardinality_end(4000);
        group.set_cardinality_step(1);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            lifo_array<double> chars(i_cardinality);
            volatile double c = 0;
            chars[0] = c;
        }, __LINE__);

        #ifdef _WIN32

            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                auto chars = static_cast<double*>(_alloca(i_cardinality * sizeof(double)));
                volatile double c = 0;
                chars[0] = c;
            }, __LINE__);

        #elif defined(__linux__)

            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                auto chars = static_cast<double*>(alloca(i_cardinality * sizeof(double)));
                volatile double c = 0;
                chars[0] = c;
            }, __LINE__);

        #endif

        i_tree["lifo_tests_2"].add_performance_test(group);
    }

    void lifo_tests(TestTree & i_tree)
    {
        lifo_tests_1(i_tree);
        lifo_tests_2(i_tree);
    }
}

