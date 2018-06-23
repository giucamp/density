
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"

//

#include <density/runtime_type.h>
#include <type_traits>

namespace density_tests
{

    template <typename FIRST_TUPLE, typename... OTHER_TUPLES> void tuple_merge()
    {
        static_assert(
          std::is_same<FIRST_TUPLE, density::type_features::Tuple_Merge_t<OTHER_TUPLES...>>::value,
          "tuple merge error");
    }

    void test_tuple_merge()
    {
        using namespace std;

        tuple_merge<tuple<>>();
        tuple_merge<tuple<>, tuple<>>();
        tuple_merge<tuple<>, tuple<>, tuple<>>();

        tuple_merge<tuple<int *>, tuple<int *>>();
        tuple_merge<tuple<int *>, tuple<>, tuple<int *>>();
        tuple_merge<
          tuple<int ***, std::tuple<float>, float>,
          tuple<>,
          tuple<int ***, std::tuple<float>, float>>();

        tuple_merge<tuple<int, double>, tuple<>, tuple<int>, tuple<double>>();
        tuple_merge<tuple<int, char, double>, tuple<int, char>, tuple<double>>();
        tuple_merge<
          tuple<int, char, double, void *>,
          tuple<int, char>,
          tuple<double>,
          tuple<void *>>();

        tuple_merge<
          tuple<int, char, double, void *>,
          tuple<>,
          tuple<>,
          tuple<int, char>,
          tuple<>,
          tuple<double>,
          tuple<void *>,
          tuple<>>();
    }

    template <typename TUPLE, typename TARGET_TYPE, size_t EXPECTED_INDEX> void tuple_find()
    {
        using namespace density;
        using namespace type_features;
        static_assert(
          Tuple_FindFirst<TUPLE, TARGET_TYPE>::index == EXPECTED_INDEX, "tuple find error");
    }

    void test_tuple_find()
    {
        using namespace std;
        using namespace density;
        using namespace type_features;

        tuple_find<tuple<>, int, 0>();

        tuple_find<tuple<int, float, double>, int, 0>();
        tuple_find<tuple<int, float, double>, float, 1>();
        tuple_find<tuple<int, float, double>, double, 2>();
        tuple_find<tuple<int, float, double>, void *, 3>();
    }

    void test_feature_cat()
    {
        using namespace density;
        using namespace type_features;
        using f1 = feature_list<size, alignment>;
        using f2 = feature_list<default_construct, destroy, rtti>;
        using f3 = feature_list<copy_construct, move_construct, equals>;

        using fu1 = feature_cat<f1, f2>;
        using fu2 = feature_cat<f1, feature_cat<f2, f3>>;

        auto t1 = runtime_type<void, f1>::make<int>();
        t1.size();

        auto ru1 = runtime_type<void, fu1>::make<int>();
        ru1.size();

        {
            auto   ru2 = runtime_type<void, fu2>::make<int>();
            void * mem = aligned_allocate(ru2.size(), ru2.alignment());

            ru2.default_construct(mem);
            auto & type_inf = ru2.type_info();
            DENSITY_TEST_ASSERT(type_inf == typeid(int));
            ru2.destroy(mem);

            int five = 5;
            ru2.copy_construct(mem, &five);
            DENSITY_TEST_ASSERT(ru2.are_equal(mem, &five));
            ru2.destroy(mem);

            aligned_deallocate(mem, ru2.size(), ru2.alignment());
        }
    }

    void type_fetaures_tests()
    {
        test_tuple_merge();
        test_feature_cat();
    }

} // namespace density_tests
