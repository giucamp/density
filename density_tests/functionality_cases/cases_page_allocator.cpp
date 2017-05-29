
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <testity/testity_common.h>
#include <testity/test_tree.h>
#include <density/void_allocator.h>
#include <vector>
#include <limits>
#include <cstring>
#include <atomic>
#include <memory>
#include <chrono>
#include <iostream>
#include <sstream>

namespace density_tests
{
    using namespace density;
    using namespace testity;

	/** This class performs a stress test of density::void_allocator */
	class PageAllocatorTest
	{
	public:

		PageAllocatorTest(std::mt19937 & i_random, size_t i_page_pool_size)
			: m_random(i_random), m_page_pool_size(i_page_pool_size), m_pages(new std::atomic<void*>[i_page_pool_size])
		{			
			for (size_t i = 0; i < i_page_pool_size; i++)
			{
				std::atomic_init(&m_pages[i], nullptr);
			}
		}

		PageAllocatorTest(const PageAllocatorTest &) = delete;
		PageAllocatorTest & operator = (const PageAllocatorTest &) = delete;

		void run(size_t i_thread_count, size_t i_iteration_count)
		{
			using namespace std;
			TESTITY_ASSERT(i_thread_count > 0);

			vector<ThreadData> entries(i_thread_count);
			
			for (auto & entry : entries)
			{
				/* create an mt19937 using m_random to generate the seeds.
					We avoid altering the user mt19937 in a nondeterministic way. */
				entry.m_random = mt19937(m_random());

				// now we can start this thread
				entry.m_thread = thread([this, &entry, i_iteration_count] {
					thread_run(entry, i_iteration_count);
				});
			}

			// print the status every 500 millisecs
			auto const iter_count_mult = 100. / i_iteration_count;
			bool all_completed = false;
			while(!all_completed)
			{
				this_thread::sleep_for(chrono::milliseconds(500));

				ostringstream text;
				all_completed = true;
				for (auto & entry : entries)
				{
					auto const curr_iteration = entry.m_curr_iteration->load(memory_order_relaxed);
					if (curr_iteration != numeric_limits<size_t>::max())
					{
						all_completed = false;
						text << static_cast<int>(curr_iteration * iter_count_mult) << "% ";
					}
					else
					{
						text << "--- ";
					}
				}
				cout << text.str() << endl;
			}

			// wait until all the threads have exited
			for (auto & entry : entries)
				entry.m_thread.join();

			// deallocate any surviving page
			for (size_t page_index = 0; page_index < m_page_pool_size; page_index++)
			{
				delete_page(m_random, m_pages[page_index]);
				m_pages[page_index] = nullptr;
			}
		}

	private:

		struct ThreadData
		{
			std::mt19937 m_random;
			std::thread m_thread;
			std::unique_ptr<std::atomic<size_t>> m_curr_iteration = std::make_unique<std::atomic<size_t>>(0);
		};

		// allocates or deallocates a page
		void thread_step(std::mt19937 & i_random)
		{
			auto const index = std::uniform_int_distribution<size_t>(0, m_page_pool_size - 1)(i_random);
			auto page = m_pages[index].load(std::memory_order_acquire);
			bool done;
			do
			{
				if (page == nullptr)
				{
					auto new_page = create_page(i_random);
					done = m_pages[index].compare_exchange_weak(page, new_page,
						std::memory_order_release, std::memory_order_acquire);
					if (!done)
					{
						delete_page(i_random, new_page);
					}
				}
				else
				{
					auto const page_to_delete = page; // make a copy to be safe against buggy implementations of compare_exchange_weak
					done = m_pages[index].compare_exchange_weak(page, nullptr,
						std::memory_order_release, std::memory_order_acquire);
					if (done)
					{
						delete_page(i_random, page_to_delete);
					}
				}

			} while (!done);
		}

		/* run the test, calling thread_step for i_iteration_count times. Every 1024 iterations
			i_local_data.m_curr_iteration is updated. At the end, i_local_data.m_curr_iteration 
			is set to the maximum size_t. */ 
		void thread_run(ThreadData & i_local_data, size_t i_iteration_count)
		{
			auto const update_it_at = std::uniform_int_distribution<size_t>(0, 1023)(i_local_data.m_random);
			for (size_t iteration_index = 0; iteration_index < i_iteration_count; iteration_index++)
			{
				thread_step(i_local_data.m_random);
				if ((iteration_index % 1024) == update_it_at)
				{
					i_local_data.m_curr_iteration->store(iteration_index, std::memory_order_relaxed);
				}
			}

			i_local_data.m_curr_iteration->store(std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
		}

		//  creates a page, filling it with an hash of the address
		void * create_page(std::mt19937 & i_random)
		{
			bool const zeroed = std::uniform_int_distribution<size_t>(0, 1)(i_random) == 0;
			void * new_page = nullptr;
			if (zeroed)
			{
				new_page = m_allocator.allocate_page_zeroed();
				TESTITY_ASSERT(address_is_aligned(new_page, void_allocator::page_alignment));
				auto const hash = address_hash(new_page);
				auto const chars = static_cast<unsigned char*>(new_page);
				for (size_t i = 0; i < void_allocator::page_size; i++)
				{
					TESTITY_ASSERT(chars[i] == 0);
					chars[i] = hash;
				}
			}
			else
			{
				new_page = m_allocator.allocate_page();
				TESTITY_ASSERT(address_is_aligned(new_page, void_allocator::page_alignment));
				auto const hash = address_hash(new_page);
				std::memset(new_page, address_hash(new_page), void_allocator::page_size);
			}
			return new_page;
		}

		// deletes a page, checking if it's filled with the hash of the address
		void delete_page(std::mt19937 & i_random, void * i_page)
		{
			if (i_page != nullptr)
			{
				TESTITY_ASSERT(address_is_aligned(i_page, void_allocator::page_alignment));

				bool const zeroed = std::uniform_int_distribution<size_t>(0, 1)(i_random) == 0;
				auto const hash = address_hash(i_page);
				auto const chars = static_cast<unsigned char*>(i_page);

				for (size_t i = 0; i < void_allocator::page_size; i++)
				{
					TESTITY_ASSERT(chars[i] == hash);
					if (zeroed)
					{
						chars[i] = 0;
					}
					else
					{
						chars[i] = std::numeric_limits<unsigned char>::max();
					}
				}

				if (zeroed)
				{
					m_allocator.deallocate_page_zeroed(i_page);
				}
				else
				{
					m_allocator.deallocate_page(i_page);
				}
			}
		}

		// makes a char hash from a memory address
		static unsigned char address_hash(const void * i_address)
		{
			// djb2 from http://www.cse.yorku.ca/~oz/hash.html
			unsigned char result = 0;
			auto address = reinterpret_cast<uintptr_t>(i_address);
			do {
				result = result * 33 + (address & std::numeric_limits<unsigned char>::max());
				address >>= std::numeric_limits<unsigned char>::digits;
			} while (address > 0);
			return result;
		}

	private:
		std::mt19937 & m_random;
		size_t const m_page_pool_size;
		std::unique_ptr<std::atomic<void*>[]> const m_pages;
		void_allocator m_allocator;
	};

    void add_page_allocator_cases(TestTree & i_dest)
    {
        using TestFunc = std::function< void(std::mt19937 & i_random)>;

        i_dest.add_case(TestFunc([](std::mt19937 & i_random) {
			PageAllocatorTest test(i_random, /* page pool size = */ 1000);
			test.run(/* thread count = */ 16, /* iterations count = */ 100 * 1000);
        }));
    }

} // namespace density_tests

