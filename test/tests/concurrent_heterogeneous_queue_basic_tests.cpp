
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
#include <density/conc_heter_queue.h>
#include <iterator>
#include <type_traits>

namespace density_tests
{
    void conc_heterogeneous_queue_lifetime_tests()
    {
        using namespace density;

        {
            default_allocator  allocator;
            conc_heter_queue<> queue(allocator); // copy construct allocator
            queue.push(1);
            queue.push(2);

            auto other_queue(std::move(queue)); // move construct queue - the source becomes empty
            DENSITY_TEST_ASSERT(queue.empty() && !other_queue.empty());

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
            move_only_void_allocator                                   movable_alloc(5);
            conc_heter_queue<runtime_type<>, move_only_void_allocator> move_only_queue(
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

    /** Basic tests conc_heter_queue<...> with a non-polymorphic base */
    template <typename QUEUE> void conc_heterogeneous_queue_basic_void_tests()
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

    template <typename ELEMENT, typename QUEUE> void dynamic_push_3(QUEUE & i_queue)
    {
        auto const type = QUEUE::runtime_type::template make<ELEMENT>();

        i_queue.dyn_push(type);

        ELEMENT copy_source;
        i_queue.dyn_push_copy(type, &copy_source);

        ELEMENT move_source;
        i_queue.dyn_push_move(type, &move_source);
    }

    /** Basic tests for conc_heter_queue<...> */
    void conc_heterogeneous_queue_basic_tests(std::ostream & i_ostream)
    {
        PrintScopeDuration dur(i_ostream, "concurrent heterogeneous queue basic tests");

        conc_heterogeneous_queue_lifetime_tests();

        using namespace density;

        conc_heterogeneous_queue_basic_void_tests<conc_heter_queue<>>();

        conc_heterogeneous_queue_basic_void_tests<
          conc_heter_queue<runtime_type<>, UnmovableFastTestAllocator<>>>();

        conc_heterogeneous_queue_basic_void_tests<
          conc_heter_queue<TestRuntimeTime, DeepTestAllocator<>>>();
    }
} // namespace density_tests
