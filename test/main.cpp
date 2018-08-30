
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "test_framework/density_test_common.h"
//

#include "test_framework/allocator_stress_test.h"
#include "test_framework/density_test_common.h"
#include "test_framework/progress.h"
#include "test_framework/threading_extensions.h"
#include "test_settings.h"
#include "tests/generic_tests/queue_generic_tests.h"
#include <cstdlib>
#include <density/heter_queue.h>
#include <density/lf_heter_queue.h>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>

#if defined(__linux__) && !defined(__ANDROID__)
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void signal_handler()
{
    void * array[256];
    size_t size = backtrace(array, 256);

    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// https://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes
void segfault_handler(int /*sig*/, siginfo_t * info, void * /*ucontext*/)
{
    printf("segmentation fault: %p\n", info->si_addr);
    signal_handler();
}
void illegalinstr_handler(int /*sig*/, siginfo_t * info, void * /*ucontext*/)
{
    printf("illegalinstr fault\n");
    signal_handler();
}
#endif

namespace density_tests
{
    void misc_examples();
    void feature_list_examples();
    void runtime_type_examples();

    void heterogeneous_queue_samples(std::ostream & i_ostream);
    void heterogeneous_queue_basic_tests(std::ostream & i_ostream);

    void conc_heterogeneous_queue_samples(std::ostream & i_ostream);
    void conc_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

    void lf_heter_queue_samples(std::ostream & i_ostream);
    void lf_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

    void spinlocking_heterogeneous_queue_samples(std::ostream & i_ostream);
    void spinlocking_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

    void load_unload_tests(std::ostream & i_ostream);

    void overview_examples();
    void dynamic_reference_examples();

    void func_queue_samples(std::ostream &);
    void conc_func_queue_samples(std::ostream &);
    void lf_func_queue_samples(std::ostream &);
    void sp_func_queue_samples(std::ostream &);

    void lifo_examples();
    void lifo_tests(
      QueueTesterFlags i_flags,
      std::ostream &   i_output,
      uint32_t         i_random_seed,
      size_t           i_depth,
      size_t           i_fork_depth);

    void type_fetaures_tests();

} // namespace density_tests

DENSITY_NO_INLINE void sandbox()
{
    using namespace density;

    static_assert(std::is_trivially_destructible<runtime_type<>>::value, "");

    //default_allocator::reserve_lockfree_page_memory(1024 * 1024 * 32);

    {
        heter_queue<> queue;

        for (int i = 0; i < 1000; i++)
            queue.push(i);

        queue.clear();
    }
    {
        lf_heter_queue<> q;
        int              i;
        for (i = 0; i < 1000; i++)
            q.push(i);

        i = 0;
        while (auto c = q.try_start_consume())
        {
            assert(c.element<int>() == i);
            c.commit();
            i++;
        }
        assert(i == 1000);
    }
    {
        lf_heter_queue<> q;
        int              i;
        for (i = 0; i < 1000; i++)
            q.push(i);

        i = 0;
        lf_heter_queue<>::consume_operation consume;
        while (q.try_start_consume(consume))
        {
            assert(consume.element<int>() == i);
            consume.commit();
            i++;
        }
        assert(i == 1000);
    }

    {
        lf_heter_queue<> q1;
    }
    {
        lf_heter_queue<> queue;
        queue.push(std::string());
        queue.push(std::make_pair(4., 1));
        assert(!queue.empty());
    }
    {
        lf_heter_queue<> queue;
        queue.push(42);
        assert(queue.try_start_consume().element<int>() == 42);
    }
}

void do_tests(const TestSettings & i_settings, std::ostream & i_ostream, uint32_t i_random_seed)
{
    {
        using namespace density;
        using namespace density::detail;

        // test conversion from proress_guarantee to LfQueue_ProgressGuarantee
        static_assert(ToLfGuarantee(progress_blocking, true) == LfQueue_Throwing, "");
        static_assert(ToLfGuarantee(progress_blocking, false) == LfQueue_Blocking, "");
        static_assert(ToLfGuarantee(progress_obstruction_free, false) == LfQueue_LockFree, "");
        static_assert(ToLfGuarantee(progress_lock_free, false) == LfQueue_LockFree, "");
        static_assert(ToLfGuarantee(progress_wait_free, false) == LfQueue_WaitFree, "");

        // test conversion from LfQueue_ProgressGuarantee to proress_guarantee
        static_assert(ToDenGuarantee(LfQueue_Throwing) == progress_blocking, "");
        static_assert(ToDenGuarantee(LfQueue_Blocking) == progress_blocking, "");
        static_assert(ToDenGuarantee(LfQueue_LockFree) == progress_lock_free, "");
        static_assert(ToDenGuarantee(LfQueue_WaitFree) == progress_wait_free, "");
    }

    auto const prev_stream_flags = i_ostream.setf(std::ios_base::boolalpha);

    using namespace density_tests;

    PrintScopeDuration dur(i_ostream, "all tests");

    auto const alloc_test = std::unique_ptr<allocator_stress_test>{
      i_settings.m_allocator_stress_test ? new allocator_stress_test{} : nullptr};

    auto flags = QueueTesterFlags::eNone;
    if (i_settings.m_spare_one_cpu)
    {
        flags = flags | QueueTesterFlags::eReserveCoreToMainThread;
    }
    if (i_settings.m_print_progress)
    {
        flags = flags | QueueTesterFlags::ePrintProgress;
    }

    type_fetaures_tests();

    if (i_settings.should_run("lifo"))
    {
        lifo_examples();
        lifo_tests(flags, i_ostream, i_random_seed, 20, 3);
        if (i_settings.m_exceptions)
        {
            lifo_tests(flags | QueueTesterFlags::eTestExceptions, i_ostream, i_random_seed, 8, 3);
        }
    }

    if (i_settings.should_run("misc_examples"))
    {
        misc_examples();
        feature_list_examples();
        runtime_type_examples();
    }

    if (i_settings.should_run("queue"))
    {
        heterogeneous_queue_samples(i_ostream);
        heterogeneous_queue_basic_tests(i_ostream);
    }

    if (i_settings.should_run("conc_queue"))
    {
        conc_heterogeneous_queue_samples(i_ostream);
        conc_heterogeneous_queue_basic_tests(i_ostream);
    }

    if (i_settings.should_run("lf_queue"))
    {
        lf_heter_queue_samples(i_ostream);
        lf_heterogeneous_queue_basic_tests(i_ostream);
    }

    if (i_settings.should_run("sp_queue"))
    {
        spinlocking_heterogeneous_queue_samples(i_ostream);
        spinlocking_heterogeneous_queue_basic_tests(i_ostream);
    }

    overview_examples();
    dynamic_reference_examples();

    func_queue_samples(i_ostream);
    conc_func_queue_samples(i_ostream);
    lf_func_queue_samples(i_ostream);
    sp_func_queue_samples(i_ostream);

    i_ostream << "\n*** executing generic tests..." << std::endl;

    all_queues_generic_tests(i_settings, flags, i_ostream, i_random_seed);

    if (i_settings.m_exceptions)
    {
        i_ostream << "\n*** executing generic tests with exceptions..." << std::endl;
        all_queues_generic_tests(
          i_settings, flags | QueueTesterFlags::eTestExceptions, i_ostream, i_random_seed);
    }

    if (i_settings.m_test_allocators)
    {
        i_ostream << "\n*** executing generic tests with test allocators..." << std::endl;
        all_queues_generic_tests(
          i_settings, flags | QueueTesterFlags::eUseTestAllocators, i_ostream, i_random_seed);

        if (i_settings.m_exceptions)
        {
            i_ostream << "\n*** executing generic tests with test allocators and exceptions..."
                      << std::endl;
            all_queues_generic_tests(
              i_settings,
              flags | QueueTesterFlags::eUseTestAllocators | QueueTesterFlags::eTestExceptions,
              i_ostream,
              i_random_seed);
        }
    }

    i_ostream << "\n*** executing load unload tests..." << std::endl;
    load_unload_tests(std::cout);

    i_ostream.flags(prev_stream_flags);
}

namespace density_examples
{
    void any_tests();
}

int run(int argc, char ** argv)
{
    std::ostream & out = std::cout;

    out << "Density tests" << std::endl;
    for (auto parameter = argv; *parameter != nullptr; parameter++)
    {
        out << "\t" << *parameter << std::endl;
    }

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
    out << "Thread-sanitizer is ON" << std::endl;
#endif
#endif

    out << "sizeof(void*): " << sizeof(void *) << std::endl;

#if defined(DENSITY_DEBUG)
    out << "DENSITY_DEBUG: defined" << std::endl;
#else
    out << "DENSITY_DEBUG: not defined" << std::endl;
#endif

#if defined(DENSITY_DEBUG_INTERNAL)
    out << "DENSITY_DEBUG_INTERNAL: defined" << std::endl;
#else
    out << "DENSITY_DEBUG_INTERNAL: not defined" << std::endl;
#endif

#if defined(NDEBUG)
    out << "NDEBUG: defined" << std::endl;
#else
    out << "NDEBUG: not defined" << std::endl;
#endif

#if defined(DENSITY_USER_DATA_STACK)
    out << "DENSITY_USER_DATA_STACK: defined" << std::endl;
#else
    out << "DENSITY_USER_DATA_STACK: not defined" << std::endl;
#endif

#if defined(__linux__) && !defined(__ANDROID__)
    struct sigaction sa
    {
    };
    sa.sa_flags     = SA_SIGINFO;
    sa.sa_sigaction = segfault_handler;
    sigaction(SIGSEGV, &sa, nullptr);
    sa.sa_sigaction = illegalinstr_handler;
    sigaction(SIGILL, &sa, nullptr);
#endif

    auto const settings = parse_settings(argc, argv);

    uint32_t random_seed = settings->m_rand_seed;
    while (random_seed == 0)
    {
        random_seed = std::random_device()();
    }

    std::cout << "number of processors: " << density_tests::get_num_of_processors() << "\n";
    std::cout << "random seed: " << random_seed << "\n";
    std::cout << "density version: " << density::version << "\n";
    std::cout << "default page size: " << density::default_allocator::page_size << "\n"
              << std::endl;

    // check that density::version and DENSITY_VERSION are consistent
    std::ostringstream version_stream;
    version_stream << (DENSITY_VERSION >> 16) << ".";
    version_stream << ((DENSITY_VERSION >> 8) & 0xFF) << ".";
    version_stream << (DENSITY_VERSION & 0xFF);
    DENSITY_TEST_ASSERT(version_stream.str() == density::version);

    //sandbox();

    density_examples::any_tests();

    do_tests(*settings, out, random_seed);
    return 0;
}

#if !defined(__ANDROID__)
int main(int argc, char ** argv) { return run(argc, argv); }
#endif
