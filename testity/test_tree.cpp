
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "test_tree.h"
#include <string.h>
#include <sstream>
#include <random>
#include <fstream>
#include <ctime>
#include <iomanip>
#ifdef _MSC_VER
    #include <time.h>
#endif

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

    void PerformanceTestGroup::add_test(const char * i_source_file, int i_start_line,
        std::function<BenchmarkTest::TestFunction> i_function, int i_end_line)
    {
        using namespace std;

        // open the source file and read the lines from i_start_line to i_end_line
        ifstream stream(i_source_file);
        int curr_line = 0;
        vector<string> lines;
        while (!stream.eof() && curr_line < i_end_line - 1)
        {
            string line;
            getline(stream, line);
            if (curr_line >= i_start_line)
            {
                lines.push_back(move(line));
            }
            curr_line++;
        }

        // find the longest white-char prefix common to all the lines
        size_t white_prefix_length = 0;
        bool match = true;
        while(match)
        {
            bool first = true;
            char target_char = 0;
            for (auto & line : lines)
            {
                if (match && white_prefix_length < line.length())
                {
                    const char curr_char = line[white_prefix_length];
                    if (!isspace(curr_char))
                    {
                        match = false;
                    }
                    else if (first)
                    {
                        target_char = curr_char;
                        first = false;
                    }
                    else
                    {
                        match = target_char == curr_char;
                    }
                }
            }
            if (match)
            {
                white_prefix_length++;
            }
        }

        // reconstruct the source code, with "#nl#" instead of newlines
        string source_code;
        for (auto & line : lines)
        {
            line.erase(line.begin(), line.begin() + std::min(white_prefix_length, line.length()));
            source_code += line + "#nl#";
        }

        add_test(BenchmarkTest(source_code.c_str(), move(i_function) ));
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
            i_ostream << "COMPILER:" << m_environment.compiler() << std::endl;
            i_ostream << "OS:" << m_environment.operating_sytem() << std::endl;
            i_ostream << "SYSTEM:" << m_environment.system_info() << std::endl;
            i_ostream << "SIZEOF_POINTER:" << m_environment.sizeof_pointer() << std::endl;

            const auto date_time = std::chrono::system_clock::to_time_t(m_environment.startup_clock());
            #ifdef _MSC_VER
                tm local_tm;
                localtime_s(&local_tm, &date_time);
            #else
                const auto local_tm = *std::localtime(&date_time);
            #endif
            i_ostream << "DATE_TIME:" << std::put_time(&local_tm, "%d-%m-%Y %H:%M:%S") << std::endl;

            i_ostream << "CARDINALITY_START:" << performance_test_group.cardinality_start() << std::endl;
            i_ostream << "CARDINALITY_STEP:" << performance_test_group.cardinality_step() << std::endl;
            i_ostream << "CARDINALITY_END:" << performance_test_group.cardinality_end() << std::endl;
            i_ostream << "MULTEPLICITY:" << m_repetitions << std::endl;

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
        Results results(i_test_tree, m_repetitions);
        double last_perc = -1.;
        const double perc_mult = 100. / operations_size;
        for (Operations::size_type index = 0; index < operations_size; index++)
        {
            const double perc = floor(index * perc_mult);
            if (perc != last_perc)
            {
                i_dest_stream << perc << "%" << endl;
                last_perc = perc;
            }
            operations[index](results);
        }
        return results;
    }

} // namespace testity
