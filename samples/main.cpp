
namespace density
{
	void pointer_arithmetic_test();
	void dense_queue_test();
	void list_benchmark();
	void list_test();
	void paged_queue_test();
}

#include <iostream>
#include <vector>
#include "..\density\density_common.h"

int main()
{	
	using namespace density;

	paged_queue_test();
	dense_queue_test();

	pointer_arithmetic_test();
		
	list_test();
	list_benchmark();

	return 0;
}