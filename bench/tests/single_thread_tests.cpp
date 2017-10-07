
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
    void single_thread_tests_1(TestTree & i_tree)
    {
        PerformanceTestGroup group("func_queue_st_b1", "");
       
        using namespace density;

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            function_queue<void()> queue;
            size_t i = 0; 
            for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            conc_function_queue<void()> queue;
            size_t i = 0; 
            for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            lf_function_queue<void()> queue;
            size_t i = 0; 
            for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            sp_function_queue<void()> queue;
            size_t i = 0; 
            for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::vector<std::function<void()>> queue;
            for (size_t i = 0; i < i_cardinality; i++)
                queue.push_back([] { volatile int u = 0; (void)u; });
            
            for (size_t i = 0; i < i_cardinality; i++)
                queue[i]();
            queue.clear();
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::queue<std::function<void()>> queue;
            size_t i = 0;  for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            while (!queue.empty()) {
                queue.front()(); queue.pop();
                i--;
            }
            assert(i == 0);
        }, __LINE__);

        i_tree["single_thread_1"].add_performance_test(group);
    }

    void single_thread_tests_2(TestTree & i_tree)
    {
        PerformanceTestGroup group("func_queue_st_b2", "");
       
        using namespace density;

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            function_queue<void(), void_allocator, function_manual_clear> queue;
            size_t i = 0; for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            conc_function_queue<void(), void_allocator, function_manual_clear> queue;
            size_t i = 0; for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            lf_function_queue<void(), void_allocator, function_manual_clear, concurrency_single, concurrency_single, consistency_relaxed> queue;
            size_t i = 0;  for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            sp_function_queue<void(), void_allocator, function_manual_clear, 
                concurrency_single, concurrency_single> queue;
            size_t i = 0; for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            decltype(queue)::consume_operation consume;
            while (queue.try_consume(consume))
                i--;
            assert(i == 0);
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::vector<std::function<void()>> queue;
            for (size_t i = 0; i < i_cardinality; i++)
                queue.push_back([] { volatile int u = 0; (void)u; });
            
            for (size_t i = 0; i < i_cardinality; i++)
                queue[i]();
            queue.clear();
        }, __LINE__);

        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::queue<std::function<void()>> queue;
            size_t i = 0; for (; i < i_cardinality; i++)
                queue.push([] { volatile int u = 0; (void)u; });

            while (!queue.empty()) {
                queue.front()(); queue.pop();
                i--;
            }
            assert(i == 0);
        }, __LINE__);

        i_tree["single_thread_2"].add_performance_test(group);
    }

    void single_thread_tests(TestTree & i_tree)
    {
        single_thread_tests_1(i_tree);
        single_thread_tests_2(i_tree);
    }
}

