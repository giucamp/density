
#include "queue_generic_tests.h"
#include "../test_framework/queue_generic_tester.h"
#include "../test_framework/test_allocators.h"
#include "../test_framework/test_objects.h"
#include "../test_framework/exception_tests.h"
#include <density/heterogeneous_queue.h>
#include <density/concurrent_heterogeneous_queue.h>
#include <density/nonblocking_heterogeneous_queue.h>
#include <string>

namespace density_tests
{
	template <typename QUEUE>
		struct PutInt
	{
		using ElementType = int;
		
		static void put(QUEUE & queue, EasyRandom & i_rand)
		{
			if(i_rand.get_bool())
				queue.push(1);
			else
				queue.reentrant_push(1);
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

		static void put(QUEUE & queue, EasyRandom & i_rand)
		{
			std::string str("hello world!");
			if(i_rand.get_bool())
				queue.push(str);
			else
				queue.reentrant_push(str);
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

		static void put(QUEUE & queue, EasyRandom & i_rand)
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

		static void put(QUEUE & queue, EasyRandom &)
		{
			auto put = queue.template start_emplace<uint16_t>(static_cast<uint16_t>(15));
			put.element() += 1;
			exception_checkpoint();
			put.commit(); // commit a 16. From now on, the element can be consumed
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

	template <typename QUEUE, size_t SIZE, size_t ALIGNMENT>
		struct PutTestObject
	{
		using ElementType = TestObject<SIZE, ALIGNMENT>;


		static void put(QUEUE & queue, EasyRandom & i_rand)
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

		static void put(QUEUE & queue, EasyRandom & i_rand)
		{
			auto put = queue.template start_emplace<ElementType>();
			put_impl(put, i_rand);
			put.commit();
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

		static void put(QUEUE & queue, EasyRandom & i_rand)
		{
			uint32_t val = 32;
			if(i_rand.get_bool())
				queue.push(val);
			else
				queue.reentrant_push(val);
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

	/** Runs the generic test on all the queues 
		@param i_flags misc options
		@param i_output output stream to use for the progression and the result
		@param i_random_seed seed to use for the PRG. If != 0, the test is deterministic. If == 0, PRG's
			are seeded with a non-deterministic source (like std::random_device).
		@param i_element_count number of elements to produce and consume in every test
	*/
	void all_queues_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
		uint32_t i_random_seed, size_t i_element_count)
	{
		using namespace density;

		std::vector<size_t> const concurrent_thread_counts{1, 2};
		std::vector<size_t> const nonblocking_thread_counts{1, 2, 3, 7, 32};

		EasyRandom rand = i_random_seed == 0 ? EasyRandom() : EasyRandom(i_random_seed);
		
		// heterogeneous_queue<void, ...>
		single_queue_generic_test<heterogeneous_queue<>>(
			i_flags, i_output, rand, i_element_count, {1});
		single_queue_generic_test<heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, {1});
		single_queue_generic_test<heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, {1});
		single_queue_generic_test<heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, {1});
		single_queue_generic_test<heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, {1});

		// concurrent_heterogeneous_queue<void, ...>
		single_queue_generic_test<concurrent_heterogeneous_queue<>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);
		single_queue_generic_test<concurrent_heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);
		single_queue_generic_test<concurrent_heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);
		single_queue_generic_test<concurrent_heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);
		single_queue_generic_test<concurrent_heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);

		// nonblocking_heterogeneous_queue<void, ..., consistency_model_linearizable>
		single_queue_generic_test<nonblocking_heterogeneous_queue<>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		single_queue_generic_test<nonblocking_heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		single_queue_generic_test<nonblocking_heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		single_queue_generic_test<nonblocking_heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		single_queue_generic_test<nonblocking_heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);

		// nonblocking_heterogeneous_queue<void, ..., consistency_model_relaxed>
		single_queue_generic_test<nonblocking_heterogeneous_queue<void, runtime_type<>, void_allocator,
				concurrent_cardinality_multiple, concurrent_cardinality_multiple, consistency_model_relaxed>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		single_queue_generic_test<nonblocking_heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<>,
				concurrent_cardinality_multiple, concurrent_cardinality_multiple, consistency_model_relaxed>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		single_queue_generic_test<nonblocking_heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<>,
				concurrent_cardinality_multiple, concurrent_cardinality_multiple, consistency_model_relaxed>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		single_queue_generic_test<nonblocking_heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>,
				concurrent_cardinality_multiple, concurrent_cardinality_multiple, consistency_model_relaxed>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		single_queue_generic_test<nonblocking_heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>,
				concurrent_cardinality_multiple, concurrent_cardinality_multiple, consistency_model_relaxed>>(
			i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
	}
}