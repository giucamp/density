
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/concurrent_heterogeneous_queue_lf.h>
#include <testity/test_tree.h>
#include <testity/testity_common.h>
#include "concurrent_producer_consumer_test.h"
#include <chrono>

namespace density_tests
{
    using namespace density;
    using namespace testity;

    template <typename QUEUE>
        void tets_concurrent_heterogeneous_queue_st(std::mt19937 &)
    {
        using namespace density::experimental;

        QUEUE queue;

        const int64_t count = 100000;
        for (int64_t i = 0; i < 100000; i++)
            queue.push(i);

        bool res;
        int64_t consumed = 0;
        auto const int_type = QUEUE::runtime_type::template make<int64_t>();
        do {
            res = queue.try_consume([&int_type, consumed](const typename QUEUE::runtime_type & i_complete_type, void * i_element) {
                TESTITY_ASSERT(i_complete_type == int_type);
                TESTITY_ASSERT(consumed == *static_cast<int*>(i_element));
            });
            consumed++;
        } while (res);

        TESTITY_ASSERT(consumed == count + 1);
    }

    template <typename QUEUE>
        void tets_concurrent_heterogeneous_queue_mt(std::mt19937 &)
    {
		for (size_t i = 0; i < std::numeric_limits<size_t>::digits; i++)
		{
			auto const size = static_cast<size_t>(1) << i;
			TESTITY_ASSERT(density::detail::size_log2(size) == i);
		}

        using namespace density::experimental;

        ConcProdConsTest<QUEUE> test(10 * 1000 * 1000 );
		using Type = typename QUEUE::runtime_type;
		using CommonType = typename QUEUE::common_type;

		test.add_test<int8_t>(
			[](QUEUE & i_queue, int64_t i_id, std::mt19937 & /*i_rand*/) { i_queue.push(static_cast<int8_t>(i_id)); },
			[](CommonType * i_element)->int64_t { return *static_cast<int8_t*>(i_element); }, 
			std::numeric_limits<int8_t>::max() );

		test.add_test<int16_t>(
			[](QUEUE & i_queue, int64_t i_id, std::mt19937 & /*i_rand*/) { i_queue.push(static_cast<int16_t>(i_id)); },
			[](CommonType * i_element)->int64_t { return *static_cast<int16_t*>(i_element); },
			std::numeric_limits<int16_t>::max());

		test.add_test<int32_t>(
			[](QUEUE & i_queue, int64_t i_id, std::mt19937 & /*i_rand*/) { i_queue.push(static_cast<int32_t>(i_id)); },
			[](CommonType * i_element)->int64_t { return *static_cast<int32_t*>(i_element); },
			std::numeric_limits<int32_t>::max());

		test.add_test<int64_t>(
			[](QUEUE & i_queue, int64_t i_id, std::mt19937 & /*i_rand*/) { i_queue.push(i_id); },
			[](CommonType * i_element) { return *static_cast<int64_t*>(i_element); });

		test.add_test<std::string>(
			[](QUEUE & i_queue, int64_t i_id, std::mt19937 & /*i_rand*/) { i_queue.push(std::to_string(i_id)); },
			[](CommonType * i_element) { 
				int64_t result = 0;
				for (auto curr_char = static_cast<std::string*>(i_element)->c_str(); *curr_char != 0; curr_char++)
				{
					result *= 10;
					result += static_cast<int64_t>(*curr_char - '0');
				}
				return result;
		});

		const size_t consumers = 6;
		const size_t producers = 6;
		test.run(consumers, producers);
    }

    void add_concurrent_heterogeneous_queue_cases(TestTree & i_dest)
    {
        using namespace density;
        using namespace density::experimental;

        //i_dest.add_case(tets_concurrent_heterogeneous_queue_st<concurrent_heterogeneous_queue_lf<>>);
        i_dest.add_case(tets_concurrent_heterogeneous_queue_mt<concurrent_heterogeneous_queue_lf<>>);
		//i_dest.add_case(tets_concurrent_heterogeneous_queue_st<concurrent_heterogeneous_queue_lf<int64_t>>);
		//i_dest.add_case(tets_concurrent_heterogeneous_queue_mt<concurrent_heterogeneous_queue_lf<int64_t>>);
    }
}
