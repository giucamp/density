
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

    using RT = runtime_type<default_type_features, f_ostream, f_istream>;
    using T = int;

      static_assert(!std::is_default_constructible<dynamic_reference<RT>>::value, "");

      // dynamic_reference is constructible from any reference
      static_assert(std::is_constructible<dynamic_reference<RT>, T&>::value, "");
      static_assert(!std::is_constructible<dynamic_reference<RT>, const T&>::value, "");
      static_assert(!std::is_constructible<dynamic_reference<RT>, volatile T&>::value, "");
      static_assert(!std::is_constructible<dynamic_reference<RT>, const volatile T&>::value, "");

      // const_dynamic_reference is constructible only from const references
      static_assert(std::is_constructible<const_dynamic_reference<RT>, T&>::value, "");
      static_assert(std::is_constructible<const_dynamic_reference<RT>, const T&>::value, "");
      static_assert(!std::is_constructible<const_dynamic_reference<RT>, volatile T&>::value, "");
      static_assert(!std::is_constructible<const_dynamic_reference<RT>, const volatile T&>::value, "");

      // const_dynamic_reference is constructible only from volatile references
      static_assert(std::is_constructible<volatile_dynamic_reference<RT>, T&>::value, "");
      static_assert(!std::is_constructible<volatile_dynamic_reference<RT>, const T&>::value, "");
      static_assert(std::is_constructible<volatile_dynamic_reference<RT>, volatile T&>::value, "");
      static_assert(!std::is_constructible<volatile_dynamic_reference<RT>, const volatile T&>::value, "");

      // const_volatile_dynamic_reference is constructible only from const volatile references
      static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, T&>::value, "");
      static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, const T&>::value, "");
      static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, volatile T&>::value, "");
      static_assert(std::is_constructible<const_volatile_dynamic_reference<RT>, const volatile T&>::value, "");

      {
            /*int t = 1;
            int const ct = 1;
            int volatile vt = 1;
            int const volatile cvt = 1;*/
      }


        {
        int t = 1;
        //! [construct example 1]
    dynamic_reference<RT> r(t);
    assert(r.type() == RT::make<decltype(t)>()  && r.address() == &t);
        //! [construct example 1]
        }

        {
        int t = 1;
        //! [construct example 2]
    dynamic_reference<RT> r(RT::make<decltype(t)>(), &t);
    assert(r.type() == RT::make<decltype(t)>()  && r.address() == &t);
        //! [construct example 2]
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
        //! [is example 1]
    int t = 1;
    dynamic_reference<RT> r(t);
    assert(r.is<int>());
    assert(r.is<const int>());
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
    }

    template <typename T1, typename T2> void test_cv()
    {
        using namespace density;

        //! [is_less_or_equal_cv_qualified example 2]
    static_assert( std::is_constructible<T1, T2>::value ==
        is_less_or_equal_cv_qualified(
            cv_qual_of<typename std::remove_reference<T2>::type>::value,
            cv_qual_of<typename std::remove_reference<T1>::type>::value), "");
        //! [is_less_or_equal_cv_qualified example 2]

        constexpr cv_qual CV = cv_qual_of<typename std::remove_reference<T1>::type>::value;

        static_assert(std::is_constructible<T1, T2>::value ==
            std::is_constructible<dynamic_reference<int, CV>, T2>::value, "");
    }

    void test_is_less_or_equal_cv_qualified()
    {
        using namespace density;

        test_cv<int &, int &>();
        test_cv<int &, const int &>();
        test_cv<int &, volatile int &>();
        test_cv<int &, const volatile int &>();

        test_cv<const int &, int &>();
        test_cv<const int &, const int &>();
        test_cv<const int &, volatile int &>();
        test_cv<const int &, const volatile int &>();

        test_cv<volatile int &, int &>();
        test_cv<volatile int &, const int &>();
        test_cv<volatile int &, volatile int &>();
        test_cv<volatile int &, const volatile int &>();

        test_cv<const volatile int &, int &>();
        test_cv<const volatile int &, const int &>();
        test_cv<const volatile int &, volatile int &>();
        test_cv<const volatile int &, const volatile int &>();
    }

    // clang-format on

} // namespace density_tests
