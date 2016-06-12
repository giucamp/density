
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/lifo.h"
#include "../testity/test_tree.h"
#include <functional>
#include <deque>
#include <queue>
#include <vector>

namespace density
{
    namespace tests
    {
        using namespace testity;

        struct Virtual
        {
            virtual ~Virtual() {}
        };

        PerformanceTestGroup make_lifo_array_benchmarks()
        {
            PerformanceTestGroup group("create array", "density version: " + std::to_string(DENSITY_VERSION));

            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                lifo_array< Virtual > array(i_cardinality);
            }, __LINE__);

            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                std::vector< Virtual > vector(i_cardinality);
            }, __LINE__);

            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
                Virtual * cpp98_array = new Virtual[i_cardinality];
                delete[] cpp98_array;
            }, __LINE__);

            return group;
        }

    } // namespace tests

} // namespace density
