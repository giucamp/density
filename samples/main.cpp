
#include <random>

namespace density
{
	void fixed_queue_test();
	void list_benchmark();
	void list_test();
}

int main()
{
	using namespace density;

	fixed_queue_test();
	list_test();
	list_benchmark();
	return 0;
}