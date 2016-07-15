
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/heterogeneous_array.h"
#include "../testity/testity_common.h"
#include "../testity/test_tree.h"
#include "../testity/test_classes.h"
#include "container_test.h"
#include "test_allocators.h"
#include <algorithm>
#include <array>

namespace density_tests
{
    using namespace density;
    using namespace testity;

    template <typename TYPE>
        using HeterogeneousArray = heterogeneous_array<TYPE, TestVoidAllocator, runtime_type<TYPE,
            typename type_features::feature_concat< typename type_features::default_type_features_t<TYPE>, type_features::feature_list<type_features::hash, type_features::equals> >::type> >;

    template <typename TYPE>
        struct HeterogeneousArrayTest
    {
        HeterogeneousArray<TYPE> m_array;
        ShadowContainer<HeterogeneousArray<TYPE>> m_shadow;

        void compare() const
        {
            m_shadow.compare_all(m_array);
        }
    };


    template <typename TYPE>
        void add_common_heterogeneous_array_cases(TestTree & i_dest)
    {
        using TestTarget = HeterogeneousArrayTest<TYPE>;
        using TestFunc = std::function< void(std::mt19937 & i_random, TestTarget & i_target)>;

        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            HeterogeneousArray<TYPE> tmp_array;
            TESTITY_ASSERT(tmp_array.size() == 0);
            TESTITY_ASSERT(tmp_array.empty());
            TESTITY_ASSERT(tmp_array.begin() == tmp_array.end());
            tmp_array = i_target.m_array;
            TESTITY_ASSERT(tmp_array == i_target.m_array);

            HeterogeneousArray<TYPE> tmp_array_1(tmp_array);
            TESTITY_ASSERT(tmp_array_1 == i_target.m_array);

            HeterogeneousArray<TYPE> tmp_array_2(std::move(tmp_array));
            TESTITY_ASSERT(tmp_array_2 == i_target.m_array);
            TESTITY_ASSERT(tmp_array.size() == 0);

            tmp_array = std::move(tmp_array_2);
            TESTITY_ASSERT(tmp_array == i_target.m_array);
            TESTITY_ASSERT(tmp_array_2.size() == 0);

            tmp_array.clear();
            TESTITY_ASSERT(tmp_array.size() == 0);
        }));
    }

    void add_void_heterogeneous_array_cases(TestTree & i_dest)
    {
        using TestTarget = HeterogeneousArrayTest<void>;
        using TestFunc = std::function< void(std::mt19937 & i_random, TestTarget & i_target)>;

                    /*---- push_back ----*/

        // push_back(1)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            i_target.m_array.push_back(1);
            i_target.m_shadow.push_back(1);
            i_target.compare();
        }));

        // push_back(1.0)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            i_target.m_array.push_back(1.0);
            i_target.m_shadow.push_back(1.0);
            i_target.compare();
        }));

        // push_back('c')
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            i_target.m_array.push_back('c');
            i_target.m_shadow.push_back('c');
            i_target.compare();
        }));

        // push_back(ElementType(seed) as rvalue), sizeof(ElementType) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::Supported, 3, 1>;
            i_target.m_array.push_back(ElementType(seed));
            i_target.m_shadow.push_back(ElementType(seed));
            i_target.compare();
        }));

        // push_back(CopyableTestClass(seed) as rvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            i_target.m_array.push_back(CopyableTestClass(seed));
            i_target.m_shadow.push_back(CopyableTestClass(seed));
            i_target.compare();
        }));

        // push_back(TestClass(seed) as lvalue), sizeof(TestClass) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::Supported, 3, 1>;
            const ElementType element(seed);
            i_target.m_array.push_back(element);
            i_target.m_shadow.push_back(element);
            i_target.compare();
        }));

        // push_back(CopyableTestClass(seed) as lvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            const CopyableTestClass element(seed);
            i_target.m_array.push_back(element);
            i_target.m_shadow.push_back(element);
            i_target.compare();
        }));

                    /*---- push_front ----*/

        // push_front(1)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            i_target.m_array.push_front(1);
            i_target.m_shadow.push_front(1);
            i_target.compare();
        }));

        // push_front(1.0)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            i_target.m_array.push_front(1.0);
            i_target.m_shadow.push_front(1.0);
            i_target.compare();
        }));

        // push_front('c')
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            i_target.m_array.push_front('c');
            i_target.m_shadow.push_front('c');
            i_target.compare();
        }));

        // push_front(ElementType(seed) as rvalue), sizeof(ElementType) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::Supported, 3, 1>;
            i_target.m_array.push_front(ElementType(seed));
            i_target.m_shadow.push_front(ElementType(seed));
            i_target.compare();
        }));

        // push_front(CopyableTestClass(seed) as rvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            i_target.m_array.push_front(CopyableTestClass(seed));
            i_target.m_shadow.push_front(CopyableTestClass(seed));
            i_target.compare();
        }));

        // push_front(TestClass(seed) as lvalue), sizeof(TestClass) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::Supported, 3, 1>;
            const ElementType element(seed);
            i_target.m_array.push_front(element);
            i_target.m_shadow.push_front(element);
            i_target.compare();
        }));

        // push_front(CopyableTestClass(seed) as lvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            const CopyableTestClass element(seed);
            i_target.m_array.push_front(element);
            i_target.m_shadow.push_front(element);
            i_target.compare();
        }));

                    /*---- insert ----*/

        // insert(1)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), 1);
            i_target.m_shadow.insert_at(at_index, 1);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, 1);
            i_target.m_shadow.insert_at(at_index_c, 1, count);
            i_target.compare();
        }));

        // insert(1.0)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), 1.0);
            i_target.m_shadow.insert_at(at_index, 1.0);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, 1.0);
            i_target.m_shadow.insert_at(at_index_c, 1.0, count);
            i_target.compare();
        }));

        // insert('c')
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), 'c');
            i_target.m_shadow.insert_at(at_index, 'c');
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, 'c');
            i_target.m_shadow.insert_at(at_index_c, 'c', count);
            i_target.compare();
        }));

        // insert(ElementType(seed) as rvalue), sizeof(ElementType) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::Supported, 3, 1>;
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), ElementType(seed));
            i_target.m_shadow.insert_at(at_index, ElementType(seed));
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, ElementType(seed));
            i_target.m_shadow.insert_at(at_index_c, ElementType(seed), count);
            i_target.compare();
        }));

        // insert(CopyableTestClass(seed) as rvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), CopyableTestClass(seed));
            i_target.m_shadow.insert_at(at_index, CopyableTestClass(seed));
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, CopyableTestClass(seed));
            i_target.m_shadow.insert_at(at_index_c, CopyableTestClass(seed), count);
            i_target.compare();
        }));

        // insert(TestClass(seed) as lvalue), sizeof(TestClass) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::Supported, 3, 1>;
            const ElementType element(seed);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), element);
            i_target.m_shadow.insert_at(at_index, element);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, element);
            i_target.m_shadow.insert_at(at_index_c, element, count);
            i_target.compare();
        }));

        // insert(CopyableTestClass(seed) as lvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            const CopyableTestClass element(seed);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), element);
            i_target.m_shadow.insert_at(at_index, element);
            i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, element);
            i_target.m_shadow.insert_at(at_index_c, element, count);
            i_target.compare();
        }));

                    /*---- erase ----*/

        // erase(at)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            if (!i_target.m_shadow.empty())
            {
                const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size() - 1)(i_random);
                i_target.m_array.erase(std::next(i_target.m_array.begin(), at_index));
                i_target.m_shadow.erase_at(at_index);
            }
        }));

        // erase(at, n)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            if (!i_target.m_shadow.empty())
            {
                const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size() - 1)(i_random);
                const auto count = std::uniform_int_distribution<size_t>(at_index, i_target.m_shadow.size())(i_random) - at_index;
                i_target.m_array.erase(std::next(i_target.m_array.begin(), at_index),
                    std::next(i_target.m_array.begin(), (at_index + count)) );
                i_target.m_shadow.erase_at(at_index, count);
            }
        }));
    }

    template <typename TYPE>
        void add_typed_heterogeneous_array_cases(TestTree & i_dest)
    {
        using TestTarget = HeterogeneousArrayTest<TYPE>;
        using TestFunc = std::function< void(std::mt19937 & i_random, TestTarget & i_target)>;

                            /*---- push_back ----*/

        // push_back(1)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            i_target.m_array.push_back(TYPE(1));
            i_target.m_shadow.push_back(TYPE(1));
            i_target.compare();
        }));
    }

    void add_heterogeneous_array_cases(TestTree & i_dest)
    {
        auto & void_test = i_dest["void"];
        add_common_heterogeneous_array_cases<void>(void_test);
        add_void_heterogeneous_array_cases(void_test);

        using BaseElement = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept,
            sizeof(std::max_align_t) * 2, alignof(std::max_align_t), Polymorphic::Yes >;
        auto & typed_test = i_dest["typed"];
        add_common_heterogeneous_array_cases<BaseElement>(typed_test);
        add_typed_heterogeneous_array_cases<BaseElement>(typed_test);
    }

} // namespace density_tests
