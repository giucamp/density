
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(_MSC_VER) // see http://stackoverflow.com/questions/6880045/how-can-i-work-around-visual-c-2005s-decorated-name-length-exceeded-name-wa
    #pragma warning(disable:4503) // '__LINE__Var': decorated name length exceeded, name was truncated
#endif

#include "../density/small_heterogeneous_queue.h"
#include "../density/heterogeneous_queue.h"
#include "../testity/testing_utils.h"
#include "container_test.h"
#include <algorithm>

namespace density
{
    namespace tests
    {
        /* TestDenseQueue<TYPE> - small_heterogeneous_queue that uses TestAllocator and adds hash to the automatic runtime type */
        template <typename TYPE>
            using TestDenseQueue = small_heterogeneous_queue<TYPE, TestAllocator<TYPE>, runtime_type<TYPE,
                typename type_features::feature_concat< typename type_features::default_type_features_t<TYPE>, type_features::hash >::type> >;

        /* TestPagedQueue<TYPE> - heterogeneous_queue that uses TestAllocator and adds hash to the automatic runtime type */
        template <typename TYPE>
            using TestPagedQueue = heterogeneous_queue<TYPE, page_allocator, runtime_type<TYPE,
                typename type_features::feature_concat< typename type_features::default_type_features_t<TYPE>, type_features::hash >::type> >;

        template <typename COMPLETE_ELEMENT, typename DENSE_CONTAINER, typename... CONSTRUCTION_PARAMS>
            void add_test_case_push_by_copy_n_times(
                ContainerTest<DENSE_CONTAINER> & i_test,
                double i_probability,
                CONSTRUCTION_PARAMS && ... i_construction_parameters )
        {
            i_test.add_test_case("push_n_times", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
                const auto times = std::uniform_int_distribution<unsigned>(0, 9)(i_random);
                for (unsigned i = 0; i < times; i++)
                {
                    const COMPLETE_ELEMENT new_element(i_construction_parameters...);
                    i_test.dense_container().push(new_element);
                    i_test.shadow_container().push_back(new_element);
                }
            }, i_probability);
        }

        template <typename DENSE_CONTAINER>
            void add_test_case_pop_n_times(
                ContainerTest<DENSE_CONTAINER> & i_test,
                double i_probability )
        {
            i_test.add_test_case("pop_n_times", [&i_test](std::mt19937 & i_random) {
                const auto times = std::uniform_int_distribution<unsigned>(0, 7)(i_random);
                for (unsigned i = 0; i < times && !i_test.dense_container().empty(); i++)
                {
                    auto first = i_test.dense_container().begin();
                    i_test.shadow_container().compare_front(first.complete_type(), first.element());
                    i_test.shadow_container().pop_front();
                    i_test.dense_container().pop();
                }
            }, i_probability);
        }

        template <typename DENSE_CONTAINER>
            void add_test_case_consume_until_empty(
                ContainerTest<DENSE_CONTAINER> & i_test,
                double i_probability )
        {
            i_test.add_test_case("consume_until_empty", [&i_test](std::mt19937 & /*i_random*/) {
                while(!i_test.dense_container().empty())
                {
                    TESTITY_ASSERT(!i_test.shadow_container().empty());
                    i_test.dense_container().manual_consume(
                        [](const typename DENSE_CONTAINER::runtime_type & i_type, typename DENSE_CONTAINER::value_type * i_element )
                    {
                        i_type.template get_feature<type_features::hash>()(i_element);
                        i_type.destroy(i_element);
                    } );
                    i_test.shadow_container().pop_front();
                }
                TESTITY_ASSERT(i_test.shadow_container().empty());
            }, i_probability);
        }

        template <typename CONTAINER_TEST>
            void set_queue_custom_check(CONTAINER_TEST & i_container_test)
        {
            i_container_test.set_custom_check([&i_container_test] {
                const bool mem_size_is_zero = i_container_test.dense_container().mem_size() == 0;
                TESTITY_ASSERT(i_container_test.dense_container().empty() == mem_size_is_zero); });
        }

        template <template <class> class QUEUE>
            void queue_test_impl(std::mt19937 & i_random, const char * i_container_name)
        {
            NoLeakScope no_leak_scope;

            {
                ContainerTest<QUEUE<TestObjectBase>> test(i_container_name);
                set_queue_custom_check(test);
                add_test_case_push_by_copy_n_times<CopyableTestObject>(test, 1., i_random);
                add_test_case_pop_n_times(test, 1.);
                add_test_case_consume_until_empty(test, .01);
                add_test_case_copy_and_assign(test, .1);
                test.run(i_random);
            }

            {
                ContainerTest<QUEUE<CopyableTestObject>> test(i_container_name);
                set_queue_custom_check(test);
                add_test_case_push_by_copy_n_times<CopyableTestObject>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<ComplexTypeBase>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<ComplexType_A>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<ComplexType_B>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<ComplexType_C>(test, 1., i_random);
                add_test_case_pop_n_times(test, 1.);
                add_test_case_consume_until_empty(test, .01);
                add_test_case_copy_and_assign(test, .1);
                test.run(i_random);
            }

            {
                ContainerTest<QUEUE<ComplexTypeBase>> test(i_container_name);
                set_queue_custom_check(test);
                add_test_case_push_by_copy_n_times<ComplexTypeBase>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<ComplexType_A>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<ComplexType_B>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<ComplexType_C>(test, 1., i_random);
                add_test_case_pop_n_times(test, 1.);
                add_test_case_consume_until_empty(test, .01);
                add_test_case_copy_and_assign(test, .1);
                test.run(i_random);
            }

            {
                ContainerTest<QUEUE<ComplexType_A>> test(i_container_name);
                set_queue_custom_check(test);
                add_test_case_push_by_copy_n_times<ComplexType_A>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<ComplexType_C>(test, 1., i_random);
                add_test_case_pop_n_times(test, 1.);
                add_test_case_consume_until_empty(test, .01);
                add_test_case_copy_and_assign(test, .1);
                test.run(i_random);
            }

            {
                ContainerTest<QUEUE<void>> test(i_container_name);
                set_queue_custom_check(test);
                add_test_case_push_by_copy_n_times<CopyableTestObject>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<int>(test, 1., 42);
                add_test_case_push_by_copy_n_times<double>(test, 1., 42.);
                add_test_case_push_by_copy_n_times<AlignedRandomStorage<32, 32> >(test, 1., i_random);
                add_test_case_pop_n_times(test, 1.);
                add_test_case_consume_until_empty(test, .01);
                add_test_case_copy_and_assign(test, .1);
                test.run(i_random);
            }
        }

        void dense_queue_leak_basic_tests()
        {
            NoLeakScope no_leaks;
            using Queue = small_heterogeneous_queue<int, TestAllocator<int>>;
            Queue queue;
            for (int i = 0; i < 1000; i++)
            {
                queue.push(i);
            }

            for (int i = 0; i < 57; i++)
            {
                queue.manual_consume([i](const Queue::runtime_type & i_type, int * i_element)
                {
                    TESTITY_ASSERT(i_type.type_info() == typeid(int) && *i_element == i);
                });
            }
        }

        void dense_queue_basic_tests()
        {
            small_heterogeneous_queue< small_heterogeneous_queue<int> > queue_of_queues;
            small_heterogeneous_queue<int> queue;
            for (int i = 0; i < 1000; i++)
            {
                queue.push(i);
            }
            for (int i = 0; i < 57; i++)
            {
                queue.manual_consume([i](const small_heterogeneous_queue<int>::runtime_type & i_type, int * i_element)
                {
                    TESTITY_ASSERT(i_type.type_info() == typeid(int) && *i_element == i );
                });
            }

            // this must use the lvalue overload, so queue must be preserved
            const auto prev_size = queue.mem_size();
            queue_of_queues.push(queue);
            TESTITY_ASSERT(queue.mem_size() == prev_size);

            // this must use the rvalue overload, so queue must be empty after the call
            queue_of_queues.push(std::move(queue));
            TESTITY_ASSERT(queue.mem_size() == 0);
            TESTITY_ASSERT(queue.empty());

            // try with a non-copyable type (std::unique_ptr)
            small_heterogeneous_queue<std::unique_ptr<int>> queue_of_uncopyable;
            queue_of_uncopyable.push(std::unique_ptr<int>(new int(10)));
            queue_of_uncopyable.emplace<std::unique_ptr<int>>(std::unique_ptr<int>(new int (10)));
            TESTITY_ASSERT(*queue_of_uncopyable.front() == 10);
            queue_of_uncopyable.pop();
            TESTITY_ASSERT(*queue_of_uncopyable.front() == 10);
            queue_of_uncopyable.pop();
            TESTITY_ASSERT(queue_of_uncopyable.empty());

            // this must fail to compile
            //auto copy = queue_of_uncopyable;
            //copy = queue_of_uncopyable;
        }

    } // namespace tests

    void dense_queue_test()
    {
        using namespace tests;

        dense_queue_leak_basic_tests();
        dense_queue_basic_tests();

        run_exception_stress_test([] {
            std::mt19937 random;
            queue_test_impl<TestDenseQueue>(random, "small_heterogeneous_queue");
        });
    }

    void paged_queue_test()
    {
        using namespace tests;

        run_exception_stress_test([] {
            std::mt19937 random;
            queue_test_impl<TestPagedQueue>(random, "heterogeneous_queue");
        });
    }

} // namespace density
