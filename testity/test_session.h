
#pragma once
#include "test_tree.h"
#include "detail\environment.h"
#include <chrono>
#include <deque>

namespace testity
{
	using Duration = std::chrono::nanoseconds;

	class Session;

	class Results
	{
	public:

		Results(const TestTree & i_test_tree, const Session & i_session) : m_test_tree(i_test_tree), m_session(i_session) {}

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
			{
				return m_test == i_source.m_test && m_cardinality == i_source.m_cardinality;
			}
		};
		struct TestIdHash
		{
			size_t operator() (const TestId & i_test_id) const
			{
				return std::hash<const void*>()(i_test_id.m_test) ^ std::hash<size_t>()(i_test_id.m_cardinality);
			}
		};
		std::unordered_multimap< TestId, Duration, TestIdHash > m_performance_results;
		const TestTree & m_test_tree;
		const Session & m_session;
		Environment m_environment;
	};

	class Session
	{
	public:

		Results run(const TestTree & i_test_tree, std::ostream & i_dest_stream) const;

		struct Config
		{
			bool m_test_functionality = true;
			bool m_test_exception_safeness = true;
			bool m_test_performances = true;
			bool m_deterministic = true;
			bool m_random_shuffle = true;
			size_t m_functionality_repetitions = 2;
			size_t m_performance_repetitions = 8;
		};

		void set_config(const Config & i_config) { m_config = i_config; }

		const Config & config() const { return m_config; }

	private:

		using Operations = std::deque<std::function<void(Results & results)>>;
		void generate_functionality_operations(const TestTree & i_test_tree, Operations & i_dest) const;
		void generate_performance_operations(const TestTree & i_test_tree, Operations & i_dest) const;

	private:
		Config m_config;
	};

} // namespace testity
