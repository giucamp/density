
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <iostream>

#ifdef _MSC_VER
    #define TESTITY_ASSERT(i_bool_expr)        if(!(i_bool_expr)) \
                                                    { \
                                                        std::cerr << "Test assert failed: '" #i_bool_expr "' at " __FILE__ "(" << __LINE__ << ")" << std::endl; \
                                                        __debugbreak(); \
                                                    }
#else
    #include <assert.h>
    #define TESTITY_ASSERT(i_bool_expr)        assert(!(i_bool_expr))
#endif // _MSC_VER

namespace testity
{
    class TestException : public std::exception
    {
        using std::exception::exception;
    };

    /** Runs an exception safeness test, calling the provided function many times.
        First the provided function is called without raising any exception.
        - Then the function is called, an the first time exception_check_point is called, an exception is thrown
        - then the function is called, an the second time exception_check_point is called, an exception is thrown
        During the execution of the function the function exception_check_point should be called to
        test the effect of throwing an exception.
    */
    void run_exception_stress_test(std::function<void()> i_test);

	int64_t run_count_exception_check_points(std::function<void()> i_test);

    void exception_check_point();

} // namespace testity
