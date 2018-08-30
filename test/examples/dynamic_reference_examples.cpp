
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
    constexpr dynamic_reference<RT> r;
    static_assert(!r, "");
    static_assert(r.empty(), "");
    static_assert(r.address() == nullptr, "");
    static_assert(r.type() == RT(), "");
        //! [construct example 1]
        }

        {
        //! [construct example 2]
    int num = 1;
    dynamic_reference<RT> r(RT::make<int>(), &num);
    assert(r.type() == RT::make<int>()  && r.address() == &num);
        //! [construct example 2]
        }

        {
        int num = 1;
        auto r = make_dynamic_type<RT>(num);

        //! [copy construct example 1]
    auto r1 = r;
    assert(r1.type() == r.type() && r1.address() == r.address());
        //! [copy construct example 1]
        }

        {
        int num = 1;
        dynamic_reference<RT> r;
        auto r1 = make_dynamic_type<RT>(num);

        //! [copy assign example 1]
    r = r1;
    assert(r1.type() == r.type() && r1.address() == r.address());
        //! [copy assign example 1]
        }

        {
        //! [is example 1]
    int num = 1;
    auto r = make_dynamic_type(num);
    assert(r.is<int>());
        //! [is example 1]
        }

        {
        //! [as example 1]
    int num = 1;
    auto r = make_dynamic_type(num);
    assert(r.as<int>() == 1);
        //! [as example 1]
        }

        {
            int num = 1;
            auto r = make_dynamic_type<RT>(num);
        //! [clear example 1]
    r.clear();
    assert(!r);
    assert(r.empty());
    assert(r.address() == nullptr);
    assert(r.type() == RT());
        //! [clear example 1]
        }

        {
            int t = 1;
        //! [make_dynamic_type example 1]
    auto const r = make_dynamic_type<RT>(t);
    auto const r1 = dynamic_reference<RT>(RT::template make<decltype(t)>(), &t);
    assert(r1.type() == r.type() && r1.address() == r.address());
        //! [make_dynamic_type example 1]
        }

        {
            //! [ostream example 1]
    std::ostringstream dest;
    int t = 1;
    auto const r = make_dynamic_type<RT>(t);
    dest << r;
    assert(dest.str() == "1");
            //! [ostream example 1]
        }

        {
            //! [istream example 1]
    std::istringstream source("1");
    
    int t = 2;
    auto const r = make_dynamic_type<RT>(t);
    source >> r;
    assert(t == 1);
            //! [istream example 1]
        }
    }

    // clang-format on

} // namespace density_tests
