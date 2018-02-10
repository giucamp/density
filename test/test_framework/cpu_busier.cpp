
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "cpu_busier.h"
#include "threading_extensions.h"
#include "easy_random.h"
#include <atomic>
#include <vector>
#include <thread>
#include <algorithm>
#include <mutex>
#include <condition_variable>

namespace density_tests
{
    namespace
    {
        class wait_counter
        {
        public:
            
            void increment()
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_counter++;
                m_condition.notify_all();
            }

            void wait_to(uint64_t i_count_to_reach)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this, i_count_to_reach]{
                    return m_counter >= i_count_to_reach;
                });
            }

        private:
            uint64_t m_counter = 0;
            std::mutex m_mutex;
            std::condition_variable m_condition;
        };

    }

    struct cpu_busier::Impl
    {
        std::atomic<bool> m_should_exit{ false };

        struct ThreadData
        {
            std::thread m_thread;
        };
        config const m_config;
        std::vector<ThreadData> m_thread_datas;
        wait_counter m_started_threads;

        Impl(config i_config)
            : m_config(i_config)
        {
            uint64_t processor_count = (std::min)(get_num_of_processors(), m_config.m_num_processors);
            m_thread_datas.resize(static_cast<size_t>(processor_count));
            for (uint64_t index = 0; index < processor_count; index++)
            {
                auto & thread_data = m_thread_datas[index];
                thread_data.m_thread = std::thread(&Impl::run_busier, this, index);
            }

            m_started_threads.wait_to(processor_count);
        }

        ~Impl()
        {
            m_should_exit.store(true);
            for (auto & thread : m_thread_datas)
            {
                thread.m_thread.join();
            }
        }

        static std::chrono::microseconds random_duration(EasyRandom & i_rand, 
            std::chrono::microseconds i_min, std::chrono::microseconds i_max) noexcept
        {
            return std::chrono::microseconds{i_rand.get_int<std::chrono::microseconds::rep>(i_min.count(), i_max.count())};
        }

        void run_busier(uint64_t i_cpu_index)
        {
            set_thread_affinity(uint64_t(1) << i_cpu_index);
            set_thread_priority(thread_priority::critical);            
            EasyRandom rand;

            m_started_threads.increment();

            while (!m_should_exit.load())
            {
                auto const wait_duration = random_duration(rand, m_config.m_min_wait, m_config.m_max_wait);
                auto const busy_duration = random_duration(rand, m_config.m_min_busy, m_config.m_max_busy);

                std::this_thread::sleep_for(wait_duration);

                auto const busy_start = std::chrono::high_resolution_clock::now();
                auto const busy_end = busy_start + busy_duration;
                while (std::chrono::high_resolution_clock::now() < busy_end)
                {
                    volatile int a = 0;
                    (void)a;
                }
            }
        }
    };

    cpu_busier::cpu_busier(config i_config)
        : m_impl(new Impl(i_config))
    {

    }

    cpu_busier::~cpu_busier() = default;

} // namespace density_tests
