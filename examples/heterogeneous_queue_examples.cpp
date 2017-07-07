
#include <string>
#include <ostream>
#include <assert.h>
#include <density/heterogeneous_queue.h>
#include "../density_tests/test_framework/progress.h"

namespace density_tests
{
void heterogeneous_queue_samples(std::ostream & i_ostream)
{
	PrintScopeDuration(i_ostream, "heterogeneous queue samples");

	using namespace density;
		
	//! [heterogeneous_queue example 1]
	heterogeneous_queue<> queue;
	queue.push(19);
	queue.push(std::string("hello world!"));
	//! [heterogeneous_queue example 1]

	//! [heterogeneous_queue example 2]
	auto consume = queue.try_start_consume();
	int my_int = consume.element<int>();
	consume.commit();

	consume = queue.try_start_consume();
	std::string my_string = consume.element<std::string>();
	consume.commit();
	//! [heterogeneous_queue example 3]
	(void)my_int;
	(void)my_string;
}

} // namespace density_tests
