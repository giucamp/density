
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//

#include <complex>
#include <density/heter_queue.h>
#include <density/io_runtimetype_features.h>
#include <density/lf_heter_queue.h>
#include <iostream>
#include <string>

// clang-format off

namespace density_tests
{
    void overview_examples()
    {
        using namespace density;


        {
    //! [queue example 1]
    // non-concurrent queue
    heter_queue<> queue;
    queue.push(42);
    queue.emplace<std::complex<double>>(1, 2);

    // lock-free queue
    lf_heter_queue<> lf_queue;
    lf_queue.push(42);
    lf_queue.emplace<std::complex<double>>(1, 2);
    //! [queue example 1]
        }

        {
        //! [queue example 2]        
    using type = runtime_type<default_type_features, f_ostream>;
    
    heter_queue<type> queue;
    queue.push(42);
    queue.emplace<std::complex<double>>(1, 2);

    for(const auto & val : queue)
        val.first.get_feature<f_ostream>()(std::cout, val.second);

        //! [queue example 2]
        }
    }


} // namespace density_tests

// clang-format on
