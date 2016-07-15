
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "detail\functionality_test.h"
#include "performance_test.h"
#include <functional>

namespace testity
{
    class TestTree
    {
    public:

        explicit TestTree(std::string i_name) : m_name(std::move(i_name)) {}

        const std::string & name() const { return m_name; }

		void add_case(std::function< void(std::mt19937 & i_random) > i_function);

		void add_child(TestTree i_child);
		
		template <typename TARGET>
			void add_case(std::function< void(std::mt19937 & i_random, TARGET & i_target) > i_function )
		{
			m_functionality_tests.emplace_back(std::unique_ptr<detail::IFunctionalityTest>(
				new detail::TargetedFunctionalityTest<TARGET>(i_function)));
		}

        void add_performance_test(PerformanceTestGroup i_group)
        {
            m_performance_tests.emplace_back(std::move(i_group));
        }

        const std::vector< std::unique_ptr<detail::IFunctionalityTest> > & functionality_tests() const { return m_functionality_tests; }
        const std::vector< PerformanceTestGroup > & performance_tests() const { return m_performance_tests;  }

        const std::vector<TestTree> & children() const { return m_children; }

        TestTree & operator [] (const char * i_path);

        const TestTree * find(const char * i_path) const;

        TestTree * find(const char * i_path);

    private:		
        std::string m_name;
        std::vector< std::unique_ptr<detail::IFunctionalityTest> > m_functionality_tests;
        std::vector< PerformanceTestGroup > m_performance_tests;
        std::vector< TestTree > m_children;
    };   

} // namespace testity

