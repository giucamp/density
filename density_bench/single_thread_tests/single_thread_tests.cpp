
#include <density/function_queue.h>
#include <density/conc_function_queue.h>
#include <density/lf_function_queue.h>
#include <density/sp_function_queue.h>
#include <vector>
#include <deque>
#include <queue>
#include <functional>
#include "density_bench\bench_framework\test_tree.h"

namespace density_bench
{
    
    void single_thread_tests(TestTree & i_tree)
    {
        PerformanceTestGroup group("single_thread", "");
       
        using namespace density;

        group.add_test(BENCH_MAKE_TEST(i_cardinality,
            function_queue<void()> queue;
            size_t i = 0; 
            for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            function_queue<void()>::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        ));

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

        group.add_test(BENCH_MAKE_TEST(i_cardinality,
            std::vector<std::function<void()>> queue;
            for (size_t i = 0; i < i_cardinality; i++)
                queue.push_back([] { volatile int u = 0; (void)u; });
            
            for (size_t i = 0; i < i_cardinality; i++)
                queue[i]();
            queue.clear();
        ));

        group.add_test(BENCH_MAKE_TEST(i_cardinality,
            std::queue<std::function<void()>> queue;
            size_t i = 0; 
            for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            while (!queue.empty())
            {
                queue.front()();
                queue.pop();
                i--;
            }
            assert(i == 0);
        ));

        i_tree["single_thread"].add_performance_test(group);
    }
}

