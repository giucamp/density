
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "correctness_test_context.h"
#include <memory>

namespace density
{
    class UnitTestingManager
    {
    private:
        UnitTestingManager();

    public:

        static UnitTestingManager & instance();

        UnitTestingManager(const UnitTestingManager &) = delete;

        UnitTestingManager & operator = (const UnitTestingManager &) = delete;

        using PerformanceTestFunction = void (*)();
        
        using CorrectnessTestFunction = void(*)(CorrectnessTestContext & i_context);

        void add_correctness_test(const char * i_path, CorrectnessTestFunction i_function);

        void add_performance_test(const char * i_path, PerformanceTestFunction i_function, const char *  i_version_label);

        void run(const char * i_path = "");

    private:
        class Impl;
        const std::unique_ptr<Impl> m_impl;    
    };

}
