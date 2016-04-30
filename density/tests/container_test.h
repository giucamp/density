
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "..\density_common.h"
#include <random>
#include <deque>
#include <string>

namespace density
{
	namespace detail
	{
		/** This class template rapresent a test session to be run on a container implementation. */
		template <typename DENSE_CONTAINER, typename BASE_TYPE>
			class ContainerTest
		{
		public:

			using Queue = DenseQueue<BASE_TYPE, TestAllocator<BASE_TYPE> >;
			using TestCaseFunction = std::function<void(std::mt19937 & i_random)>;

			ContainerTest()
			{
				using namespace std::placeholders;

				add_test_case("copy_and_assignment", std::bind(&ContainerTest::test_copy_and_assignment, this, _1));
				add_test_case("consume_until_empty", std::bind(&ContainerTest::builtin_test_case_consume_until_empty, this, _1));
				add_test_case("consume_n_times", std::bind(&ContainerTest::builtin_test_case_consume_n_times, this, _1));
			}

			void add_test_case(const char * i_name, std::function< void(std::mt19937 & i_random) > && i_function,
				double i_probability = 1. )
			{
				m_test_cases.push_back({i_name, std::move(i_function), i_probability});
			}

			void step(std::mt19937 & i_random)
			{
				// pick a random test from m_test_cases, and execute it
				if (m_test_cases.size() > 0)
				{
					auto const test_case_index = std::uniform_int_distribution<size_t>(0, m_test_cases.size() - 1)(i_random);
					m_test_cases[test_case_index].m_function(i_random);
				}

				compare();
			}

			// check for equality m_dense_container and m_std_deque
			void compare()
			{
				DENSITY_TEST_ASSERT(m_dense_container.empty() == m_std_deque.empty());
				if (!m_std_deque.empty())
				{
					DENSITY_TEST_ASSERT(m_dense_container.front() == *m_std_deque.front());
				}
				auto dense_it = m_dense_container.cbegin();
				auto std_it = m_std_deque.cbegin();
				for (;;)
				{
					bool end1 = dense_it == m_dense_container.cend();
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

			const DENSE_CONTAINER & dense_container() const { return m_dense_container; }
			DENSE_CONTAINER & dense_container() { return m_dense_container; }

			std::deque< std::unique_ptr<BASE_TYPE> > & std_deque() { return m_std_deque; }
			const std::deque< std::unique_ptr<BASE_TYPE> > & std_deque() const { return m_std_deque; }

		private:

			void test_copy_and_assignment(std::mt19937 & /*i_random*/)
			{
				/* Assign m_dense_container from a copy of itself. m_dense_container and m_std_deque must preserve the equality. */
				auto copy = m_dense_container;
				m_dense_container = copy;
				
				// check the size with the iterators
				const auto size_1 = std::distance(m_dense_container.cbegin(), m_dense_container.cend());
				const auto size_2 = std::distance(copy.cbegin(), copy.cend());
				DENSITY_TEST_ASSERT(size_1 == size_2);

				// move construct m_dense_container to tmp
				auto tmp(std::move(m_dense_container));
				DENSITY_TEST_ASSERT(m_dense_container.empty());
				m_dense_container.push(BASE_TYPE());
				m_dense_container.push(BASE_TYPE());
				const auto size_3 = std::distance(tmp.cbegin(), tmp.cend());
				DENSITY_TEST_ASSERT(size_1 == size_3);

				// move assign tmp to m_dense_container
				m_dense_container = std::move(tmp);
			}

			/* call consume until the m_dense_container is empty. */
			void builtin_test_case_consume_until_empty(std::mt19937 & /*i_random*/)
			{
				volatile unsigned i = 0; // <-- may help debug
				while (!m_dense_container.empty())
				{
					m_dense_container.consume([this](const typename DenseQueue<BASE_TYPE>::RuntimeType &, BASE_TYPE i_val) {
						DENSITY_TEST_ASSERT(m_std_deque.size() > 0 && i_val == *m_std_deque.front());
						m_std_deque.pop_front();
					});
					i++;
				}
				DENSITY_TEST_ASSERT(m_std_deque.empty());
				DENSITY_TEST_ASSERT(m_dense_container.mem_free() == m_dense_container.mem_capacity());
			}

			/* call consume n times, or until the m_dense_container is empty. */
			void builtin_test_case_consume_n_times(std::mt19937 & i_random)
			{
				const unsigned times = std::uniform_int_distribution<unsigned>(0, 100)(i_random);
				volatile unsigned i = 0;
				while (i < times && !m_dense_container.empty())
				{
					m_dense_container.consume([this](const typename DenseQueue<BASE_TYPE>::RuntimeType &, BASE_TYPE i_val) {
						DENSITY_TEST_ASSERT(m_std_deque.size() > 0 && i_val == *m_std_deque.front());
						m_std_deque.pop_front();
					});
					i++;
				}
			}

		private: // data members
			NoLeakScope m_no_leak_scope;
			
			DENSE_CONTAINER m_dense_container;
			std::deque< std::unique_ptr<BASE_TYPE> > m_std_deque;

			struct TestCase
			{
				std::string m_name;
				TestCaseFunction m_function;
				double m_probability;
			};
			std::vector<TestCase> m_test_cases;
		}; // class template ContainerTest

	} // namespace detail
	
} // namespace density
