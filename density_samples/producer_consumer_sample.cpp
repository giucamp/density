
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/small_function_queue.h"
#include "../density/function_queue.h"
#include "../density/lifo.h"
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional> // for std::bind
#include <iostream>
#include <atomic>
#ifdef __GNUC__
    #ifdef __MINGW32__
        /* MinGw does not have support for C++11 threading classes. Using the implementation provided by
            https://github.com/meganz/mingw-std-threads (it is not distributed with this project) */
        #include <mingw.thread.h>
        #include <mingw.mutex.h>
        #include <mingw.condition_variable.h>
    #endif // __MINGW32__
#endif // __GNUC__

namespace producer_consumer_sample
{
	using namespace std;
	using namespace density;

	// This class spawns n threads to process generic commands
	class WorkerPool
	{
	public:

		WorkerPool(size_t i_thread_count)
		{
			for (size_t thread_index = 0; thread_index < i_thread_count; thread_index++)
			{
				m_worker_threads.emplace_back(bind(&WorkerPool::procedure, this, thread_index));
			}
		}

		WorkerPool(const WorkerPool&) = delete;
		WorkerPool & operator = (const WorkerPool&) = delete;

		~WorkerPool()
		{
			{
				lock_guard<mutex> lock(m_mutex);
				m_termination_requested = true;
			}
			m_condition_variable.notify_all();
			for (auto & thread : m_worker_threads)
			{
				thread.join();
			}
		}

		// adds a command. The parameter must be a callable with signature: void (size_t)
		template <typename FUNC>
			void push_command(FUNC && i_func)
		{
			lock_guard<mutex> lock(m_mutex);
			m_commands.push(i_func);
			m_condition_variable.notify_one();
		}

	private:

		void procedure(size_t i_thread_index)
		{
			// this buffer is used to store temporary the command to be executed
			lifo_buffer<> buffer;

			function_queue<void(size_t)>::underlying_queue::runtime_type function_type;
			for (;;)
			{
				{
					unique_lock<mutex> lock(m_mutex);
					m_condition_variable.wait(lock, [this] { return !m_commands.empty() || m_termination_requested; });

					// exit only when the queue is empty
					if (m_termination_requested && m_commands.empty())
					{
						break;
					}

					// move construct to buffer the first command of the queue
					auto command_it = m_commands.begin();
					function_type = command_it.complete_type();
					buffer.resize(function_type.size(), function_type.alignment());
					function_type.move_construct(buffer.data(), command_it.element());

					// remove the moved command from the queue
					m_commands.pop();
				}

				// execute and destroy the command. Note: this code is not exception safe
				auto function = function_type.template get_feature<typename type_features::invoke<void(size_t)>>();
				function(buffer.data(), i_thread_index);
				function_type.destroy(buffer.data());
			}
		}

	private:
		vector<thread> m_worker_threads;
		mutex m_mutex;
		function_queue<void(size_t)> m_commands;
		condition_variable m_condition_variable;
		bool m_termination_requested = false;
	};

	// this class prints (on an parallel thread) what threads are doing every 0.5 seconds
	class ThreadState
	{
	public:
		ThreadState(size_t i_thread_count)
			: m_doing(i_thread_count, -1), m_terminate(false)
		{
			m_printer_thread = thread(bind(&ThreadState::procedure, this));
		}

		ThreadState(const ThreadState&) = delete;

		ThreadState & operator = (const ThreadState&) = delete;

		void notify_whats_doing(size_t i_thread_index, int i_operation)
		{
			unique_lock<mutex> lock(m_mutex);
			m_doing[i_thread_index] = i_operation;
		}

		~ThreadState()
		{
			m_terminate.store(true);
			m_printer_thread.join();
		}

	private:
		void procedure()
		{
			while (!m_terminate.load())
			{
				// with for 0.5 seconds
				this_thread::sleep_for(chrono::milliseconds(500));

				{
					// print the state of the threads
					unique_lock<mutex> lock(m_mutex);
					for (size_t thread_index = 0; thread_index < m_doing.size(); thread_index++)
						cout << m_doing[thread_index] << "    ";
					cout << endl;
				}
			}
		}

	private:
		vector<int> m_doing;
		mutex m_mutex;
		atomic<bool> m_terminate;
		thread m_printer_thread;
	};

	void run()
	{
		const size_t thread_count = 3;
		ThreadState state(thread_count);
		WorkerPool producer_consumer(thread_count);
		for (int i = 1; i < 10; i++)
		{
			producer_consumer.push_command([&state, i](size_t i_thread_index) {
				state.notify_whats_doing(i_thread_index, i );
				this_thread::sleep_for(chrono::seconds(i));
				state.notify_whats_doing(i_thread_index, -1);
			});
		}
	}
}

