
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../../test_framework/density_test_common.h"
//

#include "queue_generic_tests.h"
#include "test_framework/threading_extensions.h"

namespace density_tests
{
    void sp_heter_generic_tests(
      QueueTesterFlags i_flags,
      std::ostream &   i_output,
      EasyRandom &     i_rand,
      size_t           i_element_count)
    {
        using namespace density;

        uint64_t cpu_count = get_num_of_processors();
        if (cpu_count == 0)
            cpu_count = 1;

        std::vector<size_t> const nonblocking_thread_counts{static_cast<size_t>(cpu_count * 2)};

        constexpr auto mult   = density::concurrency_multiple;
        constexpr auto single = density::concurrency_single;

        detail::sp_queues_generic_tests<mult, mult>(
          i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);

        detail::sp_queues_generic_tests<mult, single>(
          i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);

        detail::sp_queues_generic_tests<single, mult>(
          i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);

        detail::sp_queues_generic_tests<single, single>(
          i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);
    }
} // namespace density_tests
