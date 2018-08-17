
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cstdlib>
#include <type_traits>
#include <utility>

/*! \file */

/** Assert used to detect user errors that causes undefined behavior */
#if !defined(DENSITY_ASSERT)
#if defined(DENSITY_DEBUG)
#define DENSITY_ASSERT DENSITY_CHECKING_ASSERT
#else
#define DENSITY_ASSERT(...) ((void)(0))
#endif
#endif

/** Assert used to detect bugs of the library that causes undefined behavior */
#if !defined(DENSITY_ASSERT_INTERNAL)
#if defined(DENSITY_DEBUG_INTERNAL)
#define DENSITY_ASSERT_INTERNAL DENSITY_CHECKING_ASSERT
#else
#define DENSITY_ASSERT_INTERNAL(...) ((void)(0))
#endif
#endif

/** Macro that tells an invariant to the compiler as hint for the optimizer. */
#if defined(DENSITY_DEBUG)
#define DENSITY_ASSUME DENSITY_CHECKING_ASSERT
#else
#if defined(__clang__)
#define DENSITY_ASSUME(bool_expr, ...)                                                             \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wassume\"")              \
      __builtin_assume((bool_expr)) _Pragma("clang diagnostic pop")
#elif defined(_MSC_VER)
#define DENSITY_ASSUME(bool_expr, ...) __assume((bool_expr))
#elif defined(__GNUC__)
// https://stackoverflow.com/questions/25667901/assume-clause-in-gcc
#define DENSITY_ASSUME(bool_expr, ...) (!(bool_expr) ? __builtin_unreachable() : (void)0)
#else
#define DENSITY_ASSUME(bool_expr, ...) (void)0
#endif
#endif

/** Macro used to enforce the alignment of a pointer as an invariant. */
#if defined(DENSITY_DEBUG)
#define DENSITY_ASSUME_ALIGNED(address, constexpr_alignment)                                       \
    DENSITY_ASSERT(::density::address_is_aligned(                                                  \
      (address), ::density::detail::ConstSizeT<(constexpr_alignment)>::value))
#elif defined(__GNUC__)
#define DENSITY_ASSUME_ALIGNED(address, constexpr_alignment)                                       \
    __builtin_assume_aligned(address, constexpr_alignment)
#else
#define DENSITY_ASSUME_ALIGNED(address, constexpr_alignment)                                       \
    DENSITY_ASSUME((uintptr_t(address) & ((constexpr_alignment)-1)) == 0)
#endif

#if defined(DENSITY_DEBUG)
#define DENSITY_ASSUME_UINT_ALIGNED(address, constexpr_alignment)                                  \
    DENSITY_ASSERT(::density::uint_is_aligned(                                                     \
      (address), ::density::detail::ConstSizeT<(constexpr_alignment)>::value))
#else
#define DENSITY_ASSUME_UINT_ALIGNED(address, constexpr_alignment)                                  \
    DENSITY_ASSUME(((address) & ((constexpr_alignment)-1)) == 0)
#endif

/** Macro that tells to the compiler that a condition is true in most cases. This is just an hint to the optimizer. */
#if defined(__GNUC__) && !defined(_MSC_VER)
#define DENSITY_LIKELY(bool_expr) (__builtin_expect((bool_expr), true), (bool_expr))
#else
#define DENSITY_LIKELY(bool_expr) (bool_expr)
#endif

/** Macro used in some circumstances to avoid inlining of a function, for
        example because the call handles a somewhat rare slow path. */
#ifdef _MSC_VER
#define DENSITY_NO_INLINE __declspec(noinline)
#elif defined(__GNUC__)
#define DENSITY_NO_INLINE __attribute__((noinline))
#else
#define DENSITY_NO_INLINE
#endif

/** Assert that on failure should cause an halt of the program. Used only locally in this header. */
#ifdef _MSC_VER
#define DENSITY_CHECKING_ASSERT(bool_expr, ...) (!(bool_expr) ? __debugbreak() : (void)0)
#elif defined(__GNUC__)
#define DENSITY_CHECKING_ASSERT(bool_expr, ...) (!(bool_expr) ? __builtin_trap() : (void)0)
#else
#define DENSITY_CHECKING_ASSERT(bool_expr, ...) (!(bool_expr) ? std::abort() : (void)0)
#endif

namespace density
{
    /** Alignment used by some concurrent data structure to avoid false sharing of cache lines. It must be a power of 2.

        This is a configuration variable, intended to be customized by the user of the library. The default value is 64.

        If a C++17 compiler is available, this constant may be defined as
        [std::hardware_destructive_interference_size](http://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size). */
    constexpr size_t destructive_interference_size = 64;

    /** Capacity (in bytes) of the pages managed by density::default_allocator. Note: the actual usable size is slightly smaller.
        This constant must be a power of 2.

        This is a configuration variable, intended to be customized by the user of the library. The default value is 1024 * 64. */
    constexpr size_t default_page_capacity = 1024 * 64;

    /** In this version of the library relaxed atomic operations are disabled.
        Concurrently data structures has been tested on x86-x64, but not on architectures with weak
        memory ordering. If you want to contribute to density, running the tests on other
        architectures, you can change this constant. */
    constexpr bool enable_relaxed_atomics = false;

    template <size_t PAGE_CAPACITY_AND_ALIGNMENT> class basic_default_allocator;

    /** Allocator type the data stack is built upon. It must satisfy the requirements of both \ref UntypedAllocator_requirements "UntypedAllocator"
        and \ref PagedAllocator_requirements "PagedAllocator" */
    using data_stack_underlying_allocator = basic_default_allocator<default_page_capacity>;

/** \def DENSITY_USER_DATA_STACK If defined enables the user data stack. */
#ifdef DENSITY_USER_DATA_STACK
    namespace user_data_stack
    {
        constexpr size_t alignment = alignof(void *);

        void * allocate(size_t i_size);

        void * allocate_empty() noexcept;

        void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size);

        void deallocate(void * i_block, size_t i_size) noexcept;
    } // namespace user_data_stack
#endif

    /* Very minimal implementation of std::optional, that can be used as target for density::optional. */
    template <typename TYPE> class builtin_optional
    {
      public:
        builtin_optional() noexcept = default;

        template <typename OTHER_TYPE> builtin_optional(OTHER_TYPE && i_value) : m_has_value(true)
        {
            new (&m_storage) TYPE(std::forward<OTHER_TYPE>(i_value));
        }

        builtin_optional(const builtin_optional & i_source) : m_has_value(i_source.m_has_value)
        {
            if (m_has_value)
            {
                new (&m_storage) TYPE(*i_source.ptr());
            }
        }

        builtin_optional(builtin_optional && i_source) : m_has_value(i_source.m_has_value)
        {
            if (m_has_value)
                new (&m_storage) TYPE(std::move(*i_source.ptr()));
        }

        builtin_optional & operator=(const builtin_optional & i_source)
        {
            swap(*this, builtin_optional(i_source));
        }

        builtin_optional & operator=(builtin_optional && i_source) noexcept
        {
            swap(*this, i_source);
        }

        TYPE *       operator->() const noexcept { return ptr(); }
        TYPE &       operator*() noexcept { return *ptr(); }
        const TYPE & operator*() const noexcept { return *ptr(); }

        explicit operator bool() const noexcept { return m_has_value; }

        ~builtin_optional()
        {
            if (m_has_value)
                ptr()->TYPE::~TYPE();
        }

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
                new (i_first.ptr()) TYPE(std::move(*i_second.ptr()));
            }
            else if (i_second.m_has_value)
            {
                new (i_second.ptr()) TYPE(std::move(*i_first.ptr()));
            }
        }

      private:
        TYPE *       ptr() noexcept { return reinterpret_cast<TYPE *>(&m_storage); }
        const TYPE * ptr() const noexcept { return reinterpret_cast<const TYPE *>(&m_storage); }

      private:
        typename std::aligned_storage<sizeof(TYPE), alignof(TYPE)>::type m_storage;
        bool                                                             m_has_value = false;
    };

    /** Alias to an implementation of optional. By default a minimal implementation of optional is used, but
        it can be replaced by the C++17 standard one, if available. */
    template <typename TYPE> using optional = builtin_optional<TYPE>;

    namespace detail
    {
        template <size_t VALUE> struct ConstSizeT
        {
            constexpr static size_t value = VALUE;
        };
    } // namespace detail

} // namespace density
