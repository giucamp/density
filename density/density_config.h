
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <assert.h>
#include <utility> // for std::move
#include <type_traits> // for std::aligned_storage and std::conditional

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
        #define DENSITY_ASSERT(bool_expr)               _Pragma("clang diagnostic push")\
                                                        _Pragma("clang diagnostic ignored \"-Wassume\"")\
                                                         __builtin_assume((bool_expr))\
                                                        _Pragma("clang diagnostic pop")
    #elif defined(_MSC_VER)
        #define DENSITY_ASSERT(bool_expr)                __assume((bool_expr))
    #else
        #define DENSITY_ASSERT(bool_expr)                (void)0
    #endif
#endif

#define DENSITY_ASSERT_ALIGNED(address, alignment)      DENSITY_ASSERT(::density::address_is_aligned(address, alignment))
#define DENSITY_ASSERT_UINT_ALIGNED(uint, alignment)    DENSITY_ASSERT(::density::uint_is_aligned(uint, alignment))

#define DENSITY_ASSERT_INTERNAL(bool_expr)              DENSITY_ASSERT((bool_expr))

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

namespace density
{
    /** Alignment used by some concurrent data structure to avoid false sharing of cache lines.  It must be a power of 2. 
    
        This is a configuration variable, intended to be customized by the user of the library. The default value is 64. 
        
        If a C++17 compiler is available, this constant may be defined as std::hardware_destructive_interference_size. */
    constexpr size_t concurrent_alignment = 64;

    /** Capacity (in bytes) of the pages managed by density::void_allocator. Note: the actual usable size is slightly smaller. 
        This constant must be a power of 2. 
        
        This is a configuration variable, intended to be customized by the user of the library. The default value is 64. */
    constexpr size_t default_page_capacity = 1024 * 64;

    /** In this version of the library relaxed atomic operations are disabled.
        Concurrently data structures has been tested on x86-x64, but not on architectures with weak
        memory ordering. If you want to contribute to density, running the tests on other
        architectures, you can change this constant. */
    constexpr bool enable_relaxed_atomics = false;

    /* Very minimal implementation of std::optional, that can be used as target for density::optional. */
    template <typename TYPE>
        class builtin_optional
    {
    public:

        builtin_optional() noexcept = default;

        template <typename OTHER_TYPE>
            builtin_optional(OTHER_TYPE && i_value)
               : m_has_value(true)
        {
            new(&m_storage) TYPE(std::forward<OTHER_TYPE>(i_value));
        }

        builtin_optional(const builtin_optional & i_source)
            : m_has_value(i_source.m_has_value)
        {
            if (m_has_value)
            {
                new (&m_storage) TYPE(*i_source.ptr());
            }
        }

        builtin_optional(builtin_optional && i_source)
            : m_has_value(i_source.m_has_value)
        {
            if (m_has_value)
                new (&m_storage) TYPE(std::move(*i_source.ptr()));
        }

        builtin_optional & operator = (const builtin_optional & i_source)
        {
            swap(*this, builtin_optional(i_source));
        }

        builtin_optional & operator = (builtin_optional && i_source)
        {
            swap(*this, i_source);
        }

        TYPE * operator -> () const                     { return ptr(); }
        TYPE & operator * ()                            { return *ptr(); }
        const TYPE & operator * () const                { return *ptr(); }

        explicit operator bool() const                  { return m_has_value; }

        ~builtin_optional()
        {
            if (m_has_value)
                ptr()->TYPE::~TYPE();
        }

        template <typename TYPE_1, typename... PARAMS>
            friend builtin_optional<TYPE_1> make_optional(PARAMS && ... i_construction_params);

        friend void swap(builtin_optional & i_first, builtin_optional & i_second) noexcept
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
        const TYPE * ptr() const { return reinterpret_cast<const TYPE *>(&m_storage); }

    private:
        typename std::aligned_storage<sizeof(TYPE), alignof(TYPE)>::type m_storage;
        bool m_has_value = false;
    };

    /** Alias to an implementation of optional. By default a minimal implementation of optional is used, but
        it can be replaced by the C++17 standard one, if available. */
    template <typename TYPE>
        using optional = builtin_optional<TYPE>;
}

