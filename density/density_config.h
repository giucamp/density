
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <assert.h>

#if !defined(NDEBUG)
    #define DENSITY_DEBUG                    1
    #define DENSITY_DEBUG_INTERNAL           1
#else
    #define DENSITY_DEBUG                    0
    #define DENSITY_DEBUG_INTERNAL           0
#endif

#if DENSITY_DEBUG
    #ifdef _MSC_VER
        #define DENSITY_ASSERT(bool_expr)              if(!(bool_expr)) { __debugbreak(); } else (void)0
    #else
        #define DENSITY_ASSERT(bool_expr)              assert((bool_expr))
    #endif
#else
    #define DENSITY_ASSERT(bool_expr)
#endif

#if DENSITY_DEBUG_INTERNAL
    #define DENSITY_ASSERT_INTERNAL(bool_expr)     DENSITY_ASSERT((bool_expr))
#else
    #define DENSITY_ASSERT_INTERNAL(bool_expr)
#endif

#define DENSITY_HANDLE_EXCEPTIONS                  1

#define DENSITY_CONCURRENT_DATA_ALIGNMENT          64

#ifdef _MSC_VER
    #define DENSITY_NO_INLINE                    __declspec(noinline)
    #define DENSITY_STRONG_INLINE                __forceinline
#else
    #define DENSITY_NO_INLINE
    #define DENSITY_STRONG_INLINE
#endif

#define DENSITY_COMPATCT_QUEUE                   1

#if defined(__GNUC__) && defined(__MINGW32__)
    #define DENSITY_ENV_HAS_THREADING                   0
#else
    #define DENSITY_ENV_HAS_THREADING                   1
#endif

#include <atomic>
#include <thread>
#include <mutex>
#include <atomic>

namespace density
{
    constexpr size_t concurrent_alignment = 64;

    /** Aliases for the synchronization classes.
        By default density uses the C++11 standard synchronization support. Anyway you can change these aliases
        to use another synchronization library, given that it is conforming to the standard one. */
    namespace sync
    {
        using std::mutex;
        using std::thread;
        using std::lock_guard;
        using std::atomic;
        namespace this_thread = std::this_thread;
		using memory_order = std::memory_order;

        /* concurrent data structures has been tested on x86-x64, but not on architectures with weak
            memory ordering. If you are willing to contribute to density, running the tests on other
            architectures, you can change these constants. */
        #if defined(_M_IX86) || defined(_M_X64)
            constexpr memory_order hint_memory_order_relaxed = std::memory_order_relaxed;
            constexpr memory_order hint_memory_order_acquire = std::memory_order_acquire;
            constexpr memory_order hint_memory_order_release = std::memory_order_release;
            constexpr memory_order hint_memory_order_acq_rel = std::memory_order_acq_rel;
            constexpr memory_order hint_memory_order_seq_cst = std::memory_order_seq_cst;
        #else
            constexpr memory_order hint_memory_order_relaxed = std::memory_order_seq_cst;
            constexpr memory_order hint_memory_order_acquire = std::memory_order_seq_cst;
            constexpr memory_order hint_memory_order_release = std::memory_order_seq_cst;
            constexpr memory_order hint_memory_order_acq_rel = std::memory_order_seq_cst;
            constexpr memory_order hint_memory_order_seq_cst = std::memory_order_seq_cst;
        #endif
    }
}

