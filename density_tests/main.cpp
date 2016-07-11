
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <vector>
#include "..\testity\test_tree.h"
#include "..\testity\test_session.h"

namespace density_tests
{
	using namespace testity;
	
	void make_heterogeneous_array_functionality_tests(TestTree & i_dest);
}

int main()
{
	using namespace testity;
	using namespace density_tests;

	TestTree test_tree("density");

	make_heterogeneous_array_functionality_tests(test_tree["heterogeneous_array"]);

	run_session(test_tree);

    return 0;
}
