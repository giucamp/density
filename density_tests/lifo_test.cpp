
#include "../density/lifo.h"
#include "../testity/testing_utils.h"
#include <random>
#include <vector>
#include <memory>
#include <algorithm>

namespace density
{
	namespace tests
	{
		class LifoTestItem
		{
		public:
			virtual void check() const = 0;
			virtual ~LifoTestItem() {}
		};

		template <typename TYPE>
			class LifoTestArray : public LifoTestItem
		{
		public:
			LifoTestArray(const lifo_array<TYPE> & i_array)
				: m_array(i_array)
			{
				m_vector.insert(m_vector.end(), i_array.begin(), i_array.end());
			}

			void check() const override
			{
				const size_t size = m_array.size();
				DENSITY_TEST_ASSERT(m_vector.size() == size);
				for(size_t i = 0; i < size; i++)
				{
					DENSITY_TEST_ASSERT( m_vector[i] == m_array[i] );
				}
			}

		private:
			const lifo_array<TYPE> & m_array;
			std::vector<TYPE> m_vector;
		};

		struct LifoTestContext
		{
			std::mt19937 m_random;
			int m_curr_depth = 0;
			int m_max_depth = 0;
			std::vector< std::unique_ptr< LifoTestItem > > m_tests;

			template <typename TYPE>
				void push_test(const lifo_array<TYPE> & i_array)
			{
				m_tests.push_back(std::make_unique<LifoTestArray<TYPE>>(i_array));
			}

			void pop_test()
			{
				m_tests.pop_back();
			}

			void check() const
			{
				for (const auto & test : m_tests)
				{
					test->check();
				}
			}
		};

		void lifo_test_push(LifoTestContext & i_context);

		void lifo_test_push_char(LifoTestContext & i_context)
		{
			std::uniform_int_distribution<unsigned> rnd(0, 100);
			lifo_array<unsigned char> arr(std::uniform_int_distribution<size_t>(0, 7)(i_context.m_random));
			std::generate(arr.begin(), arr.end(), [&i_context, &rnd] { return static_cast<unsigned char>(rnd(i_context.m_random) % 128); });
			i_context.push_test(arr);
			lifo_test_push(i_context);
			i_context.pop_test();
		}

		void lifo_test_push_int(LifoTestContext & i_context)
		{
			std::uniform_int_distribution<int> rnd(-1000, 1000);
			lifo_array<int> arr(std::uniform_int_distribution<size_t>(0, 7)(i_context.m_random));
			std::generate(arr.begin(), arr.end(), [&i_context, &rnd] { return rnd(i_context.m_random); });

			i_context.push_test(arr);
			lifo_test_push(i_context);
			i_context.pop_test();
		}

		void lifo_test_push_double(LifoTestContext & i_context)
		{
			std::uniform_real_distribution<double> rnd(-1000., 1000.);
			lifo_array<double> arr(std::uniform_int_distribution<size_t>(0, 7)(i_context.m_random));
			std::generate(arr.begin(), arr.end(), [&i_context, &rnd] { return rnd(i_context.m_random); });
			
			i_context.push_test(arr);
			lifo_test_push(i_context);
			i_context.pop_test();
		}

		void lifo_test_push(LifoTestContext & i_context)
		{
			if (i_context.m_curr_depth < i_context.m_max_depth)
			{
				using Func = void(*)(LifoTestContext & i_context);
				Func tests[] = { lifo_test_push_char, lifo_test_push_int, lifo_test_push_double };

				i_context.m_curr_depth++;
				
				const auto iter_count = std::uniform_int_distribution<int>(0, 5)(i_context.m_random);
				for (int i = 0; i < iter_count; i++)
				{
					const auto random_index = std::uniform_int_distribution<size_t>(0, sizeof(tests) / sizeof(tests[0]) - 1)(i_context.m_random);
					tests[random_index](i_context);

					i_context.check();
				}

				i_context.m_curr_depth--;
			}
		}

		void lifo_test()
		{
			LifoTestContext context;
			context.m_max_depth = 14;

			lifo_test_push(context);
		}

	} // namespace tests

	void lifo_test()
	{
		std::mt19937 random;
		tests::lifo_test();
	}

} // namespace density

