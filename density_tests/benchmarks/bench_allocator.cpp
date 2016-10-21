
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../../density/void_allocator.h"
#include "testity/test_tree.h"
#include <functional>
#include <deque>
#include <queue>
#include <vector>
#ifdef _WIN32
    #include <Windows.h>
#endif

namespace density_tests
{
    using namespace testity;
    using namespace density;

    PerformanceTestGroup make_page_allocator_benchmarks()
    {
        PerformanceTestGroup group("allocate and deallocate pages", "density version: " + std::to_string(DENSITY_VERSION));

        group.set_cardinality_step(200);
        group.set_cardinality_end(3000);

        // void_allocator
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            void_allocator allocator;
            void * pages[void_allocator::free_page_cache_size];
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
            void * pages[void_allocator::free_page_cache_size];
            for (size_t i = 0; i < i_cardinality; i++)
            {
                for (auto & page : pages)
                    *(int*)(page = operator new (void_allocator::page_size)) = 0;
                for (auto page : pages)
                    operator delete(page);
            }
        }, __LINE__);

        // new and delete
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            void * pages[void_allocator::free_page_cache_size];
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

        // void_allocator
        group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
            void_allocator allocator;
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
            for (size_t i = 0; i < i_cardinality; i++)
            {
                pages[i] = operator new (void_allocator::page_size);
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

    void add_allocator_benchmarks(TestTree & i_dest)
    {
        i_dest.add_performance_test(make_page_allocator_benchmarks());
        i_dest.add_performance_test(make_allocation_benchmarks());
    }

} // namespace density_tests


