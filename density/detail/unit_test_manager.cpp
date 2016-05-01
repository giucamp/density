
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "..\uint_test_manager.h"
#include <string.h>
#include <algorithm>
#include <chrono>
#include <iostream>

namespace density
{
    class UnitTestingManager::Impl
    {
        template <typename PREEDICATE>
            static void for_each_token(const char * i_path, PREEDICATE && i_predicate)
        {
            const char * remaining_path = i_path;
            while(*remaining_path != 0)
            {
                const size_t token_length = strcspn(remaining_path, "/\\");
                i_predicate(remaining_path, token_length);
                remaining_path += token_length;
            }
        }
        
        class CorrectnessTest
        {
        public:

            CorrectnessTest(CorrectnessTestFunction i_function)
                : m_function(i_function)
            {
            }

            void run(CorrectnessTestContext & i_context)
            {
                (*m_function)(i_context);
            }

        private:            
            CorrectnessTestFunction m_function;
            std::chrono::duration<double> m_duration;
        };

        class PerformanceTest
        {
        public:

            PerformanceTest(PerformanceTestFunction i_function, const char * i_version_label)
                : m_function(i_function), m_version_label(i_version_label, i_version_label)
            {
            }

            void run()
            {
                (*m_function)();
            }

        private:
            PerformanceTestFunction m_function;
            std::string m_version_label;
            std::chrono::duration<double> m_duration;
        };

        class Node
        {        
        public:

            Node(std::string i_name) : m_name(std::move(i_name)) {}

            const std::string & name() const { return m_name; }

            void add_correctess_test(CorrectnessTest i_correctess_test)
            {
                m_correctness_tests.emplace_back(std::move(i_correctess_test));
            }

            void add_performance_test(PerformanceTest i_performance_test)
            {
                m_performance_tests.emplace_back(std::move(i_performance_test));
            }

            const std::vector<Node> & children() const { return m_children; }

            Node * add_child(std::string i_name)
            {
                m_children.emplace_back(i_name);
                return &m_children.back();
            }

            Node * find_child(const char * i_name)
            {
                auto entry_it = std::find_if(m_children.begin(), m_children.end(), [i_name](const Node & i_entry) { return i_name == i_entry.name().c_str(); });
                if (entry_it != m_children.end())
                {
                    return &*entry_it;
                }
                else
                {
                    return nullptr;
                }
            }

            void run(CorrectnessTestContext & i_context)
            {
                for (auto & child : m_children)
                {
                    child.run(i_context);
                }

                std::cout << "testing " << m_name << "...";

                for (auto & test : m_correctness_tests)
                {
                    test.run(i_context);
                }

                for (auto & test : m_performance_tests)
                {
                    test.run();
                }

                std::cout << "done" << std::endl;
            }

        private:
            std::string m_name;
            std::vector< CorrectnessTest > m_correctness_tests;
            std::vector< PerformanceTest > m_performance_tests;
            std::vector<Node> m_children;
        };

        Node m_root;

    public:

        Impl()
            : m_root("")
        {

        }

        Node * find_entry(const char * i_path)
        {
            Node * node = &m_root;
            for_each_token(i_path, [&node](const char * i_token, size_t i_token_length) {

                if (node != nullptr)
                {
                    node = node->find_child(std::string(i_token, i_token_length).c_str());
                }
            });

            return node;
        }

        Node & find_or_add_entry(const char * i_path)
        {
            Node * node = &m_root;
            for_each_token(i_path, [&node](const char * i_token, size_t i_token_length) {

                Node * child = node->find_child(i_token);

                if (child == nullptr)
                {
                    node = node->add_child(std::string(i_token, i_token_length));
                }
                else
                {
                    node = child;
                }
            });

            return *node;
        }

        void add_correctness_test(const char * i_path, CorrectnessTestFunction i_function)
        {
            find_or_add_entry(i_path).add_correctess_test(CorrectnessTestFunction(i_function));
        }

        void add_performance_test(const char * i_path, PerformanceTestFunction i_function, const char * i_version_label)
        {
            find_or_add_entry(i_path).add_performance_test(PerformanceTest(i_function, i_version_label));
        }

        void run(const char * i_path)
        {
            CorrectnessTestContext correctnessContext;
            auto node = find_entry(i_path);
            if (node != nullptr)
            {
                node->run(correctnessContext);
            }
        }
    };

    UnitTestingManager & UnitTestingManager::instance()
    {
        static UnitTestingManager instance;
        return instance;
    }
    
    UnitTestingManager::UnitTestingManager()
        : m_impl( std::make_unique<Impl>() )
    {
    }

    void UnitTestingManager::add_correctness_test(const char * i_path, CorrectnessTestFunction i_function)
    {
        m_impl->add_correctness_test(i_path, i_function);
    }

    void UnitTestingManager::add_performance_test(const char * i_path, PerformanceTestFunction i_function, const char * i_version_label)
    {
        m_impl->add_performance_test(i_path, i_function, i_version_label);
    }

    void UnitTestingManager::run(const char * i_path)
    {
        m_impl->run(i_path);
    }
}
