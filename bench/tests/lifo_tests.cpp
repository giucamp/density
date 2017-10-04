
#include <density/lifo.h>
#include <memory>
#include "bench_framework\test_tree.h"

namespace density_bench
{
    void lifo_tests_1(TestTree & i_tree)
    {
        PerformanceTestGroup group("bench", "");
       
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

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            auto chars = static_cast<char*>(alloca(i_cardinality));
            volatile char c = 0;
            chars[0] = c;
        }, __LINE__);

        i_tree["lifo_tests_1"].add_performance_test(group);
    }

    void lifo_tests(TestTree & i_tree)
    {
        lifo_tests_1(i_tree);
    }
}

