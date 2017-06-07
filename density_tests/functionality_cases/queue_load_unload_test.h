
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <algorithm>
#include <iostream>
#include <density/density_common.h>
#include <testity/testity_common.h>


#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

namespace density_tests
{
	struct LoadUnloadTestOptions
	{
		int m_produce_probability_percent = 50;
		int8_t m_lap_count = 8;
		uint64_t m_start_consume_after = 0;
	};

	/** This class template performs a massive load\unload test on an heterogeneous queue.
		At every tick a push or a pop is executed on the queue, depending on a random value.
		The test detects if a pushed value is lost, or if a popped value was not paired by a push.
		The user creates an instance of HeterLoadUnloadTest, and then calls tick
		If the queue is concurrent, tick can be called concurrently from many threads.
	*/
	template <typename QUEUE>
		class HeterLoadUnloadTest
	{
	public:
		
		using id_t = uint64_t;
		static constexpr id_t id_map_size = 512 * 1024;

		HeterLoadUnloadTest(const LoadUnloadTestOptions & i_options)
			: m_end_id(id_map_size * static_cast<id_t>(i_options.m_lap_count)), m_options(i_options)
		{
			TESTITY_ASSERT(i_options.m_lap_count >= 0);
			TESTITY_ASSERT(i_options.m_produce_probability_percent >= 0 &&
				i_options.m_produce_probability_percent <= 100 );

			m_id_map = new std::atomic<int8_t>[id_map_size];
			for (size_t i = 0; i < id_map_size; i++)
				std::atomic_init(&m_id_map[i], 0);
		}

		// copy not allowed
		HeterLoadUnloadTest(const HeterLoadUnloadTest &) = delete;
		HeterLoadUnloadTest & operator = (const HeterLoadUnloadTest &) = delete;

		~HeterLoadUnloadTest()
		{
			delete[] m_id_map;
		}

		enum State
		{
			state_testing,
			state_finished,
		};

		/** Execute a step of the testing.
			@return true if the test session is not over, false otherwise. */
		State tick(std::mt19937 & i_random, bool i_can_produce, bool i_can_consume)
		{
			if (m_report_rendez_vous_call.load(std::memory_order_relaxed))
			{
				std::unique_lock<std::mutex> lock(m_report_rendez_mutex);

				m_report_rendez_vous_thread_count++;

				m_report_rendez_condition.notify_all();

				while (m_report_rendez_vous_call.load())
				{
					m_report_rendez_condition.wait(lock);
				}
			}

			bool produce = false;
			if (m_next_id.load(std::memory_order_relaxed) >= m_options.m_start_consume_after)
			{
				if (i_can_produce && i_can_consume)
				{
					if (!m_finished_producing.load(std::memory_order_relaxed))
						produce = std::uniform_int_distribution<int32_t>(0, 100)(i_random) < m_options.m_produce_probability_percent;
				}
				else
				{
					TESTITY_ASSERT(i_can_produce || i_can_consume);
					produce = i_can_produce;
				}
			}
			else
			{
				produce = true;
			}

			if (produce)
			{
				produce_one(i_random);
				return state_testing;
			}
			else
			{
				return consume_one();
			}
		}

		void final_check()
		{
			TESTITY_ASSERT(m_consume_count.load(std::memory_order_relaxed) == m_end_id);
			for (size_t i = 0; i < id_map_size; i++)
			{
				auto const counter = m_id_map[i].load(std::memory_order_relaxed);
				TESTITY_ASSERT(counter == 0);
			}
		}

		size_t produces_count() const { return m_next_id.load(std::memory_order_relaxed); };

		void print_report(std::ostream & i_stream, size_t i_thread_count)
		{
			m_report_rendez_vous_call.store(true);

			std::unique_lock<std::mutex> lock(m_report_rendez_mutex);

			m_report_rendez_vous_thread_count++;

			m_report_rendez_condition.notify_all();

			while (m_report_rendez_vous_thread_count != i_thread_count)
			{
				m_report_rendez_condition.wait(lock);
			}

			m_report_rendez_vous_thread_count = 0;

			m_queue.report(i_stream);

			m_report_rendez_vous_call.store(false);
		}

	private:

		std::atomic<int8_t> & id_counter(id_t i_id)
		{
			return m_id_map[i_id % id_map_size];
		}

		void produce_one(std::mt19937 &)
		{
			auto const id = m_next_id.fetch_add(1, std::memory_order_relaxed);
			if (id < m_end_id)
			{
				auto const prev_counter = id_counter(id).fetch_add(1, std::memory_order_relaxed);
				TESTITY_ASSERT(prev_counter >= 0 && prev_counter < m_options.m_lap_count);
				m_queue.push(id);
			}
			else
			{
				m_finished_producing.store(true, std::memory_order_relaxed);
			}
		}

		State consume_one()
		{
			auto consume_operation = m_queue.start_consume();
			if (consume_operation)
			{
				auto & type = consume_operation.complete_type();
				TESTITY_ASSERT(type == density::runtime_type<>::make<id_t>());
				auto const id = consume_operation.element<id_t>();

				auto const prev_counter = id_counter(id).fetch_sub(1, std::memory_order_relaxed);
				TESTITY_ASSERT(prev_counter > 0 && prev_counter <= m_options.m_lap_count);

				m_consume_count.fetch_add(1, std::memory_order_relaxed);

				consume_operation.commit();
				return state_testing;
			}
			else
			{
				if (m_finished_producing.load(std::memory_order_relaxed))
				{
					//final_check();
					return state_finished;
				}
				return state_testing;
			}
		}

	private:
		QUEUE m_queue;
		std::atomic<int8_t> * m_id_map;
		id_t const m_end_id;
		LoadUnloadTestOptions const m_options;
		std::atomic<id_t> m_next_id{0};
		std::atomic<id_t> m_consume_count{0};
		std::atomic<bool> m_finished_producing{false};

		std::atomic<bool> m_report_rendez_vous_call{false};
		size_t m_report_rendez_vous_thread_count{0};
		std::condition_variable m_report_rendez_condition;
		std::mutex m_report_rendez_mutex;
	};

	template <typename QUEUE>
		void run_queue_integrity_test(
			size_t i_producer_count, 
			size_t i_consumer_count, 
			const LoadUnloadTestOptions & i_options, 
			size_t i_print_report_produces_period,
			uint32_t i_rand_seed = 0)
	{
		auto random = i_rand_seed == 0 ? std::mt19937{std::random_device()()} : std::mt19937(i_rand_seed);

		struct alignas(density::concurrent_alignment) ThreadEntry
		{
			std::thread m_thread;
			std::mt19937 m_random;
			bool m_can_produce;
			bool m_can_consume;
			size_t m_print_report_produces_period;
		};

		TESTITY_ASSERT(i_producer_count >= 1 && i_consumer_count >= 1);
		size_t const thread_count = std::max(i_producer_count, i_consumer_count);

		HeterLoadUnloadTest<QUEUE> test(i_options);

		std::vector<ThreadEntry> threads(thread_count);
		for (size_t thread_index = 0; thread_index < thread_count; thread_index++)
		{
			auto & entry = threads[thread_index];
			entry.m_can_produce = thread_index + 1 <= i_producer_count;
			entry.m_can_consume = thread_index + 1 <= i_consumer_count;
			entry.m_print_report_produces_period = thread_index == 0 ? i_print_report_produces_period : 0;
			entry.m_random = std::mt19937(random());
			entry.m_thread = std::thread([&test, &entry, thread_count] {				
				while (test.tick(entry.m_random, entry.m_can_produce, entry.m_can_consume) != HeterLoadUnloadTest<QUEUE>::state_finished)
				{
					if (entry.m_print_report_produces_period > 0 && (test.produces_count() % entry.m_print_report_produces_period) == 0)
					{
						test.print_report(std::cout, thread_count);
						std::cout.flush();
					}
				}
			});
		}

		for (auto & thread : threads)
		{
			thread.m_thread.join();
		}

		test.final_check();
	}

} // namespace density_tests

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
