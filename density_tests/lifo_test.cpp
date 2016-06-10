
#include "../density/lifo.h"
#include "../testity/testing_utils.h"
#include <random>
#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstdlib>

namespace density
{
	namespace tests
	{
		size_t random_alignment(std::mt19937 & i_random)
		{
			size_t log2_max = 0;
			while ((static_cast<size_t>(1) << log2_max) < alignof(std::max_align_t))
			{
				log2_max++;
			}

			return static_cast<size_t>(1) << std::uniform_int_distribution<size_t>(0, log2_max * 2)(i_random);
		}

		class LifoTestItem
		{
		public:
			virtual void check() const = 0;
			virtual bool resize(std::mt19937 & /*i_random*/) { return false; }
			virtual ~LifoTestItem() {}
		};

		template <typename TYPE>
			class LifoTestArray : public LifoTestItem
		{
		public:
			LifoTestArray(const lifo_array<TYPE> & i_array)
				: m_array(i_array)
			{
				DENSITY_TEST_ASSERT( is_address_aligned( i_array.data(), alignof(TYPE) ) );
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

		class LifoTestBuffer : public LifoTestItem
		{
		public:
			LifoTestBuffer(lifo_buffer<> & i_buffer)
				: m_buffer(i_buffer)
			{
				m_vector.insert(m_vector.end(), 
					static_cast<unsigned char*>(m_buffer.data()), 
					static_cast<unsigned char*>(m_buffer.data()) + m_buffer.mem_size() );
			}

			void check() const override
			{
				DENSITY_TEST_ASSERT(m_buffer.mem_size() == m_vector.size());
				DENSITY_TEST_ASSERT(memcmp(m_buffer.data(), m_vector.data(), m_vector.size())==0);
			}

			virtual bool resize(std::mt19937 & i_random) override
			{ 
				check();
				const auto old_size = m_buffer.mem_size();
				const auto new_size = std::uniform_int_distribution<size_t>(0, 32)(i_random);

				const bool custom_alignment = std::uniform_int_distribution<int>(0, 100)(i_random) > 50;
				const bool preserve = false; //  std::uniform_int_distribution<int>(0, 100)(i_random) > 50;

				m_vector.resize(new_size);

				if (custom_alignment)
				{
					const auto alignment = random_alignment(i_random);
					m_buffer.resize(new_size, alignment);
					DENSITY_TEST_ASSERT(is_address_aligned(m_buffer.data(), alignment));
				}
				else
				{
					m_buffer.resize(new_size);					
				}				
								
				if (preserve)
				{
					if (old_size < new_size)
					{
						std::uniform_int_distribution<unsigned> rnd(0, 100);
						std::generate(static_cast<unsigned char*>(m_buffer.data()) + old_size,
							static_cast<unsigned char*>(m_buffer.data()) + new_size,
							[&i_random, &rnd] { return static_cast<unsigned char>(rnd(i_random)); });
						memcpy(m_vector.data() + old_size,
							static_cast<unsigned char*>(m_buffer.data()) + old_size,
							new_size - old_size);
					}
				}
				else
				{
					std::uniform_int_distribution<unsigned> rnd(0, 100);
					std::generate(static_cast<unsigned char*>(m_buffer.data()),
						static_cast<unsigned char*>(m_buffer.data()) + new_size,
						[&i_random, &rnd] { return static_cast<unsigned char>(rnd(i_random)); });
					memcpy(m_vector.data(),
						static_cast<unsigned char*>(m_buffer.data()),
						new_size);
				}
				check();
				return true; 
			}

		private:
			lifo_buffer<> & m_buffer;
			std::vector<unsigned char> m_vector;
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

			void push_test(lifo_buffer<> & i_buffer)
			{
				m_tests.push_back(std::make_unique<LifoTestBuffer>(i_buffer));
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

			void resize(std::mt19937 & i_random)
			{
				if (!m_tests.empty())
				{
					m_tests.back()->resize(i_random);
				}
			}
		};

		void lifo_test_push(LifoTestContext & i_context);

		void lifo_test_push_buffer(LifoTestContext & i_context)
		{
			std::uniform_int_distribution<unsigned> rnd(0, 100);
			lifo_buffer<> buffer(std::uniform_int_distribution<size_t>(0, 32)(i_context.m_random));
			DENSITY_TEST_ASSERT(is_address_aligned(buffer.data(), alignof(std::max_align_t)));
			std::generate(
				static_cast<unsigned char*>(buffer.data()), 
				static_cast<unsigned char*>(buffer.data()) + buffer.mem_size(), 
				[&i_context, &rnd] { return static_cast<unsigned char>(rnd(i_context.m_random) % 128); });
			i_context.push_test(buffer);
			lifo_test_push(i_context);
			i_context.pop_test();
		}

		void lifo_test_push_buffer_aligned(LifoTestContext & i_context)
		{
			const auto alignment = random_alignment(i_context.m_random);
			std::uniform_int_distribution<unsigned> rnd(0, 100);
			lifo_buffer<> buffer(std::uniform_int_distribution<size_t>(0, 32)(i_context.m_random), alignment);
			DENSITY_TEST_ASSERT(is_address_aligned(buffer.data(), alignment));
			std::generate(
				static_cast<unsigned char*>(buffer.data()),
				static_cast<unsigned char*>(buffer.data()) + buffer.mem_size(),
				[&i_context, &rnd] { return static_cast<unsigned char>(rnd(i_context.m_random) % 128); });
			i_context.push_test(buffer);
			lifo_test_push(i_context);
			i_context.pop_test();
		}

		void lifo_test_push_char(LifoTestContext & i_context)
		{
			std::uniform_int_distribution<unsigned> rnd(0, 100);
			lifo_array<unsigned char> arr(std::uniform_int_distribution<size_t>(0, 20)(i_context.m_random));
			std::generate(arr.begin(), arr.end(), [&i_context, &rnd] { return static_cast<unsigned char>(rnd(i_context.m_random)); });
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

		void lifo_test_push_wide_alignment(LifoTestContext & i_context)
		{
			union alignas(alignof(std::max_align_t) * 2) AlignedType
			{
				int m_value;
				std::max_align_t m_unused[2];
				bool operator == (const AlignedType & i_other) const
					{ return m_value == i_other.m_value;}
			};

			std::uniform_int_distribution<int> rnd(-1000, 1000);
			lifo_array<AlignedType> arr(std::uniform_int_distribution<size_t>(0, 7)(i_context.m_random));
			std::generate(arr.begin(), arr.end(), [&i_context, &rnd] { return AlignedType{ rnd(i_context.m_random) }; });

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
				Func tests[] = { lifo_test_push_buffer, lifo_test_push_char, lifo_test_push_int, 
					lifo_test_push_double, lifo_test_push_wide_alignment };

				i_context.m_curr_depth++;
				
				const auto iter_count = std::uniform_int_distribution<int>(0, 5)(i_context.m_random);
				for (int i = 0; i < iter_count; i++)
				{
					i_context.resize(i_context.m_random);

					const auto random_index = std::uniform_int_distribution<size_t>(0, sizeof(tests) / sizeof(tests[0]) - 1)(i_context.m_random);
					tests[random_index](i_context);

					i_context.check();

					i_context.resize(i_context.m_random);
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

		struct GraphNode
		{

		};

		void dijkstra_path_find(const GraphNode * i_nodes, size_t i_node_count, size_t i_initial_node_index)
		{
			lifo_array<float> min_distance(i_node_count, std::numeric_limits<float>::max() );
			min_distance[i_initial_node_index] = 0.f;

			// ...

			(void)i_nodes;
		}

		void string_io()
		{
			using namespace std;
			vector<string> strings{ "string", "long string", "very long string", "much longer string!!!!!!" };
			uint32_t len = 0;

			#ifndef _MSC_VER
				auto file = tmpfile();
			#else
				FILE * file = nullptr;
				tmpfile_s(&file);
			#endif
			for (const auto & str : strings)
			{
				len = static_cast<uint32_t>( str.length() + 1);
				fwrite(&len, sizeof(len), 1, file);
				fwrite(str.c_str(), 1, len, file);
			}

			rewind(file);
			lifo_buffer<> buff(8);
			while (fread(&len, sizeof(len), 1, file) == 1)
			{
				buff.resize(len);
				fread(buff.data(), len, 1, file);
				cout << static_cast<char*>(buff.data()) << endl;
			}

			fclose(file);
		}

	} // namespace tests

	void lifo_test()
	{
		tests::string_io();
		std::mt19937 random;
		tests::lifo_test();
	}

} // namespace density

