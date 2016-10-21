
#include "../../density/concurrent_function_queue.h"
#include "testity/test_tree.h"

namespace density_tests
{
	using namespace density;
	using namespace testity;

	void tets_concurrent_function_queue(std::mt19937 &)
	{
		using namespace density::experimental;

		concurrent_heterogeneous_queue<> queue;

		for(int i = 0; i < 100000; i++)
			queue.push(i);
	}

	void add_concurrent_function_queue_cases(TestTree & i_dest)
	{		
		i_dest.add_case(tets_concurrent_function_queue);
	}
}