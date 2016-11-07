
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../../density/concurrent_heterogeneous_queue.h"
#include "testity/test_tree.h"
#include "testity/testity_common.h"
#include "concurrent_producer_consumer_test.h"
#include <chrono>

namespace density_tests
{
    using namespace density;
    using namespace testity;

	template <typename CONTAINER>
		void tets_concurrent_heterogeneous_queue_st(std::mt19937 &)
	{
		using namespace density::experimental;

		CONTAINER queue;

		const int64_t count = 100000;
		for (int64_t i = 0; i < 100000; i++)
			queue.push(i);

		bool res;
		int64_t consumed = 0;
		auto const int_type = CONTAINER::runtime_type::template make<int64_t>();
		do {
			res = queue.try_consume([&int_type, consumed](const typename CONTAINER::runtime_type & i_complete_type, void * i_element) {
				TESTITY_ASSERT(i_complete_type == int_type);
				TESTITY_ASSERT(consumed == *static_cast<int*>(i_element));
			});
			consumed++;
		} while (res);

		TESTITY_ASSERT(consumed == count + 1);
	}

	template <typename CONTAINER>
		void tets_concurrent_heterogeneous_queue_mt(std::mt19937 &)
	{
		using namespace density::experimental;

		const size_t consumers = 2;
		const size_t producers = 2;
		ConcProdConsTest<CONTAINER> test(consumers, producers, 10 * 1000 * 1000 );

		//auto start_time = std::chrono::high_resolution_clock::now();

		while (!test.is_over())
		{
			test.print_stats();

			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

    void add_concurrent_heterogeneous_queue_cases(TestTree & i_dest)
    {
		using namespace density;
		using namespace density::experimental;
		
		i_dest.add_case(tets_concurrent_heterogeneous_queue_st<concurrent_heterogeneous_queue<>>);
		i_dest.add_case(tets_concurrent_heterogeneous_queue_mt<concurrent_heterogeneous_queue<>>);
    }
}
