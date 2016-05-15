
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/dense_function_queue.h"
#include "../density/paged_function_queue.h"
#include "../testity/test_tree.h"
#include <functional>
#include <deque>
#include <queue>

namespace density
{
    namespace tests
    {
		using namespace testity;

		void register_function_queue_benchmarks(TestTree & i_test_node)
		{
			PerformanceTestGroup group("push & consume");

			// paged_function_queue
			group.add_test( DENSITY_MAKE_BENCHMARK_TEST(
				paged_function_queue< void() > queue;
				for (size_t index = 0; index < i_cardinality; index++)
				{
					queue.push([]() { volatile int dummy = 1; (void)dummy; });
				}
				for (size_t index = 0; index < i_cardinality; index++)
				{
					queue.consume_front();
				}
			));

			// dense_function_queue
			group.add_test( DENSITY_MAKE_BENCHMARK_TEST(
				dense_function_queue< void() > queue;
				for (size_t index = 0; index < i_cardinality; index++)
				{
					queue.push([]() { volatile int dummy = 1; (void)dummy; });
				}
				for (size_t index = 0; index < i_cardinality; index++)
				{
					queue.consume_front();
				}
			));

			// std::queue< std::function >
			group.add_test( DENSITY_MAKE_BENCHMARK_TEST(
				std::queue< std::function<void()> > queue;
				for (size_t index = 0; index < i_cardinality; index++)
				{
					queue.push([]() { volatile int dummy = 1; (void)dummy; });
				}
				for (size_t index = 0; index < i_cardinality; index++)
				{
					queue.front()();
					queue.pop();
				}
			));

			i_test_node["/density/function_queue_test"].add_performance_test(group);
		}

    } // namespace benchmarks

} // namespace density

