
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/small_queue_function.h"
#include "../density/queue_function.h"
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
		small_queue_function<void()> den_queue;
		tests::function_queue_test(den_queue);

		queue_function<void()> queue_any;
		tests::function_queue_test(queue_any);
	}
}
