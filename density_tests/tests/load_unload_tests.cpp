
#include "../test_framework/queue_load_unload_test.h"
#include <density/nonblocking_heterogeneous_queue.h>
#include <ostream>

namespace density_tests
{
	void load_unload_tests(std::ostream & i_ostream)
	{
		using namespace density;
		using namespace density::experimental;

		density_tests::queue_load_unload_test<nonblocking_heterogeneous_queue<>>(
			3000, 1000, i_ostream);

		density_tests::queue_load_unload_test<nonblocking_heterogeneous_queue<>>(
			7, 10000000, i_ostream);

		/*EasyRandom rand;
		density_tests::run_queue_integrity_test<nonblocking_heterogeneous_queue<>>(3000, 300,
			rand, density_tests::LoadUnloadTestOptions{.5,60, 0});*/
		
		/*using q = nonblocking_heterogeneous_queue<void, runtime_type<void>, density_tests::NonblockingTestAllocator<density::default_page_capacity> >;
		{
			q a;
			for(int i = 0; i < 260; i++)
				a.push(1);
			a.start_consume().commit();
		}
		density_tests::run_queue_integrity_test<q>(4, 4,
			density_tests::LoadUnloadTestOptions{50,60, 0}, 0, 56);*/

		/*density_tests::run_queue_integrity_test<heterogeneous_queue<void>>(1, 1,
			density_tests::LoadUnloadTestOptions{}, 1000);

		density_tests::run_queue_integrity_test<concurrent_heterogeneous_queue<void>>(1, 1,
			density_tests::LoadUnloadTestOptions{}, 1000);*/
	}

} // namespace density_tests