
#pragma once
#include "test_tree.h"
#include "detail\environment.h"
#include <chrono>
#include <deque>
#include <type_traits>

namespace testity
{
	using Duration = std::chrono::nanoseconds;

	struct TestConfig
	{
		bool m_deterministic = true;
		std::random_device::result_type m_random_seed = 0;
		bool m_random_shuffle = true;
		size_t m_functionality_repetitions = 2;
		size_t m_performance_repetitions = 8;
	};

	class Results
	{
	public:

		Results(const TestTree & i_test_tree, const TestConfig & i_config, std::random_device::result_type i_random_seed)
			: m_test_tree(i_test_tree), m_config(i_config), m_random_seed(i_random_seed)  {}

		void add_result(const detail::PerformanceTest * i_test, size_t i_cardinality, Duration i_duration);

		void save_to(const char * i_filename) const;

		void save_to(std::ostream & i_ostream) const;

	private:
		void save_to_impl(std::string i_path, const TestTree & i_test_tree, std::ostream & i_ostream) const;

	private:
		struct TestId
		{
			const detail::PerformanceTest * m_test;
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
		const TestConfig m_config;
		const detail::Environment m_environment;
		const std::random_device::result_type m_random_seed;
	};

	enum class TestFlags : unsigned
	{
		none = 0,
		FunctionalityTest = 1 << 0,
		FunctionalityExceptionTest = 1 << 1,
		PerformanceTests = 1 << 2,
		All = (1 << 3) - 1,
	};

	inline constexpr TestFlags operator | (TestFlags i_first, TestFlags i_second)
	{
		return static_cast<TestFlags>(static_cast<unsigned>(i_first) | static_cast<unsigned>(i_second));
	}

	Results run_session(const TestTree & i_test_tree, TestFlags i_flags = TestFlags::All, const TestConfig & i_config = TestConfig());

	Results run_session(const TestTree & i_test_tree, TestFlags i_flags, std::ostream & i_progression_out_stream, const TestConfig & i_config = TestConfig());

	class Session
	{
	public:

		Results run(const TestTree & i_test_tree, TestFlags i_flags = TestFlags::All);

		Results run(const TestTree & i_test_tree, TestFlags i_flags, std::ostream & i_progression_out_stream);

		void set_config(const TestConfig & i_config) { m_config = i_config; }

		const TestConfig & config() const { return m_config; }

	private:

		using Operations = std::deque<std::function<void(Results & results, FunctionalityContext & i_functionality_context)>>;

		void generate_functionality_operations(const TestTree & i_test_tree, Operations & i_dest);
		
		void generate_performance_operations(const TestTree & i_test_tree, Operations & i_dest);

		Results run_impl(const TestTree & i_test_tree, TestFlags i_flags, std::ostream * i_progression_out_stream);

	private:
		TestConfig m_config;
		std::unordered_map<size_t, void*> m_functionality_targets;
	};

} // namespace testity
