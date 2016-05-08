
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "..\density_common.h"
#include "..\testing_utils.h"
#include <random>
#include <deque>
#include <string>
#include <typeinfo>

namespace density
{
    namespace detail
    {
		class TestObjectBase
		{
		public:
				
			TestObjectBase(std::mt19937 & i_random)
				: m_hash(std::allocate_shared<size_t>(TestAllocator<size_t>(), std::uniform_int_distribution<size_t>()(i_random))) {}
			
			TestObjectBase(const TestObjectBase & i_source)
				: m_hash(std::allocate_shared<size_t>(TestAllocator<size_t>(), *i_source.m_hash)) {}

			TestObjectBase(TestObjectBase && i_source) noexcept
				: m_hash(std::move(i_source.m_hash))
			{
				i_source.m_hash = nullptr;
			}

			TestObjectBase & operator = (const TestObjectBase & i_source)
			{
				m_hash = std::allocate_shared<size_t>(TestAllocator<size_t>(), *i_source.m_hash);
				return *this;
			}

			TestObjectBase & operator = (TestObjectBase && i_source) noexcept
			{
				m_hash = std::move(i_source.m_hash);
				i_source.m_hash = nullptr;
				return *this;
			}

			bool operator == (const TestObjectBase & i_other) const
			{
				return *m_hash == *i_other.m_hash;
			}

			bool operator != (const TestObjectBase & i_other) const
			{
				return *m_hash != *i_other.m_hash;
			}

			size_t hash() const { return m_hash != nullptr ? *m_hash : 0; }

		private:
			std::shared_ptr<size_t> m_hash;
		};

		inline size_t hash_func(const TestObjectBase & i_object)
		{
			return i_object.hash();
		}

		class CopyableTestObject final : public TestObjectBase
		{
		public:
			
			CopyableTestObject(std::mt19937 & i_random) : TestObjectBase(i_random) { exception_check_point(); }
			CopyableTestObject(const TestObjectBase & i_source) : TestObjectBase(i_source) {}

			// copy
			CopyableTestObject(const CopyableTestObject & i_source)
				: TestObjectBase((exception_check_point(), i_source))
			{

			}
			CopyableTestObject & operator = (const CopyableTestObject & i_source)
			{
				exception_check_point();
				TestObjectBase::operator = (i_source);
				exception_check_point();
				return *this;
			}

			// move (no except)
			CopyableTestObject(CopyableTestObject && i_source) noexcept = default;
			CopyableTestObject & operator = (CopyableTestObject && i_source) noexcept = default;

			bool operator == (const CopyableTestObject & i_other) const
			{
				return TestObjectBase::operator == (i_other);
			}

			bool operator != (const CopyableTestObject & i_other) const
			{
				return TestObjectBase::operator != (i_other);
			}
		};

		class MovableTestObject final : public TestObjectBase
		{
		public:

			MovableTestObject(std::mt19937 & i_random) : TestObjectBase(i_random) { exception_check_point(); }
			MovableTestObject(const TestObjectBase & i_source) : TestObjectBase(i_source) {}

			// copy
			MovableTestObject(const MovableTestObject & i_source) = delete;
			MovableTestObject & operator = (const MovableTestObject & i_source) = delete;

			// move (no except)
			MovableTestObject(MovableTestObject && i_source) noexcept = default;
			MovableTestObject & operator = (MovableTestObject && i_source) noexcept = default;

			bool operator == (const MovableTestObject & i_other) const
			{
				return TestObjectBase::operator == (i_other);
			}

			bool operator != (const MovableTestObject & i_other) const
			{
				return TestObjectBase::operator != (i_other);
			}
		};

		/** A ShadowContainer keeps information about a dense container, to easy unit testing. It keeps a std::type_info
			and an hash for every element in the dense container. The dense container is owned externally. 
			The units test performs the same operations on the dense container and on the shadow container, and checks
			for consistency. If an exception is thrown during an operation on a dense container, the shadow container can
			be used to check the strong exception guarantee. */
		template <typename DENSE_CONTAINER>
			class ShadowContainer
		{
		private:
			struct Element
			{
				const std::type_info * m_type_info;
				size_t m_hash;
			};
			
		public:

			/* Exception thrown as result to an exception occurred during the update of the shadow container.
			Handlers for of this exception can't check the tested container against the shadow container. */
			struct BasicGuaranteeException : TestException
			{
				using TestException::TestException;
			};


			ShadowContainer() {}

			ShadowContainer(const DENSE_CONTAINER & i_container)
			{
				const auto end_it = i_container.end();
				for (const auto it = i_container.begin(); it != end_it; it++ )
				{
					auto hasher = it->curr_type().get_feature<detail::FeatureHash>();
					const auto & type_info = it->type().type_info().type_info();
					m_deque.push_back(Element(type_info, hasher(it->element()) ));
				}
			}

			void compare_all(const DENSE_CONTAINER & i_container)
			{
				DENSITY_TEST_ASSERT(m_deque.empty() == i_container.empty());
				DENSITY_TEST_ASSERT(i_container.empty() == (i_container.begin() == i_container.end()));

				size_t index = 0;

				const auto end_it = i_container.end();
				for (auto it = i_container.begin(); it != end_it; it++)
				{
					auto hasher = it.type().template get_feature<detail::FeatureHash>();
					const auto & type_info = it.type().type_info();
					DENSITY_TEST_ASSERT(type_info == *m_deque[index].m_type_info &&
						hasher(it.element()) == m_deque[index].m_hash );
					index++;
				}

				DENSITY_TEST_ASSERT(index == m_deque.size());
			}

			void compare_at(size_t i_at, const typename DENSE_CONTAINER::runtime_type & i_type, const void * i_element)
			{
				DENSITY_TEST_ASSERT(i_at < m_deque.size());
				DENSITY_TEST_ASSERT(*m_deque[i_at].m_type_info == i_type.type_info());
				const auto element_hash = i_type.template get_feature<detail::FeatureHash>()(i_element);
				DENSITY_TEST_ASSERT(element_hash == m_deque[i_at].m_hash);
			}

			void compare_front(const typename DENSE_CONTAINER::runtime_type & i_type, const void * i_element)
			{
				DENSITY_TEST_ASSERT(m_deque.size() > 0);
				compare_at(0, i_type, i_element);
			}

			void compare_back(const typename DENSE_CONTAINER::runtime_type & i_type, const void * i_element)
			{
				DENSITY_TEST_ASSERT(m_deque.size() > 0);
				compare_at(m_deque.size() - 1, i_type, i_element);
			}

			template <typename TYPE>
				void insert_at(size_t i_at, const TYPE & i_element, size_t i_count = 1)
			{
				try
				{
					using runtime_type = typename DENSE_CONTAINER::runtime_type;
					const auto type = runtime_type::template make<TYPE>();
					Element new_element{ &type.type_info(), type.template get_feature<detail::FeatureHash>()(&i_element) };
					m_deque.insert(m_deque.begin() + i_at, i_count, new_element);
				}
				catch (...)
				{
					throw BasicGuaranteeException("insert_at failed");
				}
			}

			template <typename TYPE>
				void push_back(const TYPE & i_element)
			{
				insert_at(m_deque.size(), i_element, 1);
			}

			void erase_at(size_t i_at, size_t i_count = 1)
			{
				try
				{
					DENSITY_TEST_ASSERT(m_deque.size() <= i_at + i_count);
					m_deque.erase(m_deque.begin() + i_at, m_deque.begin() + i_at + i_count);
				}
				catch (...)
				{
					throw BasicGuaranteeException("erase failed");
				}
			}

			void pop_back()
			{
				m_deque.pop_back();
			}

			void pop_front()
			{
				m_deque.pop_front();
			}

			bool empty() const { return m_deque.empty(); }

			typename std::deque<Element>::size_type size() const { return m_deque.size(); }

		private:
			std::deque<Element> m_deque;
		};

		/** This class template rapresent a test session to be run on a container implementation.
			The tested container is tested to fullfill the strong exception guarantee: whether or
			not an exception is thrown during a test case, the DENSE_CONTAINER is compared to the
			shadow container.
		*/
		template <typename DENSE_CONTAINER, typename BASE_TYPE>
			class ContainerTest
		{
		public:

			using TestCaseFunction = std::function<void(std::mt19937 & i_random)>;

			ContainerTest()
				: m_total_probability(0.)
			{
				using namespace std::placeholders;

				add_test_case("copy_and_assignment", std::bind(&ContainerTest::test_copy_and_assignment, this, _1));
				/*add_test_case("consume_until_empty", std::bind(&ContainerTest::builtin_test_case_consume_until_empty, this, _1));
				add_test_case("consume_n_times", std::bind(&ContainerTest::builtin_test_case_consume_n_times, this, _1));*/
			}

			void add_test_case(const char * i_name, std::function< void(std::mt19937 & i_random) > && i_function,
				double i_probability = 1. )
			{
				m_total_probability += i_probability;
				m_test_cases.push_back({i_name, std::move(i_function), i_probability});
			}

			void step(std::mt19937 & i_random)
			{
				// pick a random test from m_test_cases, and execute it
				auto target_random_value = std::uniform_real_distribution<double>(0., m_total_probability)(i_random);
				double cumulative = 0.;
				for (auto & test_case : m_test_cases)
				{
					cumulative += test_case.m_probability;
					if (target_random_value < cumulative)
					{
						try
						{
							test_case.m_function(i_random);
							test_case.m_executions++;
						}
						catch (typename ShadowContainer<DENSE_CONTAINER>::BasicGuaranteeException)
						{
							// the shadow container could not be updated
							throw;
						}
						catch (...)
						{
							compare();
							throw;
						}
						break;
					}
				}
                
				compare();
			}

			// check for equality m_dense_container and shadow_container()
			void compare()
			{
				/*if (!shadow_container().empty())
				{
					DENSITY_TEST_ASSERT(m_dense_container.front() == *shadow_container().front());
				}*/
				m_shadow_container.compare_all(m_dense_container);
			}

			const DENSE_CONTAINER & dense_container() const { return m_dense_container; }
			DENSE_CONTAINER & dense_container() { return m_dense_container; }

			ShadowContainer<DENSE_CONTAINER> & shadow_container() { return m_shadow_container; }
			const ShadowContainer<DENSE_CONTAINER> & shadow_container() const { return m_shadow_container; }

		private:

			void test_copy_and_assignment(std::mt19937 & /*i_random*/)
			{
				/* Assign m_dense_container from a copy of itself. m_dense_container and shadow_container() must preserve the equality. */
				auto copy = m_dense_container;
				m_dense_container = copy;
                
				// check the size with the iterators
				const auto size_1 = std::distance(m_dense_container.cbegin(), m_dense_container.cend());
				const auto size_2 = std::distance(copy.cbegin(), copy.cend());
				DENSITY_TEST_ASSERT(size_1 == size_2);

				// move construct m_dense_container to tmp
				auto tmp(std::move(m_dense_container));
				DENSITY_TEST_ASSERT(m_dense_container.empty());
				const auto size_3 = std::distance(tmp.cbegin(), tmp.cend());
				DENSITY_TEST_ASSERT(size_1 == size_3);

				// move assign tmp to m_dense_container
				m_dense_container = std::move(tmp);
			}

		private: // data members
			NoLeakScope m_no_leak_scope;            
			DENSE_CONTAINER m_dense_container;
			ShadowContainer<DENSE_CONTAINER> m_shadow_container;

			struct TestCase
			{
				TestCase(std::string i_name, TestCaseFunction i_function, double i_probability)
					: m_name(std::move(i_name)), m_function(std::move(i_function)), m_probability(i_probability), m_executions(0)
				{
				}

				std::string m_name;
				TestCaseFunction m_function;
				double m_probability;
				uint64_t m_executions;
			};
			std::vector<TestCase> m_test_cases;
			double m_total_probability;
		}; // class template ContainerTest

    } // namespace detail
    
} // namespace density
