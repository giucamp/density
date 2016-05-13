
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "..\density_common.h"
#include "testing_utils.h"
#include <random>
#include <iostream>

namespace density
{
    namespace detail
    {
        #if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below
            _declspec(thread) TestAllocatorBase::ThreadData * TestAllocatorBase::st_thread_data;
        #else
            thread_local TestAllocatorBase::ThreadData TestAllocatorBase::st_thread_data;
        #endif

        TestAllocatorBase::ThreadData & TestAllocatorBase::GetThreadData()
        {
            #if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below
                if (st_thread_data == nullptr)
                {
                    st_thread_data = new ThreadData;
                }
                return *st_thread_data;
            #else
                return st_thread_data;
            #endif
        }

        void TestAllocatorBase::push_level()
        {
            auto & levels = GetThreadData().m_levels;
            levels.emplace_back();
        }

        void TestAllocatorBase::pop_level()
        {
            auto & levels = GetThreadData().m_levels;
            DENSITY_TEST_ASSERT(levels.size() > 0);

            for (const auto & leaking_allocation : levels.back().m_allocations)
            {
                std::cout << "Leak of " << leaking_allocation.second.m_size << ", progressive: " << leaking_allocation.second.m_progressive << std::endl;
            }

            DENSITY_TEST_ASSERT(levels.back().m_allocations.size() == 0);
            levels.pop_back();
        }

        void * TestAllocatorBase::alloc(size_t i_size)
        {
            exception_check_point();

            void * block = operator new (i_size);

            auto & thread_data = GetThreadData();
            if (thread_data.m_levels.size() > 0)
            {
                AllocationEntry entry;
                entry.m_size = i_size;
                entry.m_progressive = thread_data.m_last_progressive++;
				// DENSITY_TEST_ASSERT(entry.m_progressive != ...);

                auto & allocations = thread_data.m_levels.back().m_allocations;
                auto res = allocations.insert(std::make_pair(block, entry));
                DENSITY_TEST_ASSERT(res.second);
            }

            return block;
        }

        void TestAllocatorBase::free(void * i_block)
        {
            if (i_block != nullptr)
            {
                auto & thread_data = GetThreadData();
                if (thread_data.m_levels.size() > 0)
                {
                    auto & allocations = thread_data.m_levels.back().m_allocations;
                    auto it = allocations.find(i_block);
                    DENSITY_TEST_ASSERT(it != allocations.end());
                    allocations.erase(it);
                }

                operator delete (i_block);
            }
        }

    } // namespace detail

    namespace
    {
        struct StaticData
        {
            int64_t m_current_counter;
            int64_t m_except_at;
            StaticData() : m_current_counter(0), m_except_at(-1) {}
        };

        #if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below
            _declspec(thread) StaticData  * st_static_data;
        #else
            thread_local StaticData  * st_static_data;
        #endif
    }

    void exception_check_point()
    {
        auto const static_data = st_static_data;
        if (static_data != nullptr)
        {
            if (static_data->m_current_counter == static_data->m_except_at)
            {
                throw TestException();
            }
            static_data->m_current_counter++;
        }
    }

    void run_exception_stress_test(std::function<void()> i_test)
    {
        DENSITY_TEST_ASSERT(st_static_data == nullptr); // "run_exception_stress_test does no support recursion"

        i_test();

        StaticData static_data;
        st_static_data = &static_data;
        try
        {
            int64_t curr_iteration = 0;
            bool exception_occurred;
            do {
                exception_occurred = false;

                NoLeakScope no_leak_scope;
                try
                {
                    static_data.m_current_counter = 0;
                    static_data.m_except_at = curr_iteration;
                    i_test();
                }
                catch (TestException)
                {
                    exception_occurred = true;
                }
                curr_iteration++;

            } while (exception_occurred);
        }
        catch (...)
        {
            st_static_data = nullptr; // unknown exception, reset st_static_data and retrhow
            throw;
        }
        st_static_data = nullptr;
    }

} // namespace density
