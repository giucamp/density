
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../../density/heterogeneous_array.h"
#include "testity/testity_common.h"
#include "testity/test_tree.h"
#include "testity/test_classes.h"
#include "shadow_container.h"
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
            			
			try
			{
				tmp_array = i_target.m_array;
				TESTITY_ASSERT(tmp_array == i_target.m_array);
			}
			catch (...)
			{
				TESTITY_ASSERT(tmp_array.empty());
				throw;
			}

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
			try
			{
				i_target.m_array.push_back(1);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(1);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(1.0)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
			try
			{
				i_target.m_array.push_back(1.0);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(1.0);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back('c')
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
			try
			{
				i_target.m_array.push_back('c');
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back('c');
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(ElementType(seed) as rvalue), sizeof(ElementType) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept, 3, 1>;
			try
			{
				i_target.m_array.push_back(ElementType(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(ElementType(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(CopyableTestClass(seed) as rvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
			try
			{
				i_target.m_array.push_back(CopyableTestClass(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(CopyableTestClass(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(TestClass(seed) as lvalue), sizeof(TestClass) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept, 3, 1>;
            const ElementType element(seed);
			try
			{
				i_target.m_array.push_back(element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(CopyableTestClass(seed) as lvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            const CopyableTestClass element(seed);
			try
			{
				i_target.m_array.push_back(element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

                    /*---- push_front ----*/

        // push_front(1)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
			try
			{
				i_target.m_array.push_front(1);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(1);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(1.0)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
			try
			{
				i_target.m_array.push_front(1.0);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(1.0);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front('c')
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
			try
			{
				i_target.m_array.push_front('c');
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front('c');
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(ElementType(seed) as rvalue), sizeof(ElementType) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept, 3, 1>;
			try
			{
				i_target.m_array.push_front(ElementType(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(ElementType(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(CopyableTestClass(seed) as rvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
			try
			{
				i_target.m_array.push_front(CopyableTestClass(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(CopyableTestClass(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(TestClass(seed) as lvalue), sizeof(TestClass) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept, 3, 1>;
            const ElementType element(seed);
			try
			{
				i_target.m_array.push_front(element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(CopyableTestClass(seed) as lvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            const CopyableTestClass element(seed);
			try
			{
				i_target.m_array.push_front(element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

                    /*---- insert ----*/

        // insert(1)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), 1);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index, 1);
			
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, 1);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, 1, count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(1.0)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), 1.0);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}			
			i_target.m_shadow.insert_at(at_index, 1.0);
			
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, 1.0);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, 1.0, count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert('c')
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), 'c');
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index, 'c');
			
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, 'c');
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, 'c', count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(ElementType(seed) as rvalue), sizeof(ElementType) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept, 3, 1>;
            
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), ElementType(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}

			i_target.m_shadow.insert_at(at_index, ElementType(seed));
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, ElementType(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}

			i_target.m_shadow.insert_at(at_index_c, ElementType(seed), count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(CopyableTestClass(seed) as rvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), CopyableTestClass(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index, CopyableTestClass(seed));
			
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, CopyableTestClass(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, CopyableTestClass(seed), count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(TestClass(seed) as lvalue), sizeof(TestClass) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept, 3, 1>;
            const ElementType element(seed);
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}

			i_target.m_shadow.insert_at(at_index, element);
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, element, count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(CopyableTestClass(seed) as lvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            const CopyableTestClass element(seed);
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index, element);
			
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, element, count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

                    /*---- erase ----*/

        // erase(at)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            if (!i_target.m_shadow.empty())
            {
                const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size() - 1)(i_random);
				try
				{
					i_target.m_array.erase(std::next(i_target.m_array.begin(), at_index));
				}
				catch (...)
				{
					i_target.m_shadow.check_equal(i_target.m_array);
					throw;
				}
				i_target.m_shadow.erase_at(at_index);
            }
        }));

        // erase(at, n)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            if (!i_target.m_shadow.empty())
            {
                const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size() - 1)(i_random);
                const auto count = std::uniform_int_distribution<size_t>(at_index, i_target.m_shadow.size())(i_random) - at_index;
				try
				{
					i_target.m_array.erase(std::next(i_target.m_array.begin(), at_index),
						std::next(i_target.m_array.begin(), (at_index + count)));
				}
				catch (...)
				{
					i_target.m_shadow.check_equal(i_target.m_array);
					throw;
				}
				i_target.m_shadow.erase_at(at_index, count);
            }
        }));
    }

    template <typename BASE_TYPE, typename MI_TYPE, typename VMI_TYPE>
        void add_typed_heterogeneous_array_cases(TestTree & i_dest)
    {
        using TestTarget = HeterogeneousArrayTest<BASE_TYPE>;
        using TestFunc = std::function< void(std::mt19937 & i_random, TestTarget & i_target)>;

        static_assert(std::is_convertible<MI_TYPE*, BASE_TYPE*>::value,
            "MI_TYPE must be covariant to BASE_TYPE");
        static_assert(std::is_convertible<VMI_TYPE*, BASE_TYPE*>::value,
            "VMI_TYPE must be covariant to BASE_TYPE");

                    /*---- push_back ----*/

        // push_back(BASE_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
			try
			{
				i_target.m_array.push_back(BASE_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(BASE_TYPE(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(MI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
			try
			{
				i_target.m_array.push_back(MI_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(MI_TYPE(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(VMI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
			try
			{
				i_target.m_array.push_back(VMI_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(VMI_TYPE(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(BASE_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const BASE_TYPE element(seed);
			try
			{
				i_target.m_array.push_back(element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
            i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(MI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const MI_TYPE element(seed);
			try
			{
				i_target.m_array.push_back(element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_back(VMI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const VMI_TYPE element(seed);
			try
			{
				i_target.m_array.push_back(element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
            i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

                    /*---- push_front ----*/

        // push_front(BASE_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
			try
			{
				i_target.m_array.push_front(BASE_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(BASE_TYPE(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(MI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
			try
			{
				i_target.m_array.push_front(MI_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(MI_TYPE(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(VMI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
			try
			{
				i_target.m_array.push_front(VMI_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(VMI_TYPE(seed));
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(BASE_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const BASE_TYPE element(seed);
			try
			{
				i_target.m_array.push_front(element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.push_front(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(MI_TYPE) as rvalue
		i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
			auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
			const MI_TYPE element(seed);
			try
			{
				i_target.m_array.push_front(element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
            i_target.m_shadow.push_front(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // push_front(VMI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const VMI_TYPE element(seed);
			try
			{
				i_target.m_array.push_front(element);
			}            
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
            i_target.m_shadow.push_front(element);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

                    /*---- insert ----*/

        // insert(BASE_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
			
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), BASE_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index, BASE_TYPE(seed));
			
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, BASE_TYPE(seed));	
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
            i_target.m_shadow.insert_at(at_index_c, BASE_TYPE(seed), count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(MI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);

			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), MI_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
            i_target.m_shadow.insert_at(at_index, MI_TYPE(seed));
			
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, MI_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, MI_TYPE(seed), count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(VMI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);

			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), VMI_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index, VMI_TYPE(seed));
            
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, VMI_TYPE(seed));
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, VMI_TYPE(seed), count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(BASE_TYPE) as lvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const BASE_TYPE element(seed);
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index, element);
            
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, element, count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(MI_TYPE) as lvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const MI_TYPE element(seed);

			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index, element);
            
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, element, count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

        // insert(VMI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            // test an insert of a single element and an insert of multiple elements
            const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto at_index_c = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size())(i_random);
            const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const VMI_TYPE element(seed);
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index), element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index, element);
            
			try
			{
				i_target.m_array.insert(std::next(i_target.m_array.begin(), at_index_c), count, element);
			}
			catch (...)
			{
				i_target.m_shadow.check_equal(i_target.m_array);
				throw;
			}
			i_target.m_shadow.insert_at(at_index_c, element, count);
            i_target.m_shadow.check_equal(i_target.m_array);
        }));

                    /*---- erase ----*/

        // erase(at)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            if (!i_target.m_shadow.empty())
            {
                const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size() - 1)(i_random);
				try
				{
					i_target.m_array.erase(std::next(i_target.m_array.begin(), at_index));
				}
				catch (...)
				{
					i_target.m_shadow.check_equal(i_target.m_array);
					throw;
				}
				i_target.m_shadow.erase_at(at_index);
            }
        }));

        // erase(at, n)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            if (!i_target.m_shadow.empty())
            {
                const auto at_index = std::uniform_int_distribution<size_t>(0, i_target.m_shadow.size() - 1)(i_random);
                const auto count = std::uniform_int_distribution<size_t>(at_index, i_target.m_shadow.size())(i_random) - at_index;
				try
				{
					i_target.m_array.erase(std::next(i_target.m_array.begin(), at_index),
						std::next(i_target.m_array.begin(), (at_index + count)));
				}
				catch (...)
				{
					i_target.m_shadow.check_equal(i_target.m_array);
					throw;
				}
				i_target.m_shadow.erase_at(at_index, count);
            }
        }));

    }

    void add_heterogeneous_array_cases(TestTree & i_dest)
    {
        auto & void_test = i_dest["void"];
        add_common_heterogeneous_array_cases<void>(void_test);
        add_void_heterogeneous_array_cases(void_test);

        using BaseElement = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept,
            sizeof(std::max_align_t) * 2, alignof(std::max_align_t), Polymorphic::Yes >;

        using MI_Element = MultipleInheriTestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept,
            sizeof(std::max_align_t) * 2, alignof(std::max_align_t) >;

        using MVI_Element = MultipleVirtualInheriTestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept,
            sizeof(std::max_align_t) * 2, alignof(std::max_align_t) >;

		static_assert(std::is_nothrow_move_constructible<BaseElement>::value, "BaseElement");
		static_assert(std::is_nothrow_move_constructible<MI_Element>::value, "MI_Element");
		static_assert(std::is_nothrow_move_constructible<MVI_Element>::value, "MVI_Element");

        auto & typed_test = i_dest["typed"];
        add_common_heterogeneous_array_cases<BaseElement>(typed_test);
        add_typed_heterogeneous_array_cases<BaseElement, MI_Element, MVI_Element>(typed_test);
    }

} // namespace density_tests
