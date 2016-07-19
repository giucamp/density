
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#include "test_session.h"
#include "testity_common.h"
#include <fstream>
#include <ctime>
#include <iomanip>
#include <algorithm>
#ifdef _MSC_VER
    #include <time.h>
#endif

namespace testity
{
	namespace detail
	{
		struct ProgressionUpdater
		{
			Progression m_progression;
			ProgressionCallback m_callback;
			std::chrono::high_resolution_clock::time_point m_next_callback_call;
			std::chrono::high_resolution_clock::duration m_callback_call_period =
				std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::seconds(1));

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

						m_progression.m_remaining_time_extimate = std::chrono::duration<double>(
							m_progression.m_completion_factor > 0.0001 ? (m_progression.m_elapsed_time.count() / m_progression.m_completion_factor) : 0.);

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

			Results run(const TestTree & i_test_tree, TestFlags i_flags = TestFlags::All,
				ProgressionCallback i_progression_callback = ProgressionCallback());

			void set_config(const TestConfig & i_config) { m_config = i_config; }

			const TestConfig & config() const { return m_config; }

		private:

			using Operations = std::deque<std::function<void(Results & results, std::mt19937 & i_random)>>;

			void generate_functionality_operations(const TestTree & i_test_tree, Operations & i_dest);

			void generate_performance_operations(const TestTree & i_test_tree, Operations & i_dest);

			struct ExceptionTestState
			{
				ProgressionUpdater * m_progression_updater = nullptr;
				struct CaseInfo
				{
					int64_t m_exception_checkpoints = -1;
				};
				std::unordered_map<IFunctionalityTest*, CaseInfo> m_case_info;
				int64_t m_step_count = 0, m_curent_step = 0;
			};

			void prepare_for_exception_test(const TestTree & i_test_tree,
				std::mt19937 & i_random, ExceptionTestState & io_state);

			void exception_test(const TestTree & i_test_tree,
				std::mt19937 & i_random, ExceptionTestState & io_state);

			const TargetPtr & get_test_case_target(const IFunctionalityTest * i_case);

		private:
			TestConfig m_config;
			std::unordered_map<size_t, TargetPtr> m_functionality_targets;
			std::unordered_map<size_t, const ITargetType *> m_functionality_targets_types;
		};

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

	void exception_check_point()
	{
		auto const static_data = st_static_data;
		if (static_data != nullptr)
		{
			if (static_data->m_current_counter == static_data->m_except_at)
			{
				throw TestException();
			}
			static_data->m_current_counter++;
		}
	}

	void run_exception_stress_test(std::function<void()> i_test)
	{
		TESTITY_ASSERT(st_static_data == nullptr); // "run_exception_stress_test does no support recursion"

		i_test();

		StaticData static_data;
		st_static_data = &static_data;
		try
		{
			int64_t curr_iteration = 0;
			bool exception_occurred;
			do {
				exception_occurred = false;

				try
				{
					static_data.m_current_counter = 0;
					static_data.m_except_at = curr_iteration;
					i_test();
				}
				catch (TestException)
				{
					exception_occurred = true;
				}
				curr_iteration++;

			} while (exception_occurred);
		}
		catch (...)
		{
			st_static_data = nullptr; // unknown exception, reset st_static_data and retrhow
			throw;
		}
		st_static_data = nullptr;
	}

    void Results::add_result(const detail::PerformanceTest * i_test, size_t i_cardinality, Duration i_duration)
    {
        m_performance_results.insert(std::make_pair(TestId{ i_test, i_cardinality }, i_duration));
    }

	namespace detail
	{
		const TargetPtr & Session::get_test_case_target(const IFunctionalityTest * i_case)
		{
			auto const target_type = i_case->get_target_type_and_key();
			if (target_type.m_type != nullptr)
			{
				auto & target_ref = m_functionality_targets[target_type.m_type_key];
				if (target_ref.empty())
				{
					target_ref = TargetPtr(target_type.m_type, target_type.m_type->create_instance());
				}
				return target_ref;
			}
			else
			{
				static const TargetPtr null_target;
				return null_target;
			}
		}

		void Session::generate_functionality_operations(const TestTree & i_test_tree, Operations & i_dest)
		{
			for (auto & test_group : i_test_tree.functionality_tests())
			{
				{
					auto const target_type = test_group->get_target_type_and_key();
					if (target_type.m_type != nullptr)
					{
						m_functionality_targets[target_type.m_type_key];
					}

					m_functionality_targets_types[target_type.m_type_key] = target_type.m_type;
				}
				auto test_case = test_group.get();
				i_dest.push_back([test_case, this](Results & /*results*/, std::mt19937 & i_random) {
					auto & target_ptr = get_test_case_target(test_case);
					test_case->execute(i_random, target_ptr.object());
				});
			}

			for (auto & child : i_test_tree.children())
			{
				generate_functionality_operations(child, i_dest);
			}
		}

		void Session::prepare_for_exception_test(const TestTree & i_test_tree, 
			std::mt19937 & i_random, ExceptionTestState & io_state)
		{
			for (auto & test_case : i_test_tree.functionality_tests())
			{
				try
				{
					// make a copy of the target and the random generator
					auto random_copy = i_random;
					auto & target_ptr = get_test_case_target(test_case.get());
					auto target_copy_ptr = target_ptr.clone();

					StaticData static_data;
					st_static_data = &static_data;
					try
					{
						test_case->execute(random_copy, target_copy_ptr.object());
						st_static_data = nullptr;
					}
					catch (...)
					{
						// unknown exception, just rethrow
						st_static_data = nullptr;
						throw;
					}

					auto & info = io_state.m_case_info[test_case.get()];
					if (info.m_exception_checkpoints == -1)
					{
						// save the number of exception checkpoints, and update io_state.m_step_count
						info.m_exception_checkpoints = static_data.m_current_counter;
						io_state.m_step_count += (info.m_exception_checkpoints * (info.m_exception_checkpoints - 1)) / 2;
					}
					else
					{
						/* if this fails exception_check_point() has been called a different number of times 
							by the same test case, with the equal target and random generator. This means that
							the test case is not deterministic. Determinism is a requirement for the exception test. */
						TESTITY_ASSERT( info.m_exception_checkpoints == static_data.m_current_counter );
					}					
				}
				catch (...)
				{
					st_static_data = nullptr;
					throw;
				}
				st_static_data = nullptr;
			}

			for (auto & child : i_test_tree.children())
			{
				prepare_for_exception_test(child, i_random, io_state);
			}
		}

		void Session::exception_test(const TestTree & i_test_tree,
			std::mt19937 & i_random, ExceptionTestState & io_state)
		{
			for (auto & test_case : i_test_tree.functionality_tests())
			{
				try
				{
					/* the test case is executed once for each exception checkpoint. Every time a different checkpoint
						throws a TestException*/
					
					auto info_it = io_state.m_case_info.find(test_case.get());
					TESTITY_ASSERT(info_it != io_state.m_case_info.end()); // internal error
					auto const exception_checkpoints = info_it->second.m_exception_checkpoints;

					for (int64_t checkpoint_index = 0; checkpoint_index < exception_checkpoints; checkpoint_index++)
					{
						// update the progression
						io_state.m_curent_step++;
						io_state.m_progression_updater->update(io_state.m_curent_step, io_state.m_step_count);

						// make a copy of the target and the random generator
						auto random_copy = i_random;
						auto & target_ptr = get_test_case_target(test_case.get());
						auto target_copy_ptr = target_ptr.clone();

						StaticData static_data;
						bool exception_occurred = false;
						static_data.m_except_at = checkpoint_index;
						st_static_data = &static_data;
						try
						{														
							test_case->execute(random_copy, target_copy_ptr.object());
							st_static_data = nullptr;
						}
						catch (TestException)
						{
							exception_occurred = true;
							st_static_data = nullptr;
						}
						catch (...)
						{
							// unknown exception, just rethrow
							st_static_data = nullptr;
							throw;
						}

						// if this fails probably the test case is not deterministic
						TESTITY_ASSERT(static_data.m_current_counter <= exception_checkpoints);
						TESTITY_ASSERT(exception_occurred);
					}
				}
				catch (...)
				{
					st_static_data = nullptr;
					throw;
				}
				st_static_data = nullptr;
			}

			for (auto & child : i_test_tree.children())
			{
				exception_test(child, i_random, io_state);
			}
		}

		void Session::generate_performance_operations(const TestTree & i_test_tree, Operations & i_dest)
		{
			for (auto & test_group : i_test_tree.performance_tests())
			{
				for (size_t cardinality = test_group.cardinality_start();
					cardinality < test_group.cardinality_end(); cardinality += test_group.cardinality_step())
				{
					for (auto & test : test_group.tests())
					{
						i_dest.push_back([&test, cardinality](Results & results, std::mt19937 &) {
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

		Results Session::run(const TestTree & i_test_tree, TestFlags i_flags, ProgressionCallback i_progression_callback)
		{
			using namespace std;

			ProgressionUpdater progression;

			const std::random_device::result_type random_seed = m_config.m_deterministic ? m_config.m_random_seed : random_device()();

			mt19937 random = mt19937(random_seed);

			// generate operation array
			Operations operations;
			if (static_cast<unsigned>(i_flags) & static_cast<unsigned>(TestFlags::FunctionalityTest))
			{
				for (size_t repetition_index = 0; repetition_index < m_config.m_functionality_repetitions; repetition_index++)
				{
					generate_functionality_operations(i_test_tree, operations);
				}
			}
			if (static_cast<unsigned>(i_flags) & static_cast<unsigned>(TestFlags::PerformanceTests))
			{
				for (size_t repetition_index = 0; repetition_index < m_config.m_performance_repetitions; repetition_index++)
				{
					generate_performance_operations(i_test_tree, operations);
				}
			}

			const auto operations_size = operations.size();

			if (m_config.m_random_shuffle)
			{
				progression = ProgressionUpdater("randomizing operations...", i_progression_callback);
				std::shuffle(operations.begin(), operations.end(), random);
				for (size_t index = 0; index < operations_size; index++)
				{
					std::swap(operations[index],
						operations[std::uniform_int_distribution<size_t>(0, operations_size - 1)(random)]);
					progression.update(index, operations_size);
				}
			}

			progression = ProgressionUpdater("performing tests...", i_progression_callback);
			Results results(i_test_tree, m_config, random_seed);
			for (Operations::size_type index = 0; index < operations_size; index++)
			{
				operations[index](results, random);
				progression.update(index, operations_size);
			}

			// exception tests
			if (static_cast<unsigned>(i_flags) & static_cast<unsigned>(TestFlags::FunctionalityExceptionTest))
			{
				progression = ProgressionUpdater("exception tests...", i_progression_callback);

				ExceptionTestState state;
				state.m_progression_updater = &progression;

				// run prepare_for_exception_test twice to detect early non-deterministic cases
				prepare_for_exception_test(i_test_tree, random, state);
				prepare_for_exception_test(i_test_tree, random, state);
				
				exception_test(i_test_tree, random, state);
			}

			// delete targets
			m_functionality_targets.clear();
			m_functionality_targets_types.clear();

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
        for (const auto & performance_test_group : i_test_tree.performance_tests())
        {
            i_ostream << "-------------------------------------" << std::endl;
            i_ostream << "PERFORMANCE_TEST_GROUP:" << i_path << std::endl;
            i_ostream << "NAME:" << performance_test_group.name() << std::endl;
            i_ostream << "VERSION_LABEL:" << performance_test_group.version_label() << std::endl;
            i_ostream << "COMPILER:" << m_environment.compiler() << std::endl;
            i_ostream << "OS:" << m_environment.operating_sytem() << std::endl;
            i_ostream << "SYSTEM:" << m_environment.system_info() << std::endl;
            i_ostream << "SIZEOF_POINTER:" << m_environment.sizeof_pointer() << std::endl;
            i_ostream << "DETERMINISTIC:" << (m_config.m_deterministic ? "yes" : "no") << std::endl;
            i_ostream << "RANDOM_SHUFFLE:" << (m_config.m_random_shuffle ? "yes (with std::mt19937)" : "no") << std::endl;

            const auto date_time = std::chrono::system_clock::to_time_t(m_environment.startup_clock());

            #ifdef _MSC_VER
                tm local_tm;
                localtime_s(&local_tm, &date_time);
            #else
                const auto local_tm = *std::localtime(&date_time);
            #endif

            #ifndef __GNUC__
                i_ostream << "DATE_TIME:" << std::put_time(&local_tm, "%d-%m-%Y %H:%M:%S") << std::endl;
            #else
                // gcc 4.9 is missing the support for std::put_time
                const size_t temp_time_str_len = 64;
                char temp_time_str[temp_time_str_len];
                if (std::strftime(temp_time_str, temp_time_str_len, "%d-%m-%Y %H:%M:%S", &local_tm) > 0)
                {
                i_ostream << "DATE_TIME:" << temp_time_str << std::endl;
                }
                else
                {
                i_ostream << "DATE_TIME:" << "!std::strftime failed" << std::endl;
                }
            #endif

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

    Results run_session(const TestTree & i_test_tree, TestFlags i_flags, const TestConfig & i_config,
		ProgressionCallback i_progression_callback )
    {
        detail::Session session;
        session.set_config(i_config);
        return session.run(i_test_tree, i_flags, i_progression_callback);
    }
}
