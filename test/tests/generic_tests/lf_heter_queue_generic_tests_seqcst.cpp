
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "queue_generic_tests.h"
#include "test_framework/threading_extensions.h"
#include "test_framework/threading_extensions.h"
#include "test_settings.h"

namespace density_tests
{
    void lf_heter_seq_cst_queue_generic_tests(const TestSettings & i_settings, QueueTesterFlags i_flags, std::ostream & i_output,
        EasyRandom & i_rand, const std::vector<size_t> & i_nonblocking_thread_counts)
    {
        using namespace density;

        constexpr auto mult = density::concurrency_multiple;
        constexpr auto single = density::concurrency_single;
        constexpr auto seq_cst = density::consistency_sequential;

        detail::lf_queues_generic_tests<mult, mult, seq_cst>
            (i_settings, i_flags, i_output, i_rand, i_nonblocking_thread_counts);

        detail::lf_queues_generic_tests<mult, single, seq_cst>
            (i_settings, i_flags, i_output, i_rand, i_nonblocking_thread_counts);

        detail::lf_queues_generic_tests<single, mult, seq_cst>
            (i_settings, i_flags, i_output, i_rand, i_nonblocking_thread_counts);

        detail::lf_queues_generic_tests<single, single, seq_cst>
            (i_settings, i_flags, i_output, i_rand, i_nonblocking_thread_counts);
    }
}
