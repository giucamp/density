
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


    template <typename T1, typename T2> void dynamic_reference_test_cv_t()
    {
        using namespace density;

        //! [is_less_or_equal_cv_qualified example 2]
        static_assert(std::is_constructible<T1, T2>::value ==
            is_less_or_equal_cv_qualified(
                cv_qual_of<typename std::remove_reference<T2>::type>::value,
                cv_qual_of<typename std::remove_reference<T1>::type>::value), "");
        //! [is_less_or_equal_cv_qualified example 2]

        constexpr cv_qual CV1 = cv_qual_of<typename std::remove_reference<T1>::type>::value;
        constexpr cv_qual CV2 = cv_qual_of<typename std::remove_reference<T2>::type>::value;

        // test the viability of the constructor taking a raw reference
        static_assert(std::is_constructible<T1, T2>::value ==
            std::is_constructible<dynamic_reference<int, CV1>, T2>::value, "");

        // test the viability of the generalized copy-constructor
        static_assert(std::is_constructible<T1, T2>::value ==
            std::is_constructible<dynamic_reference<int, CV1>, dynamic_reference<int, CV2>>::value, "");
    }

    void dynamic_reference_test_cv()
    {
        using namespace density;

        //! [cv_qualifier example 1]
    static_assert(cv_qual_of<int>::value == cv_qual::no_qual, "");
    static_assert(cv_qual_of<int const>::value == cv_qual::const_qual, "");
    static_assert(cv_qual_of<int volatile>::value == cv_qual::volatile_qual, "");
    static_assert(cv_qual_of<int const volatile>::value == cv_qual::const_volatile_qual, "");
        //! [cv_qualifier example 1]

        //! [is_less_cv_qualified example 1]
    static_assert(!is_less_cv_qualified(cv_qual::no_qual, cv_qual::no_qual), "");
    static_assert(is_less_cv_qualified(cv_qual::no_qual, cv_qual::const_qual), "");
    static_assert(is_less_cv_qualified(cv_qual::no_qual, cv_qual::volatile_qual), "");
    static_assert(is_less_cv_qualified(cv_qual::no_qual, cv_qual::const_volatile_qual), "");
    
    static_assert(!is_less_cv_qualified(cv_qual::const_qual, cv_qual::no_qual), "");
    static_assert(!is_less_cv_qualified(cv_qual::const_qual, cv_qual::const_qual), "");
    static_assert(!is_less_cv_qualified(cv_qual::const_qual, cv_qual::volatile_qual), "");
    static_assert(is_less_cv_qualified(cv_qual::const_qual, cv_qual::const_volatile_qual), "");

    static_assert(!is_less_cv_qualified(cv_qual::volatile_qual, cv_qual::no_qual), "");
    static_assert(!is_less_cv_qualified(cv_qual::volatile_qual, cv_qual::const_qual), "");
    static_assert(!is_less_cv_qualified(cv_qual::volatile_qual, cv_qual::volatile_qual), "");
    static_assert(is_less_cv_qualified(cv_qual::volatile_qual, cv_qual::const_volatile_qual), "");

    static_assert(!is_less_cv_qualified(cv_qual::const_volatile_qual, cv_qual::no_qual), "");
    static_assert(!is_less_cv_qualified(cv_qual::const_volatile_qual, cv_qual::const_qual), "");
    static_assert(!is_less_cv_qualified(cv_qual::const_volatile_qual, cv_qual::volatile_qual), "");
    static_assert(!is_less_cv_qualified(cv_qual::const_volatile_qual, cv_qual::const_volatile_qual), "");
        //! [is_less_cv_qualified example 1]

        //! [is_less_or_equal_cv_qualified example 1]
    static_assert(is_less_or_equal_cv_qualified(cv_qual::no_qual, cv_qual::no_qual), "");
    static_assert(is_less_or_equal_cv_qualified(cv_qual::no_qual, cv_qual::const_qual), "");
    static_assert(is_less_or_equal_cv_qualified(cv_qual::no_qual, cv_qual::volatile_qual), "");
    static_assert(is_less_or_equal_cv_qualified(cv_qual::no_qual, cv_qual::const_volatile_qual), "");

    static_assert(!is_less_or_equal_cv_qualified(cv_qual::const_qual, cv_qual::no_qual), "");
    static_assert(is_less_or_equal_cv_qualified(cv_qual::const_qual, cv_qual::const_qual), "");
    static_assert(!is_less_or_equal_cv_qualified(cv_qual::const_qual, cv_qual::volatile_qual), "");
    static_assert(is_less_or_equal_cv_qualified(cv_qual::const_qual, cv_qual::const_volatile_qual), "");

    static_assert(!is_less_or_equal_cv_qualified(cv_qual::volatile_qual, cv_qual::no_qual), "");
    static_assert(!is_less_or_equal_cv_qualified(cv_qual::volatile_qual, cv_qual::const_qual), "");
    static_assert(is_less_or_equal_cv_qualified(cv_qual::volatile_qual, cv_qual::volatile_qual), "");
    static_assert(is_less_or_equal_cv_qualified(cv_qual::volatile_qual, cv_qual::const_volatile_qual), "");

    static_assert(!is_less_or_equal_cv_qualified(cv_qual::const_volatile_qual, cv_qual::no_qual), "");
    static_assert(!is_less_or_equal_cv_qualified(cv_qual::const_volatile_qual, cv_qual::const_qual), "");
    static_assert(!is_less_or_equal_cv_qualified(cv_qual::const_volatile_qual, cv_qual::volatile_qual), "");
    static_assert(is_less_or_equal_cv_qualified(cv_qual::const_volatile_qual, cv_qual::const_volatile_qual), "");
        //! [is_less_or_equal_cv_qualified example 1]

        //! [add_cv_qual example 1]
    static_assert(std::is_same<add_cv_qual<int, cv_qual::no_qual>::type, int>::value, "");
    static_assert(std::is_same<add_cv_qual<int *, cv_qual::const_qual>::type, int * const>::value, "");
    static_assert(std::is_same<add_cv_qual<int &, cv_qual::const_qual>::type, int &>::value, "");
    static_assert(std::is_same<add_cv_qual<const int, cv_qual::const_qual>::type, const int>::value, "");
    static_assert(std::is_same<add_cv_qual<const int, cv_qual::volatile_qual>::type, const volatile int>::value, "");
        //! [add_cv_qual example 1]

        //! [add_cv_qual example 2]
    static_assert(std::is_same<add_cv_qual_t<int, cv_qual::no_qual>, int>::value, "");
    static_assert(std::is_same<add_cv_qual_t<int *, cv_qual::const_qual>, int * const>::value, "");
    static_assert(std::is_same<add_cv_qual_t<int &, cv_qual::const_qual>, int &>::value, "");
    static_assert(std::is_same<add_cv_qual_t<const int, cv_qual::const_qual>, const int>::value, "");
    static_assert(std::is_same<add_cv_qual_t<const int, cv_qual::volatile_qual>, const volatile int>::value, "");
        //! [add_cv_qual example 2]

        dynamic_reference_test_cv_t<int &, int &>();
        dynamic_reference_test_cv_t<int &, const int &>();
        dynamic_reference_test_cv_t<int &, volatile int &>();
        dynamic_reference_test_cv_t<int &, const volatile int &>();

        dynamic_reference_test_cv_t<const int &, int &>();
        dynamic_reference_test_cv_t<const int &, const int &>();
        dynamic_reference_test_cv_t<const int &, volatile int &>();
        dynamic_reference_test_cv_t<const int &, const volatile int &>();

        dynamic_reference_test_cv_t<volatile int &, int &>();
        dynamic_reference_test_cv_t<volatile int &, const int &>();
        dynamic_reference_test_cv_t<volatile int &, volatile int &>();
        dynamic_reference_test_cv_t<volatile int &, const volatile int &>();

        dynamic_reference_test_cv_t<const volatile int &, int &>();
        dynamic_reference_test_cv_t<const volatile int &, const int &>();
        dynamic_reference_test_cv_t<const volatile int &, volatile int &>();
        dynamic_reference_test_cv_t<const volatile int &, const volatile int &>();
    }


    void dynamic_reference_examples()
    {
        using namespace density;

        using RT = runtime_type<default_type_features, f_ostream, f_istream>;
        using T = int;
        constexpr cv_qual CV_QUAL = cv_qual::no_qual;

        static_assert(!std::is_default_constructible<dynamic_reference<RT>>::value, "");

        {
        int t = 1;
        //! [construct example 1]
    dynamic_reference<RT, CV_QUAL> r(t);
    assert(r.type() == RT::make<decltype(t)>() && r.address() == &t);
        //! [construct example 1]
        }

        {
      //! [construct example 2]
    int i = 42;
    dynamic_reference<RT> r1(i);
    const_dynamic_reference<RT> r2(i);

    int const ci = 42;
    //dynamic_reference<RT> r3(ci); // <- error, would drop the const-ness
    const_volatile_dynamic_reference<RT> r4(ci);

    int volatile vi = 42;
    //const_dynamic_reference<RT> r5(vi); // <- error, would drop the volatile-ness
    const_volatile_dynamic_reference<RT> r6(vi);

    // dynamic_reference is constructible from non-cv-qualified reference
    static_assert(std::is_constructible<dynamic_reference<RT>, T&>::value, "");
    static_assert(!std::is_constructible<dynamic_reference<RT>, const T&>::value, "");
    static_assert(!std::is_constructible<dynamic_reference<RT>, volatile T&>::value, "");
    static_assert(!std::is_constructible<dynamic_reference<RT>, const volatile T&>::value, "");

    // const_dynamic_reference is constructible from non-volatile references
    static_assert(std::is_constructible<const_dynamic_reference<RT>, T&>::value, "");
    static_assert(std::is_constructible<const_dynamic_reference<RT>, const T&>::value, "");
    static_assert(!std::is_constructible<const_dynamic_reference<RT>, volatile T&>::value, "");
    static_assert(!std::is_constructible<const_dynamic_reference<RT>, const volatile T&>::value, "");

    // volatile_dynamic_reference is constructible from non-const references
    static_assert(std::is_constructible<volatile_dynamic_reference<RT>, T&>::value, "");
    static_assert(!std::is_constructible<volatile_dynamic_reference<RT>, const T&>::value, "");
    static_assert(std::is_constructible<volatile_dynamic_reference<RT>, volatile T&>::value, "");
    static_assert(!std::is_constructible<volatile_dynamic_reference<RT>, const volatile T&>::value, "");

    // const_volatile_dynamic_reference is constructible from any reference
    static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, T&>::value, "");
    static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, const T&>::value, "");
    static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, volatile T&>::value, "");
    static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, const volatile T&>::value, "");
      //! [construct example 2]
        }

        {
        int t = 1;
        //! [construct2 example 1]
    dynamic_reference<RT> r(RT::make<decltype(t)>(), &t);
    assert(r.type() == RT::make<decltype(t)>() && r.address() == &t);
        //! [construct2 example 1]
        }

        {
        int t = 1;
        dynamic_reference<RT> r(t);

        //! [copy construct example 1]
    auto r1 = r;
    assert(r1.type() == r.type() && r1.address() == r.address());
        //! [copy construct example 1]
        }

        {    
        //! [copy construct example 2]
    int t = 1;
    dynamic_reference<RT> r(t);
    const_dynamic_reference<RT> r1(r); // <- ok
    //dynamic_reference<RT> r2(r1); // <- error, would drop the const-ness
    const_volatile_dynamic_reference<RT> r3(r1); // <- ok

    // dynamic_reference is constructible from non-cv-qualified reference
    static_assert(std::is_constructible<dynamic_reference<RT>, dynamic_reference<RT>>::value, "");
    static_assert(!std::is_constructible<dynamic_reference<RT>, const_dynamic_reference<RT>>::value, "");
    static_assert(!std::is_constructible<dynamic_reference<RT>, volatile_dynamic_reference<RT>>::value, "");
    static_assert(!std::is_constructible<dynamic_reference<RT>, const_volatile_dynamic_reference<RT>>::value, "");

    // const_dynamic_reference is constructible from non-volatile references
    static_assert(std::is_constructible<const_dynamic_reference<RT>, dynamic_reference<RT>>::value, "");
    static_assert(std::is_constructible<const_dynamic_reference<RT>, const_dynamic_reference<RT>>::value, "");
    static_assert(!std::is_constructible<const_dynamic_reference<RT>, volatile_dynamic_reference<RT>>::value, "");
    static_assert(!std::is_constructible<const_dynamic_reference<RT>, const_volatile_dynamic_reference<RT>>::value, "");

    // volatile_dynamic_reference is constructible from non-const references
    static_assert(std::is_constructible<volatile_dynamic_reference<RT>, dynamic_reference<RT>>::value, "");
    static_assert(!std::is_constructible<volatile_dynamic_reference<RT>, const_dynamic_reference<RT>>::value, "");
    static_assert(std::is_constructible<volatile_dynamic_reference<RT>, volatile_dynamic_reference<RT>>::value, "");
    static_assert(!std::is_constructible<volatile_dynamic_reference<RT>, const_volatile_dynamic_reference<RT>>::value, "");

    // const_volatile_dynamic_reference is constructible from any reference
    static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, dynamic_reference<RT>>::value, "");
    static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, const_dynamic_reference<RT>>::value, "");
    static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, volatile_dynamic_reference<RT>>::value, "");
    static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, const_volatile_dynamic_reference<RT>>::value, "");
        //! [copy construct example 2]
        }

        {
        //! [is example 1]
    int t = 1;
    dynamic_reference<RT> r(t);
    assert(r.is<int>());
    assert(r.is<const int>());

    const_dynamic_reference<RT> r1(t);
    assert(!r1.is<int>());
    assert(r1.is<const int>());
    assert(r1.is<const volatile int>());
        //! [is example 1]
        }

        {
        //! [as example 1]
    int t = 1;
    dynamic_reference<RT> r(t);
    assert(r.as<int>() == 1);
    assert(r.as<const int>() == 1);
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

        dynamic_reference_test_cv();
    }

    // clang-format on

} // namespace density_tests
