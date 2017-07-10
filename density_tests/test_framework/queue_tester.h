#pragma once
#include "density_test_common.h"
#include "progress.h"
#include "easy_random.h"
#include "line_updater_stream_adapter.h"
#include "test_objects.h"
#include "histogram.h"
#include <density/density_common.h> // for density::concurrent_alignment
#include <vector>
#include <thread>
#include <unordered_map>
#include <ostream>
#include <numeric>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

namespace density_tests
{
	template <typename QUEUE>
		class QueueTester
	{
		using PutTestCase = void (*)(QUEUE & i_queue, EasyRandom &);
		using ConsumeTestCase = void (*)(typename QUEUE::consume_operation & i_queue);

	public:

		QueueTester(std::ostream & i_output, size_t m_thread_count)
			: m_output(i_output), m_thread_count(m_thread_count)
		{

		}

		template <typename PUT_CASE>
			void add_put_case(PUT_CASE)
		{
			using ElementType = typename PUT_CASE::ElementType;
			m_put_types.insert(std::make_pair(QUEUE::runtime_type::template make<ElementType>(), m_put_cases.size()));
			m_put_cases.push_back(&PUT_CASE::template put<QUEUE>);
			m_consume_cases.push_back(&PUT_CASE::template consume<typename QUEUE::consume_operation>);
		}

		/** Runs a test session. This function does not alter the object. */
		void run(QueueTesterFlags i_flags,
			EasyRandom & i_random,
			size_t i_target_put_count) const
		{
			bool const with_exceptions = (i_flags & QueueTesterFlags::eTestExceptions) != QueueTesterFlags::eNone;

			m_output << "starting queue generic test with " << m_thread_count << " threads and ";
			m_output << i_target_put_count << " total puts";
			m_output << "\nheterogeneous_queue: " << truncated_type_name<QUEUE>();
			m_output << "\ncommon_type: " << truncated_type_name<typename QUEUE::common_type>();
			m_output << "\nruntime_type: " << truncated_type_name<typename QUEUE::runtime_type>();
			m_output << "\nallocator_type: " << truncated_type_name<typename QUEUE::allocator_type>();
			m_output << "\npage_alignment: " << QUEUE::allocator_type::page_alignment;
			m_output << "\npage_size: " << QUEUE::allocator_type::page_size;
			m_output << "\nwith_exceptions: " << with_exceptions;
			m_output << std::endl;

			InstanceCounted::ScopedLeakCheck objecty_leak_check;
			run_impl(i_flags, i_random, i_target_put_count);

			m_output << "--------------------------------------------\n" << std::endl;
		}

	private:

		/**< An instance of this struct is used by a single thread to keep the counters associated 
			of a single element type. Since these data a re thread-specific, counters can even be negative.
			When the test is completed, the sum of the counters of all the threads must be coherent (that
			is m_existing must be 0 and m_spawned must be the total count for the given type).*/
		struct PutTypeCounters
		{
			int64_t m_existing{0}; /**< How many elements of this type currently exist in the queue. */
			int64_t m_spawned{0}; /**< How many elements of this type have been put in the queue. */
		};

		struct State
		{
			std::vector<PutTypeCounters> m_put_counters;
			int64_t m_exceptions_during_puts{0};
			int64_t m_exceptions_during_consumes{0};

			State(size_t i_put_type_count)
				: m_put_counters(i_put_type_count)
			{
			}

			State & operator += (const State & i_source)
			{
				auto const count = i_source.m_put_counters.size();
				for (size_t i = 0; i < count; i++)
				{
					m_put_counters[i].m_existing += i_source.m_put_counters[i].m_existing;
					m_put_counters[i].m_spawned += i_source.m_put_counters[i].m_spawned;
				}
				m_exceptions_during_puts += i_source.m_exceptions_during_puts;
				m_exceptions_during_consumes += i_source.m_exceptions_during_consumes;
				return *this;
			}
		};

		void put_one(QueueTesterFlags i_flags, QUEUE & i_queue, State & i_state, EasyRandom & i_random) const
		{
			/* pick a random type (outside the exception loop) to have a deterministic and exhaustive 
				exception test at least in isolation (in singlethread tests). */
			const auto put_index = i_random.get_int<size_t>(m_put_cases.size() - 1);

			auto put_func = [&] {

				(*m_put_cases[put_index])(i_queue, i_random);

				// done! From now on no exception can occur
				auto & counters = i_state.m_put_counters[put_index];
				counters.m_existing++;
				counters.m_spawned++;
			};

			if ((i_flags & QueueTesterFlags::eTestExceptions) == QueueTesterFlags::eTestExceptions)
			{
				i_state.m_exceptions_during_puts += run_exception_test(put_func);
			}
			else
			{
				put_func();
			}
		}

		bool try_consume_one(QueueTesterFlags i_flags, QUEUE & i_queue, State & i_state) const
		{
			bool result = false;

			auto consume_func = [&] {
				if (auto consume = i_queue.try_start_consume())
				{
					// find the type to get the index
					auto const type = consume.complete_type();
					auto const type_it = m_put_types.find(type);
					DENSITY_TEST_ASSERT(type_it != m_put_types.end());

					// call the user-provided callback
					auto const type_index = type_it->second;
					(*m_consume_cases[type_index])(consume);

					// in case of exception the element is not consumed (because we don't call commit)
					exception_checkpoint();
					
					// done! From now on no exception can occur
					consume.commit();
					i_state.m_put_counters[type_index].m_existing--;
					result = true;
				}	
			};

			if ((i_flags & QueueTesterFlags::eTestExceptions) == QueueTesterFlags::eTestExceptions)
			{
				i_state.m_exceptions_during_consumes += run_exception_test(consume_func);
			}
			else
			{
				consume_func();
			}

			return result;
		}

		void thread_run(QueueTesterFlags i_flags, QUEUE & i_queue, State & i_state, 
			EasyRandom & i_random, size_t i_target_put_count, Progress & i_progress) const
		{
			size_t put_done = 0;
			size_t consumes_done = 0;
			size_t consumes_notified = 0;
			
			bool finished = false;
			for (size_t cycles = 0; !finished ; cycles++)
			{
				// decide between put and consume
				if (put_done < i_target_put_count && i_random.get_bool())
				{
					put_one(i_flags, i_queue, i_state, i_random);
					put_done++;
				}
				else if (try_consume_one(i_flags, i_queue, i_state))
				{
					consumes_done++;
				}

				// update the progress periodically
				if ((cycles & 4095) == 0)
				{
					i_progress.add_progress(consumes_done - consumes_notified);
					finished = i_progress.is_complete();
					consumes_notified = consumes_done;
				}
			}

		}

		struct alignas(density::concurrent_alignment) ThreadEntry
		{
			std::thread m_thread;
			State m_state;
			EasyRandom m_random;

			ThreadEntry(size_t i_put_type_count)
				: m_state(i_put_type_count) {}
		};

		void run_impl(QueueTesterFlags i_flags, EasyRandom & i_random, size_t i_target_put_count) const
		{
			bool const with_exceptions = (i_flags & QueueTesterFlags::eTestExceptions) != QueueTesterFlags::eNone;

			QUEUE queue;

			/* prepare the array of threads. The initialization of the random generator
				may take some time, so we do it before starting the threads. */
			std::vector<ThreadEntry> threads;
			threads.reserve(m_thread_count);
			for (size_t thread_index = 0; thread_index < m_thread_count; thread_index++)
			{
				threads.emplace_back(m_put_cases.size());

				auto & entry = threads[thread_index];
				entry.m_random = i_random.fork();
			}

			/** Run the threads. The object 'progress' is updated periodically by the threads. */
			Progress progress(i_target_put_count);
			for (size_t thread_index = 0; thread_index < m_thread_count; thread_index++)
			{
				auto const thread_put_count = i_target_put_count / m_thread_count + 
					( (thread_index > i_target_put_count % m_thread_count) ? 1 : 0);

				auto & entry = threads[thread_index];
				entry.m_thread = std::thread([&] {
					thread_run(i_flags, queue, entry.m_state, entry.m_random, thread_put_count, progress); });
			}

			// wait for the test to be completed
			{
				LineUpdaterStreamAdapter line(m_output);
				bool complete = false;
				while(!complete)
				{
					line << progress << std::endl;
					complete = progress.is_complete();
					if (!complete)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(200));
					}
				}
			}

			for (size_t thread_index = 0; thread_index < m_thread_count; thread_index++)
			{
				threads[thread_index].m_thread.join();
			}

			histogram<size_t> histogram_spawned("spawned by i-th thread");
			histogram<size_t> histogram_except_puts("exceptions_during_puts");
			histogram<size_t> histogram_except_cons("exceptions_during_consumes");

			State final_state(m_put_cases.size());
			for (size_t thread_index = 0; thread_index < m_thread_count; thread_index++)
			{
				auto const & thread_state = threads[thread_index].m_state;
				final_state += thread_state;

				auto const spawned = std::accumulate(
					thread_state.m_put_counters.begin(), 
					thread_state.m_put_counters.end(),
					static_cast<size_t>(0),
					[](size_t i_sum, const PutTypeCounters & i_counter) {
						return i_sum + i_counter.m_spawned; });
				histogram_spawned << spawned;

				if (with_exceptions)
				{
					histogram_except_puts << thread_state.m_exceptions_during_puts;
					histogram_except_cons << thread_state.m_exceptions_during_consumes;
				}
			}

			for (auto const & counter : final_state.m_put_counters)
			{
				DENSITY_TEST_ASSERT(counter.m_existing == 0);
			}

			m_output << histogram_spawned;
			if (with_exceptions)
			{
				m_output << histogram_except_puts;
				m_output << histogram_except_cons;
			}
		}

	private:
		std::ostream & m_output;
		std::vector<PutTestCase> m_put_cases;
		std::vector<ConsumeTestCase> m_consume_cases;
		std::unordered_map<typename QUEUE::runtime_type, size_t> m_put_types;
		size_t const m_thread_count;
	};

} // namespace density_tests

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
	