
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../../density/concurrent_heterogeneous_queue.h"
#include "testity/test_tree.h"
#include "testity/testity_common.h"
#include <iostream>

namespace density_tests
{
    using namespace density;
    using namespace testity;

    void tets_concurrent_heterogeneous_queue(std::mt19937 &)
    {
        using namespace density::experimental;

        using Queue = concurrent_heterogeneous_queue<>;
        Queue queue;

		const int count = 100000;
        for (int i = 0; i < 100000; i++)
            queue.push(i);

        auto consumer = queue.make_consumer();
        bool res;
		int consumed = 0;
        auto const int_type = Queue::runtime_type::make<int>();
        do {
            res = consumer.try_consume([&int_type, consumed](const Queue::runtime_type & i_complete_type, void * i_element) {
                TESTITY_ASSERT(i_complete_type == int_type);
				TESTITY_ASSERT(consumed == *static_cast<int*>(i_element));
            });			
			consumed++;
        } while (res);

		TESTITY_ASSERT(consumed == count + 1);
        std::cout << "-----" << std::endl;
    }

    void add_concurrent_heterogeneous_queue_cases(TestTree & i_dest)
    {
        i_dest.add_case(tets_concurrent_heterogeneous_queue);
    }
}
