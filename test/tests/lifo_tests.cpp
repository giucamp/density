
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
#include "../test_framework/easy_random.h"
#include "../test_framework/progress.h"
#include "../test_framework/test_objects.h"
#include "../test_framework/exception_tests.h"
#include "../test_framework/threading_extensions.h"
#include <density/lifo.h>
#include <random>
#include <vector>
#include <algorithm>
#include <cstring>
#include <thread>
#include <sstream>
#include <limits>

namespace density_tests
{
    void lifo_test_1(std::ostream & i_output, std::mt19937 & i_random)
    {
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

    class ILifoTestItem
    {
    public:
        virtual void check() const = 0;
        virtual bool resize(std::mt19937 & i_random) = 0;
        virtual ~ILifoTestItem() = default;
    };
    
    template <typename TYPE>
        class LifoTestArray : public ILifoTestItem
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

        bool resize(std::mt19937 & /*i_random*/) override
        { 
            return false;
        }

        ~LifoTestArray()
        {
            check();
        }

    private:
        const density::lifo_array<TYPE> & m_array;
        std::vector<TYPE> m_vector;
    };

    template <typename CONTENT_GENERATOR>
        class LifoTestBuffer : public ILifoTestItem
    {
    public:

        LifoTestBuffer(const CONTENT_GENERATOR & i_content_generator)
            : m_content_generator(i_content_generator)
        {
        }

        LifoTestBuffer(const CONTENT_GENERATOR & i_content_generator, size_t i_size)
            : m_content_generator(i_content_generator), m_buffer(i_size)
        {
            auto const buffer_bytes = static_cast<unsigned char*>( m_buffer.data() ); 
            std::generate(buffer_bytes, buffer_bytes + i_size, m_content_generator);
            
            m_backup.insert(m_backup.end(),
                static_cast<unsigned char*>(m_buffer.data()),
                static_cast<unsigned char*>(m_buffer.data()) + m_buffer.size() );
        }

        void check() const override
        {
            DENSITY_TEST_ASSERT(m_buffer.size() == m_backup.size());
            DENSITY_TEST_ASSERT(std::memcmp(m_buffer.data(), m_backup.data(), m_backup.size()) == 0);
        }

        bool resize(std::mt19937 & i_random) override
        {
            auto const old_size = m_buffer.size();
            auto const new_size = std::uniform_int_distribution<size_t>(0, old_size * 2 + 30)(i_random);

            // resize the buffer first, because it may throw test exceptions, and in this case we must not update m_backup
            m_buffer.resize(new_size);
            m_backup.resize(new_size);

            if (old_size < new_size)
            {
                // generate new content
                auto const buffer_bytes = static_cast<unsigned char*>( m_buffer.data() ); 
                std::generate(buffer_bytes + old_size, buffer_bytes + new_size, m_content_generator);
                memcpy(m_backup.data() + old_size, buffer_bytes + old_size, new_size - old_size);
            }

            return true;
        }

        ~LifoTestBuffer()
        {
            check();
        }

    private:
        CONTENT_GENERATOR m_content_generator;
        density::lifo_buffer m_buffer;
        std::vector<unsigned char> m_backup;
    };

    struct RecursiveLifoTests
    {
        static std::unique_ptr<ILifoTestItem> buffer_test(std::mt19937 & i_random)
        {
            auto content_generator = [&i_random] { 
                auto const result = std::uniform_int_distribution<unsigned>(0, std::numeric_limits<unsigned char>::max())(i_random);
                return static_cast<unsigned char>(result);
            };

            auto const size = std::uniform_int_distribution<size_t>(0, 0xFFFFF)(i_random);

            return std::unique_ptr<ILifoTestItem>(new LifoTestBuffer<decltype(content_generator)>(content_generator, size));
        }

        static std::unique_ptr<ILifoTestItem> empty_buffer_test(std::mt19937 & i_random)
        {
            auto content_generator = [&i_random] { 
                auto const result = std::uniform_int_distribution<unsigned>(0, std::numeric_limits<unsigned char>::max())(i_random);
                return static_cast<unsigned char>(result);
            };

            return std::unique_ptr<ILifoTestItem>(new LifoTestBuffer<decltype(content_generator)>(content_generator));
        }

        static void recursive_test(std::mt19937 & i_random, size_t i_residual_depth, size_t i_residual_fork_depth)
        {
            // table of ILifoTestItem factory functions
            using Func = std::unique_ptr<ILifoTestItem>(*)(std::mt19937 & i_random);
            static constexpr Func tests[] = { &RecursiveLifoTests::buffer_test, &RecursiveLifoTests::empty_buffer_test };

            // pick a random factory and create a test (in the automatic storage)
            auto const random_index = std::uniform_int_distribution<size_t>(0, sizeof(tests) / sizeof(tests[0]) - 1)(i_random);
            auto const test = tests[random_index](i_random);

            const auto iter_count = i_residual_fork_depth > 0 ? std::uniform_int_distribution<int>(1, 4)(i_random) : 1;
            for (int i = 0; i < iter_count; i++)
            {
                test->check();

                if(i_residual_depth > 0)
                {
                    recursive_test(i_random, i_residual_depth - 1, 
                        i_residual_fork_depth > 0 ? i_residual_fork_depth - 1 : 0);
                }

                test->resize(i_random);

                test->check();
            }
        }
    };

    void lifo_test_2(EasyRandom & i_random, QueueTesterFlags i_flags)
    {
        const auto & std_rand = i_random.underlying_rand();

        auto do_tests = [&std_rand]{            
            auto rand_copy = std_rand;
            InstanceCounted::ScopedLeakCheck objecty_leak_check;
            RecursiveLifoTests::recursive_test(rand_copy, 10 /*=max depth of the tests*/, 3 /*=max fork depth*/);
        };

        if(i_flags && QueueTesterFlags::eTestExceptions)
            run_exception_test(std::function<void()>(do_tests));
        else
            do_tests();
    }

    void lifo_tests(QueueTesterFlags i_flags, std::ostream & i_output, uint32_t i_random_seed)
    {
        PrintScopeDuration duration(i_output, "lifo_tests");

        EasyRandom main_random = i_random_seed == 0 ? EasyRandom() : EasyRandom(i_random_seed);

        auto const num_of_processors = get_num_of_processors();
        bool const reserve_core1_to_main = (i_flags && QueueTesterFlags::eReserveCoreToMainThread) && num_of_processors >= 4;
        uint64_t affinity_mask = std::numeric_limits<uint64_t>::max();
        if (reserve_core1_to_main)
            affinity_mask -= 2;

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
        for (size_t thread_index = 0; thread_index < thread_count; thread_index++)
        {
            auto & thread_entry = threads[thread_index];
            auto & thread_random = thread_entry.m_random;
            thread_entry.m_thread = std::thread([&thread_random, &i_output, thread_index, affinity_mask, i_flags]{

                set_thread_affinity(affinity_mask);
                
                lifo_test_2(thread_random, i_flags);

                std::ostringstream label;
                label << "thread " << thread_index << " has finished\n";
                i_output << label.str();
            });
        }

        // join threads
        for (auto & thread_entry : threads)
        {
            thread_entry.m_thread.join();
        }
    }

} // namespace density_tests

