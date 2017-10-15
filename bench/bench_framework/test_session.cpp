
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "test_session.h"
#include "environment.h"
#include <fstream>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <random>
#include <chrono>
#include <algorithm>
#ifdef _MSC_VER
    #include <time.h>
#endif

namespace density_bench
{
    namespace detail
    {
        struct ProgressionUpdater
        {
            Progression m_progression;
            ProgressionCallback m_callback;
            std::chrono::high_resolution_clock::time_point m_next_callback_call;
            std::chrono::high_resolution_clock::duration m_callback_call_period =
                std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::seconds(5));

            ProgressionUpdater(const char * i_label = "", ProgressionCallback i_callback = ProgressionCallback())
                : m_callback(i_callback)
            {
                if (m_callback)
                {
                    m_progression.m_label = i_label;
                    m_progression.m_start_time = Progression::Clock::now();
                    m_progression.m_completion_factor = 0.;
                    m_next_callback_call = std::chrono::high_resolution_clock::now() - m_callback_call_period;
                }
            }

            void update(size_t i_current, size_t i_total)
            {
                if (m_callback)
                {
                    auto const now = Progression::Clock::now();

                    if (now > m_next_callback_call)
                    {
                        m_next_callback_call = now + m_callback_call_period;

                        m_progression.m_completion_factor = static_cast<double>(i_current) / i_total;

                        m_progression.m_elapsed_time = now - m_progression.m_start_time;

                        m_progression.m_time_extimate_available = m_progression.m_completion_factor > 0.001;
                        if (m_progression.m_time_extimate_available)
                        {
                            auto const factor = (1.0 - m_progression.m_completion_factor) / m_progression.m_completion_factor;
                            m_progression.m_remaining_time_extimate = std::chrono::duration<double>(
                                m_progression.m_elapsed_time.count() * factor);
                        }

                        m_callback(m_progression);
                    }
                }
            }

            void update(int64_t i_current, int64_t i_total)
            {
                if (m_callback)
                {
                    auto const now = Progression::Clock::now();

                    if (now > m_next_callback_call)
                    {
                        m_next_callback_call = now + m_callback_call_period;

                        m_progression.m_completion_factor = static_cast<double>(i_current) / i_total;

                        m_progression.m_elapsed_time = now - m_progression.m_start_time;

                        m_progression.m_remaining_time_extimate = std::chrono::duration<double>(
                            m_progression.m_completion_factor > 0.0001 ? (m_progression.m_elapsed_time.count() / m_progression.m_completion_factor) : 0.);

                        m_callback(m_progression);
                    }
                }
            }
        };

        class Session
        {
        public:

            Results run(const TestTree & i_test_tree,
                ProgressionCallback i_progression_callback = ProgressionCallback());

            void set_config(const TestConfig & i_config) { m_config = i_config; }

            const TestConfig & config() const { return m_config; }

        private:

            using Operations = std::deque<std::function<void(Results & results)>>;

            void generate_performance_operations(const TestTree & i_test_tree, Operations & i_dest);

        private:
            TestConfig m_config;
        };

        // https://stackoverflow.com/questions/2896600/how-to-replace-all-occurrences-of-a-character-in-string
        std::string replace_all(std::string str, const std::string& from, const std::string& to)
        {
            size_t start_pos = 0;
            while((start_pos = str.find(from, start_pos)) != std::string::npos)
            {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
            }
            return str;
        }

    } // namespace detail

    namespace
    {
        struct StaticData
        {
            int64_t m_current_counter;
            int64_t m_except_at;
            StaticData() : m_current_counter(0), m_except_at(-1) {}
        };

        #if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below
            _declspec(thread) StaticData  * st_static_data;
        #else
            thread_local StaticData  * st_static_data;
        #endif
    }

    int64_t run_count_exception_check_points(std::function<void()> i_test)
    {
        StaticData static_data;
        st_static_data = &static_data;
        try
        {
            i_test();
        }
        catch (...)
        {
            st_static_data = nullptr;
            throw;
        }
        st_static_data = nullptr;
        return static_data.m_current_counter;
    }

    void Results::add_result(const PerformanceTest * i_test, size_t i_cardinality, Duration i_duration)
    {
        m_performance_results.insert(std::make_pair(TestId{ i_test, i_cardinality }, i_duration));
    }

    namespace detail
    {
        void Session::generate_performance_operations(const TestTree & i_test_tree, Operations & i_dest)
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
                generate_performance_operations(child, i_dest);
            }
        }

        Results Session::run(const TestTree & i_test_tree, ProgressionCallback i_progression_callback)
        {
            using namespace std;

            ProgressionUpdater progression;

            std::mt19937 random;

            // generate operation array
            Operations operations;
            for (size_t repetition_index = 0; repetition_index < m_config.m_performance_repetitions; repetition_index++)
            {
                generate_performance_operations(i_test_tree, operations);
            }

            const auto operations_size = operations.size();

            if (m_config.m_random_shuffle)
            {
                progression = ProgressionUpdater("randomizing operations...", i_progression_callback);
                std::shuffle(operations.begin(), operations.end(), random);
            }

            progression = ProgressionUpdater("performing tests...", i_progression_callback);
            Results results(i_test_tree, m_config);
            for (Operations::size_type index = 0; index < operations_size; index++)
            {
                operations[index](results);
                progression.update(index, operations_size);
            }

            return results;
        }

    } // namespace detail

    void Results::save_to(const char * i_filename) const
    {
        std::ofstream file(i_filename, std::ios_base::out | std::ios_base::app);
        save_to(file);
    }

    void Results::save_to(std::ostream & i_ostream) const
    {
        save_to_impl("", m_test_tree, i_ostream);
    }

    void Results::save_to_impl(std::string i_path, const TestTree & i_test_tree, std::ostream & i_ostream) const
    {
        Environment environment;

        for (const auto & performance_test_group : i_test_tree.performance_tests())
        {
            i_ostream << "-------------------------------------" << std::endl;
            i_ostream << "PERFORMANCE_TEST_GROUP:" << i_path << std::endl;
            i_ostream << "NAME:" << performance_test_group.name() << std::endl;
            i_ostream << "VERSION_LABEL:" << performance_test_group.version_label() << std::endl;
            i_ostream << "COMPILER:" << environment.compiler() << std::endl;
            i_ostream << "OS:" << environment.operating_sytem() << std::endl;
            i_ostream << "SYSTEM:" << environment.system_info() << std::endl;
            i_ostream << "SIZEOF_POINTER:" << environment.sizeof_pointer() << std::endl;
            //i_ostream << "DETERMINISTIC:" << (m_config.m_deterministic ? "yes" : "no") << std::endl;
            //i_ostream << "RANDOM_SHUFFLE:" << (m_config.m_random_shuffle ? "yes (with std::mt19937)" : "no") << std::endl;

            i_ostream << "CARDINALITY_START:" << performance_test_group.cardinality_start() << std::endl;
            i_ostream << "CARDINALITY_STEP:" << performance_test_group.cardinality_step() << std::endl;
            i_ostream << "CARDINALITY_END:" << performance_test_group.cardinality_end() << std::endl;
            i_ostream << "MULTEPLICITY:" << m_config.m_performance_repetitions << std::endl;

            // write legend
            i_ostream << "LEGEND_START:" << std::endl;
            {
                for (auto & test : performance_test_group.tests())
                {
                    i_ostream << "TEST:" << test.source_code() << std::endl;
                }
            }
            i_ostream << "LEGEND_END:" << std::endl;

            // write table
            i_ostream << "TABLE_START:-----------------------" << std::endl;
            for (size_t cardinality = performance_test_group.cardinality_start();
                cardinality < performance_test_group.cardinality_end(); cardinality += performance_test_group.cardinality_step())
            {
                i_ostream << "ROW:";
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
            i_ostream << "TABLE_END:-----------------------" << std::endl;
            i_ostream << "PERFORMANCE_TEST_GROUP_END:" << i_path << std::endl;
        }

        for (const auto & node : i_test_tree.children())
        {
            save_to_impl(i_path + node.name() + '/', node, i_ostream);
        }
    }

    void Results::print_summary(std::ostream & i_ostream)
    {
        struct TestAvgDuration
        {
            double m_avg_duration{0.};
            double m_count{1.};
        };
        std::unordered_map<const PerformanceTest *, TestAvgDuration> tests;

        // compute average duration
        // http://stackoverflow.com/questions/1930454/what-is-a-good-solution-for-calculating-an-average-where-the-sum-of-all-values-e
        for(const auto & result : m_performance_results)
        {
            auto & test_data = tests[result.first.m_test];

            constexpr double mult = static_cast<double>(Duration::period::num) / static_cast<double>(Duration::period::den);
            test_data.m_avg_duration += (result.second.count() * mult - test_data.m_avg_duration) / test_data.m_count;
            test_data.m_count += 1.0;
        }

        m_test_tree.recursive_for_each_child([&tests, &i_ostream](const TestTree & i_test) {
            for (const auto & group : i_test.performance_tests())
            {
                struct TestResult
                {
                    std::string m_code;
                    double m_duration;
                };

                double max_duration = -1;
                std::vector<TestResult> results;
                for (const auto & test : group.tests())
                {
                    TestResult result;
                    result.m_code = test.source_code();
                    result.m_code = std::string("\t") + detail::replace_all(result.m_code, "#nl#", "\n\t");                    
                    result.m_duration = tests[&test].m_avg_duration;
                    max_duration = std::max(max_duration, result.m_duration);
                    results.push_back(result);
                }

                std::sort(results.begin(), results.end(), [](const TestResult & i_first, const TestResult & i_second) {
                    return i_first.m_duration >= i_second.m_duration;
                });

                i_ostream << "\n\n\n---------------------------------------\n";
                for (auto const & result : results)
                {
                    double const duration_percentage = (result.m_duration / max_duration) * 100.;
                    i_ostream << " * Duration: " << duration_percentage << "% (" << result.m_duration  << " secs)\n";
                    i_ostream << result.m_code << "\n---------------------------------------\n";
                }
            }
        });
    }

    Results run_session(const TestTree & i_test_tree, const TestConfig & i_config,
        ProgressionCallback i_progression_callback )
    {
        detail::Session session;
        session.set_config(i_config);
        return session.run(i_test_tree, i_progression_callback);
    }
}
