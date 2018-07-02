
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//

#include "../test_framework/density_test_common.h"
#include "../test_framework/progress.h"
#include "../test_framework/test_allocators.h"
#include "../test_framework/test_objects.h"
#include "complex_polymorphism.h"
#include <density/lf_heter_queue.h>
#include <iterator>
#include <type_traits>

namespace density_tests
{
    template <
      density::concurrency_cardinality PROD_CARDINALITY,
      density::concurrency_cardinality CONSUMER_CARDINALITY,
      density::consistency_model       CONSISTENCY_MODEL>
    struct NbQueueBasicTests
    {
        template <
          typename COMMON_TYPE    = void,
          typename RUNTIME_TYPE   = density::runtime_type<COMMON_TYPE>,
          typename ALLOCATOR_TYPE = density::default_allocator>
        using LfHeterQueue = density::lf_heter_queue<
          COMMON_TYPE,
          RUNTIME_TYPE,
          ALLOCATOR_TYPE,
          PROD_CARDINALITY,
          CONSUMER_CARDINALITY,
          CONSISTENCY_MODEL>;

        static void lf_heterogeneous_queue_lifetime_tests()
        {
            using namespace density;

            {
                default_allocator allocator;
                LfHeterQueue<>    queue(allocator); // copy construct allocator
                queue.push(1);
                queue.push(2);

                auto other_queue(
                  std::move(queue)); // move construct queue - the source becomes empty
                DENSITY_TEST_ASSERT(queue.empty() && !other_queue.empty());

                // test swaps
                swap(queue, other_queue);
                DENSITY_TEST_ASSERT(!queue.empty() && other_queue.empty());
                swap(queue, other_queue);
                DENSITY_TEST_ASSERT(queue.empty() && !other_queue.empty());
                auto cons = other_queue.try_start_consume();
                DENSITY_TEST_ASSERT(
                  cons && cons.complete_type().template is<int>() &&
                  cons.template element<int>() == 1);
                cons.commit();
                cons = other_queue.try_start_consume();
                DENSITY_TEST_ASSERT(
                  cons && cons.complete_type().template is<int>() &&
                  cons.template element<int>() == 2);
                cons.commit();
                DENSITY_TEST_ASSERT(other_queue.empty());

                // test allocator getters
                move_only_void_allocator                                     movable_alloc(5);
                LfHeterQueue<void, runtime_type<>, move_only_void_allocator> move_only_queue(
                  std::move(movable_alloc));

                auto allocator_copy = other_queue.get_allocator();
                (void)allocator_copy;

                move_only_queue.push(1);
                move_only_queue.push(2);

                move_only_queue.get_allocator_ref().dummy_func();

                auto const & const_move_only_queue(move_only_queue);
                const_move_only_queue.get_allocator_ref().const_dummy_func();
            }
        }

        /** Basic tests LfHeterQueue<void, ...> with a non-polymorphic base */
        template <typename QUEUE> static void lf_heterogeneous_queue_basic_void_tests()
        {
            {
                QUEUE queue;
                DENSITY_TEST_ASSERT(queue.empty());
            }

            {
                QUEUE queue;
                queue.clear();

                queue.push(1);
                DENSITY_TEST_ASSERT(!queue.empty());

                queue.clear();
                DENSITY_TEST_ASSERT(queue.empty());
                queue.clear();
            }
        }

        template <typename ELEMENT, typename QUEUE> static void dynamic_push_3(QUEUE & i_queue)
        {
            auto const type = QUEUE::runtime_type::template make<ELEMENT>();

            i_queue.dyn_push(type);

            ELEMENT copy_source;
            i_queue.dyn_push_copy(type, &copy_source);

            ELEMENT move_source;
            i_queue.dyn_push_move(type, &move_source);
        }

        /** Test LfHeterQueue with a non-polymorphic base */
        static void lf_heterogeneous_queue_basic_nonpolymorphic_base_tests()
        {
            using namespace density;

            using RunTimeType = runtime_type<
              NonPolymorphicBase,
              feature_list<
                f_default_construct,
                f_move_construct,
                f_copy_construct,
                f_destroy,
                f_size,
                f_alignment>>;
            LfHeterQueue<NonPolymorphicBase, RunTimeType> queue;

            queue.push(NonPolymorphicBase());
            queue.template emplace<SingleDerivedNonPoly>();

            dynamic_push_3<NonPolymorphicBase>(queue);
            dynamic_push_3<SingleDerivedNonPoly>(queue);

            for (;;)
            {
                auto consume = queue.try_start_consume();
                if (!consume)
                    break;

                if (consume.complete_type().template is<NonPolymorphicBase>())
                {
                    consume.template element<NonPolymorphicBase>().check();
                }
                else
                {
                    DENSITY_TEST_ASSERT(
                      consume.complete_type().template is<SingleDerivedNonPoly>());
                    consume.template element<SingleDerivedNonPoly>().check();
                }
                consume.commit();
            }

            DENSITY_TEST_ASSERT(queue.empty());
        }

        /** Test LfHeterQueue with a polymorphic base */
        static void lf_heterogeneous_queue_basic_polymorphic_base_tests()
        {
            using namespace density;

            using RunTimeType = runtime_type<
              PolymorphicBase,
              feature_list<
                f_default_construct,
                f_move_construct,
                f_copy_construct,
                f_destroy,
                f_size,
                f_alignment>>;
            LfHeterQueue<PolymorphicBase, RunTimeType> queue;

            queue.push(PolymorphicBase());
            queue.template emplace<SingleDerived>();
            queue.template emplace<Derived1>();
            queue.template emplace<Derived2>();
            queue.template emplace<MultipleDerived>();

            dynamic_push_3<PolymorphicBase>(queue);
            dynamic_push_3<SingleDerived>(queue);
            dynamic_push_3<Derived1>(queue);
            dynamic_push_3<Derived2>(queue);
            dynamic_push_3<MultipleDerived>(queue);

            polymorphic_consume<PolymorphicBase>(queue.try_start_consume());
            polymorphic_consume<SingleDerived>(queue.try_start_reentrant_consume());
            polymorphic_consume<Derived1>(queue.try_start_consume());
            polymorphic_consume<Derived2>(queue.try_start_reentrant_consume());
            polymorphic_consume<MultipleDerived>(queue.try_start_consume());

            for (int i = 0; i < 3; i++)
                polymorphic_consume<PolymorphicBase>(queue.try_start_reentrant_consume());
            for (int i = 0; i < 3; i++)
                polymorphic_consume<SingleDerived>(queue.try_start_consume());
            for (int i = 0; i < 3; i++)
                polymorphic_consume<Derived1>(queue.try_start_reentrant_consume());
            for (int i = 0; i < 3; i++)
                polymorphic_consume<Derived2>(queue.try_start_consume());
            for (int i = 0; i < 3; i++)
                polymorphic_consume<MultipleDerived>(queue.try_start_reentrant_consume());

            DENSITY_TEST_ASSERT(queue.empty());
        }

        static void tests(std::ostream & /*i_ostream*/)
        {
            using density::runtime_type;

            lf_heterogeneous_queue_lifetime_tests();

            lf_heterogeneous_queue_basic_nonpolymorphic_base_tests();

            lf_heterogeneous_queue_basic_polymorphic_base_tests();

            lf_heterogeneous_queue_basic_void_tests<LfHeterQueue<>>();

            lf_heterogeneous_queue_basic_void_tests<
              LfHeterQueue<void, runtime_type<>, UnmovableFastTestAllocator<>>>();

            lf_heterogeneous_queue_basic_void_tests<
              LfHeterQueue<void, TestRuntimeTime, DeepTestAllocator<>>>();
        }

    }; // NbQueueBasicTests


    /** Basic tests for lf_heter_queue<...> */
    void lf_heterogeneous_queue_basic_tests(std::ostream & i_ostream)
    {
        PrintScopeDuration dur(i_ostream, "lock-free heterogeneous queue basic tests");

        constexpr auto mult    = density::concurrency_multiple;
        constexpr auto single  = density::concurrency_single;
        constexpr auto seq_cst = density::consistency_sequential;
        constexpr auto relaxed = density::consistency_relaxed;

        NbQueueBasicTests<mult, mult, seq_cst>::tests(i_ostream);
        NbQueueBasicTests<single, mult, seq_cst>::tests(i_ostream);
        NbQueueBasicTests<mult, single, seq_cst>::tests(i_ostream);
        NbQueueBasicTests<single, single, seq_cst>::tests(i_ostream);

        NbQueueBasicTests<mult, mult, relaxed>::tests(i_ostream);
        NbQueueBasicTests<single, mult, relaxed>::tests(i_ostream);
        NbQueueBasicTests<mult, single, relaxed>::tests(i_ostream);
        NbQueueBasicTests<single, single, relaxed>::tests(i_ostream);
    }
} // namespace density_tests
