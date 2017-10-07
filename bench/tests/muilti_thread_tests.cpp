
#include <density/function_queue.h>
#include <density/conc_function_queue.h>
#include <density/lf_function_queue.h>
#include <density/sp_function_queue.h>
#include <vector>
#include <deque>
#include <queue>
#include <functional>
#include "bench_framework/test_tree.h"

namespace density_bench
{
    void multi_thread_tests(TestTree & i_tree)
    {
        PerformanceTestGroup group("multi_thread", "");
       
        using namespace density;

        group.add_test(BENCH_MAKE_TEST(i_cardinality,
            conc_function_queue<void()> queue;
            size_t i = 0; 
            for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        ));

        group.add_test(BENCH_MAKE_TEST(i_cardinality,
            lf_function_queue<void()> queue;
            size_t i = 0; 
            for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        ));

        group.add_test(BENCH_MAKE_TEST(i_cardinality,
            sp_function_queue<void()> queue;
            size_t i = 0; 
            for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        ));

        group.set_cardinality_start(1);
        group.set_cardinality_step(1);
        group.set_cardinality_end(16);

        i_tree["multi_thread"].add_performance_test(group);
    }
}

