
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <vector>
#include <atomic>
#include <thread>
#include <ostream>
#include <string>
#include <chrono>
#include <density/density_common.h>
#include "density_test_common.h"
#include "progress.h"
#include "line_updater_stream_adapter.h"
#include "histogram.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

namespace density_tests
{
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
		
		/* We use an array of atomic<int8_t> for the test, initialized with zeroes.
			When a consumer pushes the integer i on the queue, it increments 
			m_id_map[i % id_map_size]. When a producer consumes i, it decrements 
			m_id_map[i % id_map_size]. At the end of the test, when the queue is
			empty, every m_id_map[i] must be zero. */
		std::atomic<int8_t> * m_id_map;
		static constexpr uint32_t id_map_size = 512 * 1024;
		
		/* This is the queue being tested. */
		QUEUE m_queue;
		
		HeterLoadUnloadTest()
		{
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

		/* Runs a test session. This function is not thread safe.
		
		*/
		void run(uint32_t i_thread_count, uint32_t i_produces_per_thread, std::ostream & i_ostream)
		{
			i_ostream << "starting queue load unload test with " << i_thread_count << " threads and ";
			i_ostream << i_produces_per_thread << " puts per thread";
			i_ostream << "\nheterogeneous_queue: " << truncated_type_name<QUEUE>();
			i_ostream << "\ncommon_type: " << truncated_type_name<typename QUEUE::common_type>();
			i_ostream << "\nruntime_type: " << truncated_type_name<typename QUEUE::runtime_type>();
			i_ostream << "\nallocator_type: " << truncated_type_name<typename QUEUE::allocator_type>();
			i_ostream << "\npage_alignment: " << QUEUE::allocator_type::page_alignment;
			i_ostream << "\npage_size: " << QUEUE::allocator_type::page_size;
			i_ostream << std::endl;

			// start the threads
			std::vector<ThreadEntry> threads(i_thread_count);
			uint32_t thread_index = 0;
			for (auto & thread_entry : threads)
			{
				auto const start_id64 = (static_cast<uint64_t>(thread_index) * id_map_size) / i_thread_count;
				auto const start_id = static_cast<uint32_t>(start_id64 % id_map_size);
				auto const end_id = start_id + i_produces_per_thread;
				thread_entry.m_thread = std::thread([this, &thread_entry, start_id, end_id] {
					thread_run(thread_entry, start_id, end_id); });
				thread_index++;
			}

			{
				Progress progress(i_thread_count * i_produces_per_thread);
				LineUpdaterStreamAdapter line_updater(i_ostream);
				for (;;)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(600));

					Stats stats;
					for (auto & thread_entry : threads)
					{
						stats += thread_entry.m_stats;
					}

					auto const produced = stats.m_produced.load();
					progress.set_progress(produced);
					line_updater << progress << std::endl;

					if (produced >= progress.target_count())
						break;
				}

				for (auto & thread_entry : threads)
				{
					thread_entry.m_thread.join();
				}
			}

			size_t consumed_by_main = 0;
			while (consume_one())
				consumed_by_main++;
			i_ostream << consumed_by_main << " remaining items were consumed by the main thread" << std::endl;

			final_check();

			histogram<size_t> produced_hist("produced by i-th thread"), consumed_hist("consumed by i-th thread");
			for (const auto & thread_entry : threads)
			{
				produced_hist << thread_entry.m_stats.m_produced.load();
				consumed_hist << thread_entry.m_stats.m_consumed.load();
			}

			i_ostream << produced_hist;
			i_ostream << consumed_hist;
			i_ostream << "--------------------------------------------\n" << std::endl;
		}

	private:

		void produce_one(uint32_t const i_id)
		{
			m_queue.push(i_id);
			m_id_map[i_id].fetch_add(1, std::memory_order_relaxed);			
		}

		bool consume_one()
		{
			if (auto consume = m_queue.start_consume())
			{
				auto const id = consume.element<uint32_t>();
				consume.commit();
				m_id_map[id].fetch_sub(1, std::memory_order_relaxed);
				return true;
			}
			else
			{
				return false;
			}
		}

		struct Stats
		{
			std::atomic<size_t> m_consumed{ 0 };
			std::atomic<size_t> m_produced{ 0 };

			Stats & operator += (const Stats & i_source)
			{
				m_consumed += i_source.m_consumed.load(std::memory_order_relaxed);
				m_produced += i_source.m_produced.load(std::memory_order_relaxed);
				return *this;
			}
		};

		struct alignas(density::concurrent_alignment) ThreadEntry
		{
			Stats m_stats;
			std::thread m_thread;
		};

		void thread_run(ThreadEntry & i_entry, 
			uint32_t const i_start_id, 
			uint32_t const i_end_id)
		{
			DENSITY_TEST_ASSERT(i_start_id < i_end_id);
			size_t temp_produced = 0, temp_consumed = 0;

			uint32_t curr_id = i_start_id;
			unsigned iteration = 0;
			while (curr_id < i_end_id)
			{
				iteration++;
				if ((iteration & 1) == 0)
				{
					// push(curr_id)
					produce_one(curr_id % id_map_size);
					curr_id++;
					temp_produced++;

					// periodically we update the stats
					if ((curr_id % (1024 * 16)) == 0)
					{
						i_entry.m_stats.m_produced.fetch_add(temp_produced, std::memory_order_relaxed);
						i_entry.m_stats.m_consumed.fetch_add(temp_consumed, std::memory_order_relaxed);
						temp_produced = 0;
						temp_consumed = 0;
					}
				}
				else
				{
					// consume
					if (consume_one())
					{
						temp_consumed++;
					}
				}
			}
			
			i_entry.m_stats.m_produced.fetch_add(temp_produced);
			i_entry.m_stats.m_consumed.fetch_add(temp_consumed);

			DENSITY_TEST_ASSERT(i_entry.m_stats.m_produced.load(std::memory_order_relaxed) == i_end_id - i_start_id );
		}

		void final_check()
		{
			for (size_t i = 0; i < id_map_size; i++)
			{
				auto const counter = m_id_map[i].load(std::memory_order_relaxed);
				DENSITY_TEST_ASSERT(counter == 0);
			}
		}
	};

	template <typename QUEUE>
		void queue_load_unload_test(uint32_t i_thread_count, uint32_t i_produces_per_thread, std::ostream & i_ostream)
	{
		HeterLoadUnloadTest<QUEUE> test;
		test.run(i_thread_count, i_produces_per_thread, i_ostream);
	}

} // namespace density_tests

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
