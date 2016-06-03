
#include "../density/lifo.h"
#include "../testity/testing_utils.h"


namespace density
{
	namespace tests
	{

		void lifo_test()
		{
			lifo_buffer<> a(16);
			lifo_buffer<> b(32);
		}

	} // namespace tests

	void lifo_test()
	{
		tests::lifo_test();
	}

} // namespace density

