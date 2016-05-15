
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "correctness_test_context.h"
#include <memory>
#include <string>
#include <deque>
#include <chrono>
#include <functional>
#include <algorithm>
#include <ostream>
#include <unordered_map>

#define DENSITY_MAKE_BENCHMARK_TEST(...)			testity::BenchmarkTest(#__VA_ARGS__, [](size_t i_cardinality) { __VA_ARGS__; } )

namespace testity
{
	class TestEnvironment
	{
	public:

		TestEnvironment();

		const std::string & operating_sytem() const { return m_operating_sytem; }
		const std::string & compiler() const { return m_compiler; }
		const std::string & system_info() const { return m_system_info; }
		size_t sizeof_pointer() const { return sizeof(void*); }
		const std::chrono::system_clock::time_point & startup_clock() const { return m_startup_clock; }

	private:
		std::string m_operating_sytem;
		std::string m_compiler;
		std::string m_system_info;
		std::chrono::system_clock::time_point m_startup_clock;
	};

	class BenchmarkTest
	{
	public:

		using TestFunction = void(size_t i_cardinality);

		BenchmarkTest(std::string i_source_code, std::function<TestFunction> i_function)
			: m_source_code(std::move(i_source_code)), m_function(std::move(i_function))
				{ }

		const std::string & source_code() const { return m_source_code; }
		const std::function<TestFunction> & function() const { return m_function; }

	private:
		std::string m_source_code;
		std::function<TestFunction> m_function;
	};

	class PerformanceTestGroup
	{
	public:

		PerformanceTestGroup(std::string i_name)
			: m_name(std::move(i_name)) {}

		void add_test(BenchmarkTest i_test)
		{
			m_tests.push_back(std::move(i_test));
		}

		void add_test(const char * i_source_file, int i_start_line,
			std::function<BenchmarkTest::TestFunction> i_function, int i_end_line);

		size_t cardinality_start() const { return m_cardinality_start; }
		size_t cardinality_step() const { return m_cardinality_step; }
		size_t cardinality_end() const { return m_cardinality_end; }
		const std::vector<BenchmarkTest> & tests() const { return m_tests; }

	private:
		size_t m_cardinality_start = 0;
		size_t m_cardinality_step = 10000;
		size_t m_cardinality_end = 800000;
		std::vector<BenchmarkTest> m_tests;
		std::string m_name;
	};

	using CorrectnessTestFunction = void(*)(CorrectnessTestContext & i_context);

	class CorrectnessTest
	{
	public:		

		CorrectnessTest(CorrectnessTestFunction i_function)
			: m_function(i_function)
		{
		}

	private:
		CorrectnessTestFunction m_function;
	};

	class TestTree
	{
	public:

		explicit TestTree(std::string i_name) : m_name(std::move(i_name)) {}

		const std::string & name() const { return m_name; }

		void add_correctess_test(CorrectnessTest i_correctess_test)
		{
			m_correctness_tests.emplace_back(std::move(i_correctess_test));
		}

		void add_performance_test(PerformanceTestGroup i_group)
		{
			m_performance_tests.emplace_back(std::move(i_group));
		}

		const std::vector< CorrectnessTest > & correctness_tests() const { return m_correctness_tests; }
		const std::vector< PerformanceTestGroup > & performance_tests() const { return m_performance_tests;  }

		const std::vector<TestTree> & children() const { return m_children; }

		TestTree & operator [] (const char * i_path);

		const TestTree * find(const char * i_path) const;

		TestTree * find(const char * i_path);

	private:
		std::string m_name;
		std::vector< CorrectnessTest > m_correctness_tests;
		std::vector< PerformanceTestGroup > m_performance_tests;
		std::vector< TestTree > m_children;
	};

	using Duration = std::chrono::nanoseconds;

	class Results
	{
	public:

		Results(const TestTree & i_test_tree, size_t i_repetitions) : m_test_tree(i_test_tree), m_repetitions(i_repetitions) {}

		void add_result(const BenchmarkTest * i_test, size_t i_cardinality, Duration i_duration);

		void save_to(const char * i_filename) const;

		void save_to(std::ostream & i_ostream) const;

	private:
		void save_to_impl(std::string i_path, const TestTree & i_test_tree, std::ostream & i_ostream) const;

	private:
		struct TestId
		{
			const BenchmarkTest * m_test;
			size_t m_cardinality;

			bool operator == (const TestId & i_source) const
				{ return m_test == i_source.m_test && m_cardinality == i_source.m_cardinality; }
		};
		struct TestIdHash
		{
			size_t operator() (const TestId & i_test_id) const
				{ return std::hash<const void*>()(i_test_id.m_test) ^ std::hash<size_t>()(i_test_id.m_cardinality); }
		};
		std::unordered_multimap< TestId, Duration, TestIdHash > m_performance_results;
		const TestTree & m_test_tree;
		TestEnvironment m_environment;
		size_t m_repetitions;
	};
	
	class Session
	{
	public:
		Results run(const TestTree & i_test_tree, std::ostream & i_dest_stream) const;

	private:

		using Operations = std::deque<std::function<void(Results & results)>>;
		void generate_operations(const TestTree & i_test_tree, Operations & i_dest) const;

	private:
		bool m_deterministic = true;
		bool m_random_shuffle = true;
		size_t m_repetitions = 8;
	};

} // namespace testity

