
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/any.h>
#include <testity/testity_common.h>
#include <testity/test_tree.h>
#include <type_traits>

namespace density_tests
{
    using namespace density;
    using namespace testity;

    template <typename TYPE>
        void test_value(const TYPE & i_value)
    {
        // copy construct
        TYPE copy1(i_value);
        TESTITY_ASSERT(copy1 == i_value);
        TESTITY_ASSERT( !(copy1 != i_value) );

        // move construct
        TYPE copy2(std::move(copy1));
        TESTITY_ASSERT(copy2 == i_value);
        TESTITY_ASSERT(!(copy2 != i_value));

        // copy assign
        copy1 = i_value;
        TESTITY_ASSERT(copy1 == i_value);
        TESTITY_ASSERT(!(copy1 != i_value));

        // move assign
        copy2 = std::move(copy1);
        TESTITY_ASSERT(copy1 == i_value);
        TESTITY_ASSERT(!(copy1 != i_value));
    }


    void add_any_cases(TestTree & i_dest)
    {
        static_assert(std::is_default_constructible<any<>>::value, "");
        static_assert(std::is_copy_constructible<any<>>::value, "");
        static_assert(std::is_copy_assignable<any<>>::value, "");
        static_assert(!std::is_nothrow_copy_constructible<any<>>::value, "");
        static_assert(!std::is_nothrow_copy_assignable<any<>>::value, "");
        static_assert(std::is_nothrow_move_constructible<any<>>::value, "");
        static_assert(std::is_nothrow_move_assignable<any<>>::value, "");

        using TestFunc = std::function< void(std::mt19937 & i_random, any<> & i_any)>;

        i_dest.add_case(TestFunc([](std::mt19937 & i_random, any<> & i_any) {

            using namespace type_features;
            using RuntimeType = runtime_type<void, feature_concat_t< default_type_features_t<void>, equals>>;
            test_value(any<void, void_allocator, RuntimeType>::make<int>(6));

            any<> a = any<>::make<int>(6);
            any<> d;
            auto b = a;
            auto c = std::move(a);
            a = std::move(c);
            a = b;
            d = b;
        }));
    }

} // namespace density_tests

