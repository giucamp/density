
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
#include <density/heter_queue.h>
#include <iterator>
#include <type_traits>

namespace density_tests
{
    void heterogeneous_queue_lifetime_tests()
    {
        using namespace density;

        {
            default_allocator allocator;
            heter_queue<>     queue(allocator); // copy construct allocator
            queue.push(1);
            queue.push(2);

            auto queue_copy(queue); // copy construct queue
            DENSITY_TEST_ASSERT(!queue.empty());
            DENSITY_TEST_ASSERT(!queue_copy.empty());
            DENSITY_TEST_ASSERT(std::distance(queue_copy.begin(), queue_copy.end()) == 2);

            auto other_queue(std::move(queue)); // move construct queue - the source becomes empty
            DENSITY_TEST_ASSERT(queue.empty() && !other_queue.empty());
            DENSITY_TEST_ASSERT(std::distance(other_queue.begin(), other_queue.end()) == 2);
            DENSITY_TEST_ASSERT(std::distance(queue.begin(), queue.end()) == 0);

            // test swaps
            swap(queue, other_queue);
            DENSITY_TEST_ASSERT(!queue.empty() && other_queue.empty());
            swap(queue, other_queue);
            DENSITY_TEST_ASSERT(queue.empty() && !other_queue.empty());
            auto cons = other_queue.try_start_consume();
            DENSITY_TEST_ASSERT(cons && cons.complete_type().is<int>() && cons.element<int>() == 1);
            cons.commit();
            cons = other_queue.try_start_consume();
            DENSITY_TEST_ASSERT(cons && cons.complete_type().is<int>() && cons.element<int>() == 2);
            cons.commit();
            DENSITY_TEST_ASSERT(other_queue.empty());

            // test allocator getters
            move_only_void_allocator                              movable_alloc(5);
            heter_queue<runtime_type<>, move_only_void_allocator> move_only_queue(
              std::move(movable_alloc));

            auto allocator_copy = other_queue.get_allocator();
            (void)allocator_copy;

            move_only_queue.push(1);
            move_only_queue.push(2);

            move_only_queue.get_allocator_ref().dummy_func();

            auto const & const_move_only_queue(move_only_queue);
            const_move_only_queue.get_allocator_ref().const_dummy_func();
        }

        //move_only_void_allocator
    }

    /** Basic tests heter_queue<...> with a non-polymorphic base */
    template <typename QUEUE> void heterogeneous_queue_basic_void_tests()
    {
        {
            QUEUE queue;
            DENSITY_TEST_ASSERT(queue.empty());
            DENSITY_TEST_ASSERT(queue.begin() == queue.end());
            DENSITY_TEST_ASSERT(queue.cbegin() == queue.cend());
        }

        {
            QUEUE queue;
            queue.clear();

            queue.push(1);
            DENSITY_TEST_ASSERT(!queue.empty());
            DENSITY_TEST_ASSERT(queue.begin() != queue.end());
            DENSITY_TEST_ASSERT(queue.cbegin() != queue.cend());

            queue.clear();
            DENSITY_TEST_ASSERT(queue.empty());
            DENSITY_TEST_ASSERT(queue.begin() == queue.end());
            DENSITY_TEST_ASSERT(queue.cbegin() == queue.cend());
            queue.clear();
        }
    }

    template <typename ELEMENT, typename QUEUE> void dynamic_push_3(QUEUE & i_queue)
    {
        auto const type = QUEUE::runtime_type::template make<ELEMENT>();

        i_queue.dyn_push(type);

        ELEMENT copy_source;
        i_queue.dyn_push_copy(type, &copy_source);

        ELEMENT move_source;
        i_queue.dyn_push_move(type, &move_source);
    }

    /** Test heter_queue with a non-polymorphic base */
    void heterogeneous_queue_basic_nonpolymorphic_base_tests()
    {
        using namespace density;
        using RunTimeType = runtime_type<
          f_default_construct,
          f_move_construct,
          f_copy_construct,
          f_destroy,
          f_size,
          f_alignment>;
        heter_queue<RunTimeType> queue;

        queue.push(NonPolymorphicBase());
        queue.emplace<SingleDerivedNonPoly>();

        dynamic_push_3<NonPolymorphicBase>(queue);
        dynamic_push_3<SingleDerivedNonPoly>(queue);

        for (;;)
        {
            auto consume = queue.try_start_consume();
            if (!consume)
                break;

            if (consume.complete_type().is<NonPolymorphicBase>())
            {
                consume.element<NonPolymorphicBase>().check();
            }
            else
            {
                DENSITY_TEST_ASSERT(consume.complete_type().is<SingleDerivedNonPoly>());
                consume.element<SingleDerivedNonPoly>().check();
            }
            consume.commit();
        }

        DENSITY_TEST_ASSERT(queue.empty());
    }

    /** Test heter_queue with a polymorphic base */
    void heterogeneous_queue_basic_polymorphic_base_tests()
    {
        using namespace density;
        using RunTimeType = runtime_type<
          f_default_construct,
          f_move_construct,
          f_copy_construct,
          f_destroy,
          f_size,
          f_alignment>;
        heter_queue<RunTimeType> queue;

        queue.push(PolymorphicBase());
        queue.reentrant_emplace<SingleDerived>();
        queue.emplace<Derived1>();
        queue.reentrant_emplace<Derived2>();
        queue.emplace<MultipleDerived>();

        dynamic_push_3<PolymorphicBase>(queue);
        dynamic_push_3<SingleDerived>(queue);
        dynamic_push_3<Derived1>(queue);
        dynamic_push_3<Derived2>(queue);
        dynamic_push_3<MultipleDerived>(queue);

        int const put_count = 5 * 4;

        int elements = 0;
        for (const auto & val : queue)
        {
            (void)val;
            elements++;
        }
        DENSITY_TEST_ASSERT(elements == put_count);

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

    /** Basic tests for heter_queue<...> */
    void heterogeneous_queue_basic_tests(std::ostream & i_ostream)
    {
        PrintScopeDuration dur(i_ostream, "heterogeneous queue basic tests");

        heterogeneous_queue_lifetime_tests();

        heterogeneous_queue_basic_nonpolymorphic_base_tests();

        heterogeneous_queue_basic_polymorphic_base_tests();

        using namespace density;

        heterogeneous_queue_basic_void_tests<heter_queue<>>();

        heterogeneous_queue_basic_void_tests<
          heter_queue<runtime_type<>, UnmovableFastTestAllocator<>>>();

        heterogeneous_queue_basic_void_tests<heter_queue<TestRuntimeTime<>, DeepTestAllocator<>>>();
    }
} // namespace density_tests
