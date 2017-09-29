
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/queue_load_unload_test.h"
#include <density/lf_heter_queue.h>
#include <density/conc_heter_queue.h>
#include <ostream>

namespace density_tests
{
    void load_unload_tests(std::ostream & i_ostream)
    {
        using namespace density;

        density_tests::queue_load_unload_test<lf_heter_queue<>>(
            300, 100, i_ostream);

        density_tests::queue_load_unload_test<lf_heter_queue<>>(
            7, 4000, i_ostream);

        /*density_tests::queue_load_unload_test<conc_heter_queue<>>(
            3000, 1000, i_ostream);

        density_tests::queue_load_unload_test<conc_heter_queue<>>(
            7, 10000000, i_ostream);*/

        /*EasyRandom rand;
        density_tests::run_queue_integrity_test<lf_heter_queue<>>(3000, 300,
            rand, density_tests::LoadUnloadTestOptions{.5,60, 0});*/

        /*using q = lf_heter_queue<void, runtime_type<void>, density_tests::NonblockingTestAllocator<density::default_page_capacity> >;
        {
            q a;
            for(int i = 0; i < 260; i++)
                a.push(1);
            a.start_consume().commit();
        }
        density_tests::run_queue_integrity_test<q>(4, 4,
            density_tests::LoadUnloadTestOptions{50,60, 0}, 0, 56);*/

        /*density_tests::run_queue_integrity_test<heter_queue<void>>(1, 1,
            density_tests::LoadUnloadTestOptions{}, 1000);

        density_tests::run_queue_integrity_test<conc_heter_queue<void>>(1, 1,
            density_tests::LoadUnloadTestOptions{}, 1000);*/
    }

} // namespace density_tests
