
#include "test_framework/density_test_common.h"
#include "test_framework/progress.h"
#include "tests/queue_generic_tests.h"
#include <iostream>
#include <cstdlib>
#include <density/heterogeneous_queue.h>
#include <density/nonblocking_heterogeneous_queue.h>

namespace density_tests
{
	void heterogeneous_queue_samples(std::ostream & i_ostream);
	void heterogeneous_queue_basic_tests(std::ostream & i_ostream);

	void concurrent_heterogeneous_queue_samples(std::ostream & i_ostream);
	void concurrent_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

	void nonblocking_heterogeneous_queue_samples(std::ostream & i_ostream);
	void nonblocking_heterogeneous_queue_basic_tests(std::ostream & i_ostream);

	void load_unload_tests(std::ostream & i_ostream);
}

DENSITY_NO_INLINE void sandbox()
{
	using namespace density;

	{
		heterogeneous_queue<> queue;

		for (int i = 0; i < 1000; i++)
			queue.push(i);

		queue.clear();
	}
	{
		nonblocking_heterogeneous_queue<> q;
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
		nonblocking_heterogeneous_queue<> q1;
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

	i_ostream << "\n*** executing generic tests..." << std::endl;
	all_queues_generic_tests(QueueTesterFlags::eNone, i_ostream, 3, 100000);

	i_ostream << "\n*** executing generic tests with exceptions..." << std::endl;
	all_queues_generic_tests(QueueTesterFlags::eTestExceptions, i_ostream, 3, 100000);

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