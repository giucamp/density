
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "queue_generic_tests.h"
#include "test_framework/threading_extensions.h"

namespace density_tests
{
    void lf_heter_seq_cst_queue_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
        EasyRandom & i_rand, size_t i_element_count)
    {
        using namespace density;

        uint64_t cpu_count = get_num_of_processors();
        if(cpu_count == 0)
            cpu_count = 1;

        std::vector<size_t> const nonblocking_thread_counts{
            static_cast<size_t>(cpu_count * 3) };

        constexpr auto mult = density::concurrency_multiple;
        constexpr auto single = density::concurrency_single;
        constexpr auto seq_cst = density::consistency_sequential;

  /*      detail::lf_queues_generic_tests<mult, mult, seq_cst>
            (i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);

        detail::lf_queues_generic_tests<mult, single, seq_cst>
            (i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);*/

        detail::lf_queues_generic_tests<single, mult, seq_cst>
            (i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);

/*        detail::lf_queues_generic_tests<single, single, seq_cst>
            (i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);*/
    }
}
