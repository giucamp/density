
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "..\dense_fixed_queue.h"
#include "..\testing_utils.h"
#include <deque>
#include <complex>
#include <tuple>
#include <random>
#include <memory>
#include <functional>

namespace density
{
	namespace detail
	{
		template <typename BASE_TYPE>
			class FixedQueueTest
		{
		public:

			using Queue = DenseFixedQueue<BASE_TYPE, TestAllocator<BASE_TYPE> >;

			FixedQueueTest(size_t i_dense_queue_mem_size)
				: m_dense_queue(i_dense_queue_mem_size)
			{
				using namespace std::placeholders;
				
				add_test_case("copy_and_assignment", std::bind(&FixedQueueTest::test_copy_and_assignment, this, _1));
				add_test_case("consume_until_empty", std::bind(&FixedQueueTest::builtin_test_case_consume_until_empty, this, _1));
				add_test_case("consume_n_times", std::bind(&FixedQueueTest::builtin_test_case_consume_n_times, this, _1));
			}

			void add_test_case(const char * /*i_name*/, std::function< void(std::mt19937 & i_random) > && i_function )
			{
				m_test_cases.push_back(std::move(i_function));
			}

			void step(std::mt19937 & i_random)
			{
				// pick a random test from m_test_cases, and execute it
				if (m_test_cases.size() > 0)
				{
					auto const test_case_index = std::uniform_int_distribution<size_t>(0, m_test_cases.size() - 1)(i_random);
					m_test_cases[test_case_index](i_random);
				}

				compare();
			}

			// check for equality m_dense_queue and m_std_deque
			void compare()
			{				
				DENSITY_TEST_ASSERT(m_dense_queue.empty() == m_std_deque.empty());
				if (!m_std_deque.empty())
				{
					DENSITY_TEST_ASSERT(m_dense_queue.front() == *m_std_deque.front());
				}
				auto dense_it = m_dense_queue.cbegin();
				auto std_it = m_std_deque.cbegin();
				for (;;)
				{
					bool end1 = dense_it == m_dense_queue.cend();
					bool end2 = std_it == m_std_deque.cend();
					DENSITY_TEST_ASSERT(end1 == end2);
					if (end1)
					{
						break;
					}
					DENSITY_TEST_ASSERT(*dense_it == **std_it);
					++dense_it;
					++std_it;
				}
			}

		private:			

			void test_copy_and_assignment(std::mt19937 & /*i_random*/)
			{
				/* Assign m_dense_queue from a copy of itself. m_dense_queue and m_std_deque must preserve the equality. */
				auto copy = m_dense_queue;
				DENSITY_TEST_ASSERT(copy.mem_free() == m_dense_queue.mem_free());
				m_dense_queue = copy;
				DENSITY_TEST_ASSERT(copy.mem_free() == m_dense_queue.mem_free());

				const auto size_1 = std::distance(m_dense_queue.cbegin(), m_dense_queue.cend());
				const auto size_2 = std::distance(copy.cbegin(), copy.cend());
				DENSITY_TEST_ASSERT(size_1 == size_2);
			}

			void builtin_test_case_consume_until_empty(std::mt19937 & /*i_random*/)
			{
				/* call consume until the m_dense_queue is empty. */
				volatile unsigned i = 0; // <-- may help debug
				while (!m_dense_queue.empty())
				{
					m_dense_queue.consume([this](const typename DenseFixedQueue<BASE_TYPE>::RuntimeType &, BASE_TYPE i_val) {
						DENSITY_TEST_ASSERT(m_std_deque.size() > 0 && i_val == *m_std_deque.front());
						m_std_deque.pop_front();
					});
					i++;
				}
				DENSITY_TEST_ASSERT(m_std_deque.empty());
				DENSITY_TEST_ASSERT(m_dense_queue.mem_free() == m_dense_queue.mem_capacity());
			}

			void builtin_test_case_consume_n_times(std::mt19937 & i_random)
			{
				/* call consume n times, or until the m_dense_queue is empty. */
				const unsigned times = std::uniform_int_distribution<unsigned>(0, 100)(i_random);
				volatile unsigned i = 0; // <--may help debug
				while ( i < times && !m_dense_queue.empty())
				{
					m_dense_queue.consume([this](const typename DenseFixedQueue<BASE_TYPE>::RuntimeType &, BASE_TYPE i_val) {
						DENSITY_TEST_ASSERT(m_std_deque.size() > 0 && i_val == *m_std_deque.front());
						m_std_deque.pop_front();
					});
					i++;
				}
			}

		//private: // data members
		public:
			NoLeakScope m_no_leak_scope;
			Queue m_dense_queue;
			std::deque<std::unique_ptr<BASE_TYPE>> m_std_deque;
			std::vector< std::function<void(std::mt19937 & i_random) > > m_test_cases;
		};
	
		template <typename TYPE, typename NEW_ELEMENT_PREDICATE>
			void fixed_queue_test_same_type(std::mt19937 & i_random, size_t i_queue_mem_size,
				NEW_ELEMENT_PREDICATE && i_new_element_predicate)
		{
			FixedQueueTest<TYPE> test(i_queue_mem_size);

			/* test case that fills the m_dense_queue with try_push until there is no more place. */
			test.add_test_case("push_until_full", [&test, &i_new_element_predicate](std::mt19937 & i_random) {
							
				bool failed = false;
				while (!failed)
				{
					const TYPE new_element = i_new_element_predicate(i_random);
					if (test.m_dense_queue.try_push(new_element))
					{
						test.m_std_deque.push_back( std::make_unique<TYPE>(new_element) );
					}
					else
					{
						failed = true;
					}
				}
				
				/* check that the free space is not enough not insert new elements. We must take
				account of alignment padding and buffer wrapping padding */
				const auto free = test.m_dense_queue.mem_free();
				const auto max_element_requirement = sizeof(TYPE) + alignof(TYPE) +
					sizeof(typename FixedQueueTest<TYPE>::Queue::RuntimeType) + alignof(typename FixedQueueTest<TYPE>::Queue::RuntimeType);
				DENSITY_TEST_ASSERT(free < max_element_requirement);
			} );

			/* test case that fills the m_dense_queue with try_push for n times or until there is no more place. */
			test.add_test_case("push_until_full", [&test, &i_new_element_predicate](std::mt19937 & i_random) {

				const unsigned times = std::uniform_int_distribution<unsigned>(0, 100)(i_random);

				bool failed = false;
				for (unsigned i = 0; !failed && i < times; i++ )
				{
					const TYPE new_element = i_new_element_predicate(i_random);
					if (test.m_dense_queue.try_push(new_element))
					{
						test.m_std_deque.push_back(std::make_unique<TYPE>(new_element));
					}
					else
					{
						failed = true;
					}
				}
			});

			unsigned step_count = std::uniform_int_distribution<unsigned>(0, 10000)(i_random);
			for (unsigned step_index = 0; step_index < step_count; step_index++)
			{
				test.step(i_random);
			}	
		}

		void fixed_queue_test_impl(std::mt19937 & i_random)
		{
			NoLeakScope no_leak_scope;

			auto rand_mem_size = [&i_random] { return std::uniform_int_distribution<size_t>(0, 64 * 1024)(i_random); };

			detail::fixed_queue_test_same_type<uint64_t>(i_random,
				rand_mem_size(),
				[](std::mt19937 & i_random) { return std::uniform_int_distribution<uint64_t>()(i_random); }
			);

			detail::fixed_queue_test_same_type<double>(i_random,
				rand_mem_size(),
				[](std::mt19937 & i_random) { return std::uniform_real_distribution<double>()(i_random); }
			);
		}
	
	} // namespace detail

	void fixed_queue_test()
	{
		run_exception_stress_test([] {
			std::mt19937 random;
			detail::fixed_queue_test_impl(random);
		});
	}

} // namespace density