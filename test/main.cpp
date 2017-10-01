
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "test_framework/density_test_common.h"
#include "test_framework/progress.h"
#include "test_framework/threading_extensions.h"
#include "tests/generic_tests/queue_generic_tests.h"
#include <iostream>
#include <cstdlib>
#include <random>
#include <density/heter_queue.h>
#include <density/lf_heter_queue.h>

namespace density_tests
{
    void misc_examples();

    void heterogeneous_queue_samples(std::ostream & i_ostream);
    void heterogeneous_queue_basic_tests(std::ostream & i_ostream);

    void conc_heterogeneous_queue_samples(std::ostream & i_ostream);
    void conc_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

    void lf_heter_queue_samples(std::ostream & i_ostream);
    void lf_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

    void spinlocking_heterogeneous_queue_samples(std::ostream & i_ostream);
    void spinlocking_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

    void load_unload_tests(std::ostream & i_ostream);

    void func_queue_samples(std::ostream &);
    void conc_func_queue_samples(std::ostream &);
    void lf_func_queue_samples(std::ostream &);
    void sp_func_queue_samples(std::ostream &);

    void lifo_tests(std::ostream & i_output, uint32_t i_random_seed);
}

DENSITY_NO_INLINE void sandbox()
{
    using namespace density;

    static_assert(std::is_trivially_destructible<runtime_type<>>::value, "");

    //void_allocator::reserve_lockfree_page_memory(1024 * 1024 * 32);

    {
        heter_queue<> queue;

        for (int i = 0; i < 1000; i++)
            queue.push(i);

        queue.clear();
    }
    {
        lf_heter_queue<> q;
        int i;
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
        int i;
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
        lf_heter_queue<void> queue;
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

void do_tests(std::ostream & i_ostream)
{
    uint32_t random_seed = 0; // 0 -> non-deterministic

    while (random_seed == 0)
    {
        random_seed = std::random_device()();
    }

    std::cout << "Number of processors: " << density_tests::get_num_of_processors() << "\n";
    std::cout << "Random seed: " << random_seed << "\n" << std::endl;

    auto const prev_stream_flags = i_ostream.setf(std::ios_base::boolalpha);

    using namespace density_tests;

    PrintScopeDuration dur(i_ostream, "all tests");

    misc_examples();

    heterogeneous_queue_samples(i_ostream);
    heterogeneous_queue_basic_tests(i_ostream);

    conc_heterogeneous_queue_samples(i_ostream);
    conc_heterogeneous_queue_basic_tests(i_ostream);

    lf_heter_queue_samples(i_ostream);
    lf_heterogeneous_queue_basic_tests(i_ostream);

    spinlocking_heterogeneous_queue_samples(i_ostream);
    spinlocking_heterogeneous_queue_basic_tests(i_ostream);

    func_queue_samples(i_ostream);
    conc_func_queue_samples(i_ostream);
    lf_func_queue_samples(i_ostream);
    sp_func_queue_samples(i_ostream);

    size_t const element_count = 1000;

    lifo_tests(i_ostream, random_seed);

    i_ostream << "\n*** executing generic tests..." << std::endl;
    all_queues_generic_tests(QueueTesterFlags::eReserveCoreToMainThread, i_ostream, random_seed, element_count);

    i_ostream << "\n*** executing generic tests with exceptions..." << std::endl;
    all_queues_generic_tests(QueueTesterFlags::eReserveCoreToMainThread | QueueTesterFlags::eTestExceptions, i_ostream, random_seed, element_count);

    i_ostream << "\n*** executing generic tests with test allocators..." << std::endl;
    all_queues_generic_tests(QueueTesterFlags::eUseTestAllocators | QueueTesterFlags::eReserveCoreToMainThread, i_ostream, random_seed, element_count);

    i_ostream << "\n*** executing generic tests with test allocators and exceptions..." << std::endl;
    all_queues_generic_tests(QueueTesterFlags::eUseTestAllocators | QueueTesterFlags::eReserveCoreToMainThread | QueueTesterFlags::eTestExceptions, i_ostream, random_seed, element_count);

    i_ostream << "\n*** executing load unload tests..." << std::endl;
    load_unload_tests(std::cout);

    i_ostream.flags(prev_stream_flags);
}

int main()
{
    //sandbox();

    do_tests(std::cout);
    return 0;
}
