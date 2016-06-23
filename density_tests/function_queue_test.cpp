
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/small_function_queue.h"
#include "../density/function_queue.h"
#include "../testity/testing_utils.h"

namespace density
{
	namespace tests
	{
		template <typename FUNC_QUEUE>
			void function_queue_test(FUNC_QUEUE & i_queue)
		{
			size_t count = 0;
			for (size_t i = 0; i < 1000; i++)
			{
				i_queue.push([&count, i] {
					TESTITY_ASSERT(count == i);
					count++;
				});
			}

			while (!i_queue.empty())
			{
				i_queue.consume_front();
			}

			TESTITY_ASSERT(count == 1000);
		}
	}

	void function_queue_test()
	{
		small_function_queue<void()> den_queue;
		tests::function_queue_test(den_queue);

		function_queue<void()> heterogeneous_queue;
		tests::function_queue_test(heterogeneous_queue);
	}
}
