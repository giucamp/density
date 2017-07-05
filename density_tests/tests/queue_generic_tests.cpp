
#include "queue_generic_tests.h"
#include "../test_framework/queue_tester.h"
#include "../test_framework/test_allocators.h"
#include "../test_framework/test_objects.h"
#include "../test_framework/exception_tests.h"
#include <density/heterogeneous_queue.h>
#include <string>

namespace density_tests
{
	struct PutInt
	{
		using ElementType = int;

		template <typename QUEUE>
			static void put(QUEUE & queue)
		{
			//! [queue push example 1]
	queue.push(1);
			//! [queue push example 1]
		}

		template <typename CONSUME>
			static void consume(CONSUME & i_consume)
		{
			DENSITY_TEST_ASSERT(i_consume.element<ElementType>() == 1);			
		}
	};

	struct PutString
	{
		using ElementType = std::string;

		template <typename QUEUE>
			static void put(QUEUE & queue)
		{
			//! [queue push example 1]
				std::string str("hello world!");
	queue.push(str);
			//! [queue push example 1]
		}

		template <typename CONSUME>
			static void consume(CONSUME & i_consume)
		{
			DENSITY_TEST_ASSERT(i_consume.element<ElementType>() == "hello world!");			
		}
	};

	struct PutUInt8
	{
		using ElementType = uint8_t;

		template <typename QUEUE>
			static void put(QUEUE & queue)
		{
		//! [queue push emplace 2]
	queue.emplace<uint8_t>(static_cast<uint8_t>(8));
		//! [queue push emplace 2]
		}

		template <typename CONSUME>
			static void consume(CONSUME & i_consume)
		{
			DENSITY_TEST_ASSERT(i_consume.element<ElementType>() == 8);			
		}
	};
	
	struct PutUInt16
	{
		using ElementType = uint16_t;

		template <typename QUEUE>
			static void put(QUEUE & queue)
		{
		//! [queue push emplace 2]
	auto put = queue.start_emplace<uint16_t>(static_cast<uint16_t>(15));
	put.element() += 1;
	put.commit(); // commit a 16. From now on, the element can be consumed
		//! [queue push emplace 2]
		}

		template <typename CONSUME>
			static void consume(CONSUME & i_consume)
		{
			DENSITY_TEST_ASSERT(i_consume.element<ElementType>() == 16);			
		}
	};

	template <size_t SIZE, size_t ALIGNMENT>
		struct PutTestObject
	{
		using ElementType = TestObject<SIZE, ALIGNMENT>;

		template <typename QUEUE>
			static void put(QUEUE & queue)
		{
			//! [queue push example 1]
	queue.push(ElementType());
			//! [queue push example 1]
		}

		template <typename CONSUME>
			static void consume(CONSUME & /*i_consume*/)
		{
		}
	};

	template <typename QUEUE>
		void single_queue_generic_test(QueueTesterFlags i_flags, std::ostream & i_output, EasyRandom & i_random, size_t i_element_count, size_t i_thread_count)
	{
		QueueTester<QUEUE> tester(i_output, i_thread_count);
		tester.add_put_case(PutInt());
		tester.add_put_case(PutUInt8());
		tester.add_put_case(PutUInt16());
		tester.add_put_case(PutString());
		tester.add_put_case(PutTestObject<128, 8>());
		tester.add_put_case(PutTestObject<256, 128>());
		tester.run(i_flags, i_random, i_element_count);
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

		EasyRandom rand = i_random_seed == 0 ? EasyRandom() : EasyRandom(i_random_seed);
		
		single_queue_generic_test<heterogeneous_queue<>>(
			i_flags, i_output, rand, i_element_count, 1);

		single_queue_generic_test<heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, 1);

		single_queue_generic_test<heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, 1);

		single_queue_generic_test<heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, 1);

		single_queue_generic_test<heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, 1);
	}
}