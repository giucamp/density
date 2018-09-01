
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//
#include <complex>
#include <density/dynamic_reference.h>
#include <density/io_runtimetype_features.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <type_traits>

namespace density_tests
{
    // clang-format off

    void dynamic_reference_examples()
    {
        using namespace density;

        using RT = runtime_type<default_type_features, f_ostream, f_istream>;

        {
        //! [construct example 1]
    int target = 1;
    dynamic_reference<RT> r(target);
    assert(r.type() == RT::make<int>()  && r.address() == &target);
        //! [construct example 1]
        }

        {
        //! [construct example 2]
    int target = 1;
    dynamic_reference<RT> r(RT::make<int>(), &target);
    assert(r.type() == RT::make<int>()  && r.address() == &target);
        //! [construct example 2]
        }

        {
        int target = 1;
        dynamic_reference<RT> r(target);

        //! [copy construct example 1]
    auto r1 = r;
    assert(r1.type() == r.type() && r1.address() == r.address());
        //! [copy construct example 1]
        }

        {
        //! [is example 1]
    int target = 1;
    dynamic_reference<RT> r(target);
    assert(r.is<int>());
        //! [is example 1]
        }

        {
        //! [as example 1]
    int target = 1;
    dynamic_reference<RT> r(target);
    assert(r.as<int>() == 1);
        //! [as example 1]
        }

        {
            //! [ostream example 1]
    std::ostringstream dest;
    int t = 1;
    dynamic_reference<RT> r(t);
    dest << r;
    assert(dest.str() == "1");
            //! [ostream example 1]
        }

        {
            //! [istream example 1]
    std::istringstream source("1");
    
    int t = 2;
    dynamic_reference<RT> r(t);
    source >> r;
    assert(t == 1);
            //! [istream example 1]
        }
    }

    // clang-format on

} // namespace density_tests
