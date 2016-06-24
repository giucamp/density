
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/page_allocator.h"
#include "../testity/test_tree.h"
#include <functional>
#include <deque>
#include <queue>
#include <vector>
#ifdef _WIN32
	#include <Windows.h>
#endif

namespace density
{
    namespace tests
    {
        using namespace testity;

        PerformanceTestGroup make_page_allocator_benchmarks()
        {
            PerformanceTestGroup group("allocate and deallocate pages", "density version: " + std::to_string(DENSITY_VERSION));

			group.set_cardinality_step(200);
			group.set_cardinality_end(3000);

            // page_allocator
            group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				page_allocator allocator;
				void * pages[page_allocator::thread_store_size];
				for( size_t i = 0; i < i_cardinality; i++)
				{					
					for (auto & page : pages)
						*(int*)(page = allocator.allocate_page()) = 0;
					for (auto page : pages)
						 allocator.deallocate_page(page);
				}
            }, __LINE__);

			// new and delete
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				void * pages[page_allocator::thread_store_size];
				const size_t allocator_page_size = page_allocator::page_size();
				for (size_t i = 0; i < i_cardinality; i++)
				{
					for (auto & page : pages)
						*(int*)(page = operator new (allocator_page_size)) = 0;
					for (auto page : pages)
						operator delete(page);
				}
			}, __LINE__);

			// new and delete
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				void * pages[page_allocator::thread_store_size];
				for (size_t i = 0; i < i_cardinality; i++)
				{
					for (auto & page : pages)
						*(int*)(page = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_READWRITE)) = 0;
					for (auto page : pages)
						VirtualFree(page, 0, MEM_RELEASE);
				}
			}, __LINE__);

            return group;
        }

		PerformanceTestGroup make_allocation_benchmarks()
		{
			PerformanceTestGroup group("allocate a lot of memory", "density version: " + std::to_string(DENSITY_VERSION));

			group.set_cardinality_step(1000);
			group.set_cardinality_end(100000); // 390 MB

			static std::vector<void*> pages;
			pages.resize(group.cardinality_end() + 1);

			// page_allocator
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				page_allocator allocator;
				for (size_t i = 0; i < i_cardinality; i++)
				{
					pages[i] = allocator.allocate_page();
					*(int*)pages[i] = 42;
				}
				for (size_t i = 0; i < i_cardinality; i++)
					allocator.deallocate_page(pages[i]);
			}, __LINE__);

			// new and delete
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				const size_t allocator_page_size = page_allocator::page_size();
				for (size_t i = 0; i < i_cardinality; i++)
				{
					pages[i] = operator new (allocator_page_size);
					*(int*)pages[i] = 42;
				}
				for (size_t i = 0; i < i_cardinality; i++)
					operator delete(pages[i]);
			}, __LINE__);

			// new and delete
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				for (size_t i = 0; i < i_cardinality; i++)
				{
					pages[i] = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
					*(int*)pages[i] = 42;
				}
				for (size_t i = 0; i < i_cardinality; i++)
					VirtualFree(pages[i], 0, MEM_RELEASE);
			}, __LINE__);

			return group;
		}

        void add_page_allocator_benchmarks(TestTree & i_tree)
        {
            i_tree.add_performance_test(make_page_allocator_benchmarks());
			i_tree.add_performance_test(make_allocation_benchmarks());
        }

    } // namespace benchmarks

} // namespace density

