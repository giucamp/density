
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
#include "../test_framework/easy_random.h"
#include "../test_framework/progress.h"
#include "../test_framework/test_objects.h"
#include "../test_framework/exception_tests.h"
#include <density/lifo.h>
#include <random>
#include <vector>
#include <algorithm>
#include <cstring>
#include <thread>

namespace density_tests
{
    // verify that lifo_allocator can have constant initialization
    constexpr density::lifo_allocator<> constexpr_lifo_allocator;

    void lifo_test_1(std::ostream & i_output, std::mt19937 & i_random)
    {
        (void)constexpr_lifo_allocator;
        InstanceCounted::ScopedLeakCheck objecty_leak_check;

        PrintScopeDuration duration(i_output, "lifo_test_1");

        using namespace density;

        // instance a lifo_allocator
        void_allocator underlying_allocator;
        lifo_allocator<> allocator(underlying_allocator);

        // for a random number of times....
        while (std::uniform_int_distribution<size_t>(0, 10000)(i_random) > 10)
        {
            // allocate a block and fill it with progressive numbers
            auto size = uint_upper_align(std::uniform_int_distribution<size_t>(0, 1000)(i_random), decltype(allocator)::alignment);
            auto block = static_cast<unsigned char*>( allocator.allocate(size) );
            for (size_t index = 0; index < size; index++)
            {
                block[index] = static_cast<unsigned char>(index & 0xFF);
            }

            // reallocate the block with reallocate_preserve, and check the content
            auto new_size = uint_upper_align(std::uniform_int_distribution<size_t>(0, 8000)(i_random), decltype(allocator)::alignment);
            block = static_cast<unsigned char*>(allocator.reallocate(block, size, new_size));
            for (size_t index = 0; index < std::min(size, new_size); index++)
            {
                DENSITY_TEST_ASSERT( block[index] == static_cast<unsigned char>(index & 0xFF) );
            }
            size = new_size;

            // done
            allocator.deallocate(block, size);
        }
    }

    size_t random_alignment(std::mt19937 & i_random)
    {
        size_t log2_max = 0;
        while ((static_cast<size_t>(1) << log2_max) < MaxAlignment)
        {
            log2_max++;
        }
        return static_cast<size_t>(1) << std::uniform_int_distribution<size_t>(0, log2_max * 2)(i_random);
    }

    class LifoTestItem
    {
    public:
        virtual void check() const = 0;
        virtual bool resize(std::mt19937 & /*i_random*/) { return false; }
        virtual ~LifoTestItem() {}
    };
    
    template <typename TYPE>
        class LifoTestArray : public LifoTestItem
    {
    public:
        LifoTestArray(const density::lifo_array<TYPE> & i_array)
            : m_array(i_array)
        {
            DENSITY_TEST_ASSERT( density::address_is_aligned( i_array.data(), alignof(TYPE) ) );
            m_vector.insert(m_vector.end(), i_array.begin(), i_array.end());
        }

        void check() const override
        {
            const size_t size = m_array.size();
            DENSITY_TEST_ASSERT(m_vector.size() == size);
            for(size_t i = 0; i < size; i++)
            {
                DENSITY_TEST_ASSERT( m_vector[i] == m_array[i] );
            }
        }

    private:
        const density::lifo_array<TYPE> & m_array;
        std::vector<TYPE> m_vector;
    };

    class LifoTestBuffer : public LifoTestItem
    {
    public:
        LifoTestBuffer(density::lifo_buffer & i_buffer)
            : m_buffer(i_buffer)
        {
            m_vector.insert(m_vector.end(),
                static_cast<unsigned char*>(m_buffer.data()),
                static_cast<unsigned char*>(m_buffer.data()) + m_buffer.size() );
        }

        void check() const override
        {
            DENSITY_TEST_ASSERT(m_buffer.size() == m_vector.size());
            DENSITY_TEST_ASSERT(std::memcmp(m_buffer.data(), m_vector.data(), m_vector.size())==0);
        }

        virtual bool resize(std::mt19937 & i_random) override
        {
            using namespace density;

            check();
            const auto old_size = m_buffer.size();
            const auto new_size = std::uniform_int_distribution<size_t>(0, 32)(i_random);

            m_buffer.resize(new_size);

            m_vector.resize(new_size);

            if (old_size < new_size)
            {
                std::uniform_int_distribution<unsigned> rnd(0, 100);
                std::generate(static_cast<unsigned char*>(m_buffer.data()) + old_size,
                    static_cast<unsigned char*>(m_buffer.data()) + new_size,
                    [&i_random, &rnd] { return static_cast<unsigned char>(rnd(i_random)); });
                memcpy(m_vector.data() + old_size,
                    static_cast<unsigned char*>(m_buffer.data()) + old_size,
                    new_size - old_size);
            }

            check();
            return true;
        }

    private:
        density::lifo_buffer & m_buffer;
        std::vector<unsigned char> m_vector;
    };

    struct LifoTestContext
    {
    private:
        std::mt19937 & m_random;
        int m_curr_depth = 0;
        int m_max_depth = 0;
        std::vector< std::unique_ptr< LifoTestItem > > m_tests;

    public:

        LifoTestContext(std::mt19937 & i_random, int i_max_depth)
            : m_random(i_random), m_max_depth(i_max_depth)
        {

        }

        std::mt19937 & random() { return m_random; }

        template <typename TYPE>
            void push_test(const density::lifo_array<TYPE> & i_array)
        {
            m_tests.emplace_back( new LifoTestArray<TYPE>(i_array) );
        }

        void push_test(density::lifo_buffer & i_buffer)
        {
            m_tests.emplace_back( new LifoTestBuffer(i_buffer) );
        }

        void pop_test()
        {
            m_tests.pop_back();
        }

        void check() const
        {
            for (const auto & test : m_tests)
            {
                test->check();
            }
        }

        void resize(std::mt19937 & i_random)
        {
            if (!m_tests.empty())
            {
                m_tests.back()->resize(i_random);
            }
        }

        void lifo_test_push_buffer()
        {
            using namespace density;

            std::uniform_int_distribution<unsigned> rnd(0, 100);
            lifo_buffer buffer(std::uniform_int_distribution<size_t>(0, void_allocator::page_size * 2)(m_random));
            DENSITY_TEST_ASSERT(address_is_aligned(buffer.data(), lifo_buffer::alignment));
            std::generate(
                static_cast<unsigned char*>(buffer.data()),
                static_cast<unsigned char*>(buffer.data()) + buffer.size(),
                [this, &rnd] { return static_cast<unsigned char>(rnd(m_random) % 128); });
            push_test(buffer);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_empty_buffer()
        {
            using namespace density;

            lifo_buffer buffer;
            DENSITY_TEST_ASSERT(address_is_aligned(buffer.data(), lifo_buffer::alignment));
            push_test(buffer);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_char()
        {
            using namespace density;

            std::uniform_int_distribution<unsigned> rnd(0, 100);
            lifo_array<unsigned char> arr(std::uniform_int_distribution<size_t>(0, void_allocator::page_size * 2)(m_random));
            std::generate(arr.begin(), arr.end(), [this, &rnd] { return static_cast<unsigned char>(rnd(m_random)); });
            push_test(arr);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_int()
        {
            using namespace density;

            std::uniform_int_distribution<int> rnd(-1000, 1000);
            lifo_array<int> arr(std::uniform_int_distribution<size_t>(0, void_allocator::page_size)(m_random));
            std::generate(arr.begin(), arr.end(), [this, &rnd] { return rnd(m_random); });

            push_test(arr);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_wide_alignment()
        {
            using namespace density;

            union alignas(MaxAlignment * 2) AlignedType
            {
                int m_value;
                max_align_t m_unused[2];
                bool operator == (const AlignedType & i_other) const
                {
                    return m_value == i_other.m_value;
                }
            };

            std::uniform_int_distribution<int> rnd(-1000, 1000);
            lifo_array<AlignedType> arr(std::uniform_int_distribution<size_t>(0, void_allocator::page_size)(m_random));
            std::generate(arr.begin(), arr.end(), [this, &rnd] { return AlignedType{ rnd(m_random) }; });

            push_test(arr);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_double()
        {
            using namespace density;

            std::uniform_real_distribution<double> rnd(-1000., 1000.);
            lifo_array<double> arr(std::uniform_int_distribution<size_t>(0, void_allocator::page_size)(m_random));
            std::generate(arr.begin(), arr.end(), [this, &rnd] { return rnd(m_random); });

            push_test(arr);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push()
        {
            if (m_curr_depth < m_max_depth)
            {
                using Func = void(LifoTestContext::*)();
                Func tests[] = { &LifoTestContext::lifo_test_push_buffer, &LifoTestContext::lifo_test_push_empty_buffer,
                    &LifoTestContext::lifo_test_push_char,
                    &LifoTestContext::lifo_test_push_int, &LifoTestContext::lifo_test_push_double,
                    &LifoTestContext::lifo_test_push_wide_alignment };

                m_curr_depth++;

                const auto iter_count = std::uniform_int_distribution<int>(0, 2)(m_random);
                for (int i = 0; i < iter_count; i++)
                {
                    resize(m_random);

                    const auto random_index = std::uniform_int_distribution<size_t>(0, sizeof(tests) / sizeof(tests[0]) - 1)(m_random);
                    (this->*tests[random_index])();

                    check();

                    resize(m_random);
                }

                m_curr_depth--;
            }
        }

    }; // LifoTestContext

    void lifo_test_2(EasyRandom & i_random)
    {
        const auto & std_rand = i_random.underlying_rand();

        run_exception_test([&std_rand]{
            
            auto rand_copy = std_rand;

            InstanceCounted::ScopedLeakCheck objecty_leak_check;
            LifoTestContext context(rand_copy, 7);
            context.lifo_test_push();
        });

    }

    void lifo_tests(QueueTesterFlags i_flags, std::ostream & i_output, uint32_t i_random_seed)
    {
        PrintScopeDuration duration(i_output, "lifo_tests");

        EasyRandom main_random = i_random_seed == 0 ? EasyRandom() : EasyRandom(i_random_seed);
        
        // create thread entries
        const size_t thread_count = 1;
        struct ThreadEntry
        {
            EasyRandom m_random;
            std::thread m_thread;
            ThreadEntry(EasyRandom & i_main_random)
                : m_random(i_main_random.fork())
            {

            }
        };
        std::vector<ThreadEntry> threads;
        threads.reserve(thread_count);
        for (size_t thread_index = 0; thread_index < thread_count; thread_index++)
        {
            threads.emplace_back(main_random);
        }

        // start threads
        for (auto & thread_entry : threads)
        {
            auto & thread_random = thread_entry.m_random;
            thread_entry.m_thread = std::thread([&thread_random]{
                lifo_test_2(thread_random);
            });
        }

        // join threads
        for (auto & thread_entry : threads)
        {
            thread_entry.m_thread.join();
        }
    }

} // namespace density_tests

