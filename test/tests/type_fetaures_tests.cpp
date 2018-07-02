
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
          std::is_same<FIRST_TUPLE, density::detail::Tuple_Merge_t<OTHER_TUPLES...>>::value,
          "tuple merge error");
    }

    void test_tuple_merge()
    {
        using std::tuple;

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
        static_assert(
          density::detail::Tuple_FindFirst<TUPLE, TARGET_TYPE>::index == EXPECTED_INDEX,
          "tuple find error");
    }

    template <typename TUPLE_1, typename TARGET_TYPE, typename TUPLE_2> void tuple_remove()
    {
        using tuple_2 = density::detail::Tuple_Remove_t<TUPLE_1, TARGET_TYPE>;
        static_assert(std::is_same<TUPLE_2, tuple_2>::value, "tuple_remove failed");
    }

    void test_tuple_remove()
    {
        using std::tuple;

        tuple_remove<std::tuple<>, int, std::tuple<>>();
        tuple_remove<std::tuple<int, int>, int, std::tuple<>>();
        tuple_remove<std::tuple<int, float, char, int>, int, std::tuple<float, char>>();
        tuple_remove<std::tuple<float, char, int>, void **, std::tuple<float, char, int>>();
        tuple_remove<
          std::tuple<float, char, int, int, void *, double, double>,
          int,
          std::tuple<float, char, void *, double, double>>();
    }

    void test_tuple_find()
    {
        using std::tuple;

        tuple_find<tuple<>, int, 0>();

        tuple_find<tuple<int, float, double>, int, 0>();
        tuple_find<tuple<int, float, double>, float, 1>();
        tuple_find<tuple<int, float, double>, double, 2>();
        tuple_find<tuple<int, float, double>, void *, 3>();
    }

    void test_feature_diff()
    {
        using density::detail::Tuple_Diff_t;
        using std::tuple;

        static_assert(
          std::is_same<
            tuple<int, char, float>,
            Tuple_Diff_t<
              tuple<int, double, void **, char, void ***, float>,
              tuple<double, void *, void **, void ***>>>::value,
          "Tuple_Diff_t failed");
    }

    template <typename FEATURE_SET, typename... FEATURES> void check_feature_set()
    {
        static_assert(
          std::is_same<typename FEATURE_SET::tuple_type, std::tuple<FEATURES...>>::value,
          "feature_set test failed");
    };

    void test_feature_set()
    {
        using namespace density;

        check_feature_set<
          feature_list<f_size, f_size, f_alignment, f_none, f_alignment>,
          f_size,
          f_alignment>();

        check_feature_set<
          feature_list<
            feature_list<feature_list<f_none>>,
            feature_list<f_default_construct, f_size>,
            f_size,
            f_size,
            f_alignment,
            f_none,
            f_alignment,
            feature_list<feature_list<>>>,

          f_default_construct,
          f_size,
          f_alignment>();

        check_feature_set<
          feature_list<
            feature_list<feature_list<f_none, f_copy_construct, feature_list<>>>,
            feature_list<f_default_construct, f_size>,
            f_size,
            f_none,
            feature_list<f_copy_construct>,
            f_size,
            f_alignment,
            f_none,
            f_alignment,
            feature_list<feature_list<>>>,

          f_copy_construct,
          f_default_construct,
          f_size,
          f_alignment>();


        using f1 = feature_list<f_size, f_size, f_alignment>;
        check_feature_set<f1, f_size, f_alignment>();

        using f2 = feature_list<f_default_construct, f_size, f_destroy, f_rtti>;
        check_feature_set<f2, f_default_construct, f_size, f_destroy, f_rtti>();

        using f3 = feature_list<f_copy_construct, f_move_construct, f_equals>;
        check_feature_set<f3, f_copy_construct, f_move_construct, f_equals>();

        using fu1 = feature_list<f_size, f1, f2, f_hash>;
        using fu2 = feature_list<f1, feature_list<f2, f3>>;
        check_feature_set<
          fu1,
          f_size,
          f_alignment,
          f_default_construct,
          f_destroy,
          f_rtti,
          f_hash>();

        auto t1 = runtime_type<f1>::make<int>();
        t1.size();

        auto ru1 = runtime_type<fu1>::make<int>();
        ru1.size();

        {
            auto   ru2 = runtime_type<fu2>::make<int>();
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
        test_tuple_remove();
        test_tuple_find();
        test_feature_set();
        test_feature_diff();
    }

} // namespace density_tests
