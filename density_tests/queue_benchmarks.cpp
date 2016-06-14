
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/dense_queue.h"
#include "../density/paged_queue.h"
#include "../testity/test_tree.h"
#include <functional>
#include <deque>
#include <queue>
#include <vector>
#include <boost\any.hpp>

namespace density
{
    namespace tests
    {
        using namespace testity;

		PerformanceTestGroup iterate_list_benchmarks()
		{
			PerformanceTestGroup group("push & consume, no capture", "density version: " + std::to_string(DENSITY_VERSION));

			// dense_queue
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				dense_queue<> queue;
				for (size_t index = 0; index < i_cardinality; index++)
					queue.push(static_cast<int>(index));
			}, __LINE__);

			// paged_queue
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				paged_queue<> queue;
				for (size_t index = 0; index < i_cardinality; index++)
					queue.push(static_cast<int>(index));
			}, __LINE__);

			// vector<any>
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				std::vector<boost::any> queue;
				for (size_t index = 0; index < i_cardinality; index++)
					queue.push_back(static_cast<int>(index));
			}, __LINE__);

			// deque<any>
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				std::queue<boost::any> queue;
				for (size_t index = 0; index < i_cardinality; index++)
					queue.push(static_cast<int>(index));
			}, __LINE__);

			return group;
		}

        PerformanceTestGroup fill_queue_benchmarks()
        {
            PerformanceTestGroup group("fill queue by ints", "density version: " + std::to_string(DENSITY_VERSION));

            // dense_queue
            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                dense_queue<> queue;
                for (size_t index = 0; index < i_cardinality; index++)
                    queue.push(static_cast<int>(index));
            }, __LINE__);

			// paged_queue
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				paged_queue<> queue;
				for (size_t index = 0; index < i_cardinality; index++)
					queue.push(static_cast<int>(index));
			}, __LINE__);

            // vector<any>
            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                std::vector<boost::any> queue;
				for (size_t index = 0; index < i_cardinality; index++)
					queue.push_back(static_cast<int>(index));
            }, __LINE__);

			// deque<any>
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				std::queue<boost::any> queue;
				for (size_t index = 0; index < i_cardinality; index++)
					queue.push(static_cast<int>(index));
			}, __LINE__);

            return group;
        }

        void add_queue_benchmarks(TestTree & i_tree)
        {
            i_tree.add_performance_test(fill_queue_benchmarks());
        }

    } // namespace benchmarks

} // namespace density

