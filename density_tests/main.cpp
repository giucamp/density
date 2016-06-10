
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <vector>
#include "..\testity\test_tree.h"

namespace density
{
    void pointer_arithmetic_test();
    void dense_queue_test();
    void list_benchmark();
    void list_test();
    void paged_queue_test();
	void lifo_test();
	void function_queue_test();

    namespace tests
    {
		testity::PerformanceTestGroup make_list_benchmarks();
		testity::PerformanceTestGroup make_function_queue_benchmarks();
		testity::PerformanceTestGroup make_lifo_array_benchmarks();
    }
}

int main()
{
    using namespace density;
	using namespace density::tests;
	using namespace testity;

	list_benchmark();

	#ifdef  _DEBUG
		function_queue_test();
		list_test();
		lifo_test();
		paged_queue_test();
		dense_queue_test();
	#endif //  _DEBUG

    testity::TestTree test_tree("");

	//test_tree["/density/lifo_array_test"].add_performance_test(make_lifo_array_benchmarks());
	//test_tree["/density/function_queue_test"].add_performance_test(make_function_queue_benchmarks());
	test_tree["/density/list_test"].add_performance_test(make_list_benchmarks());
	
    testity::Session test_session;
    auto results = test_session.run(test_tree, std::cout);
    results.save_to("perf.txt");

    /*pointer_arithmetic_test();

    list_test();
    list_benchmark();*/

    return 0;
}
