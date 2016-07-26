
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


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
        size_t m_functionality_repetitions = 12800;
        size_t m_performance_repetitions = 8;
		size_t m_exceptions_repetitions = 12800;
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

	struct Progression
	{
		using Clock = std::chrono::steady_clock;

		std::string m_label;
		Clock::time_point m_start_time;
		double m_completion_factor;
		std::chrono::duration<double> m_elapsed_time;
		std::chrono::duration<double> m_remaining_time_extimate;
	};

	using ProgressionCallback = std::function<void(const Progression&)>;

    Results run_session(const TestTree & i_test_tree, TestFlags i_flags = TestFlags::All, 
		const TestConfig & i_config = TestConfig(), ProgressionCallback i_progression_callback = ProgressionCallback());


} // namespace testity
