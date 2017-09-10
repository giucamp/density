

#include "density_test_common.h"
#include "easy_random.h"
#include <iostream>
#include <assert.h>
#include <thread>
#include <memory>

namespace density_tests
{
	namespace detail
	{
		void assert_failed(const char * i_source_file, const char * i_function, int i_line, const char * i_expr)
		{
			std::cerr << "assert failed in " << i_source_file << " (" << i_line << ")\n";
			std::cerr << "function: " << i_function << "\n";
			std::cerr << "expression: " << i_expr << std::endl;
			
			assert(false);
		}
	}
}