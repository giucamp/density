
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
        void add_list_benchmarks(testity::TestTree & i_tree);
		void add_queue_benchmarks(testity::TestTree & i_tree);
        void add_function_queue_benchmarks(testity::TestTree & i_tree);
		void add_allocator_benchmarks(testity::TestTree & i_tree);
        testity::PerformanceTestGroup make_lifo_array_benchmarks();
    }
}

namespace producer_consumer_sample  { void run(); }
namespace function_queue_sample	    { void run(); }
namespace runtime_type_sample		{ void run(); }
namespace lifo_sample				{ void run(); }
namespace misc_samples				{ void run(); }

int main()
{
    using namespace density;
    using namespace density::tests;
    using namespace testity;

	//#ifndef NDEBUG		
		paged_queue_test();
		dense_queue_test();
		lifo_test();
		list_test();
		function_queue_test();
	//#endif

    testity::TestTree test_tree("");
	
    test_tree["/density/lifo_array_test"].add_performance_test(make_lifo_array_benchmarks());
	add_queue_benchmarks(test_tree["/density/queue_test"]);
    add_function_queue_benchmarks(test_tree["/density/function_queue_test"]);
	add_allocator_benchmarks(test_tree["/density/page_allocator_test"]);
	//add_list_benchmarks(test_tree["/density/list_test"]);

    testity::Session test_session;
    auto results = test_session.run(test_tree["/density/page_allocator_test"], std::cout);
	results.save_to("perf.txt");

    /*pointer_arithmetic_test();
    list_test();
    list_benchmark();*/

	misc_samples::run();
	producer_consumer_sample::run();
	function_queue_sample::run();
	lifo_sample::run();
	runtime_type_sample::run();

    return 0;
}
