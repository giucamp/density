
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "testing_utils.h"
#include <iostream>

namespace testity
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
        TESTITY_ASSERT(levels.size() > 0);

        for (const auto & leaking_allocation : levels.back().m_allocations)
        {
            std::cout << "Leak of " << leaking_allocation.second.m_size << ", progressive: " << leaking_allocation.second.m_progressive << std::endl;
        }

        TESTITY_ASSERT(levels.back().m_allocations.size() == 0);
        levels.pop_back();
    }

    void TestAllocatorBase::notify_alloc(void * i_block, size_t i_size, size_t i_alignment)
    {
        auto & thread_data = GetThreadData();
        if (thread_data.m_levels.size() > 0)
        {
            AllocationEntry entry;
            entry.m_size = i_size;
            entry.m_alignment = i_alignment;
            entry.m_progressive = thread_data.m_last_progressive++;
            // TESTITY_ASSERT(entry.m_progressive != ...);

            auto & allocations = thread_data.m_levels.back().m_allocations;
            auto res = allocations.insert(std::make_pair(i_block, entry));
            TESTITY_ASSERT(res.second);
        }
    }

    void TestAllocatorBase::notify_deallocation(void * i_block, size_t i_size, size_t i_alignment)
    {
        if (i_block != nullptr)
        {
            auto & thread_data = GetThreadData();
            if (thread_data.m_levels.size() > 0)
            {
                auto & allocations = thread_data.m_levels.back().m_allocations;
                auto it = allocations.find(i_block);
                TESTITY_ASSERT(it != allocations.end());
                TESTITY_ASSERT(it->second.m_size == i_size);
                TESTITY_ASSERT(it->second.m_alignment == i_alignment);
                allocations.erase(it);
            }
        }
    }

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
        TESTITY_ASSERT(st_static_data == nullptr); // "run_exception_stress_test does no support recursion"

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

} // namespace testity
