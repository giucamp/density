
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <iostream>

#ifdef _MSC_VER
	#define TESTITY_ASSERT(i_bool_expr)        if(!(i_bool_expr)) \
													{ \
														std::cerr << "Test assert failed: '" #i_bool_expr "' at " __FILE__ "(" << __LINE__ << ")" << std::endl; \
														__debugbreak(); \
													}
#else
	#include <assert.h>
	#define TESTITY_ASSERT(i_bool_expr)        assert(!(i_bool_expr))
#endif // _MSC_VER

namespace testity
{
    namespace detail
    {
        class TestAllocatorBase
        {
        public:

            static void push_level();
            static void pop_level();

            static void * alloc(size_t i_size);
            static void free(void * i_block);

        private:
            struct AllocationEntry
            {
                size_t m_progressive = 0;
                size_t m_size = 0;
            };
            struct Levels
            {
                std::unordered_map<void*, AllocationEntry> m_allocations;
            };
            struct ThreadData
            {
                std::vector<Levels> m_levels;
                size_t m_last_progressive = 0;
            };
            static ThreadData & GetThreadData();

        private:
            #if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below
                static _declspec(thread) ThreadData * st_thread_data;
            #else
                static thread_local ThreadData st_thread_data;
            #endif
        };

    } // namespace detail

    template <class TYPE>
        class TestAllocator : private detail::TestAllocatorBase
    {
    public:
        typedef TYPE value_type;

        TestAllocator() {}

        template <class OTHER_TYPE> TestAllocator(const TestAllocator<OTHER_TYPE>& /*i_other*/) { }

        TYPE * allocate(std::size_t i_count)
        {
            return static_cast<TYPE *>(detail::TestAllocatorBase::alloc(i_count * sizeof(TYPE)) );
        }

        void deallocate(TYPE * i_block, std::size_t /*i_count*/)
        {
            detail::TestAllocatorBase::free(i_block);
        }

        template <typename OTHER_TYPE>
            bool operator == (const TestAllocator<OTHER_TYPE> &) const
        {
            return true;
        }

        template <typename OTHER_TYPE>
            bool operator != (const TestAllocator<OTHER_TYPE> &) const
        {
            return false;
        }

        #if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below

            template<class Other> struct rebind { typedef TestAllocator<Other> other; };

            void construct(TYPE * i_pointer)
            {
                new (i_pointer) TYPE();
            }

            void construct(TYPE * i_pointer, const TYPE & i_source)
            {
                new (i_pointer) TYPE(i_source);
            }

            template<class OTHER_TYPE, class... ARG_TYPES>
                void construct(OTHER_TYPE * i_pointer, ARG_TYPES &&... i_args)
            {
                new (i_pointer)OTHER_TYPE(std::forward<ARG_TYPES>(i_args)...);
            }

            void destroy(TYPE * i_pointer)
            {
                i_pointer->~TYPE();
                (void)i_pointer; // avoid warning C4100: 'i_pointer' : unreferenced formal parameter
            }

        #endif
    };

    class NoLeakScope
    {
    public:
        NoLeakScope() { detail::TestAllocatorBase::push_level(); }
        ~NoLeakScope() { detail::TestAllocatorBase::pop_level(); }
        NoLeakScope(const NoLeakScope &) = delete;
        NoLeakScope & operator = (const NoLeakScope &) = delete;
    };

    class TestException : public std::exception
    {
        using std::exception::exception;
    };

    /** Runs an exception safeness test, calling the provided function many times.
        First the provided function is called without raising any exception.
        - Then the function is called, an the first time exception_check_point is called, an exception is thrown
        - then the function is called, an the second time exception_check_point is called, an exception is thrown
        During the execution of the function the function exception_check_point should be called to
        test the effect of throwing an exception.
    */
    void run_exception_stress_test(std::function<void()> i_test);

    void exception_check_point();

} // namespace testity
