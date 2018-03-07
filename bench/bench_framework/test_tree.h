
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "performance_test.h"
#include <functional>

namespace density_bench
{
    class TestTree
    {
      public:
        explicit TestTree(std::string i_name) : m_name(std::move(i_name)) {}

        const std::string & name() const { return m_name; }

        void add_child(TestTree i_child);

        void add_performance_test(PerformanceTestGroup i_group)
        {
            m_performance_tests.emplace_back(std::move(i_group));
        }

        const std::vector<PerformanceTestGroup> & performance_tests() const
        {
            return m_performance_tests;
        }

        const std::vector<TestTree> & children() const { return m_children; }

        TestTree & operator[](const char * i_path);

        const TestTree * find(const char * i_path) const;

        TestTree * find(const char * i_path);

        template <typename FUNC> void recursive_for_each_child(const FUNC & i_callable) const
        {
            for (auto const & child : m_children)
            {
                i_callable(child);
                child.recursive_for_each_child(i_callable);
            }
        }

        template <typename FUNC> void recursive_for_each_child(const FUNC & i_callable)
        {
            for (auto const & child : m_children)
            {
                i_callable(child);
                child.recursive_for_each_child(i_callable);
            }
        }

      private:
        std::string const                 m_name;
        std::vector<PerformanceTestGroup> m_performance_tests;
        std::vector<TestTree>             m_children;
    };

} // namespace density_bench
