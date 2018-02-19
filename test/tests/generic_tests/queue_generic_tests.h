
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../../test_framework/density_test_common.h"
#include "../../test_framework/queue_generic_tester.h"
#include "../../test_framework/test_allocators.h"
#include "../../test_framework/test_objects.h"
#include "../../test_framework/exception_tests.h"
#include <density/heter_queue.h>
#include <density/conc_heter_queue.h>
#include <density/lf_heter_queue.h>
#include <density/sp_heter_queue.h>
#include <string>
#include <cstdint>
#include <ostream>
#include "test_settings.h"

namespace density_tests
{
    // see queue_generic_tests.cpp for the doc
    void all_queues_generic_tests(const TestSettings & i_settings,
        QueueTesterFlags i_flags, std::ostream & i_output,
        uint32_t i_random_seed);

    namespace detail
    {
        template <typename QUEUE>
            struct PutInt
        {
            using ElementType = int;

            static bool put(QUEUE & queue, EasyRandom & i_rand)
            {
                if(i_rand.get_bool())
                    queue.push(1);
                else
                    queue.reentrant_push(1);
                return true;
            }

            static typename QUEUE::template reentrant_put_transaction<void> reentrant_put(QUEUE & i_queue, EasyRandom &)
            {
                auto transaction = i_queue.start_reentrant_push(1);
                exception_checkpoint();
                return std::move(transaction);
            }

            static void consume(const typename QUEUE::consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == 1);
            }

            static void reentrant_consume(const typename QUEUE::reentrant_consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == 1);
            }
        };

        template <typename QUEUE>
            struct PutString
        {
            using ElementType = std::string;

            static bool put(QUEUE & queue, EasyRandom & i_rand)
            {
                std::string str("hello world!");
                if(i_rand.get_bool())
                    queue.push(str);
                else
                    queue.reentrant_push(str);
                return true;
            }

            static typename QUEUE::template reentrant_put_transaction<void> reentrant_put(QUEUE & i_queue, EasyRandom &)
            {
                std::string str("hello world!");
                auto transaction = i_queue.start_reentrant_push(str);
                exception_checkpoint();
                return std::move(transaction);
            }

            static void consume(const typename QUEUE::consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == "hello world!");
            }

            static void reentrant_consume(const typename QUEUE::reentrant_consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == "hello world!");
            }
        };

        template <typename QUEUE>
            struct PutUInt8
        {
            using ElementType = uint8_t;

            static bool put(QUEUE & queue, EasyRandom & i_rand)
            {
                if (i_rand.get_bool(.9))
                {
                    if(i_rand.get_bool())
                        queue.template emplace<uint8_t>(static_cast<uint8_t>(8));
                    else
                        queue.template reentrant_emplace<uint8_t>(static_cast<uint8_t>(8));
                }
                else
                {
                    ElementType val = 8;
                    auto type = QUEUE::runtime_type::template make<ElementType>();
                    switch(i_rand.get_int<unsigned>(0, 3))
                    {
                        case 0: queue.dyn_push_copy(type, &val); break;
                        case 1: queue.dyn_push_move(type, &val); break;
                        case 2: queue.reentrant_dyn_push_copy(type, &val); break;
                        case 3: queue.reentrant_dyn_push_move(type, &val); break;
                    }
                }
                return true;
            }

            static typename QUEUE::template reentrant_put_transaction<void> reentrant_put(QUEUE & i_queue, EasyRandom &)
            {
                ElementType val = 8;
                auto transaction = i_queue.start_reentrant_push(val);
                exception_checkpoint();
                return std::move(transaction);
            }

            static void consume(const typename QUEUE::consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == 8);
            }

            static void reentrant_consume(const typename QUEUE::reentrant_consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == 8);
            }
        };

        template <typename QUEUE>
            struct PutUInt16
        {
            using ElementType = uint16_t;

            static bool put(QUEUE & queue, EasyRandom &)
            {
                auto put = queue.template start_emplace<uint16_t>(static_cast<uint16_t>(15));
                put.element() += 1;
                exception_checkpoint();
                put.commit(); // commit a 16. From now on, the element can be consumed
                return true;
            }

            static typename QUEUE::template reentrant_put_transaction<void> reentrant_put(QUEUE & i_queue, EasyRandom &)
            {
                ElementType val = 16;
                auto transaction = i_queue.start_reentrant_push(val);
                exception_checkpoint();
                return std::move(transaction);
            }

            static void consume(const typename QUEUE::consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == 16);
            }

            static void reentrant_consume(const typename QUEUE::reentrant_consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == 16);
            }
        };

        inline density::progress_guarantee get_rand_progress_guarantee(EasyRandom & i_rand)
        {
            using namespace density;
            switch (i_rand.get_int(3))
            {
                default:
                    DENSITY_TEST_ASSERT(false);
                case 0:
                    return progress_blocking;
                case 1:
                    return progress_obstruction_free;
                case 2:
                    return progress_lock_free;
                case 3:
                    return progress_wait_free;
            }

        }

        template <typename QUEUE>
            struct TryPutFloat
        {
            using ElementType = float;

            static constexpr float s_value = 3.1415f;

            static bool put(QUEUE & queue, EasyRandom & i_rand)
            {
                auto source = s_value;
                auto const progress_guarantee = get_rand_progress_guarantee(i_rand);
                switch (i_rand.get_int(3))
                {
                default:
                    DENSITY_TEST_ASSERT(false);
                case 0:
                    return queue.try_push(progress_guarantee, source);
                case 1:
                    return queue.template try_emplace<float>(progress_guarantee, source);
                case 2:
                    return queue.try_dyn_push_copy(progress_guarantee,
                        QUEUE::runtime_type::template make<float>(), &source);
                case 3:
                    return queue.try_dyn_push_move(progress_guarantee,
                        QUEUE::runtime_type::template make<float>(), &source);

                /*case 2:
                {
                    auto put = queue.template try_start_dyn_push(progress_guarantee,
                        QUEUE::runtime_type::template make<float>());
                    if (put)
                    {
                        *static_cast<float*>(put.element_ptr()) += source;
                        put.commit();
                    }
                    return static_cast<bool>(put);
                }*/
                }
            }

            static typename QUEUE::template reentrant_put_transaction<void> reentrant_put(QUEUE & queue, EasyRandom & i_rand)
            {
                auto source = s_value;
                auto const progress_guarantee = get_rand_progress_guarantee(i_rand);
                switch (i_rand.get_int(3))
                {
                default:
                    DENSITY_TEST_ASSERT(false);
                case 0:
                    return queue.try_start_reentrant_push(progress_guarantee, source);
                case 1:
                    return queue.template try_start_reentrant_emplace<float>(progress_guarantee, source);
                case 2:
                    return queue.try_start_reentrant_dyn_push_copy(progress_guarantee,
                        QUEUE::runtime_type::template make<float>(), &source);
                case 3:
                    return queue.try_start_reentrant_dyn_push_move(progress_guarantee,
                        QUEUE::runtime_type::template make<float>(), &source);

                /*case 2:
                {
                    auto put = queue.template try_start_reentrant_dyn_push(progress_guarantee,
                        QUEUE::runtime_type::template make<float>());
                    if (put)
                    {
                        *static_cast<float*>(put.element_ptr()) += source;
                    }
                    return static_cast<bool>(put);
                }*/
                }
            }

            static void consume(const typename QUEUE::consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == s_value);
            }

            static void reentrant_consume(const typename QUEUE::reentrant_consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == s_value);
            }
        };

        template <typename QUEUE, size_t SIZE, size_t ALIGNMENT>
            struct PutTestObject
        {
            using ElementType = TestObject<SIZE, ALIGNMENT>;


            static bool put(QUEUE & queue, EasyRandom & i_rand)
            {
                if (i_rand.get_bool(.9))
                {
                    queue.push(ElementType());
                }
                else
                {
                    auto type = QUEUE::runtime_type::template make<ElementType>();
                    ElementType source;
                    queue.dyn_push_copy(type, &source);
                }
                return true;
            }

            static typename QUEUE::template reentrant_put_transaction<void> reentrant_put(QUEUE & i_queue, EasyRandom &)
            {
                auto transaction = i_queue.start_reentrant_push(ElementType());
                exception_checkpoint();
                return std::move(transaction);
            }

            static void consume(const typename QUEUE::consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                i_consume.template element<ElementType>().check();
            }

            static void reentrant_consume(const typename QUEUE::reentrant_consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                i_consume.template element<ElementType>().check();
            }
        };

        template <typename QUEUE>
            struct PutRawBlocks
        {
            struct Data : InstanceCounted
            {
                std::vector<char*> m_blocks;
            };

            using ElementType = Data;

            static bool put(QUEUE & queue, EasyRandom & i_rand)
            {
                auto put = queue.template start_emplace<ElementType>();
                put_impl(put, i_rand);
                put.commit();
                return true;
            }

            static typename QUEUE::template reentrant_put_transaction<> reentrant_put(QUEUE & i_queue, EasyRandom & i_rand)
            {
                auto put = i_queue.template start_reentrant_emplace<ElementType>();
                put_impl(put, i_rand);
                return std::move(put);
            }

            static void consume(const typename QUEUE::consume_operation & i_consume)
            {
                consume_impl(i_consume);
            }

            static void reentrant_consume(const typename QUEUE::reentrant_consume_operation & i_consume)
            {
                consume_impl(i_consume);
            }

        private:

            template <typename PUT_TRANSACTION>
                static void put_impl(PUT_TRANSACTION & i_transaction, EasyRandom & i_rand)
            {
                size_t count = i_rand.get_int<size_t>(0, 200);
                for (size_t index = 0; index < count; index++)
                {
                    auto const size = count - index;
                    auto const fill_char = static_cast<char>('0' + size % 10);
                    auto const chars = static_cast<char*>(i_transaction.raw_allocate(size + 1, 1));
                    memset(chars, fill_char, size);
                    chars[size] = 0;
                    i_transaction.element().m_blocks.push_back(chars);

                    if (i_rand.get_bool(.05))
                    {
                        exception_checkpoint();
                    }
                }
                exception_checkpoint();
            }

            template <typename CONSUME_OPERATION>
                static void consume_impl(const CONSUME_OPERATION & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());

                auto & data = i_consume.template element<ElementType>();
                auto const count = data.m_blocks.size();

                exception_checkpoint();

                for (size_t index = 0; index < count; index++)
                {
                    auto const size = count - index;
                    auto const fill_char = static_cast<char>('0' + size % 10);
                    auto const chars = data.m_blocks[index];
                    for (size_t i = 0; i < size; i++)
                    {
                        DENSITY_TEST_ASSERT(chars[i] == fill_char);
                    }
                    DENSITY_TEST_ASSERT(chars[size] == 0);
                }
            }
        };

        template <typename QUEUE>
            struct ReentrantPush
        {
            using ElementType = uint32_t;

            static bool put(QUEUE & queue, EasyRandom & i_rand)
            {
                uint32_t val = 32;
                if(i_rand.get_bool())
                    queue.push(val);
                else
                    queue.reentrant_push(val);
                return true;
            }

            static typename QUEUE::template reentrant_put_transaction<> reentrant_put(QUEUE & i_queue, EasyRandom &)
            {
                uint32_t val = 32;
                auto transaction = i_queue.start_reentrant_push(val);
                exception_checkpoint();
                return std::move(transaction);
            }

            static void consume(const typename QUEUE::consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == 32);
            }

            static void reentrant_consume(const typename QUEUE::reentrant_consume_operation & i_consume)
            {
                DENSITY_TEST_ASSERT(i_consume.complete_type().template is<ElementType>());
                DENSITY_TEST_ASSERT(i_consume.template element<ElementType>() == 32);
            }
        };

        template <typename QUEUE>
            void single_queue_generic_test(QueueTesterFlags i_flags, std::ostream & i_output, EasyRandom & i_random, size_t i_element_count,
                std::vector<size_t> i_thread_count_vector)
        {
            for (auto thread_count : i_thread_count_vector)
            {
                QueueGenericTester<QUEUE> tester(i_output, thread_count);
                tester.template add_test_case<PutInt<QUEUE>>();
                tester.template add_test_case<PutUInt8<QUEUE>>();
                tester.template add_test_case<PutUInt16<QUEUE>>();
                tester.template add_test_case<PutString<QUEUE>>();
                tester.template add_test_case<PutTestObject<QUEUE, 128, 8>>();
                tester.template add_test_case<PutTestObject<QUEUE, 256, 128>>();
                tester.template add_test_case<PutTestObject<QUEUE, 2048, 2048>>();
                tester.template add_test_case<PutRawBlocks<QUEUE>>();
                tester.template add_test_case<ReentrantPush<QUEUE>>();

                tester.run(i_flags, i_random, i_element_count);
            }
        }

        template <typename QUEUE>
            void single_lf_queue_generic_test(QueueTesterFlags i_flags, std::ostream & i_output, EasyRandom & i_random, size_t i_element_count,
                std::vector<size_t> i_thread_count_vector)
        {
            for (auto thread_count : i_thread_count_vector)
            {
                QueueGenericTester<QUEUE> tester(i_output, thread_count);
                tester.template add_test_case<PutInt<QUEUE>>();
                tester.template add_test_case<PutUInt8<QUEUE>>();
                tester.template add_test_case<PutUInt16<QUEUE>>();
                tester.template add_test_case<PutString<QUEUE>>();
                tester.template add_test_case<PutTestObject<QUEUE, 128, 8>>();
                tester.template add_test_case<PutTestObject<QUEUE, 256, 128>>();
                tester.template add_test_case<PutTestObject<QUEUE, 2048, 2048>>();
                tester.template add_test_case<PutRawBlocks<QUEUE>>();
                tester.template add_test_case<TryPutFloat<QUEUE>>();
                tester.template add_test_case<ReentrantPush<QUEUE>>();

                tester.run(i_flags, i_random, i_element_count);
            }
        }

        template <density::concurrency_cardinality PROD_CARDINALITY = density::concurrency_multiple,
                density::concurrency_cardinality CONSUMER_CARDINALITY = density::concurrency_multiple,
                density::consistency_model CONSISTENCY_MODEL = density::consistency_sequential>
                void lf_queues_generic_tests(const TestSettings & i_settings, QueueTesterFlags i_flags, std::ostream & i_output,
                    EasyRandom & i_random, std::vector<size_t> const & i_nonblocking_thread_counts)
        {
            using namespace density;

            {
                // test conversion from proress_guarantee to LfQueue_ProgressGuarantee
                using namespace density::detail;
                static_assert(ToLfGuarantee(progress_blocking, true) == LfQueue_Throwing, "");
                static_assert(ToLfGuarantee(progress_blocking, false) == LfQueue_Blocking, "");
                static_assert(ToLfGuarantee(progress_obstruction_free, false) == LfQueue_LockFree, "");
                static_assert(ToLfGuarantee(progress_lock_free, false) == LfQueue_LockFree, "");
                static_assert(ToLfGuarantee(progress_wait_free, false) == LfQueue_WaitFree, "");

                // test conversion from LfQueue_ProgressGuarantee to proress_guarantee
                static_assert(ToDenGuarantee(LfQueue_Throwing) == progress_blocking, "");
                static_assert(ToDenGuarantee(LfQueue_Blocking) == progress_blocking, "");
                static_assert(ToDenGuarantee(LfQueue_LockFree) == progress_lock_free, "");
                static_assert(ToDenGuarantee(LfQueue_WaitFree) == progress_wait_free, "");
            }

            bool const run_page_default = i_settings.should_run("page_def");
            bool const run_page_256 = i_settings.should_run("page_256");

            if (i_flags && QueueTesterFlags::eUseTestAllocators)
            {
                if(run_page_default)
                {
                    single_lf_queue_generic_test<lf_heter_queue<void, runtime_type<>, UnmovableFastTestAllocator<>,
                        PROD_CARDINALITY, CONSUMER_CARDINALITY, CONSISTENCY_MODEL>>(
                        i_flags, i_output, i_random, i_settings.m_queue_tests_cardinality, i_nonblocking_thread_counts);

                    single_lf_queue_generic_test<lf_heter_queue<void, TestRuntimeTime<>, DeepTestAllocator<>,
                        PROD_CARDINALITY, CONSUMER_CARDINALITY, CONSISTENCY_MODEL>>(
                        i_flags, i_output, i_random, i_settings.m_queue_tests_cardinality, i_nonblocking_thread_counts);
                }
                if(run_page_256)
                {
                    single_lf_queue_generic_test<lf_heter_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>,
                        PROD_CARDINALITY, CONSUMER_CARDINALITY, CONSISTENCY_MODEL>>(
                        i_flags, i_output, i_random, i_settings.m_queue_tests_cardinality, i_nonblocking_thread_counts);

                    single_lf_queue_generic_test<lf_heter_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>,
                        PROD_CARDINALITY, CONSUMER_CARDINALITY, CONSISTENCY_MODEL>>(
                        i_flags, i_output, i_random, i_settings.m_queue_tests_cardinality, i_nonblocking_thread_counts);
                }
            }
            else
            {
                if(run_page_default)
                {
                    single_lf_queue_generic_test<lf_heter_queue<void, runtime_type<>, default_allocator,
                            PROD_CARDINALITY, CONSUMER_CARDINALITY, CONSISTENCY_MODEL>>(
                        i_flags, i_output, i_random, i_settings.m_queue_tests_cardinality, i_nonblocking_thread_counts);
                }
            }
        }

        template <density::concurrency_cardinality PROD_CARDINALITY = density::concurrency_multiple,
                density::concurrency_cardinality CONSUMER_CARDINALITY = density::concurrency_multiple>
                void sp_queues_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
                    EasyRandom & i_random, size_t i_element_count, std::vector<size_t> const & i_nonblocking_thread_counts)
        {
            using namespace density;

            if (i_flags && QueueTesterFlags::eUseTestAllocators)
            {
                single_lf_queue_generic_test<sp_heter_queue<void, runtime_type<>, UnmovableFastTestAllocator<>,
                    PROD_CARDINALITY, CONSUMER_CARDINALITY>>(
                    i_flags, i_output, i_random, i_element_count, i_nonblocking_thread_counts);

                single_lf_queue_generic_test<sp_heter_queue<void, TestRuntimeTime<>, DeepTestAllocator<>,
                    PROD_CARDINALITY, CONSUMER_CARDINALITY>>(
                    i_flags, i_output, i_random, i_element_count, i_nonblocking_thread_counts);

                single_lf_queue_generic_test<sp_heter_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>,
                    PROD_CARDINALITY, CONSUMER_CARDINALITY>>(
                    i_flags, i_output, i_random, i_element_count, i_nonblocking_thread_counts);

                single_lf_queue_generic_test<sp_heter_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>,
                    PROD_CARDINALITY, CONSUMER_CARDINALITY>>(
                    i_flags, i_output, i_random, i_element_count, i_nonblocking_thread_counts);
            }
            else
            {
                single_lf_queue_generic_test<sp_heter_queue<void, runtime_type<>, default_allocator,
                        PROD_CARDINALITY, CONSUMER_CARDINALITY>>(
                    i_flags, i_output, i_random, i_element_count, i_nonblocking_thread_counts);
            }
        }

    } // namespace detail

} // namespace density_tests
