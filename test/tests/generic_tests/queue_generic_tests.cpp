
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../../test_framework/density_test_common.h"
//

#include "queue_generic_tests.h"
#include "test_framework/threading_extensions.h"
#include "test_settings.h"

namespace density_tests
{
    void heter_queue_generic_tests(
      QueueTesterFlags i_flags,
      std::ostream &   i_output,
      EasyRandom &     i_rand,
      size_t           i_element_count);

    void concurr_heter_queue_generic_tests(
      QueueTesterFlags i_flags,
      std::ostream &   i_output,
      EasyRandom &     i_rand,
      size_t           i_element_count);

    void lf_heter_relaxed_queue_generic_tests(
      const TestSettings &        i_settings,
      QueueTesterFlags            i_flags,
      std::ostream &              i_output,
      EasyRandom &                i_rand,
      const std::vector<size_t> & i_nonblocking_thread_counts);

    void lf_heter_seq_cst_queue_generic_tests(
      const TestSettings &        i_settings,
      QueueTesterFlags            i_flags,
      std::ostream &              i_output,
      EasyRandom &                i_rand,
      const std::vector<size_t> & i_nonblocking_thread_counts);

    void sp_heter_generic_tests(
      QueueTesterFlags i_flags,
      std::ostream &   i_output,
      EasyRandom &     i_rand,
      size_t           i_element_count);

    /** Runs the generic test on all the queues
        @param i_flags misc options
        @param i_output output stream to use for the progression and the result
        @param i_random_seed seed to use for the PRG. If != 0, the test is deterministic. If == 0, PRG's
            are seeded with a non-deterministic source (like std::random_device).
        @param i_element_count number of elements to produce and consume in every test
    */
    void all_queues_generic_tests(
      const TestSettings & i_settings,
      QueueTesterFlags     i_flags,
      std::ostream &       i_output,
      uint32_t             i_random_seed)
    {
        EasyRandom rand      = i_random_seed == 0 ? EasyRandom() : EasyRandom(i_random_seed);
        uint64_t   cpu_count = get_num_of_processors();
        if (cpu_count == 0)
            cpu_count = 1;

#if defined(__GNUC__) && !defined(__clang__)
        /* on travis gcc build take more time, so we use lees threads. 
                to do: expose the number of threads with a commandline argument */
        std::vector<size_t> const nonblocking_thread_counts{static_cast<size_t>(cpu_count * 3)};
#else
        std::vector<size_t> const nonblocking_thread_counts{static_cast<size_t>(cpu_count * 6)};
#endif

        if (i_settings.should_run("queue"))
            heter_queue_generic_tests(
              i_flags, i_output, rand, i_settings.m_queue_tests_cardinality);

        if (i_settings.should_run("conc_queue"))
            concurr_heter_queue_generic_tests(
              i_flags, i_output, rand, i_settings.m_queue_tests_cardinality);

        if (i_settings.should_run("lf_queue"))
        {
            if (i_settings.should_run("relaxed"))
                lf_heter_relaxed_queue_generic_tests(
                  i_settings, i_flags, i_output, rand, nonblocking_thread_counts);

            if (i_settings.should_run("seq_cnst"))
                lf_heter_seq_cst_queue_generic_tests(
                  i_settings, i_flags, i_output, rand, nonblocking_thread_counts);
        }

        if (i_settings.should_run("sp_queue"))
            sp_heter_generic_tests(i_flags, i_output, rand, i_settings.m_queue_tests_cardinality);
    }
} // namespace density_tests
