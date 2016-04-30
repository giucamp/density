
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "..\dense_queue.h"
#include "..\testing_utils.h"
#include "container_test.h"
#include <deque>
#include <complex>
#include <tuple>
#include <random>
#include <memory>
#include <functional>
#include <algorithm>

namespace density
{
	namespace detail
	{
		template <typename TYPE, typename NEW_ELEMENT_PREDICATE>
			void dense_queue_test_same_type(std::mt19937 & i_random,
				NEW_ELEMENT_PREDICATE && i_new_element_predicate)
		{
			ContainerTest<DenseQueue<TYPE, TestAllocator<TYPE>>, TYPE> test;

			/* test case that fills the m_dense_queue with push for n times */
			test.add_test_case("push_until_full", [&test, &i_new_element_predicate](std::mt19937 & i_random) {

				const unsigned times = std::uniform_int_distribution<unsigned>(0, 100)(i_random);

				for (unsigned i = 0; i < times; i++ )
				{
					const TYPE new_element = i_new_element_predicate(i_random);
					test.dense_container().push(new_element);
					test.std_deque().push_back(std::make_unique<TYPE>(new_element));
				}
			});

			unsigned step_count = std::uniform_int_distribution<unsigned>(0, 1000)(i_random);
			for (unsigned step_index = 0; step_index < step_count; step_index++)
			{
				test.step(i_random);
			}	
		}

		void dense_queue_test_impl(std::mt19937 & i_random)
		{
			NoLeakScope no_leak_scope;

			detail::dense_queue_test_same_type<uint64_t>(i_random,
				[](std::mt19937 & i_random) { return std::uniform_int_distribution<uint64_t>()(i_random); }
			);

			detail::dense_queue_test_same_type<double>(i_random,
				[](std::mt19937 & i_random) { return std::uniform_real_distribution<double>()(i_random); }
			);
		}

		void dense_queue_basic_tests()
		{
			DenseQueue< DenseQueue<int> > queue_of_queues;
			DenseQueue<int> queue;
			for (int i = 0; i < 1000; i++)
			{
				queue.push(i);
			}

			// this must use the lvalue overload, so queue must be preserved
			const auto prev_size = queue.mem_size();
			queue_of_queues.push(queue);
			DENSITY_TEST_ASSERT(queue.mem_size() == prev_size);

			// this must use the rvalue overload, so queue must be empty after the call
			queue_of_queues.push(std::move(queue));
			DENSITY_TEST_ASSERT(queue.mem_size().value() == 0);
			DENSITY_TEST_ASSERT(queue.empty());

			// try with a non-copyable type (std::unique_ptr)
			DenseQueue<std::unique_ptr<int>> queue_of_uncopyable;
			queue_of_uncopyable.push(std::make_unique<int>(10));
			queue_of_uncopyable.emplace<std::unique_ptr<int>>(std::make_unique<int>(10));
			DENSITY_TEST_ASSERT(*queue_of_uncopyable.front() == 10);
			queue_of_uncopyable.pop();
			DENSITY_TEST_ASSERT(*queue_of_uncopyable.front() == 10);
			queue_of_uncopyable.pop();
			DENSITY_TEST_ASSERT(queue_of_uncopyable.empty());

			// this must fail to compile
			//auto copy = queue_of_uncopyable;
			//copy = queue_of_uncopyable;
		}
	
	} // namespace detail

	void dense_queue_test()
	{
		detail::dense_queue_basic_tests();

		run_exception_stress_test([] {
			std::mt19937 random;
			detail::dense_queue_test_impl(random);
		});
	}

} // namespace density
