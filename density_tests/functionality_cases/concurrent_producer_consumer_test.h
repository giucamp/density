
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

    template <typename CONTAINER>
        class ConcProdConsTest
    {
    public:

        ConcProdConsTest(const ConcProdConsTest&) = delete;
        ConcProdConsTest & operator = (const ConcProdConsTest&) = delete;

        /* If i_consumer_count and are both zero, the test runs on a single thread */
        ConcProdConsTest(int64_t i_consumer_count, int64_t i_producer_count, int64_t i_cell_count)
            : m_produced(0), m_consumed(0), m_cell_count(i_cell_count)
        {
            m_cells = std::make_unique<sync::atomic<uint8_t>[]>(i_cell_count);
            for (int64_t i = 0; i < i_cell_count; i++)
                m_cells[i].store(0);

            for (int64_t i = 0; i < i_consumer_count; i++)
            {
                m_threads.emplace_back([=] { consumer_procedure(i_cell_count); });
            }

            for (int64_t i = 0; i < i_producer_count; i++)
            {
                m_threads.emplace_back([=] { producer_procedure(i, i_cell_count, i_producer_count); });
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
            for (auto & thread : m_threads)
            {
                thread.join();
            }

            for (int64_t i = 0; i < m_cell_count; i++)
            {
                auto val = m_cells[i].load();
                TESTITY_ASSERT(val == 2);
            }
        }

    private:

        void producer_procedure(int64_t i_from, int64_t i_to, int64_t i_step)
        {
            for (int64_t i = i_from; i < i_to; i += i_step)
            {
                auto prev_val = m_cells[i].fetch_add(1);
                TESTITY_ASSERT(prev_val == 0);
                m_container.push((int64_t)i);
                ++m_produced;
            }
        }

        void consumer_procedure(int64_t i_cell_count)
        {
            auto const int_type = CONTAINER::runtime_type::template make<int64_t>();

            while (m_consumed.load() < i_cell_count)
            {
                bool res = m_container.try_consume([this, &int_type](const typename CONTAINER::runtime_type & i_complete_type, void * i_element) {
                    TESTITY_ASSERT(i_complete_type == int_type);

                    auto value = *static_cast<int64_t*>(i_element);

                    *static_cast<int64_t*>(i_element) = ~*static_cast<int64_t*>(i_element);

                    auto prev_val = m_cells[value].fetch_add(1);
                    TESTITY_ASSERT(prev_val == 1);

                    ++m_consumed;
                });

                if (!res)
                {
                    //std::cout << "nothing to consume (not a problem)" << std::endl;
                }
            }
        }

    private:
        std::vector<std::thread> m_threads;
        sync::atomic<int64_t> m_produced, m_consumed;
        CONTAINER m_container;
        std::unique_ptr<sync::atomic<uint8_t>[]> m_cells;
        const int64_t m_cell_count;
    };
}
