
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "test_allocators.h"

namespace density_tests
{
    thread_local EasyRandom * ThreadAllocRandomFailures::t_fail_random = nullptr;
    thread_local double ThreadAllocRandomFailures::t_fail_probability = 0.;

    ThreadAllocRandomFailures::ThreadAllocRandomFailures(EasyRandom & i_random, double i_fail_probability)
    {
        assert(t_fail_random == nullptr); // ThreadAllocRandomFailures does not support nesting
        t_fail_random = &i_random;
        t_fail_probability = i_fail_probability;
    }

    ThreadAllocRandomFailures::~ThreadAllocRandomFailures()
    {
        t_fail_random = nullptr;
    }

} // density_tests

