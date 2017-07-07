
#include "test_framework/density_test_common.h"
#include "test_framework/progress.h"
#include "tests/queue_generic_tests.h"
#include <iostream>
#include <cstdlib>

namespace density_tests
{
	void heterogeneous_queue_samples(std::ostream & i_ostream);
	void heterogeneous_queue_basic_tests(std::ostream & i_ostream);
}

void do_tests(std::ostream & i_ostream)
{
	using namespace density_tests;

	PrintScopeDuration dur(i_ostream, "all tests");


	heterogeneous_queue_samples(i_ostream);

	heterogeneous_queue_basic_tests(i_ostream);

	i_ostream << "\n*** executing generic tests..." << std::endl;
	all_queues_generic_tests(QueueTesterFlags::eNone, i_ostream, 3, 100000);

	i_ostream << "\n*** executing generic tests with exceptions..." << std::endl;
	all_queues_generic_tests(QueueTesterFlags::eTestExceptions, i_ostream, 3, 100000);
}

int main()
{
	do_tests(std::cout);
	system("PAUSE");
	return 0;
}