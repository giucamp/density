
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/small_function_queue.h>
#include <density/function_queue.h>
#include <testity/test_tree.h>
#include <functional>
#include <deque>
#include <queue>
#include <vector>

namespace density_tests
{
    using namespace density;
    using namespace testity;

    PerformanceTestGroup make_function_queue_benchmarks_nocapture()
    {
        PerformanceTestGroup group("push & consume, no capture", "density version: " + std::to_string(DENSITY_VERSION));

        // function_queue
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            function_queue< void() > queue;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push([]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue.consume_front();
        }, __LINE__);

        // small_function_queue
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            small_function_queue< void() > queue;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push([]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue.consume_front();
        }, __LINE__);

        // std::queue< std::function >
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::queue< std::function<void()> > queue;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push([]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue.front()(), queue.pop();
        }, __LINE__);

        // std::vector< std::function >
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::vector< std::function<void()> > queue;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push_back([]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue[index]();
        }, __LINE__);

        // std::vector< std::function > with reserve
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::vector< std::function<void()> > queue;
            queue.reserve(i_cardinality);
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push_back([]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue[index]();
        }, __LINE__);

        return group;
    }

    PerformanceTestGroup make_function_queue_benchmarks_midcapture()
    {
        PerformanceTestGroup group("push & consume, middle capture (46 bytes)", "density version: " + std::to_string(DENSITY_VERSION));

        // function_queue
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            function_queue< void() > queue;
            struct { volatile char chars[46] = "just a string"; } str;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue.consume_front();
        }, __LINE__);

        // small_function_queue
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            small_function_queue< void() > queue;
            struct { volatile char chars[46] = "just a string"; } str;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue.consume_front();
        }, __LINE__);

        // std::queue< std::function >
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::queue< std::function<void()> > queue;
            struct { volatile char chars[46] = "just a string"; } str;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue.front()(), queue.pop();
        }, __LINE__);

        // std::vector< std::function >
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::vector< std::function<void()> > queue;
            struct { volatile char chars[46] = "just a string"; } str;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push_back([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue[index]();
        }, __LINE__);

        // std::vector< std::function > with reserve
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::vector< std::function<void()> > queue;
            struct { volatile char chars[46] = "just a string"; } str;
            queue.reserve(i_cardinality);
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push_back([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue[index]();
        }, __LINE__);

        return group;
    }

    PerformanceTestGroup make_function_queue_benchmarks_bigcapture()
    {
        PerformanceTestGroup group("push & consume, big capture (64 bytes)", "density version: " + std::to_string(DENSITY_VERSION));

        // function_queue
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            function_queue< void() > queue;
            struct { volatile char chars[64] = "just a string"; } str;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue.consume_front();
        }, __LINE__);

        // small_function_queue
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            small_function_queue< void() > queue;
            struct { volatile char chars[64] = "just a string"; } str;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue.consume_front();
        }, __LINE__);

        // std::queue< std::function >
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::queue< std::function<void()> > queue;
            struct { volatile char chars[64] = "just a string"; } str;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue.front()(), queue.pop();
        }, __LINE__);

        // std::vector< std::function >
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::vector< std::function<void()> > queue;
            struct { volatile char chars[64] = "just a string"; } str;
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push_back([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue[index]();
        }, __LINE__);

        // std::vector< std::function > with reserve
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            std::vector< std::function<void()> > queue;
            struct { volatile char chars[64] = "just a string"; } str;
            queue.reserve(i_cardinality);
            for (size_t index = 0; index < i_cardinality; index++)
                queue.push_back([str]() { volatile int dummy = 1; (void)dummy; });
            for (size_t index = 0; index < i_cardinality; index++)
                queue[index]();
        }, __LINE__);

        return group;
    }

    void add_function_queue_benchmarks(TestTree & i_dest)
    {
        i_dest.add_performance_test(make_function_queue_benchmarks_nocapture());
        i_dest.add_performance_test(make_function_queue_benchmarks_midcapture());
        i_dest.add_performance_test(make_function_queue_benchmarks_bigcapture());
    }

} // namespace density_tests

