
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
        template <typename TYPE>
            void dense_queue_test_same_type(std::mt19937 & i_random)
        {
			// add detail::FeatureHash to the automatic feature list
			using Features = typename detail::FeatureConcat< detail::AutoGetFeatures<TYPE>::type, detail::FeatureHash >::type;
			using Queue = DenseQueue<TYPE, TestAllocator<TYPE>, RuntimeType<TYPE, Features> >;
			ContainerTest<Queue, TYPE> test;

            test.add_test_case("push_n_times", [&test](std::mt19937 & i_random) {

                const unsigned times = std::uniform_int_distribution<unsigned>(0, 100)(i_random);

                for (unsigned i = 0; i < times; i++ )
                {
                    const CopyableTestObject new_element(i_random);
                    test.dense_container().push(new_element);
                    test.shadow_container().push_back(new_element);
                }
            });

			test.add_test_case("consume_n_times", [&test](std::mt19937 & i_random) {

				const unsigned times = std::uniform_int_distribution<unsigned>(0, 100)(i_random);

				for (unsigned i = 0; i < times && !test.shadow_container().empty(); i++)
				{
					test.dense_container().consume([&test](const typename Queue::RuntimeType &, const TYPE &) {
						test.shadow_container().pop_front();
					});
				}
			});

			/* call consume n times, or until the m_dense_container is empty. */
			/*void builtin_test_case_consume_n_times(std::mt19937 & i_random)
			{
				const unsigned times = std::uniform_int_distribution<unsigned>(0, 100)(i_random);
				volatile unsigned i = 0;
				while (i < times && !m_dense_container.empty())
				{
					m_dense_container.consume([this](const typename DenseQueue<BASE_TYPE>::RuntimeType &, BASE_TYPE) {
						shadow_container().pop_front();
					});
					i++;
				}
			}*/

			/* call consume until the m_dense_container is empty. */
			/*void builtin_test_case_consume_until_empty(std::mt19937 & )
			{
				volatile unsigned i = 0; // <-- may help debug
				while (!m_dense_container.empty())
				{
					m_dense_container.consume([this](const typename DenseQueue<BASE_TYPE>::RuntimeType &, BASE_TYPE i_val) {
						DENSITY_TEST_ASSERT(shadow_container().size() > 0 && i_val == *shadow_container().front());
						shadow_container().pop_front();
					});
					i++;
				}
				DENSITY_TEST_ASSERT(shadow_container().empty());
				DENSITY_TEST_ASSERT(m_dense_container.mem_free() == m_dense_container.mem_capacity());
			}*/

            unsigned step_count = std::uniform_int_distribution<unsigned>(0, 1000)(i_random);
            for (unsigned step_index = 0; step_index < step_count; step_index++)
            {
                test.step(i_random);
            }    
        }

        void dense_queue_test_impl(std::mt19937 & i_random)
        {
            NoLeakScope no_leak_scope;

            detail::dense_queue_test_same_type<TestObjectBase>(i_random);

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
