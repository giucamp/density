
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/density_common.h>
#include <testity/testity_common.h>
#include <memory>
#include <thread>
#include <atomic>

namespace density_tests
{
    using namespace density;

    template <typename QUEUE>
        class ConcProdConsTest
    {
    public:

        ConcProdConsTest(const ConcProdConsTest&) = delete;
        ConcProdConsTest & operator = (const ConcProdConsTest&) = delete;

        /* If i_consumer_count and are both zero, the test runs on a single thread */
        ConcProdConsTest(int64_t i_cell_count)
            : m_cell_count(i_cell_count)
        {
            m_cells = std::make_unique<sync::atomic<uint8_t>[]>(i_cell_count);
        }

		void run(int64_t i_consumer_count, int64_t i_producer_count)
		{
			std::vector<std::thread> threads;
			m_produced.store(0);
			m_consumed.store(0);
			m_next_id_to_produce.store(0);

			for (int64_t i = 0; i < m_cell_count; i++)
				m_cells[i].store(0);

			for (int64_t i = 0; i < i_consumer_count; i++)
			{
				threads.emplace_back([this] { consumer_procedure(); });
			}

			for (int64_t i = 0; i < i_producer_count; i++)
			{
				threads.emplace_back([this] { producer_procedure(); });
			}

			while (!is_over())
			{
				print_stats();

				std::this_thread::sleep_for(std::chrono::seconds(1));
			}

			for (auto & thread : threads)
			{
				thread.join();
			}

			for (int64_t i = 0; i < m_cell_count; i++)
			{
				auto val = m_cells[i].load();
				TESTITY_ASSERT(val == 2);
			}
		}

        bool is_over()
        {
            return m_consumed.load() == m_cell_count;
        }

        void print_stats()
        {
            auto const produced = static_cast<int64_t>( m_produced.load() );
            auto const consumed = static_cast<int64_t>( m_consumed.load() );
            std::cout << "Completed: " << static_cast<int>(0.5 + (m_consumed.load() / static_cast<double>(m_cell_count)) * 100.);
            std::cout << "%, Produced: " << produced;
            std::cout << ", To consume: " << produced - consumed << std::endl;
        }

        ~ConcProdConsTest()
        {

        }

		template <typename ELEMENT_TYPE> 
			void add_test( void (*i_producer)(QUEUE & i_queue, int64_t i_id, std::mt19937 & i_rand),
				int64_t (*i_consumer)(typename QUEUE::common_type * i_element),
				int64_t i_max_id = std::numeric_limits<int64_t>::max() )
		{
			m_tests.push_back(Test{ QUEUE::runtime_type::template make<ELEMENT_TYPE>(), i_max_id, i_producer, i_consumer });
		}

    private:

		struct Test
		{
			typename QUEUE::runtime_type m_type;
			int64_t m_max_id;
			void (*m_producer)(QUEUE & i_queue, int64_t i_id, std::mt19937 & i_rand);
			int64_t (*m_consumer)(typename QUEUE::common_type * i_element);
		};		

        void producer_procedure()
        {
			std::mt19937 rand{ std::random_device()() };
			for (;;)
			{
				auto const element_id = m_next_id_to_produce.fetch_add(1, std::memory_order_relaxed);
				if (element_id >= m_cell_count)
					break;

				auto prev_val = m_cells[element_id].fetch_add(1);
				TESTITY_ASSERT(prev_val == 0);

				bool done;
				do {
					size_t const test_index = std::uniform_int_distribution<size_t>(0, m_tests.size() - 1)(rand);
					auto & test = m_tests[test_index];
					done = element_id <= test.m_max_id;
					if (done)
					{
						(test.m_producer)(m_queue, element_id, rand);
					}
				} while (!done);

				++m_produced;
			}
        }

        void consumer_procedure()
        {
            while (m_consumed.load() < m_cell_count)
            {
                bool res = m_queue.try_consume([this](
						const typename QUEUE::runtime_type & i_complete_type, 
						typename QUEUE::common_type * i_element) {
					for (const auto & test : m_tests)
					{
						if (test.m_type == i_complete_type)
						{
							auto const id = test.m_consumer(i_element);
							auto prev_val = m_cells[id].fetch_add(1);
							TESTITY_ASSERT(prev_val == 1);
							++m_consumed;
						}
					}
                });

                if (!res)
                {
					std::this_thread::yield();
                }
            }
        }

    private:        
        sync::atomic<int64_t> m_produced, m_consumed;
		std::atomic<int64_t> m_next_id_to_produce;
		std::vector<Test> m_tests;
        QUEUE m_queue;
        std::unique_ptr<sync::atomic<uint8_t>[]> m_cells;
        const int64_t m_cell_count;
    };
}
