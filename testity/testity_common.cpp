
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "testity_common.h"

namespace testity
{
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

} // namespace testity
