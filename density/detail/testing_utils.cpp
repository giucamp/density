
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "..\testing_utils.h"
#include <random>
#include <assert.h>

namespace density
{
	namespace details
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
			assert(levels.size() > 0);
			assert(levels.back().m_allocations.size() == 0);
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

				auto & allocations = thread_data.m_levels.back().m_allocations;
				auto res = allocations.insert(std::make_pair(block, entry));
				assert(res.second);
			}

			return block;
		}

		void TestAllocatorBase::free(void * i_block)
		{
			auto & thread_data = GetThreadData();
			if (thread_data.m_levels.size() > 0)
			{
				auto & allocations = thread_data.m_levels.back().m_allocations;
				auto it = allocations.find(i_block);
				assert(it != allocations.end());
				allocations.erase(it);
			}

			operator delete (i_block);
		}
	
	} // namespace details
	
	NoLeakScope::NoLeakScope()
	{
		details::TestAllocatorBase::push_level();
	}

	NoLeakScope::~NoLeakScope()
	{
		details::TestAllocatorBase::pop_level();
	}

	namespace
	{
		struct StaticData
		{
			int64_t m_current_counter;
			int64_t m_except_at;
			StaticData() : m_current_counter(0), m_except_at(-1) {}
		};

		class TestException
		{
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

	std::random_device g_random_device;
	std::mt19937 g_rand(g_random_device());

	namespace details
	{
		AllocatingTester::AllocatingTester()
			: m_rand_value(std::allocate_shared<int>(TestAllocator<int>(), std::uniform_int_distribution<int>(100000)(g_rand)))
		{
		}
	}

	void run_exception_stress_test(std::function<void()> i_test)
	{
		assert(st_static_data == nullptr); // "run_exception_stress_test does no support recursion"

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
