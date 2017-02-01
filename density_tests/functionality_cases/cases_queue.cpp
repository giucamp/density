
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/heterogeneous_queue.h>
#include <testity/testity_common.h>
#include <testity/test_tree.h>
#include <testity/test_classes.h>
#include "shadow_container.h"
#include "test_void_allocator.h"
#include "dynamic_type.h"
#include <algorithm>
#include <array>

namespace heter_queue_samples
{
    void run();
}

namespace density_tests
{
    using namespace density;
    using namespace testity;

    template <typename QUEUE>
        void add_heterogeneous_queue_base_tests(TestTree & i_dest)
    {
        i_dest["base_tests"].add_case([](std::mt19937 & /*i_random*/) {
            heter_queue_samples::run();
        });

        i_dest["base_tests"].add_case([](std::mt19937 & /*i_random*/) {

            QUEUE queue;
            using runtime_type = typename QUEUE::runtime_type;

            TESTITY_ASSERT(!queue.start_manual_consume());

            for (int i = 0; i < 1000; i++)
                queue.push(i);

            auto it = queue.cbegin();
            for (int i = 0; i < 1000; i++)
            {
                TESTITY_ASSERT(i == *(*it).second);
                it++;
            }
            TESTITY_ASSERT(it == queue.cend());

            queue.consume([](const runtime_type & i_type, int * i_element) {
                TESTITY_ASSERT(*i_element == 0 && i_type == runtime_type::make<int>());
            });

            it = queue.cbegin();
            for (int i = 1; i < 1000; i++)
            {
                TESTITY_ASSERT(i == *(*it).second);
                it++;
            }
            TESTITY_ASSERT(it == queue.cend());

        });
    }

    template <typename QUEUE>
        struct QueueTest_DynType
    {
        QUEUE m_queue;
        std::deque<DynamicType> m_shadow;
    };

    template <typename QUEUE>
        void add_heterogeneous_queue_dynamic_type_tests(TestTree & i_dest)
    {
        using TestTarget = QueueTest_DynType<QUEUE>;
        using TestFunc = std::function< void(std::mt19937 & i_random, TestTarget & i_target)>;

        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto type = DynamicType::make_random(i_random);
            //i_target.m_queue.push_by_copy(type, );
        }));
    }

    /* HeterogeneousQueue<TYPE> - heterogeneous_queue that uses TestVoidAllocator and adds hash to the automatic runtime type */
    template <typename TYPE>
        using HeterogeneousQueue = heterogeneous_queue<TYPE, runtime_type<TYPE,
            typename type_features::feature_concat< typename type_features::default_type_features_t<TYPE>, type_features::feature_list<type_features::hash, type_features::equals> >::type>,
                TestVoidAllocator >;

    template <typename QUEUE>
        struct QueueTest
    {
        QUEUE m_queue;
        ShadowContainer<QUEUE> m_shadow;
    };

    template <typename QUEUE>
        void add_common_queue_cases(TestTree & i_dest)
    {
        using TestTarget = QueueTest<QUEUE>;
        using TestFunc = std::function< void(std::mt19937 & i_random, TestTarget & i_target)>;

        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {

            QUEUE tmp_queue;
            TESTITY_ASSERT(tmp_queue.empty());
            TESTITY_ASSERT(tmp_queue.begin() == tmp_queue.end());

            try
            {
                tmp_queue = i_target.m_queue;
                TESTITY_ASSERT(tmp_queue == i_target.m_queue);
            }
            catch (...)
            {
                TESTITY_ASSERT(tmp_queue.empty());
                throw;
            }

            QUEUE tmp_queue_1(tmp_queue);
            TESTITY_ASSERT(tmp_queue_1 == i_target.m_queue);

            QUEUE tmp_queue_2(std::move(tmp_queue));
            TESTITY_ASSERT(tmp_queue_2 == i_target.m_queue);
            TESTITY_ASSERT(tmp_queue.empty());

            tmp_queue = std::move(tmp_queue_2);
            TESTITY_ASSERT(tmp_queue == i_target.m_queue);
            TESTITY_ASSERT(tmp_queue_2.empty());

            tmp_queue.clear();
            TESTITY_ASSERT(tmp_queue.empty());
        }));
    }

    template <typename QUEUE>
        void add_void_queue_cases(TestTree & i_dest)
    {
        using TestTarget = QueueTest<QUEUE>;
        using TestFunc = std::function< void(std::mt19937 & i_random, TestTarget & i_target)>;

                    /*---- push_back ----*/

        // push(1)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            try
            {
                i_target.m_queue.push(1);
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(1);
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(1.0)
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            try
            {
                i_target.m_queue.push(1.0);
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(1.0);
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push('c')
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            try
            {
                i_target.m_queue.push('c');
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back('c');
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(ElementType(seed) as rvalue), sizeof(ElementType) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept, 3, 1>;
            try
            {
                i_target.m_queue.push(ElementType(seed));
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(ElementType(seed));
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(CopyableTestClass(seed) as rvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            try
            {
                i_target.m_queue.push(CopyableTestClass(seed));
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(CopyableTestClass(seed));
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(TestClass(seed) as lvalue), sizeof(TestClass) = 3
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            using ElementType = TestClass<FeatureKind::Supported, FeatureKind::Supported, FeatureKind::SupportedNoExcept, 3, 1>;
            const ElementType element(seed);
            try
            {
                i_target.m_queue.push(element);
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(CopyableTestClass(seed) as lvalue)
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-100, 100)(i_random);
            const CopyableTestClass element(seed);
            try
            {
                i_target.m_queue.push(element);
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

                    /*---- pop ----*/

        // pop()
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            if (!i_target.m_shadow.empty())
            {
                i_target.m_queue.pop();
                i_target.m_shadow.erase_at(0);
                i_target.m_shadow.check_equal(i_target.m_queue);
            }
        }));
    }

    template <typename QUEUE, typename MI_TYPE, typename VMI_TYPE>
        void add_typed_queue_cases(TestTree & i_dest)
    {
        using TestTarget = QueueTest<QUEUE>;
        using TestFunc = std::function< void(std::mt19937 & i_random, TestTarget & i_target)>;
        using BaseType = typename QUEUE::common_type;

        static_assert(std::is_convertible<MI_TYPE*, BaseType*>::value,
            "MI_TYPE must be covariant to BaseType");
        static_assert(std::is_convertible<VMI_TYPE*, BaseType*>::value,
            "VMI_TYPE must be covariant to BaseType");


                    /*---- push_back ----*/

        // push(BaseType) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            try
            {
                i_target.m_queue.push(BaseType(seed));
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(BaseType(seed));
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(MI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            try
            {
                i_target.m_queue.push(MI_TYPE(seed));
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(MI_TYPE(seed));
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(VMI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            try
            {
                i_target.m_queue.push(VMI_TYPE(seed));
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(VMI_TYPE(seed));
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(BaseType) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const BaseType element(seed);
            try
            {
                i_target.m_queue.push(element);
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(MI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const MI_TYPE element(seed);
            try
            {
                i_target.m_queue.push(element);
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));

        // push(VMI_TYPE) as rvalue
        i_dest.add_case(TestFunc([](std::mt19937 & i_random, TestTarget & i_target) {
            auto const seed = std::uniform_int_distribution<int>(-200, 200)(i_random);
            const VMI_TYPE element(seed);
            try
            {
                i_target.m_queue.push(element);
            }
            catch (...)
            {
                i_target.m_shadow.check_equal(i_target.m_queue);
                throw;
            }
            i_target.m_shadow.push_back(element);
            i_target.m_shadow.check_equal(i_target.m_queue);
        }));


                    /*---- pop ----*/

        // pop()
        i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
            if (!i_target.m_shadow.empty())
            {
                i_target.m_queue.pop();
                i_target.m_shadow.erase_at(0);
                i_target.m_shadow.check_equal(i_target.m_queue);
            }
        }));
    }

    void add_heterogeneous_queue_cases(TestTree & i_dest)
    {
        auto & void_test = i_dest["void"];
        add_common_queue_cases<HeterogeneousQueue<void>>(void_test);
        add_void_queue_cases<HeterogeneousQueue<void>>(void_test);

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
        add_common_queue_cases<HeterogeneousQueue<BaseElement>>(typed_test);
        add_typed_queue_cases<HeterogeneousQueue<BaseElement>, MI_Element, MVI_Element>(typed_test);
    }

    void add_queue_cases(TestTree & i_dest)
    {
        add_heterogeneous_queue_base_tests< heterogeneous_queue<int> >(i_dest);
        add_heterogeneous_queue_base_tests< heterogeneous_queue<int, runtime_type<int>, TestVoidAllocator> >(i_dest);

        add_heterogeneous_queue_cases(i_dest["heterogeneous_queue"]);
    }

} // namespace density_tests
