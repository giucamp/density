
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "queue_generic_tests.h"

namespace density_tests
{
    void concurr_heter_queue_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
        EasyRandom & i_rand, size_t i_element_count)
    {
        using namespace density;

        std::vector<size_t> const concurrent_thread_counts{ 1, 2 };

        if (i_flags && QueueTesterFlags::eUseTestAllocators)
        {
            detail::single_queue_generic_test<conc_heter_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>(
                i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);

            detail::single_queue_generic_test<conc_heter_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>(
                i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);

            detail::single_queue_generic_test<conc_heter_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>>>(
                i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);

            detail::single_queue_generic_test<conc_heter_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>>>(
                i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);
        }
        else
        {
            detail::single_queue_generic_test<conc_heter_queue<>>(
                i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);
        }
    }
}
