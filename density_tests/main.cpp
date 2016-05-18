
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

    namespace tests
    {
        void register_function_queue_benchmarks(testity::TestTree & i_test_manager);
    }
}



int main()
{
    using namespace density;

    //paged_queue_test();
    //dense_queue_test();

    testity::TestTree test_tree("");
    tests::register_function_queue_benchmarks(test_tree);

    testity::Session test_session;
    auto results = test_session.run(test_tree, std::cout);
    results.save_to("perf.txt");

    /*pointer_arithmetic_test();

    list_test();
    list_benchmark();*/

    return 0;
}
