
#include "test_framework/density_test_common.h"
#include "test_framework/progress.h"
#include "tests/generic_tests/queue_generic_tests.h"
#include <iostream>
#include <cstdlib>
#include <density/heter_queue.h>
#include <density/lf_heter_queue.h>

namespace density_tests
{
	void heterogeneous_queue_samples(std::ostream & i_ostream);
	void heterogeneous_queue_basic_tests(std::ostream & i_ostream);

	void concurrent_heterogeneous_queue_samples(std::ostream & i_ostream);
	void concurrent_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

	void nonblocking_heterogeneous_queue_samples(std::ostream & i_ostream);
	void nonblocking_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

    void spinlocking_heterogeneous_queue_samples(std::ostream & i_ostream);
	void spinlocking_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

	void load_unload_tests(std::ostream & i_ostream);
}

DENSITY_NO_INLINE void sandbox()
{
	using namespace density;

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
	auto const prev_stream_flags = i_ostream.setf(std::ios_base::boolalpha);
    
	using namespace density_tests;

	PrintScopeDuration dur(i_ostream, "all tests");

	heterogeneous_queue_samples(i_ostream);
	heterogeneous_queue_basic_tests(i_ostream);

	concurrent_heterogeneous_queue_samples(i_ostream);
	concurrent_heterogeneous_queue_basic_tests(i_ostream);

	nonblocking_heterogeneous_queue_samples(i_ostream);
	nonblocking_heterogeneous_queue_basic_tests(i_ostream);

    spinlocking_heterogeneous_queue_samples(i_ostream);
	spinlocking_heterogeneous_queue_basic_tests(i_ostream);

    size_t const element_count = 10000;

    uint32_t random_seed = 0; // 0 -> non-deterministic

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
	sandbox();

	do_tests(std::cout);
	return 0;
}