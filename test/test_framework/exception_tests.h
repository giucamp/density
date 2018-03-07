
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_test_common.h"
#include <functional>

namespace density_tests
{
    /** Type possibly thrown by exception_checkpoint, during a run of run_exception_test. */
    class TestException
    {
    };

    /** This function marks a point that may throw an exception in the test cases. During a call
        to run_exception_test exception_checkpoint may throw a TestException. If a call to
        run_exception_test is not in progress, exception_checkpoint returns without throwing. */
    void exception_checkpoint();

    /** Turns on exception throwing mode in exception_checkpoint, and then calls the provided
        function repeatedly until no TestException is thrown.
        This function effectively executes a loop: during the i-th call of i_test, the i-th
        call to exception_checkpoint throws a TestException.
        Exception of type TestException are caught (and the loop is repeated). Exceptions of any
        other type escape this function.
        This function can be used to test the behavior of classes in case of exceptions: if the
        execution flow of i_test is always the same, every site in which exception_checkpoint is
        called will throw before the function returns.
        This function does not support recursion.

        @param i_test function to call
        @return number of times TestException was thrown. */
    int64_t run_exception_test(std::function<void()> i_test);

} // namespace density_tests
