
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "test_tree.h"
#include <string.h>
#include <sstream>
#include <random>
#include <fstream>

namespace testity
{
	namespace
	{
		template <typename PREEDICATE>
			static void for_each_token(const char * i_path, PREEDICATE && i_predicate)
		{
			const char * remaining_path = i_path;
			for(;;)
			{
				const size_t token_length = strcspn(remaining_path, "/\\");
				i_predicate(remaining_path, token_length);
				remaining_path += token_length;
				if (*remaining_path == 0)
					break;
				remaining_path++;
			}
		}
	}

	TestEnvironment::TestEnvironment()
		: m_startup_clock(std::chrono::system_clock::now())
	{
		std::ostringstream compiler, os;
			
		// detect the compiler
		#ifdef _MSC_VER
			compiler << "MSC - " << _MSC_VER;
		#elif __clang__
			compiler << "Clang - "<< __clang_major__ << '.' << __clang_minor__ << '.' << __clang_patchlevel__;
		#else
			compiler << "unknown";
		#endif

		// detect the os
		#ifdef _WIN32
			os << "Windows";
		#else
			os << "unknown";
		#endif

		m_compiler = compiler.str();
		m_operating_sytem = os.str();
		m_system_info = "unknown";
	}

	TestTree & TestTree::operator [] (const char * i_path)
	{
		using namespace std;

		auto node = this;
		for_each_token(i_path, [&node](const char * i_token, size_t i_token_length) {

			auto entry_it = find_if(node->m_children.begin(), node->m_children.end(), 
				[i_token, i_token_length](const TestTree & i_entry) { return i_entry.name().compare(0, string::npos, i_token, i_token_length); });

			if (entry_it != node->m_children.end())
			{
				node = &*entry_it;
			}
			else
			{
				node->m_children.emplace_back(TestTree(string(i_token, i_token_length)));
				node = &node->m_children.back();
			}
		});

		return *node;
	}

	const TestTree * TestTree::find(const char * i_path) const
	{
		using namespace std;

		auto node = this;
		for_each_token(i_path, [&node](const char * i_token, size_t i_token_length) {

			if (node != nullptr)
			{
				auto entry_it = find_if(node->m_children.begin(), node->m_children.end(),
					[i_token, i_token_length](const TestTree & i_entry) { return i_entry.name().compare(0, string::npos, i_token, i_token_length); });

				if (entry_it != node->m_children.end())
				{
					node = &*entry_it;
				}
				else
				{
					node = nullptr;
				}
			}
		});

		return node;
	}

	TestTree * TestTree::find(const char * i_path)
	{
		using namespace std;

		auto node = this;
		for_each_token(i_path, [&node](const char * i_token, size_t i_token_length) {

			if (node != nullptr)
			{
				auto entry_it = find_if(node->m_children.begin(), node->m_children.end(),
					[i_token, i_token_length](const TestTree & i_entry) { return i_entry.name().compare(0, string::npos, i_token, i_token_length); });

				if (entry_it != node->m_children.end())
				{
					node = &*entry_it;
				}
				else
				{
					node = nullptr;
				}
			}
		});

		return node;
	}

	void Results::add_result(const BenchmarkTest * i_test, size_t i_cardinality, Duration i_duration)
	{
		m_performance_results.insert(std::make_pair(TestId{ i_test, i_cardinality }, i_duration));
	}

	void Results::save_to(const char * i_filename) const
	{
		std::ofstream file(i_filename);
		save_to(file);
	}

	void Results::save_to(std::ostream & i_ostream) const
	{
		save_to_impl("", m_test_tree, i_ostream);
	}

	void Results::save_to_impl(std::string i_path, const TestTree & i_test_tree, std::ostream & i_ostream) const
	{
		for (const auto & performance_test_group : i_test_tree.performance_tests())
		{
			i_ostream << i_path << std::endl;

			for (size_t cardinality = performance_test_group.cardinality_start();
				cardinality < performance_test_group.cardinality_end(); cardinality += performance_test_group.cardinality_step())
			{
				i_ostream << cardinality << '\t';

				for (auto & test : performance_test_group.tests())
				{
					auto range = m_performance_results.equal_range(TestId{ &test, cardinality });
					bool is_first = true;
					for (auto it = range.first; it != range.second; ++it)
					{
						if (!is_first)
						{
							i_ostream << ", ";
						}
						else
						{
							is_first = false;
						}
						i_ostream << it->second.count();
					}

					i_ostream << '\t';
				}

				i_ostream << std::endl;
			}
		}

		for (const auto & node : i_test_tree.children())
		{
			save_to_impl(i_path + node.name() + '/', node, i_ostream);
		}
	}

	void Session::generate_operations(const TestTree & i_test_tree, Operations & i_dest) const
	{
		for (auto & test_group : i_test_tree.performance_tests())
		{
			for (size_t cardinality = test_group.cardinality_start();
				cardinality < test_group.cardinality_end(); cardinality += test_group.cardinality_step())
			{
				for (auto & test : test_group.tests())
				{
					i_dest.push_back([&test, cardinality](Results & results) {
						using namespace std::chrono;
						const auto time_before = high_resolution_clock::now();
						test.function()(cardinality);
						const auto duration = duration_cast<nanoseconds>(high_resolution_clock::now() - time_before);
						results.add_result(&test, cardinality, duration);
					});
				}
			}
		}

		for (auto & child : i_test_tree.children())
		{
			generate_operations(child, i_dest);
		}
	}

	Results Session::run(const TestTree & i_test_tree, std::ostream & i_dest_stream) const
	{
		using namespace std;

		mt19937 random = m_deterministic ? mt19937() : mt19937(random_device()());

		Operations operations;
		for (size_t repetition_index = 0; repetition_index < m_repetitions; repetition_index++)
		{
			generate_operations(i_test_tree, operations);
		}		
		const auto operations_size = operations.size();

		if (m_random_shuffle)
		{
			i_dest_stream << "randomizing operations..." << endl;
			for (Operations::size_type index = 0; index < operations_size; index++)
			{
				swap(operations[index], operations[uniform_int_distribution<Operations::size_type>(0, operations_size - 1)(random)]);
			}
		}

		i_dest_stream << "performing tests..." << endl;
		Results results(i_test_tree);
		for (Operations::size_type index = 0; index < operations_size; index++)
		{
			operations[index](results);
		}
		return results;
	}

} // namespace testity
