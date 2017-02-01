
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <assert.h>
#include <utility> // for std::move
#include <type_traits> // for std::aligned_storage

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
    #if defined( __clang__ )
        #define DENSITY_ASSERT(bool_expr)                _Pragma("clang diagnostic push")\
                                                        _Pragma("clang diagnostic ignored \"-Wassume\"")\
                                                        __assume((bool_expr))\
                                                        _Pragma("clang diagnostic pop")
    #elif defined(_MSC_VER)
        #define DENSITY_ASSERT(bool_expr)                __assume((bool_expr))
    #else
        #define DENSITY_ASSERT(bool_expr)
    #endif
#endif

#define DENSITY_ASSERT_INTERNAL(bool_expr)     DENSITY_ASSERT((bool_expr))

#define DENSITY_LIKELY(bool_expr)                    (bool_expr)
#define DENSITY_UNLIKELY(bool_expr)                  (bool_expr)

#ifdef _MSC_VER
    #define DENSITY_NO_INLINE                    __declspec(noinline)
    #if DENSITY_DEBUG
        #define DENSITY_STRONG_INLINE
    #else
        #define DENSITY_STRONG_INLINE            __forceinline
    #endif
#else
    #define DENSITY_NO_INLINE
    #define DENSITY_STRONG_INLINE
#endif

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

    // very minimal implementation of std::optional
    template <typename TYPE>
        class optional
    {
    public:

        optional() noexcept = default;

        optional(const optional & i_source)
            : m_has_value(i_source.m_has_value)
        {
            if (m_has_value)
            {
                new (&m_storage) TYPE(*i_source.ptr());
            }
        }

        optional(optional && i_source)
            : m_has_value(i_source.m_has_value)
        {
            if (m_has_value)
                new (&m_storage) TYPE(std::move(*i_source.ptr()));
        }

        optional & operator = (const optional & i_source)
        {
            swap(*this, optional(i_source));
        }

        optional & operator = (optional && i_source)
        {
            swap(*this, i_source);
        }

        TYPE * operator -> () const                        { return ptr(); }
        TYPE & operator * ()                            { return *ptr(); }
        const TYPE & operator * () const                { return *ptr(); }

        explicit operator bool() const                    { return m_has_value; }

        ~optional()
        {
            if (m_has_value)
                ptr()->TYPE::~TYPE();
        }

        template <typename TYPE, typename... PARAMS>
            friend optional<TYPE> make_optional(PARAMS && ... i_construction_params);

        friend void swap(optional & i_first, optional & i_second)
        {
            using std::swap;
            swap(i_first.m_has_value, i_second.m_has_value);
            if (i_first.m_has_value && i_second.m_has_value)
            {
                swap(*i_first.ptr(), *i_second.ptr());
            }
            else if (i_first.m_has_value)
            {
                new(i_first.ptr()) TYPE(std::move(*i_second.ptr()));
            }
            else if (i_second.m_has_value)
            {
                new(i_second.ptr()) TYPE(std::move(*i_first.ptr()));
            }
        }

    private:
        TYPE * ptr() { return reinterpret_cast<TYPE *>(&m_storage); }
        const TYPE * ptr() const { return reinterpret_cast<TYPE *>(&m_storage); }

    private:
        typename std::aligned_storage<sizeof(TYPE), alignof(TYPE)>::type m_storage;
        bool m_has_value = false;
    };

    template <typename TYPE, typename... PARAMS>
        inline optional<TYPE> make_optional(PARAMS && ... i_construction_params)
    {
        optional<TYPE> res;
        res.m_has_value = true;
        new(&res.m_storage) TYPE(std::forward<PARAMS>(i_construction_params)...);
        return res;
    }
}

