
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//

#include "../test_framework/density_test_common.h"
#include "../test_framework/easy_random.h"
#include "../test_framework/exception_tests.h"
#include "../test_framework/progress.h"
#include "../test_framework/test_objects.h"
#include "../test_framework/threading_extensions.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <density/lifo.h>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef DENSITY_USER_DATA_STACK
namespace density
{
    namespace user_data_stack
    {
        /* If DENSITY_USER_DATA_STACK is defined, this test program implements this function
                that make a complete check of the data-stack, and updates the internal statistics. */
        void stat_sample();

        void stats_header(std::ostream & i_dest);

        /* Prints the internal statistics. */
        void stats_print(std::ostream & i_dest, const char * i_thread_name);
    } // namespace user_data_stack
} // namespace density
#endif // #ifdef DENSITY_USER_DATA_STACK

namespace density_tests
{
    class ILifoTestItem
    {
      public:
        virtual void check() const                   = 0;
        virtual bool resize(std::mt19937 & i_random) = 0;
        virtual ~ILifoTestItem()                     = default;
    };

    template <typename TYPE> class LifoArrayWithBackup : public ILifoTestItem
    {
      public:
        template <typename CONTENT_GENERATOR>
        LifoArrayWithBackup(size_t i_size, const CONTENT_GENERATOR & i_content_generator)
            : m_array(i_size)
        {
            std::generate(m_array.begin(), m_array.end(), i_content_generator);
            DENSITY_TEST_ASSERT(density::address_is_aligned(m_array.data(), alignof(TYPE)));
            m_backup.insert(m_backup.end(), m_array.begin(), m_array.end());
        }

        void check() const override
        {
            const size_t size = m_array.size();
            DENSITY_TEST_ASSERT(m_backup.size() == size);
            for (size_t i = 0; i < size; i++)
            {
                DENSITY_TEST_ASSERT(m_backup[i] == m_array[i]);
            }
        }

        bool resize(std::mt19937 & /*i_random*/) override { return false; }

        ~LifoArrayWithBackup() { check(); }

      private:
        density::lifo_array<TYPE> m_array;
        std::vector<TYPE>         m_backup;
    };

    template <size_t SIZE, size_t ALIGNMENT> class LifoArrayOfTestObjects : public ILifoTestItem
    {
      public:
        LifoArrayOfTestObjects(size_t i_size) : m_array(i_size) {}

        void check() const override
        {
            for (auto const & obj : m_array)
                obj.check();
        }

        bool resize(std::mt19937 & /*i_random*/) override { return false; }

        ~LifoArrayOfTestObjects() { check(); }

      private:
        density::lifo_array<TestObject<SIZE, ALIGNMENT>> m_array;
    };

    template <typename CONTENT_GENERATOR> class LifoBufferWithBackup : public ILifoTestItem
    {
      public:
        LifoBufferWithBackup(const CONTENT_GENERATOR & i_content_generator)
            : m_content_generator(i_content_generator)
        {
        }

        LifoBufferWithBackup(const CONTENT_GENERATOR & i_content_generator, size_t i_size)
            : m_content_generator(i_content_generator), m_buffer(i_size)
        {
            auto const buffer_bytes = static_cast<unsigned char *>(m_buffer.data());
            std::generate(buffer_bytes, buffer_bytes + i_size, m_content_generator);

            m_backup.insert(
              m_backup.end(),
              static_cast<unsigned char *>(m_buffer.data()),
              static_cast<unsigned char *>(m_buffer.data()) + m_buffer.size());
        }

        void check() const override
        {
            DENSITY_TEST_ASSERT(m_buffer.size() == m_backup.size());
            DENSITY_TEST_ASSERT(
              std::memcmp(m_buffer.data(), m_backup.data(), m_backup.size()) == 0);
        }

        bool resize(std::mt19937 & i_random) override
        {
            auto const old_size = m_buffer.size();
            auto const new_size =
              std::uniform_int_distribution<size_t>(0, old_size * 2 + 30)(i_random);

            // resize the buffer first, because it may throw test exceptions, and in this case we must not update m_backup
            m_buffer.resize(new_size);
            m_backup.resize(new_size);

            if (old_size < new_size)
            {
                // generate new content
                auto const buffer_bytes = static_cast<unsigned char *>(m_buffer.data());
                std::generate(
                  buffer_bytes + old_size, buffer_bytes + new_size, m_content_generator);
                memcpy(m_backup.data() + old_size, buffer_bytes + old_size, new_size - old_size);
            }

            return true;
        }

        ~LifoBufferWithBackup() { check(); }

      private:
        CONTENT_GENERATOR          m_content_generator;
        density::lifo_buffer       m_buffer;
        std::vector<unsigned char> m_backup;
    };

    struct RecursiveLifoTests
    {
        // test lifo_array<int>
        static std::unique_ptr<ILifoTestItem> make_int_array_test(std::mt19937 & i_random)
        {
            auto const size = std::uniform_int_distribution<size_t>(0, 0xFFF)(i_random);
            return std::unique_ptr<ILifoTestItem>(new LifoArrayWithBackup<int>(
              size, [&i_random] { return std::uniform_int_distribution<int>(0, 1000)(i_random); }));
        }

        // test lifo_array<std::string>
        static std::unique_ptr<ILifoTestItem> make_string_array_test(std::mt19937 & i_random)
        {
            auto const size = std::uniform_int_distribution<size_t>(0, 0xFFF)(i_random);
            return std::unique_ptr<ILifoTestItem>(new LifoArrayWithBackup<std::string>(
              size, [&i_random] { return std::string("This is a very long string..."); }));
        }

        // test lifo_array<TestObject<SIZE, ALIGNMENT>>
        template <size_t SIZE, size_t ALIGNMENT>
        static std::unique_ptr<ILifoTestItem> make_test_obj_array_test(std::mt19937 & i_random)
        {
            auto const size = std::uniform_int_distribution<size_t>(0, 3)(i_random);
            return std::unique_ptr<ILifoTestItem>(
              new LifoArrayOfTestObjects<SIZE, ALIGNMENT>(size));
        }

        // test lifo_buffer(size)
        static std::unique_ptr<ILifoTestItem> make_buffer_test(std::mt19937 & i_random)
        {
            auto content_generator = [&i_random] {
                auto const result = std::uniform_int_distribution<unsigned>(
                  0, std::numeric_limits<unsigned char>::max())(i_random);
                return static_cast<unsigned char>(result);
            };

            auto const size = std::uniform_int_distribution<size_t>(0, 0xFFFFF)(i_random);

            return std::unique_ptr<ILifoTestItem>(
              new LifoBufferWithBackup<decltype(content_generator)>(content_generator, size));
        }

        // test lifo_buffer()
        static std::unique_ptr<ILifoTestItem> make_empty_buffer_test(std::mt19937 & i_random)
        {
            auto content_generator = [&i_random] {
                auto const result = std::uniform_int_distribution<unsigned>(
                  0, std::numeric_limits<unsigned char>::max())(i_random);
                return static_cast<unsigned char>(result);
            };

            return std::unique_ptr<ILifoTestItem>(
              new LifoBufferWithBackup<decltype(content_generator)>(content_generator));
        }

        /** \internal Function that for 'i_residual_depth' times calls itself recursively multiple times, or one
            time after reaching the depth 'i_residual_fork_depth'. On every call it creates a lifo tests, that is
            an object implementing ILifoTestItem, optionally resizing it.

            @param i_random random generator to se for the tests
            @param i_residual_depth depth to reach in recursion
            @param i_residual_fork_depth below this depth, this function invokes itself multiple times, forking every time,
                and causing an exponential growth of the tests. From this depth on, recursive_test calls itself only one time. */
        static void recursive_test(
          std::mt19937 & i_random, size_t i_residual_depth, size_t i_residual_fork_depth)
        {
            // tests factory: table of ILifoTestItem factory functions
            using Func = std::unique_ptr<ILifoTestItem> (*)(std::mt19937 & i_random);
            static constexpr Func tests[] = {
              &RecursiveLifoTests::make_buffer_test,
              &RecursiveLifoTests::make_empty_buffer_test,
              &RecursiveLifoTests::make_int_array_test,
              &RecursiveLifoTests::make_string_array_test,
              &RecursiveLifoTests::make_test_obj_array_test<1, 1>,
              &RecursiveLifoTests::make_test_obj_array_test<8, 2>,
              &RecursiveLifoTests::make_test_obj_array_test<16, 1>,
              &RecursiveLifoTests::make_test_obj_array_test<128 * 3, 128>};

            // pick a random factory and create a test (in the automatic storage)
            auto const random_index = std::uniform_int_distribution<size_t>(
              0, sizeof(tests) / sizeof(tests[0]) - 1)(i_random);
            auto const test = tests[random_index](i_random);

            const auto iter_count =
              i_residual_fork_depth > 0 ? std::uniform_int_distribution<int>(1, 3)(i_random) : 1;
            for (int i = 0; i < iter_count; i++)
            {
                test->check();

                if (i_residual_depth > 0)
                {
                    recursive_test(
                      i_random,
                      i_residual_depth - 1,
                      i_residual_fork_depth > 0 ? i_residual_fork_depth - 1 : 0);
                }
                else
                {
#ifdef DENSITY_USER_DATA_STACK
                    density::user_data_stack::stat_sample();
#endif
                }

                test->resize(i_random);

                test->check();
            }
        }
    };

    /** \internal Thread procedure used for lifo tests */
    void lifo_test_thread_proc(
      QueueTesterFlags i_flags, EasyRandom & i_random, size_t i_depth, size_t i_fork_depth)
    {
        const auto & std_rand = i_random.underlying_rand();

        // local function that executes the tests
        auto do_tests = [&std_rand, i_depth, i_fork_depth] {
            auto rand_copy = std_rand;
            RecursiveLifoTests::recursive_test(rand_copy, i_depth, i_fork_depth);
        };

        // check if we should run the normal test or the exhaustive exception test
        if (i_flags && QueueTesterFlags::eTestExceptions)
        {
            run_exception_test(std::function<void()>(do_tests));
        }
        else
        {
            do_tests();
        }
    }

    /** \internal Stars 6 threads, each executing interdependently a recursive test of lifo_array and lifo_buffer. Each thread
        has its own random generator, but all are forked deterministically from the one passes as argument. */
    void lifo_tests(
      QueueTesterFlags i_flags,
      std::ostream &   i_output,
      uint32_t         i_random_seed,
      size_t           i_depth,
      size_t           i_fork_depth)
    {
        PrintScopeDuration duration(
          i_output,
          i_flags && QueueTesterFlags::eTestExceptions ? "lifo_tests with exceptions"
                                                       : "lifo_tests");

        /* this checks that TestObject's don't leak. Note: the instance counting is not thread local, so we have to scope the thread
            procedures inside it. */
        InstanceCounted::ScopedLeakCheck scoped_leak_check;

        EasyRandom main_random = i_random_seed == 0 ? EasyRandom() : EasyRandom(i_random_seed);

        auto const num_of_processors = get_num_of_processors();
        bool const reserve_core1_to_main =
          (i_flags && QueueTesterFlags::eReserveCoreToMainThread) && num_of_processors >= 4;
        uint64_t affinity_mask = std::numeric_limits<uint64_t>::max();
        if (reserve_core1_to_main)
            affinity_mask -= 2;

        // create thread entries - here we pay the cost of forking the random generator
        const size_t thread_count = 6;
        struct ThreadEntry
        {
            EasyRandom  m_random;
            std::thread m_thread;
            ThreadEntry(EasyRandom & i_main_random) : m_random(i_main_random.fork()) {}
        };
        std::vector<ThreadEntry> threads;
        threads.reserve(thread_count);
        for (size_t thread_index = 0; thread_index < thread_count; thread_index++)
        {
            threads.emplace_back(main_random);
        }

#ifdef DENSITY_USER_DATA_STACK
        density::user_data_stack::stats_header(i_output);
#endif

        // start threads
        for (size_t thread_index = 0; thread_index < thread_count; thread_index++)
        {
            auto & thread_entry   = threads[thread_index];
            auto & thread_random  = thread_entry.m_random;
            thread_entry.m_thread = std::thread([&thread_random,
                                                 &i_output,
                                                 thread_index,
                                                 affinity_mask,
                                                 i_flags,
                                                 i_depth,
                                                 i_fork_depth] {
                set_thread_affinity(affinity_mask);

                lifo_test_thread_proc(i_flags, thread_random, i_depth, i_fork_depth);

                std::ostringstream thread_name_stream;
                thread_name_stream << "thread " << thread_index;
                std::string thread_name = thread_name_stream.str();

                std::ostringstream stats_stream;
#ifdef DENSITY_USER_DATA_STACK
                density::user_data_stack::stats_print(stats_stream, thread_name.c_str());
#else
                stats_stream << thread_name << " has finished\n";
#endif
                i_output << stats_stream.str();
                i_output.flush();
            });
        }

        // join threads
        for (auto & thread_entry : threads)
        {
            thread_entry.m_thread.join();
        }
    }

} // namespace density_tests
