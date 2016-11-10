
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/lifo.h>
#include <testity/testity_common.h>
#include <testity/test_tree.h>
#include <random>
#include <vector>
#include <algorithm>
#include <cstring>

namespace density_tests
{
    using namespace density;
    using namespace testity;

    size_t random_alignment(std::mt19937 & i_random)
    {
        size_t log2_max = 0;
        while ((static_cast<size_t>(1) << log2_max) < alignof(std::max_align_t))
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
        LifoTestArray(const lifo_array<TYPE> & i_array)
            : m_array(i_array)
        {
            TESTITY_ASSERT( is_address_aligned( i_array.data(), alignof(TYPE) ) );
            m_vector.insert(m_vector.end(), i_array.begin(), i_array.end());
        }

        void check() const override
        {
            const size_t size = m_array.size();
            TESTITY_ASSERT(m_vector.size() == size);
            for(size_t i = 0; i < size; i++)
            {
                TESTITY_ASSERT( m_vector[i] == m_array[i] );
            }
        }

    private:
        const lifo_array<TYPE> & m_array;
        std::vector<TYPE> m_vector;
    };

    class LifoTestBuffer : public LifoTestItem
    {
    public:
        LifoTestBuffer(lifo_buffer<> & i_buffer)
            : m_buffer(i_buffer)
        {
            m_vector.insert(m_vector.end(),
                static_cast<unsigned char*>(m_buffer.data()),
                static_cast<unsigned char*>(m_buffer.data()) + m_buffer.mem_size() );
        }

        void check() const override
        {
            TESTITY_ASSERT(m_buffer.mem_size() == m_vector.size());
            TESTITY_ASSERT(std::memcmp(m_buffer.data(), m_vector.data(), m_vector.size())==0);
        }

        virtual bool resize(std::mt19937 & i_random) override
        {
            check();
            const auto old_size = m_buffer.mem_size();
            const auto new_size = std::uniform_int_distribution<size_t>(0, 32)(i_random);

            const bool custom_alignment = std::uniform_int_distribution<int>(0, 100)(i_random) > 50;
            const bool preserve = false; // preserve is currently not supported ---- std::uniform_int_distribution<int>(0, 100)(i_random) > 50;

            m_vector.resize(new_size);

            if (custom_alignment)
            {
                const auto alignment = random_alignment(i_random);
                m_buffer.resize(new_size, alignment);
                TESTITY_ASSERT(is_address_aligned(m_buffer.data(), alignment));
            }
            else
            {
                m_buffer.resize(new_size);
            }

            if (preserve)
            {
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
            }
            else
            {
                std::uniform_int_distribution<unsigned> rnd(0, 100);
                std::generate(static_cast<unsigned char*>(m_buffer.data()),
                    static_cast<unsigned char*>(m_buffer.data()) + new_size,
                    [&i_random, &rnd] { return static_cast<unsigned char>(rnd(i_random)); });
                memcpy(m_vector.data(),
                    static_cast<unsigned char*>(m_buffer.data()),
                    new_size);
            }
            check();
            return true;
        }

    private:
        lifo_buffer<> & m_buffer;
        std::vector<unsigned char> m_vector;
    };

    template <typename TYPE>
        class LifoTestAny : public LifoTestItem
    {
    public:
        LifoTestAny(lifo_any<> & i_any, const TYPE & i_value )
            : m_any(i_any), m_value(i_value)
        {
            LifoTestAny::check();
        }

        void check() const override
        {
            TESTITY_ASSERT(m_any.type() == runtime_type<>::template make<TYPE>());
            TESTITY_ASSERT(*static_cast<const TYPE*>(m_any.data()) == m_value);
        }

    private:
        lifo_any<> & m_any;
        TYPE m_value;
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
            void push_test(const lifo_array<TYPE> & i_array)
        {
            m_tests.emplace_back( new LifoTestArray<TYPE>(i_array) );
        }

        void push_test(lifo_buffer<> & i_buffer)
        {
            m_tests.emplace_back( new LifoTestBuffer(i_buffer) );
        }

        template <typename TYPE>
            void push_test(lifo_any<> & i_any, const TYPE & i_value)
        {
            m_tests.emplace_back(new LifoTestAny<TYPE>(i_any, i_value));
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
            std::uniform_int_distribution<unsigned> rnd(0, 100);
            lifo_buffer<> buffer(std::uniform_int_distribution<size_t>(0, 32)(m_random));
            TESTITY_ASSERT(is_address_aligned(buffer.data(), alignof(std::max_align_t)));
            std::generate(
                static_cast<unsigned char*>(buffer.data()),
                static_cast<unsigned char*>(buffer.data()) + buffer.mem_size(),
                [this, &rnd] { return static_cast<unsigned char>(rnd(m_random) % 128); });
            push_test(buffer);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_any_int()
        {
            int value = std::uniform_int_distribution<int>(-100, 100)(m_random);
            lifo_any<> any( value );

            TESTITY_ASSERT(is_address_aligned(any.data(), alignof(std::max_align_t)));
            push_test(any, value);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_buffer_aligned()
        {
            const auto alignment = random_alignment(m_random);
            std::uniform_int_distribution<unsigned> rnd(0, 100);
            lifo_buffer<> buffer(std::uniform_int_distribution<size_t>(0, 32)(m_random), alignment);
            TESTITY_ASSERT(is_address_aligned(buffer.data(), alignment));
            std::generate(
                static_cast<unsigned char*>(buffer.data()),
                static_cast<unsigned char*>(buffer.data()) + buffer.mem_size(),
                [this, &rnd] { return static_cast<unsigned char>(rnd(m_random) % 128); });
            push_test(buffer);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_char()
        {
            std::uniform_int_distribution<unsigned> rnd(0, 100);
            lifo_array<unsigned char> arr(std::uniform_int_distribution<size_t>(0, 20)(m_random));
            std::generate(arr.begin(), arr.end(), [this, &rnd] { return static_cast<unsigned char>(rnd(m_random)); });
            push_test(arr);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_int()
        {
            std::uniform_int_distribution<int> rnd(-1000, 1000);
            lifo_array<int> arr(std::uniform_int_distribution<size_t>(0, 7)(m_random));
            std::generate(arr.begin(), arr.end(), [this, &rnd] { return rnd(m_random); });

            push_test(arr);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_wide_alignment()
        {
            union alignas(alignof(std::max_align_t) * 2) AlignedType
            {
                int m_value;
                std::max_align_t m_unused[2];
                bool operator == (const AlignedType & i_other) const
                {
                    return m_value == i_other.m_value;
                }
            };

            std::uniform_int_distribution<int> rnd(-1000, 1000);
            lifo_array<AlignedType> arr(std::uniform_int_distribution<size_t>(0, 7)(m_random));
            std::generate(arr.begin(), arr.end(), [this, &rnd] { return AlignedType{ rnd(m_random) }; });

            push_test(arr);
            lifo_test_push();
            pop_test();
        }

        void lifo_test_push_double()
        {
            std::uniform_real_distribution<double> rnd(-1000., 1000.);
            lifo_array<double> arr(std::uniform_int_distribution<size_t>(0, 7)(m_random));
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
                Func tests[] = { &LifoTestContext::lifo_test_push_buffer, &LifoTestContext::lifo_test_push_char,
                    &LifoTestContext::lifo_test_push_int, &LifoTestContext::lifo_test_push_double,
                    &LifoTestContext::lifo_test_push_wide_alignment, &LifoTestContext::lifo_test_push_any_int };

                m_curr_depth++;

                const auto iter_count = std::uniform_int_distribution<int>(0, 5)(m_random);
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

    void add_lifo_cases(TestTree & i_dest)
    {
        using TestFunc = std::function< void(std::mt19937 & i_random)>;

        // lifo_allocator
        i_dest.add_case(TestFunc([](std::mt19937 & i_random) {

            // instance a lifo_allocator
            void_allocator underlying_allocator;
            lifo_allocator<void_allocator> allocator(underlying_allocator);

            // for a random number of times....
            while (std::uniform_int_distribution<size_t>(0, 100)(i_random) > 10)
            {
                // allocate a block and fill it with progressive numbers
                auto size = std::uniform_int_distribution<size_t>(0, 8000)(i_random);
                auto block = static_cast<unsigned char*>( allocator.allocate(size) );
                for (size_t index = 0; index < size; index++)
                {
                    block[index] = static_cast<unsigned char>(index & 0xFF);
                }

                // reallocate the block with reallocate_preserve, and check the content
                auto new_size = std::uniform_int_distribution<size_t>(0, 8000)(i_random);
                block = static_cast<unsigned char*>(allocator.reallocate_preserve(block, new_size));
                for (size_t index = 0; index < std::min(size, new_size); index++)
                {
                    TESTITY_ASSERT( block[index] == static_cast<unsigned char>(index & 0xFF) );
                }

                // reallocate the block with reallocate
                new_size = std::uniform_int_distribution<size_t>(0, 8000)(i_random);
                block = static_cast<unsigned char*>(allocator.reallocate(block, new_size));

                // done
                allocator.deallocate(block);
            }
        }));

        // thread_lifo_allocator
        i_dest.add_case(TestFunc([](std::mt19937 & i_random) {

            // instance a lifo_allocator
            thread_lifo_allocator<> allocator;

            // for a random number of times....
            while (std::uniform_int_distribution<size_t>(0, 100)(i_random) > 10)
            {
                // allocate a block and fill it with progressive numbers
                auto size = std::uniform_int_distribution<size_t>(0, 8000)(i_random);
                auto block = static_cast<unsigned char*>(allocator.allocate(size));
                for (size_t index = 0; index < size; index++)
                {
                    block[index] = static_cast<unsigned char>(index & 0xFF);
                }

                // reallocate the block with reallocate_preserve, and check the content
                auto new_size = std::uniform_int_distribution<size_t>(0, 8000)(i_random);
                block = static_cast<unsigned char*>(allocator.reallocate_preserve(block, new_size));
                for (size_t index = 0; index < std::min(size, new_size); index++)
                {
                    TESTITY_ASSERT(block[index] == static_cast<unsigned char>(index & 0xFF));
                }

                // reallocate the block with reallocate
                new_size = std::uniform_int_distribution<size_t>(0, 8000)(i_random);
                block = static_cast<unsigned char*>(allocator.reallocate(block, new_size));

                // done
                allocator.deallocate(block);
            }
        }));

        i_dest.add_case(TestFunc([](std::mt19937 & i_random ) {
            LifoTestContext context(i_random, 4);
            context.lifo_test_push();
        }));
    }

} // namespace density_tests

