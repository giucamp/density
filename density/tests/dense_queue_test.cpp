
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(_MSC_VER) // see http://stackoverflow.com/questions/6880045/how-can-i-work-around-visual-c-2005s-decorated-name-length-exceeded-name-wa
    #pragma warning(disable:4503) // '__LINE__Var': decorated name length exceeded, name was truncated
#endif

#include "..\dense_queue.h"
#include "testing_utils.h"
#include "container_test.h"
#include <deque>
#include <complex>
#include <tuple>
#include <random>
#include <memory>
#include <functional>
#include <algorithm>

namespace density
{
    namespace detail
    {
        /* TestDenseQueue - dense_queue that uses TestAlocator and adds detail::FeatureHash to the automatic runtime type */
        template <typename TYPE>
            using TestDenseQueue = dense_queue<TYPE, TestAllocator<TYPE>, runtime_type<TYPE, 
                typename detail::FeatureConcat< typename detail::AutoGetFeatures<TYPE>::type, detail::FeatureHash >::type> >;

        void dense_queue_test_impl(std::mt19937 & i_random)
        {
            NoLeakScope no_leak_scope;

            {
                ContainerTest<TestDenseQueue<TestObjectBase>> test("dense_queue");
                add_test_case_push_by_copy_n_times<CopyableTestObject>(test, 1., i_random);
                add_test_case_pop_n_times(test, 1.);
                add_test_case_consume_until_empty(test, .01);
                add_test_case_copy_and_assign(test, .1);
                test.run(i_random);
            }

            {
                ContainerTest<TestDenseQueue<CopyableTestObject>> test("dense_queue");
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
                ContainerTest<TestDenseQueue<ComplexTypeBase>> test("dense_queue");
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
                ContainerTest<TestDenseQueue<ComplexType_A>> test("dense_queue");
                add_test_case_push_by_copy_n_times<ComplexType_A>(test, 1., i_random);
                add_test_case_push_by_copy_n_times<ComplexType_C>(test, 1., i_random);
                add_test_case_pop_n_times(test, 1.);
                add_test_case_consume_until_empty(test, .01);
                add_test_case_copy_and_assign(test, .1);
                test.run(i_random);
            }

            {
                ContainerTest<TestDenseQueue<void>> test("dense_queue");
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
            using Queue = dense_queue<int, TestAllocator<int>>;
            Queue queue;
            for (int i = 0; i < 1000; i++)
            {
                queue.push(i);
            }

            for (int i = 0; i < 57; i++)
            {
                queue.consume([i](const Queue::runtime_type & i_type, int * i_element)
                {
                    DENSITY_TEST_ASSERT(i_type.type_info() == typeid(int) && *i_element == i);
                });
            }
        }

        void dense_queue_basic_tests()
        {
            dense_queue< dense_queue<int> > queue_of_queues;
            dense_queue<int> queue;
            for (int i = 0; i < 1000; i++)
            {
                queue.push(i);
            }
            for (int i = 0; i < 57; i++)
            {
                queue.consume([i](const dense_queue<int>::runtime_type & i_type, int * i_element)
                {
                    DENSITY_TEST_ASSERT(i_type.type_info() == typeid(int) && *i_element == i );
                });
            }

            // this must use the lvalue overload, so queue must be preserved
            const auto prev_size = queue.mem_size();
            queue_of_queues.push(queue);
            DENSITY_TEST_ASSERT(queue.mem_size() == prev_size);

            // this must use the rvalue overload, so queue must be empty after the call
            queue_of_queues.push(std::move(queue));
            DENSITY_TEST_ASSERT(queue.mem_size().value() == 0);
            DENSITY_TEST_ASSERT(queue.empty());

            // try with a non-copyable type (std::unique_ptr)
            dense_queue<std::unique_ptr<int>> queue_of_uncopyable;
            queue_of_uncopyable.push(std::make_unique<int>(10));
            queue_of_uncopyable.emplace<std::unique_ptr<int>>(std::make_unique<int>(10));
            DENSITY_TEST_ASSERT(*queue_of_uncopyable.front() == 10);
            queue_of_uncopyable.pop();
            DENSITY_TEST_ASSERT(*queue_of_uncopyable.front() == 10);
            queue_of_uncopyable.pop();
            DENSITY_TEST_ASSERT(queue_of_uncopyable.empty());

            // this must fail to compile
            //auto copy = queue_of_uncopyable;
            //copy = queue_of_uncopyable;
        }
    
    } // namespace detail

    void dense_queue_test()
    {
        /*function_queue<double (int a , int b)> q;

        auto lam = 1;
        static_assert( std::is_copy_constructible<decltype(lam)>::value, "");
        q.push([](int a, int b)
        {
            return (double)(a + b);
        });

        double p = q.invoke_front(4,5);*/
        
        detail::dense_queue_leak_basic_tests();
        detail::dense_queue_basic_tests();

        run_exception_stress_test([] {
            std::mt19937 random;
            detail::dense_queue_test_impl(random);
        });
    }

} // namespace density
