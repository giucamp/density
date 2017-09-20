
#include <string>
#include <iostream>
#include <iterator>
#include <complex>
#include <string>
#include <chrono>
#include <assert.h>
#include <density/sp_heter_queue.h>
#include <density/io_runtimetype_features.h>
#include "../density_tests/test_framework/progress.h"

// if assert expands to nothing, some local variable becomes unused
#if defined(_MSC_VER) && defined(NDEBUG)
	#pragma warning(push)
	#pragma warning(disable:4189) // local variable is initialized but not referenced
#endif

namespace density_tests
{
	uint32_t compute_checksum(const void * i_data, size_t i_lenght);

	template < density::concurrency_cardinality PROD_CARDINALITY,
			density::concurrency_cardinality CONSUMER_CARDINALITY>
		struct SpQueueSamples
	{	
		static void spinlocking_heterogeneous_queue_put_samples()
		{
			using namespace density;

		{

        {
			//! [sp_heter_queue push example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            SpQueue queue;
            queue.push(12);
			queue.push(std::string("Hello world!!"));
			//! [sp_heter_queue push example 1]	
        }
        {
			//! [sp_heter_queue emplace example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            SpQueue queue;
			queue.template emplace<int>();
			queue.template emplace<std::string>(12, '-');
			//! [sp_heter_queue emplace example 1]		
        }
		{
			//! [sp_heter_queue start_push example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            SpQueue queue;
			auto put = queue.start_push(12);
			put.element() += 2;
			put.commit(); // commits a 14
			//! [sp_heter_queue start_push example 1]	
		}
		{
			//! [sp_heter_queue start_emplace example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            SpQueue queue;
			auto put = queue.template start_emplace<std::string>(4, '*');
			put.element() += "****";
			put.commit(); // commits a "********"
			//! [sp_heter_queue start_emplace example 1]	
		}
		}
		{
			//! [sp_heter_queue dyn_push example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			auto const type = MyRunTimeType::template make<int>();
			queue.dyn_push(type); // appends 0
			//! [sp_heter_queue dyn_push example 1]
		}
		{
			//! [sp_heter_queue dyn_push_copy example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			std::string const source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
			queue.dyn_push_copy(type, &source);
			//! [sp_heter_queue dyn_push_copy example 1]
		}
		{
			//! [sp_heter_queue dyn_push_move example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			std::string source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
			queue.dyn_push_move(type, &source);
			//! [sp_heter_queue dyn_push_move example 1]
		}

		{
			//! [sp_heter_queue start_dyn_push example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			auto const type = MyRunTimeType::make<int>();
			auto put = queue.start_dyn_push(type);
			put.commit();
			//! [sp_heter_queue start_dyn_push example 1]
		}
		{
			//! [sp_heter_queue start_dyn_push_copy example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			std::string const source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
            auto put = queue.start_dyn_push_copy(type, &source);
            put.commit();
			//! [sp_heter_queue start_dyn_push_copy example 1]
		}
		{
			//! [sp_heter_queue start_dyn_push_move example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			std::string source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
			auto put = queue.start_dyn_push_move(type, &source);
			put.commit();
			//! [sp_heter_queue start_dyn_push_move example 1]
		}
		}

        static void spinlocking_heterogeneous_queue_try_put_samples()
		{
			using namespace density;

		{
			sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

        {
			//! [sp_heter_queue try_push example 1]
            bool successful = false;
            if (queue.try_push(progress_wait_free, 12))
            {
                successful = queue.try_push(progress_wait_free, std::string("Hello world!!"));
            }
			//! [sp_heter_queue try_push example 1]	
            (void)successful;
        }
        {
			//! [sp_heter_queue try_emplace example 1]
            bool successful = false;
			if (queue.template try_emplace<int>(progress_wait_free))
            {
                successful = queue.template try_emplace<std::string>(progress_wait_free, 12, '-');
            }
			//! [sp_heter_queue try_emplace example 1]
            (void)successful;
        }
		{
			//! [sp_heter_queue try_start_push example 1]
            if (auto put = queue.try_start_push(progress_wait_free, 12))
            {
                // ..
			    put.element() += 2;
			    put.commit(); // commits a 14
            }
			//! [sp_heter_queue try_start_push example 1]	
		}
		{
			//! [sp_heter_queue try_start_emplace example 1]
			if (auto put = queue.template try_start_emplace<std::string>(progress_wait_free, 4, '*'))
            {
                // ..
                put.element() += "****";
                put.commit(); // commits a "********"
            }
			//! [sp_heter_queue try_start_emplace example 1]	
		}
		}
		{
			//! [sp_heter_queue try_dyn_push example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			auto const type = MyRunTimeType::make<int>();
            if (queue.try_dyn_push(progress_wait_free, type)) // appends 0
            {
                // ...
            }
			//! [sp_heter_queue try_dyn_push example 1]
		}
		{
			//! [sp_heter_queue try_dyn_push_copy example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			std::string const source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
            if (queue.try_dyn_push_copy(progress_wait_free, type, &source))
            {
                // ...
            }
			//! [sp_heter_queue try_dyn_push_copy example 1]
		}
		{
			//! [sp_heter_queue try_dyn_push_move example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			std::string source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
            if (queue.try_dyn_push_move(progress_wait_free, type, &source))
            {
                // ...
            }
			//! [sp_heter_queue try_dyn_push_move example 1]
		}

		{
			//! [sp_heter_queue try_start_dyn_push example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			auto const type = MyRunTimeType::make<int>();
            if (auto put = queue.try_start_dyn_push(progress_wait_free, type))
            {
                // ....
                put.commit();
            }
			//! [sp_heter_queue try_start_dyn_push example 1]
		}
		{
			//! [sp_heter_queue try_start_dyn_push_copy example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			std::string const source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
            if (auto put = queue.try_start_dyn_push_copy(progress_wait_free, type, &source))
            {
                // ...
                put.commit();
            }            
			//! [sp_heter_queue try_start_dyn_push_copy example 1]
		}
		{
			//! [sp_heter_queue try_start_dyn_push_move example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;

			std::string source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
            if (auto put = queue.try_start_dyn_push_move(progress_wait_free, type, &source))
            {
                // ..
                put.commit();
            }
			//! [sp_heter_queue try_start_dyn_push_move example 1]
		}
		}

		static void spinlocking_heterogeneous_queue_put_transaction_samples()
		{
            using namespace density;
			using namespace type_features;
			

		{
			//! [sp_heter_queue put_transaction default_construct example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            
            typename SpQueue::template put_transaction<> transaction;
			assert(transaction.empty());
			//! [sp_heter_queue put_transaction default_construct example 1]
		}
		{
			//! [sp_heter_queue put_transaction copy_construct example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            using SpQueueInt = sp_heter_queue<int, runtime_type<int>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            static_assert(!std::is_copy_constructible<typename SpQueue::template put_transaction<>>::value, "");
			static_assert(!std::is_copy_constructible<typename SpQueueInt::template put_transaction<>>::value, "");
			//! [sp_heter_queue put_transaction copy_construct example 1]
		}
		{
			//! [sp_heter_queue put_transaction copy_assign example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            using SpQueueInt = sp_heter_queue<int, runtime_type<int>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            static_assert(!std::is_copy_assignable<typename SpQueue::template put_transaction<>>::value, "");
			static_assert(!std::is_copy_assignable<typename SpQueueInt::template put_transaction<>>::value, "");
			//! [sp_heter_queue put_transaction copy_assign example 1]
		}
		{
			//! [sp_heter_queue put_transaction move_construct example 1]
			sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY> queue;
			auto transaction1 = queue.start_push(1);

			// move from transaction1 to transaction2
			auto transaction2(std::move(transaction1));
			assert(transaction1.empty());
			assert(transaction2.element() == 1);
	
			// commit transaction2
			transaction2.commit();
			assert(transaction2.empty());

			//! [sp_heter_queue put_transaction move_construct example 1]

			//! [sp_heter_queue put_transaction move_construct example 2]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			// put_transaction<void> can be move constructed from any put_transaction<T>
			static_assert(std::is_constructible<typename SpQueue::template put_transaction<void>, typename SpQueue::template put_transaction<void> &&>::value, "");
			static_assert(std::is_constructible<typename SpQueue::template put_transaction<void>, typename SpQueue::template put_transaction<int> &&>::value, "");

			// put_transaction<T> can be move constructed only from put_transaction<T>
			static_assert(!std::is_constructible<typename SpQueue::template put_transaction<int>, typename SpQueue::template put_transaction<void> &&>::value, "");
			static_assert(!std::is_constructible<typename SpQueue::template put_transaction<int>, typename SpQueue::template put_transaction<float> &&>::value, "");
			static_assert(std::is_constructible<typename SpQueue::template put_transaction<int>, typename SpQueue::template put_transaction<int> &&>::value, "");
			//! [sp_heter_queue put_transaction move_construct example 2]
		}
        {
            //! [sp_heter_queue put_transaction move_assign example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue;
            auto transaction1 = queue.start_push(1);

            typename SpQueue::template put_transaction<> transaction2;
            transaction2 = std::move(transaction1);
            assert(transaction1.empty());
            transaction2.commit();
            assert(transaction2.empty());
            //! [sp_heter_queue put_transaction move_assign example 1]
        }
        {
			//! [sp_heter_queue put_transaction move_assign example 2]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			// put_transaction<void> can be move assigned from any put_transaction<T>
			static_assert(std::is_assignable<typename SpQueue::template put_transaction<void>, typename SpQueue::template put_transaction<void> &&>::value, "");
			static_assert(std::is_assignable<typename SpQueue::template put_transaction<void>, typename SpQueue::template put_transaction<int> &&>::value, "");

			// put_transaction<T> can be move assigned only from put_transaction<T>
			static_assert(!std::is_assignable<typename SpQueue::template put_transaction<int>, typename SpQueue::template put_transaction<void> &&>::value, "");
			static_assert(!std::is_assignable<typename SpQueue::template put_transaction<int>, typename SpQueue::template put_transaction<float> &&>::value, "");
			static_assert(std::is_assignable<typename SpQueue::template put_transaction<int>, typename SpQueue::template put_transaction<int> &&>::value, "");
			//! [sp_heter_queue put_transaction move_assign example 2]
		}
		{
			//! [sp_heter_queue put_transaction raw_allocate example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue;
			struct Msg
			{
				std::chrono::high_resolution_clock::time_point m_time = std::chrono::high_resolution_clock::now();
				size_t m_len = 0;
				void * m_data = nullptr;
			};

			auto post_message = [&queue](const void * i_data, size_t i_len) {
				auto transaction = queue.template start_emplace<Msg>();
				transaction.element().m_len = i_len;
				transaction.element().m_data = transaction.raw_allocate(i_len, 1);
				memcpy(transaction.element().m_data, i_data, i_len);
		
				assert(!transaction.empty()); // a put transaction is not empty if it's bound to an element being put
				transaction.commit();
				assert(transaction.empty()); // the commit makes the transaction empty
			};

			auto const start_time = std::chrono::high_resolution_clock::now();

			auto consume_all_msgs = [&queue, &start_time] {
				while (auto consume = queue.try_start_consume())
				{
                    auto const & msg = consume.template element<Msg>();
					auto const checksum = compute_checksum(msg.m_data, msg.m_len);
					std::cout << "Message with checksum " << checksum << " at ";
					std::cout << (msg.m_time - start_time).count() << std::endl;
					consume.commit();
				}
			};

			int msg_1 = 42, msg_2 = 567;
			post_message(&msg_1, sizeof(msg_1));
			post_message(&msg_2, sizeof(msg_2));

			consume_all_msgs();
			//! [sp_heter_queue put_transaction raw_allocate example 1]
		}
		{
			//! [sp_heter_queue put_transaction raw_allocate_copy example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			struct Msg
			{
				size_t m_len = 0;
				char * m_chars = nullptr;
			};
			auto post_message = [&queue](const char * i_data, size_t i_len) {
				auto transaction = queue.template start_emplace<Msg>();
				transaction.element().m_len = i_len;
				transaction.element().m_chars = transaction.raw_allocate_copy(i_data, i_data + i_len);
				memcpy(transaction.element().m_chars, i_data, i_len);
				transaction.commit();
			};
			//! [sp_heter_queue put_transaction raw_allocate_copy example 1]
			(void)post_message;
		}
		{
			//! [sp_heter_queue put_transaction raw_allocate_copy example 2]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			struct Msg
			{
				char * m_chars = nullptr;
			};
			auto post_message = [&queue](const std::string & i_string) {
				auto transaction = queue.template start_emplace<Msg>();
				transaction.element().m_chars = transaction.raw_allocate_copy(i_string);
				transaction.commit();
			};
			//! [sp_heter_queue put_transaction raw_allocate_copy example 2]
			(void)post_message;
		}
        {
			//! [sp_heter_queue put_transaction try_raw_allocate example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue;
			struct Msg
			{
				std::chrono::high_resolution_clock::time_point m_time = std::chrono::high_resolution_clock::now();
				size_t m_len = 0;
				void * m_data = nullptr;
			};

			auto post_message = [&queue](const void * i_data, size_t i_len) {
                if (auto transaction = queue.template try_start_emplace<Msg>(progress_lock_free))
                {
                    // if we fail to allocate the raw block, we don't commit the put transaction
				    transaction.element().m_len = i_len;
                    transaction.element().m_data = transaction.try_raw_allocate(progress_lock_free, i_len, 1);
                    if (transaction.element().m_data)
                    {
                        memcpy(transaction.element().m_data, i_data, i_len);

                        assert(!transaction.empty()); // a put transaction is not empty if it's bound to an element being put
                        transaction.commit();
                        assert(transaction.empty()); // the commit makes the transaction empty
                    }
                }
			};

			auto const start_time = std::chrono::high_resolution_clock::now();

			auto consume_all_msgs = [&queue, &start_time] {
				while (auto consume = queue.try_start_consume())
				{
                    auto const & msg = consume.template element<Msg>();
					auto const checksum = compute_checksum(msg.m_data, msg.m_len);
					std::cout << "Message with checksum " << checksum << " at ";
					std::cout << (msg.m_time - start_time).count() << std::endl;
					consume.commit();
				}
			};

			int msg_1 = 42, msg_2 = 567;
			post_message(&msg_1, sizeof(msg_1));
			post_message(&msg_2, sizeof(msg_2));

			consume_all_msgs();
			//! [sp_heter_queue put_transaction try_raw_allocate example 1]
		}
		{
			//! [sp_heter_queue put_transaction try_raw_allocate_copy example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			struct Msg
			{
				size_t m_len = 0;
				char * m_chars = nullptr;
			};
			auto post_message = [&queue](const char * i_data, size_t i_len) {
				auto transaction = queue.template try_start_emplace<Msg>(progress_lock_free);
				
                // if we fail to allocate the raw block, we don't commit the put transaction
                transaction.element().m_len = i_len;
                transaction.element().m_chars = transaction.try_raw_allocate_copy(progress_lock_free, i_data, i_data + i_len);
                if (transaction.element().m_chars)
                {
                    memcpy(transaction.element().m_chars, i_data, i_len);
                    transaction.commit();
                }
			};
			//! [sp_heter_queue put_transaction try_raw_allocate_copy example 1]
			(void)post_message;
		}
		{
			//! [sp_heter_queue put_transaction try_raw_allocate_copy example 2]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			struct Msg
			{
				char * m_chars = nullptr;
			};
			auto post_message = [&queue](const std::string & i_string) {
				auto transaction = queue.template try_start_emplace<Msg>(progress_lock_free);

                // if we fail to allocate the raw block, we don't commit the put transaction
                transaction.element().m_chars = transaction.try_raw_allocate_copy(progress_lock_free, i_string);
                if (transaction.element().m_chars)
                {
                    transaction.commit();
                }
			};
			//! [sp_heter_queue put_transaction try_raw_allocate_copy example 2]
			(void)post_message;
		}
        {
			//! [sp_heter_queue put_transaction empty example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

            typename SpQueue::template put_transaction<> transaction;
			assert(transaction.empty());

			transaction = queue.start_push(1);
			assert(!transaction.empty());
			//! [sp_heter_queue put_transaction empty example 1]
		}
		{
			//! [sp_heter_queue put_transaction operator_bool example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

            typename SpQueue::template put_transaction<> transaction;
			assert(!transaction);

			transaction = queue.start_push(1);
			assert(transaction);
			//! [sp_heter_queue put_transaction operator_bool example 1]
		}
		{
			//! [sp_heter_queue put_transaction cancel example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			// start and cancel a put
			assert(queue.empty());
			auto put = queue.start_push(42);
			/* assert(queue.empty()); <- this assert would trigger an undefined behavior, because it would access
				the queue during a non-reentrant put transaction. */
			assert(!put.empty());
			put.cancel();
			assert(queue.empty() && put.empty());
	
			// start and commit a put
			put = queue.start_push(42);
			put.commit();
			assert(queue.try_start_consume().template element<int>() == 42);
			//! [sp_heter_queue put_transaction cancel example 1]
		}
		{
			//! [sp_heter_queue put_transaction element_ptr example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
            
            int value = 42;
			auto put = queue.start_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);
			assert(*static_cast<int*>(put.element_ptr()) == 42);
			std::cout << "Putting an " << put.complete_type().type_info().name() << "..." << std::endl;
			put.commit();
			//! [sp_heter_queue put_transaction element_ptr example 1]

			//! [sp_heter_queue put_transaction element_ptr example 2]
			auto put_1 = queue.start_push(1);
			assert(*static_cast<int*>(put_1.element_ptr()) == 1); // this is fine
			assert(put_1.element() == 1); // this is better
			put_1.commit();
			//! [sp_heter_queue put_transaction element_ptr example 2]
		}
		{
            //! [sp_heter_queue put_transaction complete_type example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			int value = 42;
			auto put = queue.start_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);
			assert(put.complete_type().template is<int>());
			std::cout << "Putting an " << put.complete_type().type_info().name() << "..." << std::endl;
			//! [sp_heter_queue put_transaction complete_type example 1]
		}
		{
			//! [sp_heter_queue put_transaction destroy example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
            
            queue.start_push(42); /* this transaction is destroyed without being committed,
									so it gets canceled automatically. */
			//! [sp_heter_queue put_transaction destroy example 1]
		}
		{
			//! [sp_heter_queue typed_put_transaction element example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			int value = 42;
			auto untyped_put = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);

			auto typed_put = queue.start_reentrant_push(42.);

			/* typed_put = std::move(untyped_put); <- this would not compile: can't assign an untyped 
				transaction to a typed transaction */
	
			assert(typed_put.element() == 42.);

			//! [sp_heter_queue typed_put_transaction element example 1]
		}

		}

		static void spinlocking_heterogeneous_queue_consume_operation_samples()
		{
            using namespace density;
			using namespace density::type_features;
			
			{
			//! [sp_heter_queue consume_operation default_construct example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
                
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            typename SpQueue::consume_operation consume;
			assert(consume.empty());
		    //! [sp_heter_queue consume_operation default_construct example 1]
			}

			//! [sp_heter_queue consume_operation copy_construct example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            static_assert(!std::is_copy_constructible<typename SpQueue::consume_operation>::value, "");
			//! [sp_heter_queue consume_operation copy_construct example 1]

			//! [sp_heter_queue consume_operation copy_assign example 1]
			static_assert(!std::is_copy_assignable<typename SpQueue::consume_operation>::value, "");
			//! [sp_heter_queue consume_operation copy_assign example 1]

		{
			//! [sp_heter_queue consume_operation move_construct example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
	
			queue.push(42);
			auto consume = queue.try_start_consume();
	
			auto consume_1 = std::move(consume);
			assert(consume.empty() && !consume_1.empty());
			consume_1.commit();
			//! [sp_heter_queue consume_operation move_construct example 1]
		}
		{
			//! [sp_heter_queue consume_operation move_assign example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
	
			queue.push(42);
			queue.push(43);
			auto consume = queue.try_start_consume();

			typename SpQueue::consume_operation consume_1;
			consume_1 = std::move(consume);
			assert(consume.empty() && !consume_1.empty());
			consume_1.commit();
			//! [sp_heter_queue consume_operation move_assign example 1]
		}
		{
			//! [sp_heter_queue consume_operation destroy example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			queue.push(42);

			// this consumed is started and destroyed before being committed, so it has no observable effects
			queue.try_start_consume();
			//! [sp_heter_queue consume_operation destroy example 1]
		}
		{
			//! [sp_heter_queue consume_operation empty example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			queue.push(42);

			typename SpQueue::consume_operation consume;
			assert(consume.empty());
			consume = queue.try_start_consume();
			assert(!consume.empty());
			//! [sp_heter_queue consume_operation empty example 1]
		}
		{
			//! [sp_heter_queue consume_operation operator_bool example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			typename SpQueue::consume_operation consume;
			assert(consume.empty() == !consume);
			consume = queue.try_start_consume();
			assert(consume.empty() == !consume);
			//! [sp_heter_queue consume_operation operator_bool example 1]
		}
		{
			//! [sp_heter_queue consume_operation commit_nodestroy example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			queue.template emplace<std::string>("abc");

			typename SpQueue::consume_operation consume = queue.try_start_consume();
			consume.complete_type().destroy(consume.element_ptr());

			// the string has already been destroyed. Calling commit would trigger an undefined behavior
			consume.commit_nodestroy();
			//! [sp_heter_queue consume_operation commit_nodestroy example 1]
		}
		{
			//! [sp_heter_queue consume_operation cancel example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			queue.push(42);

			typename SpQueue::consume_operation consume = queue.try_start_consume();
			consume.cancel();

			// there is still a 42 in the queue
			assert(queue.try_start_consume().template element<int>() == 42);
			//! [sp_heter_queue consume_operation cancel example 1]
		}
		{
			//! [sp_heter_queue consume_operation complete_type example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			queue.push(42);

			typename SpQueue::consume_operation consume = queue.try_start_consume();
			assert(consume.complete_type().template is<int>());
			assert(consume.complete_type() == runtime_type<>::make<int>()); // same to the previous assert
			assert(consume.template element<int>() == 42);
			consume.commit();
			//! [sp_heter_queue consume_operation complete_type example 1]
		}
		{
			//! [sp_heter_queue consume_operation element_ptr example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			queue.push(42);

			typename SpQueue::consume_operation consume = queue.try_start_consume();
			++*static_cast<int*>(consume.element_ptr());
			assert(consume.template element<int>() == 43);
			consume.commit();
			//! [sp_heter_queue consume_operation element_ptr example 1]
		}
		{
			//! [sp_heter_queue consume_operation swap example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			queue.push(42);

			typename SpQueue::consume_operation consume_1 = queue.try_start_consume();
			typename SpQueue::consume_operation consume_2;
			{
				using namespace std;
				swap(consume_1, consume_2);
			}
			assert(consume_2.complete_type().template is<int>());
			assert(consume_2.complete_type() == runtime_type<>::make<int>()); // same to the previous assert
			assert(consume_2.template element<int>() == 42);
			consume_2.commit();
	
			assert(queue.empty());
			//! [sp_heter_queue consume_operation swap example 1]
		}
		{
			//! [sp_heter_queue consume_operation unaligned_element_ptr example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			queue.push(42);

			typename SpQueue::consume_operation consume = queue.try_start_consume();
			bool const is_overaligned = alignof(int) > SpQueue::min_alignment;
			void * const unaligned_ptr = consume.unaligned_element_ptr();
			int * element_ptr;
			if (is_overaligned)
			{
				element_ptr = static_cast<int*>(address_upper_align(unaligned_ptr, alignof(int)));
			}
			else
			{
				assert(unaligned_ptr == consume.element_ptr());
				element_ptr = static_cast<int*>(unaligned_ptr);
			}
			assert(address_is_aligned(element_ptr, alignof(int)));
			std::cout << "An int: " << *element_ptr << std::endl;
			consume.commit();
			//! [sp_heter_queue consume_operation unaligned_element_ptr example 1]
		}
		{
			//! [sp_heter_queue consume_operation element example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			queue.push(42);

			typename SpQueue::consume_operation consume = queue.try_start_consume();
			assert(consume.complete_type().template is<int>());
			std::cout << "An int: " << consume.template element<int>() << std::endl;
			/* std::cout << "An float: " << consume.element<float>() << std::endl; this would
				trigger an undefined behavior, because the element is not a float */
			consume.commit();
			//! [sp_heter_queue consume_operation element example 1]
		}
		}

		static void spinlocking_heterogeneous_queue_reentrant_put_samples()
		{
			using namespace density;

		{
        {
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			//! [sp_heter_queue reentrant_push example 1]
			queue.reentrant_push(12);
			queue.reentrant_push(std::string("Hello world!!"));
			//! [sp_heter_queue reentrant_push example 1]	

			//! [sp_heter_queue reentrant_emplace example 1]
			queue.template reentrant_emplace<int>();
			queue.template reentrant_emplace<std::string>(12, '-');
			//! [sp_heter_queue reentrant_emplace example 1]	
        }
		{
			//! [sp_heter_queue start_reentrant_push example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			auto put = queue.start_reentrant_push(12);
			put.element() += 2;
			put.commit(); // commits a 14
			//! [sp_heter_queue start_reentrant_push example 1]	
		}
		{
			//! [sp_heter_queue start_reentrant_emplace example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			auto put = queue.template start_reentrant_emplace<std::string>(4, '*');
			put.element() += "****";
			put.commit(); // commits a "********"
			//! [sp_heter_queue start_reentrant_emplace example 1]	
		}
		}
		{
			//! [sp_heter_queue reentrant_dyn_push example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			auto const type = MyRunTimeType::make<int>();
			queue.reentrant_dyn_push(type); // appends 0
			//! [sp_heter_queue reentrant_dyn_push example 1]
		}
		{
			//! [sp_heter_queue reentrant_dyn_push_copy example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			std::string const source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
			queue.reentrant_dyn_push_copy(type, &source);
			//! [sp_heter_queue reentrant_dyn_push_copy example 1]
		}
		{
			//! [sp_heter_queue reentrant_dyn_push_move example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			std::string source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
			queue.reentrant_dyn_push_move(type, &source);
			//! [sp_heter_queue reentrant_dyn_push_move example 1]
		}

		{
			//! [sp_heter_queue start_reentrant_dyn_push example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			auto const type = MyRunTimeType::make<int>();
			auto put = queue.start_reentrant_dyn_push(type);
			put.commit();
			//! [sp_heter_queue start_reentrant_dyn_push example 1]
		}
		{
			//! [sp_heter_queue start_reentrant_dyn_push_copy example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			std::string const source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
			auto put = queue.start_reentrant_dyn_push_copy(type, &source);
			put.commit();
			//! [sp_heter_queue start_reentrant_dyn_push_copy example 1]
		}
		{
			//! [sp_heter_queue start_reentrant_dyn_push_move example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			std::string source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
			auto put = queue.start_reentrant_dyn_push_move(type, &source);
			put.commit();
			//! [sp_heter_queue start_reentrant_dyn_push_move example 1]
		}
		}

        static void spinlocking_heterogeneous_queue_reentrant_try_put_samples()
		{
			using namespace density;

		{
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;

			//! [sp_heter_queue try_reentrant_push example 1]
            if (queue.try_reentrant_push(progress_blocking, 12))
            {
                if (queue.try_reentrant_push(progress_blocking, std::string("Hello world!!")))
                {
                    // ...
                }
            }
			//! [sp_heter_queue try_reentrant_push example 1]	

			//! [sp_heter_queue try_reentrant_emplace example 1]
			if (queue.template try_reentrant_emplace<int>(progress_blocking))
            {
                if (queue.template try_reentrant_emplace<std::string>(progress_blocking, 12, '-'))
                {
                    // ...
                }
            }
			//! [sp_heter_queue try_reentrant_emplace example 1]	

		{
			//! [sp_heter_queue try_start_reentrant_push example 1]
            if (auto put = queue.try_start_reentrant_push(progress_blocking, 12))
            {
                // ...
                put.element() += 2;
                put.commit(); // commits a 14
            }
            //! [sp_heter_queue try_start_reentrant_push example 1]	
		}
		{
			//! [sp_heter_queue try_start_reentrant_emplace example 1]
            if (auto put = queue.template try_start_reentrant_emplace<std::string>(progress_blocking, 4, '*'))
            {
                // ...
                put.element() += "****";
                put.commit(); // commits a "********"
            }
            //! [sp_heter_queue try_start_reentrant_emplace example 1]	
		}
		}
		{
			//! [sp_heter_queue try_reentrant_dyn_push example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			auto const type = MyRunTimeType::make<int>();
            if (queue.try_reentrant_dyn_push(progress_blocking, type)) // appends 0
            {
                // ...
            }
			//! [sp_heter_queue try_reentrant_dyn_push example 1]
		}
		{
			//! [sp_heter_queue try_reentrant_dyn_push_copy example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			std::string const source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
            if (queue.try_reentrant_dyn_push_copy(progress_blocking, type, &source))
            {
                // ...
            }
			//! [sp_heter_queue try_reentrant_dyn_push_copy example 1]
		}
		{
			//! [sp_heter_queue try_reentrant_dyn_push_move example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			std::string source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
            if (queue.try_reentrant_dyn_push_move(progress_blocking, type, &source))
            {
                // ...
            }
			//! [sp_heter_queue try_reentrant_dyn_push_move example 1]
		}

		{
			//! [sp_heter_queue try_start_reentrant_dyn_push example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			auto const type = MyRunTimeType::make<int>();
            if (auto put = queue.try_start_reentrant_dyn_push(progress_blocking, type))
            {
                // ...
                put.commit();
            }
			//! [sp_heter_queue try_start_reentrant_dyn_push example 1]
		}
		{
			//! [sp_heter_queue try_start_reentrant_dyn_push_copy example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			std::string const source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
            if (auto put = queue.try_start_reentrant_dyn_push_copy(progress_blocking, type, &source))
            {
                // ...
                put.commit();
            }
			//! [sp_heter_queue try_start_reentrant_dyn_push_copy example 1]
		}
		{
			//! [sp_heter_queue try_start_reentrant_dyn_push_move example 1]
			using namespace density::type_features;
			using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
			sp_heter_queue<void, MyRunTimeType> queue;

			std::string source("Hello world!!");
			auto const type = MyRunTimeType::make<decltype(source)>();
            if (auto put = queue.try_start_reentrant_dyn_push_move(progress_blocking, type, &source))
            {
                // ...
                put.commit();
            }
			//! [sp_heter_queue try_start_reentrant_dyn_push_move example 1]
		}
		}

		static void spinlocking_heterogeneous_queue_reentrant_put_transaction_samples()
		{
            using namespace density;
			using namespace type_features;

		{
			//! [sp_heter_queue reentrant_put_transaction default_construct example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
                
            typename SpQueue::template reentrant_put_transaction<> transaction;
			assert(transaction.empty());
			//! [sp_heter_queue reentrant_put_transaction default_construct example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction copy_construct example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            using SpQueueInt = sp_heter_queue<int, runtime_type<int>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			static_assert(!std::is_copy_constructible<typename SpQueue::template reentrant_put_transaction<>>::value, "");
			static_assert(!std::is_copy_constructible<typename SpQueueInt::template reentrant_put_transaction<>>::value, "");
			//! [sp_heter_queue reentrant_put_transaction copy_construct example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction copy_assign example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            using SpQueueInt = sp_heter_queue<int, runtime_type<int>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			static_assert(!std::is_copy_assignable<typename SpQueue::template reentrant_put_transaction<>>::value, "");
			static_assert(!std::is_copy_assignable<typename SpQueueInt::template reentrant_put_transaction<>>::value, "");
			//! [sp_heter_queue reentrant_put_transaction copy_assign example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction move_construct example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue;
			auto transaction1 = queue.start_reentrant_push(1);

			// move from transaction1 to transaction2
			auto transaction2(std::move(transaction1));
			assert(transaction1.empty());
			assert(transaction2.element() == 1);
	
			// commit transaction2
			transaction2.commit();
			assert(transaction2.empty());

			//! [sp_heter_queue reentrant_put_transaction move_construct example 1]

			//! [sp_heter_queue reentrant_put_transaction move_construct example 2]
			
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            
            // reentrant_put_transaction<void> can be move constructed from any reentrant_put_transaction<T>
			static_assert(std::is_constructible<typename SpQueue::template reentrant_put_transaction<void>, typename SpQueue::template reentrant_put_transaction<void> &&>::value, "");
			static_assert(std::is_constructible<typename SpQueue::template reentrant_put_transaction<void>, typename SpQueue::template reentrant_put_transaction<int> &&>::value, "");

			// reentrant_put_transaction<T> can be move constructed only from reentrant_put_transaction<T>
			static_assert(!std::is_constructible<typename SpQueue::template reentrant_put_transaction<int>, typename SpQueue::template reentrant_put_transaction<void> &&>::value, "");
			static_assert(!std::is_constructible<typename SpQueue::template reentrant_put_transaction<int>, typename SpQueue::template reentrant_put_transaction<float> &&>::value, "");
			static_assert(std::is_constructible<typename SpQueue::template reentrant_put_transaction<int>, typename SpQueue::template reentrant_put_transaction<int> &&>::value, "");
			//! [sp_heter_queue reentrant_put_transaction move_construct example 2]
		}
        {
            //! [sp_heter_queue reentrant_put_transaction move_assign example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue;
            auto transaction1 = queue.start_reentrant_push(1);

            typename SpQueue::template reentrant_put_transaction<> transaction2;
            transaction2 = queue.start_reentrant_push(1);
            transaction2 = std::move(transaction1);
            transaction2.commit();
            assert(transaction2.empty());
            //! [sp_heter_queue reentrant_put_transaction move_assign example 1]
        }
        {
			//! [sp_heter_queue reentrant_put_transaction move_assign example 2]
			// reentrant_put_transaction<void> can be move assigned from any reentrant_put_transaction<T>
	        using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                        PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            static_assert(std::is_assignable<typename SpQueue::template reentrant_put_transaction<void>, typename SpQueue::template reentrant_put_transaction<void> &&>::value, "");
			static_assert(std::is_assignable<typename SpQueue::template reentrant_put_transaction<void>, typename SpQueue::template reentrant_put_transaction<int> &&>::value, "");

			// reentrant_put_transaction<T> can be move assigned only from reentrant_put_transaction<T>
			static_assert(!std::is_assignable<typename SpQueue::template reentrant_put_transaction<int>, typename SpQueue::template reentrant_put_transaction<void> &&>::value, "");
			static_assert(!std::is_assignable<typename SpQueue::template reentrant_put_transaction<int>, typename SpQueue::template reentrant_put_transaction<float> &&>::value, "");
			static_assert(std::is_assignable<typename SpQueue::template reentrant_put_transaction<int>, typename SpQueue::template reentrant_put_transaction<int> &&>::value, "");
			//! [sp_heter_queue reentrant_put_transaction move_assign example 2]

		}
		{
			//! [sp_heter_queue reentrant_put_transaction raw_allocate example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			struct Msg
			{
				std::chrono::high_resolution_clock::time_point m_time = std::chrono::high_resolution_clock::now();
				size_t m_len = 0;
				void * m_data = nullptr;
			};

			auto post_message = [&queue](const void * i_data, size_t i_len) {
				auto transaction = queue.template start_reentrant_emplace<Msg>();
				transaction.element().m_len = i_len;
				transaction.element().m_data = transaction.raw_allocate(i_len, 1);
				memcpy(transaction.element().m_data, i_data, i_len);
		
				assert(!transaction.empty()); // a put transaction is not empty if it's bound to an element being put
				transaction.commit();
				assert(transaction.empty()); // the commit makes the transaction empty
			};

			auto const start_time = std::chrono::high_resolution_clock::now();

			auto consume_all_msgs = [&queue, &start_time] {
				while (auto consume = queue.try_start_reentrant_consume())
				{
                    auto const & msg = consume.template element<Msg>();
					auto const checksum = compute_checksum(msg.m_data, msg.m_len);
					std::cout << "Message with checksum " << checksum << " at ";
					std::cout << (msg.m_time - start_time).count() << std::endl;
					consume.commit();
				}
			};

			int msg_1 = 42, msg_2 = 567;
			post_message(&msg_1, sizeof(msg_1));
			post_message(&msg_2, sizeof(msg_2));

			consume_all_msgs();
			//! [sp_heter_queue reentrant_put_transaction raw_allocate example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction raw_allocate_copy example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			struct Msg
			{
				size_t m_len = 0;
				char * m_chars = nullptr;
			};
			auto post_message = [&queue](const char * i_data, size_t i_len) {
				auto transaction = queue.template start_reentrant_emplace<Msg>();
				transaction.element().m_len = i_len;
				transaction.element().m_chars = transaction.raw_allocate_copy(i_data, i_data + i_len);
				memcpy(transaction.element().m_chars, i_data, i_len);
				transaction.commit();
			};
			//! [sp_heter_queue reentrant_put_transaction raw_allocate_copy example 1]
			(void)post_message;
		}
		{
			//! [sp_heter_queue reentrant_put_transaction raw_allocate_copy example 2]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			struct Msg
			{
				char * m_chars = nullptr;
			};
			auto post_message = [&queue](const std::string & i_string) {
				auto transaction = queue.template start_reentrant_emplace<Msg>();
				transaction.element().m_chars = transaction.raw_allocate_copy(i_string);
				transaction.commit();
			};
			//! [sp_heter_queue reentrant_put_transaction raw_allocate_copy example 2]
			(void)post_message;
		}
        {
			//! [sp_heter_queue reentrant_put_transaction try_raw_allocate example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue;
			struct Msg
			{
				std::chrono::high_resolution_clock::time_point m_time = std::chrono::high_resolution_clock::now();
				size_t m_len = 0;
				void * m_data = nullptr;
			};

			auto post_message = [&queue](const void * i_data, size_t i_len) {
                if (auto transaction = queue.template try_start_reentrant_emplace<Msg>(progress_lock_free))
                {
                    // if we fail to allocate the raw block, we don't commit the put transaction
				    transaction.element().m_len = i_len;
                    transaction.element().m_data = transaction.try_raw_allocate(progress_lock_free, i_len, 1);
                    if (transaction.element().m_data)
                    {
                        memcpy(transaction.element().m_data, i_data, i_len);

                        assert(!transaction.empty()); // a put transaction is not empty if it's bound to an element being put
                        transaction.commit();
                        assert(transaction.empty()); // the commit makes the transaction empty
                    }
                }
			};

			auto const start_time = std::chrono::high_resolution_clock::now();

			auto consume_all_msgs = [&queue, &start_time] {
				while (auto consume = queue.try_start_consume())
				{
                    auto const & msg = consume.template element<Msg>();
					auto const checksum = compute_checksum(msg.m_data, msg.m_len);
					std::cout << "Message with checksum " << checksum << " at ";
					std::cout << (msg.m_time - start_time).count() << std::endl;
					consume.commit();
				}
			};

			int msg_1 = 42, msg_2 = 567;
			post_message(&msg_1, sizeof(msg_1));
			post_message(&msg_2, sizeof(msg_2));

			consume_all_msgs();
			//! [sp_heter_queue reentrant_put_transaction try_raw_allocate example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction try_raw_allocate_copy example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			struct Msg
			{
				size_t m_len = 0;
				char * m_chars = nullptr;
			};
			auto post_message = [&queue](const char * i_data, size_t i_len) {
				auto transaction = queue.template try_start_reentrant_emplace<Msg>(progress_lock_free);
				
                // if we fail to allocate the raw block, we don't commit the put transaction
                transaction.element().m_len = i_len;
                transaction.element().m_chars = transaction.try_raw_allocate_copy(progress_lock_free, i_data, i_data + i_len);
                if (transaction.element().m_chars)
                {
                    memcpy(transaction.element().m_chars, i_data, i_len);
                    transaction.commit();
                }
			};
			//! [sp_heter_queue reentrant_put_transaction try_raw_allocate_copy example 1]
			(void)post_message;
		}
		{
			//! [sp_heter_queue reentrant_put_transaction try_raw_allocate_copy example 2]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			struct Msg
			{
				char * m_chars = nullptr;
			};
			auto post_message = [&queue](const std::string & i_string) {
				auto transaction = queue.template try_start_reentrant_emplace<Msg>(progress_lock_free);

                // if we fail to allocate the raw block, we don't commit the put transaction
                transaction.element().m_chars = transaction.try_raw_allocate_copy(progress_lock_free, i_string);
                if (transaction.element().m_chars)
                {
                    transaction.commit();
                }
			};
			//! [sp_heter_queue reentrant_put_transaction try_raw_allocate_copy example 2]
			(void)post_message;
		}
		{
			//! [sp_heter_queue reentrant_put_transaction empty example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
            typename SpQueue::template reentrant_put_transaction<> transaction;
			assert(transaction.empty());

			transaction = queue.start_reentrant_push(1);
			assert(!transaction.empty());
			//! [sp_heter_queue reentrant_put_transaction empty example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction operator_bool example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;

			typename SpQueue::template reentrant_put_transaction<> transaction;
			assert(!transaction);

			transaction = queue.start_reentrant_push(1);
			assert(transaction);
			//! [sp_heter_queue reentrant_put_transaction operator_bool example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction queue example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;

			typename SpQueue::template reentrant_put_transaction<> transaction;
			assert(transaction.queue() == nullptr);

			transaction = queue.start_reentrant_push(1);
			assert(transaction.queue() == &queue);
			//! [sp_heter_queue reentrant_put_transaction queue example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction cancel example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;

			// start and cancel a put
			assert(queue.empty());
			auto put = queue.start_reentrant_push(42);
			/* assert(queue.empty()); <- this assert would trigger an undefined behavior, because it would access
				the queue during a non-reentrant put transaction. */
			assert(!put.empty());
			put.cancel();
			assert(queue.empty() && put.empty());
	
			// start and commit a put
			put = queue.start_reentrant_push(42);
			put.commit();
			assert(queue.try_start_reentrant_consume().template element<int>() == 42);
			//! [sp_heter_queue reentrant_put_transaction cancel example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction element_ptr example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;

			int value = 42;
			auto put = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);
			assert(*static_cast<int*>(put.element_ptr()) == 42);
			std::cout << "Putting an " << put.complete_type().type_info().name() << "..." << std::endl;
			put.commit();
			//! [sp_heter_queue reentrant_put_transaction element_ptr example 1]

			//! [sp_heter_queue reentrant_put_transaction element_ptr example 2]
			auto put_1 = queue.start_reentrant_push(1);
			assert(*static_cast<int*>(put_1.element_ptr()) == 1); // this is fine
			assert(put_1.element() == 1); // this is better
			put_1.commit();
			//! [sp_heter_queue reentrant_put_transaction element_ptr example 2]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction complete_type example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;

			int value = 42;
			auto put = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);
			assert(put.complete_type().template is<int>());
			std::cout << "Putting an " << put.complete_type().type_info().name() << "..." << std::endl;
			//! [sp_heter_queue reentrant_put_transaction complete_type example 1]
		}
		{
			//! [sp_heter_queue reentrant_put_transaction destroy example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.start_reentrant_push(42);/* this transaction is destroyed without being committed,
									so it gets canceled automatically. */
			//! [sp_heter_queue reentrant_put_transaction destroy example 1]
		}
		{
			//! [sp_heter_queue reentrant_typed_put_transaction element example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;

			int value = 42;
			auto untyped_put = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);

			auto typed_put = queue.start_reentrant_push(42.);

			/* typed_put = std::move(untyped_put); <- this would not compile: can't assign an untyped 
				transaction to a typed transaction */
	
			assert(typed_put.element() == 42.);

			//! [sp_heter_queue reentrant_typed_put_transaction element example 1]
		}
		}

		static void spinlocking_heterogeneous_queue_reentrant_consume_operation_samples()
		{
            using namespace density;
			using namespace type_features;
			
			{
		//! [sp_heter_queue reentrant_consume_operation default_construct example 1]
	    using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
            PROD_CARDINALITY, CONSUMER_CARDINALITY>;
	    typename SpQueue::reentrant_consume_operation consume;
	    assert(consume.empty());
		//! [sp_heter_queue reentrant_consume_operation default_construct example 1]
			}
        {
            //! [sp_heter_queue reentrant_consume_operation copy_construct example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            static_assert(!std::is_copy_constructible<typename SpQueue::reentrant_consume_operation>::value, "");
            //! [sp_heter_queue reentrant_consume_operation copy_construct example 1]

            //! [sp_heter_queue reentrant_consume_operation copy_assign example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator,
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
            static_assert(!std::is_copy_assignable<typename SpQueue::reentrant_consume_operation>::value, "");
            //! [sp_heter_queue reentrant_consume_operation copy_assign example 1]
        }
		{
			//! [sp_heter_queue reentrant_consume_operation move_construct example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
	
			queue.push(42);
			auto consume = queue.try_start_reentrant_consume();
	
			auto consume_1 = std::move(consume);
			assert(consume.empty() && !consume_1.empty());
			consume_1.commit();
			//! [sp_heter_queue reentrant_consume_operation move_construct example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation move_assign example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
	
			queue.push(42);
			queue.push(43);
			auto consume = queue.try_start_reentrant_consume();
			consume.cancel();

			typename SpQueue::reentrant_consume_operation consume_1;
			consume = queue.try_start_reentrant_consume();
			consume_1 = std::move(consume);
			assert(consume.empty());
			assert(!consume_1.empty());
			consume_1.commit();
			//! [sp_heter_queue reentrant_consume_operation move_assign example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation destroy example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(42);

			// this consumed is started and destroyed before being committed, so it has no observable effects
			queue.try_start_reentrant_consume();
			//! [sp_heter_queue reentrant_consume_operation destroy example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation empty example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume;
			assert(consume.empty());
			consume = queue.try_start_reentrant_consume();
			assert(!consume.empty());
			//! [sp_heter_queue reentrant_consume_operation empty example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation operator_bool example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume;
			assert(consume.empty() == !consume);
			consume = queue.try_start_reentrant_consume();
			assert(consume.empty() == !consume);
			//! [sp_heter_queue reentrant_consume_operation operator_bool example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation queue example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume;
			assert(consume.empty() && !consume && consume.queue() == nullptr);
			consume = queue.try_start_reentrant_consume();
			assert(!consume.empty() && !!consume && consume.queue() == &queue);
			//! [sp_heter_queue reentrant_consume_operation queue example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation commit_nodestroy example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.template emplace<std::string>("abc");

			typename SpQueue::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
			consume.complete_type().destroy(consume.element_ptr());

			// the string has already been destroyed. Calling commit would trigger an undefined behavior
			consume.commit_nodestroy();
			//! [sp_heter_queue reentrant_consume_operation commit_nodestroy example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation swap example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume_1 = queue.try_start_reentrant_consume();
			typename SpQueue::reentrant_consume_operation consume_2;
			{
				using namespace std;
				swap(consume_1, consume_2);
			}
			assert(consume_2.complete_type().template is<int>());
			assert(consume_2.complete_type() == runtime_type<>::make<int>()); // same to the previous assert
			assert(consume_2.template element<int>() == 42);
			consume_2.commit();
	
			assert(queue.empty());
			//! [sp_heter_queue reentrant_consume_operation swap example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation cancel example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;

			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
			consume.cancel();

			// there is still a 42 in the queue
			assert(queue.try_start_reentrant_consume().template element<int>() == 42);
			//! [sp_heter_queue reentrant_consume_operation cancel example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation complete_type example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
			assert(consume.complete_type().template is<int>());
			assert(consume.complete_type() == runtime_type<>::make<int>()); // same to the previous assert
			consume.commit();
	
			assert(queue.empty());
			//! [sp_heter_queue reentrant_consume_operation complete_type example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation element_ptr example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
			++*static_cast<int*>(consume.element_ptr());
			assert(consume.template element<int>() == 43);
			consume.commit();
			//! [sp_heter_queue reentrant_consume_operation element_ptr example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation unaligned_element_ptr example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
			bool const is_overaligned = alignof(int) > SpQueue::min_alignment;
			void * const unaligned_ptr = consume.unaligned_element_ptr();
			int * element_ptr;
			if (is_overaligned)
			{
				element_ptr = static_cast<int*>(address_upper_align(unaligned_ptr, alignof(int)));
			}
			else
			{
				assert(unaligned_ptr == consume.element_ptr());
				element_ptr = static_cast<int*>(unaligned_ptr);
			}
			assert(address_is_aligned(element_ptr, alignof(int)));
			std::cout << "An int: " << *element_ptr << std::endl;
			consume.commit();
			//! [sp_heter_queue reentrant_consume_operation unaligned_element_ptr example 1]
		}
		{
			//! [sp_heter_queue reentrant_consume_operation element example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
			assert(consume.complete_type().template is<int>());
			std::cout << "An int: " << consume.template element<int>() << std::endl;
			/* std::cout << "An float: " << consume.element<float>() << std::endl; this would
				trigger an undefined behavior, because the element is not a float */
			consume.commit();
			//! [sp_heter_queue reentrant_consume_operation element example 1]
		}
		}

		static void spinlocking_heterogeneous_queue_samples_1()
		{
			//! [sp_heter_queue example 3]
			using namespace density;
            using namespace density::type_features;

			/* a runtime_type is internally like a pointer to a v-table, but it can 
				contain functions or data (like in the case of size and alignment). */
			using MyRunTimeType = runtime_type<void, feature_list<
                default_construct, copy_construct, destroy, size, alignment, ostream, istream, rtti>>;
			using SpQueue = sp_heter_queue<void, MyRunTimeType, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;		

			SpQueue queue;
			queue.push(4);
			queue.push(std::complex<double>(1., 4.));
			queue.template emplace<std::string>("Hello!!");
	
			// This would not compile because std::thread does not have a << operator for streams
			// queue.emplace<std::thread>();

			// consume all the elements
			while (auto consume = queue.try_start_consume())
			{
				/* this is like: give me the function at the 6-th row in the v-table. The type ostream 
					is converted to an index at compile time. */
				auto const ostream_feature = consume.complete_type().template get_feature<ostream>();
		
				ostream_feature(std::cout, consume.element_ptr()); // this invokes the feature
				std::cout << "\n";
				consume.commit();  // don't forget the commit, otherwise the element will remain in the queue
			}
			//! [sp_heter_queue example 3]

			//! [sp_heter_queue example 4]
			// this local function reads from std::cin an object of a given type and puts it in the queue
			auto ask_and_put = [&](const MyRunTimeType & i_type) {

				// for this we exploit the feature rtti that we have included in MyRunTimeType
				std::cout << "Enter a " << i_type.type_info().name() << std::endl;

				auto const istream_feature = i_type.get_feature<istream>();

				auto put = queue.start_dyn_push(i_type);
				istream_feature(std::cin, put.element_ptr());
		
				/* if an exception is thrown before the commit, the put is canceled without ever 
					having observable side effects. */
				put.commit();
			};
	
			ask_and_put(MyRunTimeType::make<int>());
			ask_and_put(MyRunTimeType::make<std::string>());
			//! [sp_heter_queue example 4]
		}

		static void samples(std::ostream & )
		{
            using namespace density;
			using namespace density::type_features;
			
		{
			//! [sp_heter_queue put example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue;
			queue.push(19); // the parameter can be an l-value or an r-value
			queue.template emplace<std::string>(8, '*'); // pushes "********"
			//! [sp_heter_queue put example 1]

			//! [sp_heter_queue example 2]
			auto consume = queue.try_start_consume();
			int my_int = consume.template element<int>();
			consume.commit();

			consume = queue.try_start_consume();
			std::string my_string = consume.template element<std::string>();
			consume.commit();
			//! [sp_heter_queue example 3
			(void)my_int;
			(void)my_string;
		}

		{
			//! [sp_heter_queue put example 2]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue;
			struct MessageInABottle
			{
				const char * m_text = nullptr;
			};
			auto transaction = queue.template start_emplace<MessageInABottle>();
			transaction.element().m_text = transaction.raw_allocate_copy("Hello world!");
			transaction.commit();
			//! [sp_heter_queue put example 2]

			//! [sp_heter_queue consume example 1]
			auto consume = queue.try_start_consume();
			if (consume.complete_type().template is<std::string>())
			{
				std::cout << consume.template element<std::string>() << std::endl;
			}
			else if (consume.complete_type().template is<MessageInABottle>())
			{
				std::cout << consume.template element<MessageInABottle>().m_text << std::endl;
			}
			consume.commit();
			//! [sp_heter_queue consume example 1]
		}

		{
			//! [sp_heter_queue default_construct example 1]
			using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			
            SpQueue queue;
			assert(queue.empty());
			//! [sp_heter_queue default_construct example 1]
		}
		{
			//! [sp_heter_queue move_construct example 1]
			using MyRunTimeType = runtime_type<void, feature_list<default_construct, copy_construct, destroy, size, alignment, equals>>;
		
			sp_heter_queue<void, MyRunTimeType> queue;
			queue.push(std::string());
			queue.push(std::make_pair(4., 1));

			sp_heter_queue<void, MyRunTimeType> queue_1(std::move(queue));

			assert(queue.empty());
			assert(!queue_1.empty());
			//! [sp_heter_queue move_construct example 1]
		}
		{
			//! [sp_heter_queue construct_copy_alloc example 1]
			void_allocator allocator;
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue(allocator);
			//! [sp_heter_queue construct_copy_alloc example 1]
		}
		{
			//! [sp_heter_queue construct_move_alloc example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			void_allocator allocator;
			SpQueue queue(std::move(allocator));
			//! [sp_heter_queue construct_move_alloc example 1]
		}
		{
			//! [sp_heter_queue move_assign example 1]
            using MyRunTimeType = runtime_type<void, feature_list<default_construct, copy_construct, destroy, size, alignment, equals>>;
		
            using SpQueue = sp_heter_queue<void, MyRunTimeType, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

			SpQueue queue;
			queue.push(std::string("abc"));
			queue.push(std::make_pair(4., 1));

			SpQueue queue_1;
			queue_1 = std::move(queue);

			assert(queue.empty());
	
			assert(!queue_1.empty());
			//! [sp_heter_queue move_assign example 1]
		}
		{
			//! [sp_heter_queue get_allocator example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue;
			assert(queue.get_allocator() == void_allocator());
			//! [sp_heter_queue get_allocator example 1]
		}
		{
			//! [sp_heter_queue get_allocator_ref example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue;
			assert(queue.get_allocator_ref() == void_allocator());
			//! [sp_heter_queue get_allocator_ref example 1]
		}
		{
			//! [sp_heter_queue get_allocator_ref example 2]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue;
			auto const & queue_ref = queue;
			assert(queue_ref.get_allocator_ref() == void_allocator());
			//! [sp_heter_queue get_allocator_ref example 2]
			(void)queue_ref;
		}
		{
			//! [sp_heter_queue swap example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue, queue_1;
			queue.push(1);
			swap(queue, queue_1);

			assert(queue.empty());
			assert(!queue_1.empty());
			//! [sp_heter_queue swap example 1]
		}
		{
			//! [sp_heter_queue swap example 2]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue, queue_1;
			queue.push(1);
			{
				using namespace std;
				swap(queue, queue_1);
			}
			assert(queue.empty());
			assert(!queue_1.empty());
			//! [sp_heter_queue swap example 2]
		}
		{
			//! [sp_heter_queue empty example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue;
			assert(queue.empty());
			queue.push(1);
			assert(!queue.empty());
			//! [sp_heter_queue empty example 1]
		}
		{
			//! [sp_heter_queue clear example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;

            SpQueue queue;
			queue.push(1);
			queue.clear();
			assert(queue.empty());
			//! [sp_heter_queue clear example 1]
		}
		{
			//! [sp_heter_queue try_pop example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
	
            SpQueue queue;

			bool pop_result = queue.try_pop();
			assert(pop_result == false);
	
			queue.push(1);
			queue.push(2);

			pop_result = queue.try_pop();
			assert(pop_result == true);
			auto consume = queue.try_start_consume();
			assert(consume.template element<int>() == 2);
			consume.commit();
			//! [sp_heter_queue try_pop example 1]
            (void)pop_result;
		}
		{
			//! [sp_heter_queue try_start_consume example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
	
            SpQueue queue;
			auto consume_1 = queue.try_start_consume();
			assert(!consume_1);
	
			queue.push(42);

			auto consume_2 = queue.try_start_consume();
			assert(consume_2);
			assert(consume_2.template element<int>() == 42);
			consume_2.commit();
			//! [sp_heter_queue try_start_consume example 1]
		}
		{
			//! [sp_heter_queue try_start_consume_ example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
	
            SpQueue queue;

			typename SpQueue::consume_operation consume_1;
			bool const bool_1 = queue.try_start_consume(consume_1);
			assert(!bool_1 && !consume_1);
	
			queue.push(42);

			typename SpQueue::consume_operation consume_2;
			auto bool_2 = queue.try_start_consume(consume_2);
			assert(consume_2 && bool_2);
			assert(consume_2.template element<int>() == 42);
			consume_2.commit();
			//! [sp_heter_queue try_start_consume_ example 1]
			(void)bool_1;
			(void)bool_2;
		}
		{
            //! [sp_heter_queue reentrant example 1]
			// start 3 reentrant put transactions
			
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue;

            auto put_1 = queue.start_reentrant_push(1);
			auto put_2 = queue.template start_reentrant_emplace<std::string>("Hello world!");
			double pi = 3.14;
			auto put_3 = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<double>(), &pi);
			assert(queue.empty()); // the queue is still empty, because no transaction has been committed

			// commit and start consuming "Hello world!"
			put_2.commit();
			auto consume2 = queue.try_start_reentrant_consume();
			assert(!consume2.empty() && consume2.complete_type().template is<std::string>());

			// commit and start consuming 1
			put_1.commit();
			auto consume1 = queue.try_start_reentrant_consume();
			assert(!consume1.empty() && consume1.complete_type().template is<int>());

			// cancel 3.14, and commit the consumes
			put_3.cancel();
			consume1.commit();
			consume2.commit();
			assert(queue.empty());

			//! [sp_heter_queue reentrant example 1]
		}
		{
			//! [sp_heter_queue try_reentrant_pop example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue;
	
			bool pop_result = queue.try_reentrant_pop();
			assert(pop_result == false);
	
			queue.push(1);
			queue.push(2);

			pop_result = queue.try_reentrant_pop();
			assert(pop_result == true);
			auto consume = queue.try_start_reentrant_consume();
			assert(consume.template element<int>() == 2);
			consume.commit();
			//! [sp_heter_queue try_reentrant_pop example 1]
            (void)pop_result;
		}
		{
			//! [sp_heter_queue try_start_reentrant_consume example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue;
	
			auto consume_1 = queue.try_start_reentrant_consume();
			assert(!consume_1);
	
			queue.push(42);

			auto consume_2 = queue.try_start_reentrant_consume();
			assert(consume_2);
			assert(consume_2.template element<int>() == 42);
			consume_2.commit();
			//! [sp_heter_queue try_start_reentrant_consume example 1]
		}
		{
			//! [sp_heter_queue try_start_reentrant_consume_ example 1]
            using SpQueue = sp_heter_queue<void, runtime_type<>, void_allocator, 
                PROD_CARDINALITY, CONSUMER_CARDINALITY>;
			SpQueue queue;
	
			typename SpQueue::reentrant_consume_operation consume_1;
			bool const bool_1 = queue.try_start_reentrant_consume(consume_1);
			assert(!bool_1 && !consume_1);
	
			queue.push(42);

			typename SpQueue::reentrant_consume_operation consume_2;
			auto bool_2 = queue.try_start_reentrant_consume(consume_2);
			assert(consume_2 && bool_2);
			assert(consume_2.template element<int>() == 42);
			consume_2.commit();
			//! [sp_heter_queue try_start_reentrant_consume_ example 1]
			(void)bool_2;
			(void)bool_1;
		}

			// this samples uses std::cout and std::cin
			// spinlocking_heterogeneous_queue_samples_1();

			spinlocking_heterogeneous_queue_put_samples();

            spinlocking_heterogeneous_queue_try_put_samples();

			spinlocking_heterogeneous_queue_put_transaction_samples();

			spinlocking_heterogeneous_queue_consume_operation_samples();

			spinlocking_heterogeneous_queue_reentrant_put_samples();

            spinlocking_heterogeneous_queue_reentrant_try_put_samples();

			spinlocking_heterogeneous_queue_reentrant_put_transaction_samples();

			spinlocking_heterogeneous_queue_reentrant_consume_operation_samples();
		}	
	
	}; // struct SpQueueSamples<...>

	void spinlocking_heterogeneous_queue_samples(std::ostream & i_ostream)
	{
		PrintScopeDuration dur(i_ostream, "spin-locking heterogeneous queue samples");
		
		constexpr auto mult = density::concurrency_multiple;
		constexpr auto single = density::concurrency_single;

		SpQueueSamples<		mult,		mult		>::samples(i_ostream);
		SpQueueSamples<		single,		mult		>::samples(i_ostream);
		SpQueueSamples<		mult,		single		>::samples(i_ostream);
		SpQueueSamples<		single,		single		>::samples(i_ostream);
	}

} // namespace density_tests

#if defined(_MSC_VER) && defined(NDEBUG)		
	#pragma warning(pop)
#endif
