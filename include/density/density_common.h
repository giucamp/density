
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <atomic> // for std::memory_order
#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#include <density/density_config.h>

/*! \file */

/** Version of the library, in the format 0xMMMMNNRR, where MMMM = major version (16 bits), NN = minor version (8 bits), and RR = revision (8 bits) */
#define DENSITY_VERSION 0x00020000

// detect 'Relaxed constraints on constexpr functions / constexpr member functions and implicit const'
// see http://en.cppreference.com/w/User:D41D8CD98F/feature_testing_macros#C.2B.2B14
#if __cpp_constexpr >= 201304
#define DENSITY_CPP14_CONSTEXPR constexpr
#else
#define DENSITY_CPP14_CONSTEXPR
#endif

#if __cpp_noexcept_function_type >= 201510
#define DENSITY_CPP17_NOEXCEPT noexcept
#else
#define DENSITY_CPP17_NOEXCEPT
#endif

// DENSITY_NODISCARD
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(nodiscard) >= 201603
// clang-format off
#define DENSITY_NODISCARD [[nodiscard]]
// clang-format on
#endif
#endif
#ifndef DENSITY_NODISCARD
#define DENSITY_NODISCARD
#endif

/** namespace density */
namespace density
{
    /** string decimal variant of DENSITY_VERSION. The length of this string may change between versions.
            Example of value: "7.240.22" */
    constexpr char version[] = "2.0.0";

    /** Specifies whether a set of functions actually support concurrency */
    enum concurrency_cardinality
    {
        concurrency_single, /**< Functions with this concurrent cardinality can be called by only one thread,
                                            or by multiple threads if externally synchronized. */
        concurrency_multiple, /**< Multiple threads can call the functions with this concurrent cardinality
                                            without external synchronization. */
    };

    /** Specifies which guarantee is provided on the order in which actions on a data structure are observable. */
    enum consistency_model
    {
        consistency_relaxed, /**< The order in which actions happens (or are observable) is not defined and
                                  may vary from one thread to another. A single thread may observe its own
                                  actions out of order. */
        consistency_sequential, /**< A total ordering exists of all actions on a data structure. Given
                                    three actions A, B and C, if A happens before B, and B happens before
                                    C, the A happens before C. */
    };

    /** Specifies which guarantee an algorithm on a concurrent data struct provides about the progress and
        the completion of the work.

        Members are sorted so that lower values specifies weaker guarantee. Progress guarantees are cumulative:
        the guarantee G provides all the guarantees less than G.

        Dead locks and priority inversion may happen only in blocking algorithms.

        See https://en.wikipedia.org/wiki/Non-blocking_algorithm. */
    enum progress_guarantee
    {
        progress_blocking, /**< The calling thread may wait for other threads to finish their work. Blocking
                                algorithms usually protects shared data with a mutex. */
        progress_obstruction_free, /**< If all other threads are suspended, the calling thread is guaranteed
                                        to finish its work in a finite number of steps. */
        progress_lock_free, /**< In case of contention, in a finite number of steps at least one thread
                                 finishes the work. */
        progress_wait_free /**<  The calling thread completes the work in a finite number of steps,
                                 independently from the other threads. */
    };

    /** Specifies a set of features provided by the built-in type erasure system for callable objects */
    enum function_type_erasure
    {
        function_standard_erasure, /**< Callable objects can be invoked-destroyed (i.e. consumed), and just
                                        destroyed (i.e. discarded). No copy or move is supported. */
        function_manual_clear /**< Callable objects only support invoke-destroy (i.e. consume). Destruction
                                    without invocation is not supported.*/
    };

    // address functions

    /** Returns true whether the given unsigned integer number is a power of 2 (1, 2, 4, 8, ...)
    @param i_number must be > 0, otherwise the behavior is undefined */
    constexpr bool is_power_of_2(size_t i_number) noexcept
    {
        //DENSITY_ASSERT(i_number > 0);
        return (i_number & (i_number - 1)) == 0;
    }

    /** Returns true whether the given address has the specified alignment
        @param i_address address to be checked
        @param i_alignment must be > 0 and a power of 2 */
    inline bool address_is_aligned(const void * i_address, size_t i_alignment) noexcept
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));
        DENSITY_ASSUME(i_alignment > 0);
        return (reinterpret_cast<uintptr_t>(i_address) & (i_alignment - 1)) == 0;
    }

    /** Returns true whether the given unsigned integer has the specified alignment
        @param i_uint integer to be checked
        @param i_alignment must be > 0 and a power of 2 */
    template <typename UINT> inline bool uint_is_aligned(UINT i_uint, UINT i_alignment) noexcept
    {
        static_assert(
          std::numeric_limits<UINT>::is_integer && !std::numeric_limits<UINT>::is_signed,
          "UINT mus be an unsigned integer");
        DENSITY_ASSERT(is_power_of_2(i_alignment));
        DENSITY_ASSUME(i_alignment > 0);
        return (i_uint & (i_alignment - 1)) == 0;
    }

    /** Adds an offset to a pointer.
        @param i_address source address
        @param i_offset number to add to the address
        @return i_address plus i_offset */
    inline void * address_add(void * i_address, size_t i_offset) noexcept
    {
        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>(i_address);
        return reinterpret_cast<void *>(uint_pointer + i_offset);
    }

    /** Adds an offset to a pointer.
        @param i_address source address
        @param i_offset number to add to the address
        @return i_address plus i_offset */
    inline const void * address_add(const void * i_address, size_t i_offset) noexcept
    {
        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>(i_address);
        return reinterpret_cast<const void *>(uint_pointer + i_offset);
    }

    /** Subtracts an offset from a pointer
        @param i_address source address
        @param i_offset number to subtract from the address
        @return i_address minus i_offset */
    inline void * address_sub(void * i_address, size_t i_offset) noexcept
    {
        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>(i_address);
        DENSITY_ASSUME(uint_pointer >= i_offset);
        return reinterpret_cast<void *>(uint_pointer - i_offset);
    }

    /** Subtracts an offset from a pointer
        @param i_address source address
        @param i_offset number to subtract from the address
        @return i_address minus i_offset */
    inline const void * address_sub(const void * i_address, size_t i_offset) noexcept
    {
        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>(i_address);
        DENSITY_ASSUME(uint_pointer >= i_offset);
        return reinterpret_cast<const void *>(uint_pointer - i_offset);
    }

    /** Computes the unsigned difference between two pointers. The first must be above or equal to the second.
        @param i_end_address first address
        @param i_start_address second address
        @return i_end_address minus i_start_address */
    inline uintptr_t address_diff(const void * i_end_address, const void * i_start_address) noexcept
    {
        DENSITY_ASSUME(i_end_address >= i_start_address);

        const uintptr_t end_uint_pointer   = reinterpret_cast<uintptr_t>(i_end_address);
        const uintptr_t start_uint_pointer = reinterpret_cast<uintptr_t>(i_start_address);

        return end_uint_pointer - start_uint_pointer;
    }

    /** Returns the biggest aligned address lesser than or equal to a given address
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2.
        @return the aligned address */
    inline void * address_lower_align(void * i_address, size_t i_alignment) noexcept
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));
        DENSITY_ASSUME(i_alignment > 0);

        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>(i_address);

        const size_t mask = i_alignment - 1;

        return reinterpret_cast<void *>(uint_pointer & ~mask);
    }

    /** Returns the biggest aligned address lesser than or equal to a given address
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2.
        @return the aligned address */
    inline const void * address_lower_align(const void * i_address, size_t i_alignment) noexcept
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));
        DENSITY_ASSUME(i_alignment > 0);

        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>(i_address);

        const size_t mask = i_alignment - 1;

        return reinterpret_cast<void *>(uint_pointer & ~mask);
    }

    /** Returns the biggest address lesser than the first parameter, such that i_address + i_alignment_offset is aligned
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2
        @param i_alignment_offset alignment offset
        @return the result address */
    inline void *
      address_lower_align(void * i_address, size_t i_alignment, size_t i_alignment_offset) noexcept
    {
        void * address = address_add(i_address, i_alignment_offset);

        address = address_lower_align(address, i_alignment);

        address = address_sub(address, i_alignment_offset);

        return address;
    }

    /** Returns the biggest address lesser than the first parameter, such that i_address + i_alignment_offset is aligned
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2
        @param i_alignment_offset alignment offset
        @return the result address */
    inline const void * address_lower_align(
      const void * i_address, size_t i_alignment, size_t i_alignment_offset) noexcept
    {
        const void * address = address_add(i_address, i_alignment_offset);

        address = address_lower_align(address, i_alignment);

        address = address_sub(address, i_alignment_offset);

        return address;
    }

    /** Returns the smallest aligned address greater than or equal to a given address
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2.
        @return the aligned address */
    inline void * address_upper_align(void * i_address, size_t i_alignment) noexcept
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));
        DENSITY_ASSUME(i_alignment > 0);

        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>(i_address);

        const size_t mask = i_alignment - 1;

        return reinterpret_cast<void *>((uint_pointer + mask) & ~mask);
    }

    /** Returns the smallest aligned address greater than or equal to a given address
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2.
        @return the aligned address */
    inline const void * address_upper_align(const void * i_address, size_t i_alignment) noexcept
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));
        DENSITY_ASSUME(i_alignment > 0);

        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>(i_address);

        const size_t mask = i_alignment - 1;

        return reinterpret_cast<void *>((uint_pointer + mask) & ~mask);
    }

    /** Returns the smallest aligned integer greater than or equal to a given address
        @param i_uint integer to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2.
        @return the aligned address */
    template <typename UINT>
    constexpr UINT uint_upper_align(UINT i_uint, size_t i_alignment) noexcept
    {
        static_assert(
          std::numeric_limits<UINT>::is_integer && !std::numeric_limits<UINT>::is_signed,
          "UINT must be an unsigned integer");
        return (i_uint + (i_alignment - 1)) & ~(i_alignment - 1);
    }


    /** Returns the biggest aligned address lesser than or equal to a given address
        @param i_uint integer to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2.
        @return the aligned address */
    template <typename UINT>
    constexpr UINT uint_lower_align(UINT i_uint, size_t i_alignment) noexcept
    {
        static_assert(
          std::numeric_limits<UINT>::is_integer && !std::numeric_limits<UINT>::is_signed,
          "UINT must be an unsigned integer");
        return i_uint & ~(i_alignment - 1);
    }

    /** Returns the smallest address greater than the first parameter, such that i_address + i_alignment_offset is aligned
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2
        @param i_alignment_offset alignment offset
        @return the result address */
    inline void *
      address_upper_align(void * i_address, size_t i_alignment, size_t i_alignment_offset) noexcept
    {
        void * address = address_add(i_address, i_alignment_offset);

        address = address_upper_align(address, i_alignment);

        address = address_sub(address, i_alignment_offset);

        return address;
    }

    /** Returns the smallest address greater than the first parameter, such that i_address + i_alignment_offset is aligned
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2
        @param i_alignment_offset alignment offset
        @return the result address */
    inline const void * address_upper_align(
      const void * i_address, size_t i_alignment, size_t i_alignment_offset) noexcept
    {
        const void * address = address_add(i_address, i_alignment_offset);

        address = address_upper_align(address, i_alignment);

        address = address_sub(address, i_alignment_offset);

        return address;
    }

    /** \cond INTERNAL_DOC */

    namespace detail
    {
        // size_min: avoid including <algorithm> just to use std::min<size_t>
        constexpr inline size_t size_min(size_t i_first, size_t i_second) noexcept
        {
            return i_first < i_second ? i_first : i_second;
        }

        // size_max: avoid including <algorithm> just to use std::max<size_t>
        constexpr inline size_t size_max(size_t i_first, size_t i_second) noexcept
        {
            return i_first > i_second ? i_first : i_second;
        }
        constexpr inline size_t size_max(size_t i_first, size_t i_second, size_t i_third) noexcept
        {
            return size_max(size_max(i_first, i_second), i_third);
        }
        constexpr inline size_t
          size_max(size_t i_first, size_t i_second, size_t i_third, size_t i_fourth) noexcept
        {
            return size_max(size_max(i_first, i_second), size_max(i_third, i_fourth));
        }

        struct AlignmentHeader
        {
            void * m_block;
        };

        constexpr auto mem_relaxed =
          !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_relaxed;
        constexpr auto mem_acquire =
          !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_acquire;
        constexpr auto mem_release =
          !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_release;
        constexpr auto mem_acq_rel =
          !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_acq_rel;
        constexpr auto mem_seq_cst =
          !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_seq_cst;

        /** Computes the base2 logarithm of a size_t. If the argument is zero or is
            not a power of 2, the behavior is undefined. */
        constexpr size_t size_log2(size_t i_size) noexcept
        {
            return i_size <= 1 ? 0 : size_log2(i_size / 2) + 1;
        }

        /** This function is used to suppress warnings about constant conditional expressions
            without declaring useless variables. */
        inline bool ConstConditional(bool i_value) noexcept { return i_value; }

        struct ExternalBlock
        {
            void * m_block;
            size_t m_size, m_alignment;
        };

        /** \internal Provides the size of a type. Unlike the built-in sizeof operator, if the type is empty returns 0. */
        template <typename TYPE> struct size_of
        {
            static constexpr size_t value = std::is_empty<TYPE>::value ? 0 : sizeof(TYPE);
        };

        // old versions of libstdc++ define max_align_t only outside std::
#if defined(__GLIBCXX__)
        constexpr size_t MaxAlignment = alignof(max_align_t);
#else
        constexpr size_t MaxAlignment = alignof(std::max_align_t);
#endif

        /** internal macro - rethrows disabling the warning "function assumed not to throw an exception but does" 
            This macro is used by functions conditionally noexcept that has to hook and pass-through exceptions. */
#ifdef _MSC_VER
#define DENSITY_INTERNAL_RETHROW_FROM_NOEXCEPT                                                     \
    __pragma(warning(push)) __pragma(warning(disable : 4297)) throw;                               \
    __pragma(warning(pop))
#elif defined(__GNUG__) && __GNUG__ >= 6 && !defined(__clang__)
#define DENSITY_INTERNAL_RETHROW_FROM_NOEXCEPT                                                     \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wterminate\"") throw;        \
    _Pragma("GCC diagnostic pop")
#else
#define DENSITY_INTERNAL_RETHROW_FROM_NOEXCEPT throw;
#endif
    } // namespace detail

    /** \endcond */

    /** Uses the global operator new to allocate a memory block with at least the specified size and alignment
            @param i_size size of the requested memory block, in bytes
            @param i_alignment alignment of the requested memory block, in bytes
            @param i_alignment_offset offset of the block to be aligned, in bytes. The alignment is guaranteed only at i_alignment_offset
                from the beginning of the block.
            @return address of the new memory block

            \pre The behavior is undefined if either:
                - i_alignment is zero or it is not an integer power of 2
                - i_size is not a multiple of i_alignment
                - i_alignment_offset is greater than i_size

        A violation of any precondition results in undefined behavior.

        \n <b>Throws</b>: std::bad_alloc if the allocation fails
        \n <b>Progress guarantee</b>: the same of the built-in operator new (usually blocking)


        The content of the newly allocated block is undefined. */
    inline void * aligned_allocate(size_t i_size, size_t i_alignment, size_t i_alignment_offset = 0)
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));
        DENSITY_ASSUME(i_alignment > 0);
        DENSITY_ASSUME(i_alignment_offset <= i_size);

        void * user_block;
        if (i_alignment <= detail::MaxAlignment && i_alignment_offset == 0)
        {
            user_block = operator new(i_size);
        }
        else
        {
            // reserve an additional space in the block equal to the max(i_alignment, sizeof(AlignmentHeader) - sizeof(void*) )
            size_t const extra_size =
              detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
            size_t const actual_size  = i_size + extra_size;
            auto const complete_block = operator new(actual_size);
            user_block                = address_lower_align(
              address_add(complete_block, extra_size), i_alignment, i_alignment_offset);
            detail::AlignmentHeader & header =
              *(static_cast<detail::AlignmentHeader *>(user_block) - 1);
            header.m_block = complete_block;
            DENSITY_ASSERT_INTERNAL(
              user_block >= complete_block &&
              address_add(user_block, i_size) <= address_add(complete_block, actual_size));
        }
        return user_block;
    }

    /** Uses the global operator new to try to allocate a memory block with at least the specified size and alignment.
        Returns nullptr in case of failure.
            @param i_size size of the requested memory block, in bytes
            @param i_alignment alignment of the requested memory block, in bytes
            @param i_alignment_offset offset of the block to be aligned, in bytes. The alignment is guaranteed only at i_alignment_offset
                from the beginning of the block.
            @return address of the new memory block, or nullptr in case of failure

            \pre The behavior is undefined if either:
                - i_alignment is zero or it is not an integer power of 2
                - i_size is not a multiple of i_alignment
                - i_alignment_offset is greater than i_size

        A violation of any precondition results in undefined behavior.

        \n <b>Throws</b>: nothing
        \n <b>Progress guarantee</b>: the same of the built-in operator new (usually blocking)


        The content of the newly allocated block is undefined. */
    inline void * try_aligned_allocate(
      size_t i_size, size_t i_alignment, size_t i_alignment_offset = 0) noexcept
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));
        DENSITY_ASSUME(i_alignment > 0);
        DENSITY_ASSUME(i_alignment_offset <= i_size);

        void * user_block;
        if (i_alignment <= detail::MaxAlignment && i_alignment_offset == 0)
        {
            user_block = operator new(i_size, std::nothrow);
        }
        else
        {
            // reserve an additional space in the block equal to the max(i_alignment, sizeof(AlignmentHeader) - sizeof(void*) )
            size_t const extra_size =
              detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
            size_t const actual_size  = i_size + extra_size;
            auto const complete_block = operator new(actual_size, std::nothrow);
            if (complete_block != nullptr)
            {
                user_block = address_lower_align(
                  address_add(complete_block, extra_size), i_alignment, i_alignment_offset);
                detail::AlignmentHeader & header =
                  *(static_cast<detail::AlignmentHeader *>(user_block) - 1);
                header.m_block = complete_block;
                DENSITY_ASSERT_INTERNAL(
                  user_block >= complete_block &&
                  address_add(user_block, i_size) <= address_add(complete_block, actual_size));
            }
            else
            {
                user_block = nullptr;
            }
        }
        return user_block;
    }

    /** Deallocates a memory block allocated by aligned_allocate, using the global operator delete.
        After the call any access to the memory block results in undefined behavior.
            @param i_block block to deallocate, or nullptr.
            @param i_size size of the block to deallocate, in bytes.
            @param i_alignment alignment of the memory block.
            @param i_alignment_offset offset of the alignment

            \pre The behavior is undefined if either:
                - i_block is not a memory block allocated by the function allocate
                - i_size, i_alignment and i_alignment_offset are not the same specified when the block was allocated

        \n <b>Throws</b>: nothing
        \n <b>Progress guarantee</b>: the same of the built-in operator delete (usually blocking)


        If i_block is nullptr, the call has no effect. */
    inline void aligned_deallocate(
      void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset = 0) noexcept
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));
        DENSITY_ASSUME(i_alignment > 0);

        if (i_alignment <= detail::MaxAlignment && i_alignment_offset == 0)
        {
#if __cpp_sized_deallocation >= 201309
            operator delete(i_block, i_size); // since C++14
#else
            (void)i_size;
            operator delete(i_block);
#endif
        }
        else
        {
            if (i_block != nullptr)
            {
                const auto & header = *(static_cast<detail::AlignmentHeader *>(i_block) - 1);
#if __cpp_sized_deallocation >= 201309 // since C++14
                size_t const extra_size =
                  detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
                size_t const actual_size = i_size + extra_size;
                             operator delete(header.m_block, actual_size);
#else
                (void)i_size;
                operator delete(header.m_block);
#endif
            }
        }
    }

    // clang-format off
    /*!

    \mainpage overview Overview

Density Overview
----------------
Density is a C++ library providing very efficient concurrent heterogeneous queues, non-concurrent stacks, and a page-based memory manager.

Heterogeneous queues can store objects of any type:

\snippet overview_examples.cpp queue example 1

This table shows all the available queues:

concurrency strategy|function queue|heterogeneous queue|Consumers cardinality|Producers cardinality
--------------- |------------------ |--------------------|--------------------|--------------------
single threaded   |[function_queue](\ref density::function_queue)      |[heter_queue](\ref density::heter_queue)| - | -
locking         |[conc_function_queue](\ref density::conc_function_queue) |[conc_heter_queue](\ref density::conc_heter_queue)|multiple|multiple
lock-free       |[lf_function_queue](\ref density::lf_function_queue) |[lf_heter_queue](\ref density::lf_heter_queue)|single or multiple|single or multiple
spin-locking    |[sp_function_queue](\ref density::sp_function_queue) |[sp_heter_queue](\ref density::sp_heter_queue)|single or multiple|single or multiple

Elements are stored linearly in memory pages, achieving a great memory locality. 
Furthermore allocating an element most times means just adding the size to allocate to a pointer.

density provides a lifo_allocator built upon the page allocator, and a thread-local lifo memory pool (the *data-stack*), which has roughly
the [same performance](\ref lifo_array_benchmarks) of the unsafe, C-ish and non-standard [alloca](http://man7.org/linux/man-pages/man3/alloca.3.html).
The data stack can only be used indirectly with lifo_array and lifo_buffer.

This library is tested against these compilers:
    - Msc (Visual Studio 2017)
    - g++-4.9 and g++-7
    - clang++-4.0 and clang++-5.0

[Github Repository](https://github.com/giucamp/density)

<a name="lifo">The data stack</a>
--------------
The data-stack is a thread-local paged memory pool dedicated to *lifo* allocations. The lifo ordering implies that a thread may
reallocate or deallocate only the most recently allocated living block. A violation of this constraint causes undefined behavior.
The data stack is actually composed by a set of memory pages and an internal thread-local pointer (the top of the stack).
The underlying algorithm is very simple: allocations just add the requested size to the top pointer, and deallocations set the top
pointer to the block to deallocate. In case of page switch, the execution jumps to a non-inlined slow path.
The data stack has constant initialization, so it doesn't slow down thread creation or require dynamic any initialization guard
by the compiler on access. The first time a thread uses the data stack it allocates the first memory page.

The data stack can be accessed only indirectly, with [lifo_array](\ref density::lifo_array) and [lifo_buffer](\ref density::lifo_buffer).

<code>%lifo_array</code> is the easiest and safest way of using the data-stack. A <code>%lifo_array</code> is very
similar to a raw array, but its size is not a compile time constant. The elements are allocated on the data-stack, rather than on the
call-stack, so there is no risk of stack overflow. In case of out of system memory, a <code>std::bad_alloc</code> is thrown.

\snippet lifo_examples.cpp lifo_array example 1

To avoid breaking the lifo constraint and therefore causing the undefined behavior, <code>%lifo_array</code>s should be instantiated
only in the automatic storage (locally in a function or code block). Following this simple rule there is no way to way to break the
lifo constraint.

Actually it is possible instantiating a <code>lifo_array</code> anywhere, as long as the lifo constraint on the calling thread is not broken.
The C++ language is very LIFO friendly, as members of structs and elements of arrays are destroyed in the reverse order they
are constructed. Anyway this may be dangerous, so it's not recommended.

\snippet lifo_examples.cpp lifo_array example 4

Just like built-in arrays and <code>std::array</code>, <code>%lifo_array</code> does not initialize elements if they have
[POD type](https://stackoverflow.com/questions/146452/what-are-pod-types-in-c), unless an explicit initialization value is provided to the constructor.
This is a big difference with <code>std::vector</code>.

A <code>lifo_buffer</code> allocates on the data stack an untyped raw memory block with dynamic size. Unlike <code>%lifo_array</code> it supports
resizing, but only on the most recently instantiated living instance, and only if a more recent living <code>%lifo_array</code> doesn't exist.
If the resize changes the address of the block, the surviving content is preserved <code>memcpy</code>'ing it.

\snippet lifo_examples.cpp lifo example 1

Internally the data stack is a thread-local instance of \ref lifo_allocator, a class template that provides lifo memory management.
This class can be used to exploit performances of lifo memory in other contexts, but it is a low-level class: it should be wrapped in some
data structure, rather than used directly.

<a name="queues">About function queues</a>
--------------
A function queue is an heterogeneous FIFO pseudo-container that stores callable objects, each with a different type, but all invokable with the same signature.
Fundamentally a function queue is a queue of `std::function`-like objects that uses an in-page linear allocation for the storage of the captures.

\snippet misc_examples.cpp function_queue example 1

If the return type of the signature is `void`, the function `try_consume` returns a boolean indicating whether a function has been called. Otherwise it returns an `optional` containing the return value of the function, or empty if no function was present.
Any callable object can be pushed on the queue: a lambda, the result of an `std::bind`, a (possibly local) class defining an operator (), a pointer to member function or variable. The actual signature of the callable does not need to match the one of the queue, as long as an implicit conversion exists:

\snippet misc_examples.cpp function_queue example 3

All queues in density have a very simple implementation: a *tail pointer*, an *head pointer*, and a set of memory pages. Paging is a kind of quantization of memory: all pages have a fixed size (by default slightly less than 64 kibibyte) and a fixed alignment (by default 64 kibibyte), so that the allocator can be simple and efficient.
Values are arranged in the queue by *linear allocation*: allocating a value means just adding its size to the tail pointer. If the new tail pointer would point outside the last page, a new page is allocated, and the linear allocation is performed from there. In case of very huge values, only a pointer is allocated in the pages, and the actual storage for the value is allocated in the heap.


Transactional puts and raw allocations
------------------------------------

The functions `function_queue::push` and `function_queue::emplace` append a callable at the end of the queue. This is the quick way of doing a *put transaction*. We can have more control breaking it using the **start_** put functions:

\snippet misc_examples.cpp function_queue example 4

The start_* put functions return a [put_transaction](classdensity_1_1heter__queue_1_1put__transaction.html) by which the caller can:

 - access the object being pushed before it becomes observable
 - [commit](classdensity_1_1heter__queue_1_1put__transaction.html#a96491d550e91a5918050bfdafe43a72c) the transaction so that the element becomes observable to consumers (the `put_transaction` becomes empty).
 - [cancel](classdensity_1_1heter__queue_1_1put__transaction.html#a209a4e37e25451c144910d6f6aa4911e) the transaction, in which case the transaction is discarded with no observable side effects (the `put_transaction` becomes empty).
 - allocate *raw memory blocks*, that is uninitialized memory, or arrays of a trivially destructible type (`char`s, `float`s) that are linearly allocated in the pages, right after the last allocated value. There is no function to deallocate raw blocks: they are automatically deallocated after the associated element has been canceled or consumed.

When a non-empty `put_transaction` is destroyed, the bound transaction is canceled. As a consequence, if the `raw_allocate_copy` in the code above throws an exception, the transaction is discarded with no side effects.

Raw memory blocks are handled in the same way of canceled and consumed values (they are referred as *dead* values). Internally the storage of dead values is deallocated when the whole page is returned to the page allocator, but this is an implementation detail. When a consume is committed, the head pointer is advanced, skipping any dead element.

Internally instant puts are implemented in terms of transactional puts, so there is no performance difference between them:

<code>
    // ... = omissis\n
    template <...> void emplace(...)\n
    {\n
        start_emplace(...).commit();\n
    }\n
</code>


Consume operations have the `start_*` variant in heterogeneous queues (but not in function queues). Anyway this operation is not a transaction, as the element disappears from the queue when the operation starts, and will reappear if the operation is canceled.

Reentrancy
----------
During a put or a consume operation an heterogeneous or function queue is not in a consistent state *for the caller thread*. So accessing the queue in any way,
in the between of a start_* function and the cancel/commit, causes undefined behavior.
Anyway, especially for consumers, reentrancy is sometimes necessary: a callable object, during the invocation, may need to push another callable object to the same queue.
For every put or consume function, in every queue, there is a reentrant variant.

\snippet conc_func_queue_examples.cpp conc_function_queue try_reentrant_consume example 1

If an operation is not reentrant the implementation can do some optimizations: single-thread queues start puts in the committed state, so that the commit is a no-operation.
Furthermore reentrancy can affect thread synchronization: `conc_function_queue` and `conc_heter_queue`, when executing a non-reentrant operation, lock their internal mutex
when starting, and unlock it when committing or canceling. In contrast, when they execute a reentrant operation, they have to lock and unlock when starting, and lock and
unlock again when committing or canceling. The internal mutex is not recursive, so if a thread starts a non-reentrant operation and then tries to access the queue, it
causes undefined behavior.

Relaxed guarantees
------------
Function queues use type erasure to handle callable objects of heterogeneous types. By default two operations are captured for each element type: invoke-destroy, and just-destroy.
The third template parameter of all function queues is an enum of type [function_type_erasure](namespacedensity.html#a80100b808e35e98df3ffe74cc2293309) that controls the type erasure: the value function_manual_clear excludes the second operation, so that the function queue will not be able to destroy a callable without invoking it. This gives a performance benefit, at the price that the queue can't be cleared, and that the user must ensure that the queue is empty when destroyed.
Internally, in manual-clean mode, the layout of a value in the function queue is composed by:

 - an overhead pointer (that points to the next value, and keeps the state of the value in the least significant bits)
 - the runtime type, actually a pointer to the invoke-destroy function
 - the eventual capture

So if you put a capture-less lambda or a pointer to a function, you are advancing the tail pointer by the space required by 2 pointers.
Anyway lock-free queues and spin-locking queues align their values to [density::destructive_interference_size](namespacedensity.html#ae8f72b2dd386b61bf0bc4f30478c2941), so they are less dense than the other queues.

All queues but `function_queue` and `heter_queue` are concurrency enabled. By default they allow multiple producers and multiple consumers.
The class templates [lf_function_queue](classdensity_1_1lf__function__queue.html), [lf_hetr_queue](classdensity_1_1lf__heter__queue.html), [sp_function_queue](classdensity_1_1sp__function__queue.html) and [sp_hetr_queue](classdensity_1_1sp__heter__queue.html) allow to specify, with 2 independent template arguments of type [concurrency_cardinality](namespacedensity.html#aeef74ec0c9bea0ed2bc9802697c062cb), whether multiple threads are allowed to produce, and whether multiple threads are allowed to consume:

\snippet misc_examples.cpp lf_function_queue cardinality example

When dealing with a multiple producers, the tail pointer is an atomic variable. Otherwise it is a plain variable.
When dealing with a multiple consumers, the head pointer is an atomic variable. Otherwise it is a plain variable.

The class templates [lf_function_queue](classdensity_1_1lf__function__queue.html) and [lf_hetr_queue](classdensity_1_1lf__heter__queue.html) allow a further optimization with a template argument of type [consistency_model](namespacedensity.html#ad5d59321f5f1b9a040c6eb9bc500a051): by default the queue is sequential consistent (that is all threads observe the operations happening in the same order). If \ref consistency_relaxed is specified, this guarantee is removed, with a great performance benefit.

For all queues, the functions `try_consume` and `try_reentrant_consume` have 2 variants:

- one returning a consume operation. If the queue was empty, an empty consume is returned.
- one taking a reference to a consume as parameter and returning a boolean, recommended if a thread performs many consecutive consumes.

There is no functional difference between the two consumes. Anyway, currently only for lock-free and spin-locking queues supporting multi-producers,
the second consume can be much faster. The reason has to do with the way they ensure that a consumer does not deallocate a page while another consumer
is reading it.
When a consumer needs to access a page, it increments a ref-count in the page (it *pins* the page), to notify to the allocator that it is using it.
When it has finished, the consumer decrements the ref-count (it *unpins* the page). If a thread performs many consecutive consumes, it will ends up
doing many atomic increments and decrements of the same page (which is a somewhat expensive operation). Since the pin\unpin logic is encapsulated in
the <code>consume_operation</code>, if the consumer thread keeps the <code>consume_operation</code> alive, pinning and unpinning will be performed only in case of
page switch.
Note: a forgotten <code>consume_operation</code> which has pinned a page prevents the page from being recycled by the page allocator, even if it was
deallocated by another consumer.

Heterogeneous queues
--------------------
Every function queue is actually an adaptor for the corresponding heterogeneous pseudo-container.  Heterogeneous queues have put and consume functions, just like function queues, but elements are not required to be callable objects.

\snippet heter_queue_examples.cpp heter_queue put example 1

The first 3 template parameters are the same for all the heterogeneous queues.

Template parameter            |Meaning  |Constraints|Default  |
------------------------------|---------|-----------|---------|
typename `COMMON_TYPE`|Common type of elements|Must decay to itself (see [std::decay](http://en.cppreference.com/w/cpp/types/decay))|`void`|
typename `RUNTIME_TYPE`|Type eraser type|Must model [RuntimeType](RuntimeType_requirements.html)|[runtime_type](classdensity_1_1runtime__type.html)|
typename `ALLOCATOR_TYPE`|Allocator|Must model both [PageAllocator](PagedAllocator_requirements.html) and [UntypedAllocator](UntypedAllocator_requirements.html)|[default_allocator](namespacedensity.html#a06c6ce21f0d3cede79e2b503a90b731e)|

An element can be pushed on a queue if its address is is implicitly convertible to `COMMON_TYPE*`. By default any type is allowed in the queue.
The `RUNTIME_TYPE` type allows much more customization than the [function_type_erasure](namespacedensity.html#a80100b808e35e98df3ffe74cc2293309) template parameter of function queues. Even using the built-in [runtime_type](classdensity_1_1runtime__type.html) you can select which operations the elements of the queue should support, and add your own.

\snippet misc_examples.cpp runtime_type example 2
\snippet misc_examples.cpp runtime_type example 3

Output:

~~~~~~~~~~~~~~
ObjectA::update(0.0166667)
ObjectB::update(0.0166667)
ObjectB::update(0.0166667)
~~~~~~~~~~~~~~

Implementation details
----------
<a href="implementation.pdf" target="_blank">This document</a> describes some of the internals of density.

Named requirements
----------

[RuntimeType](RuntimeType_requirements.html) | [runtime_type](classdensity_1_1runtime__type.html)
[TypeFeature](TypeFeature_requirements.html) | f_size, f_alignment, f_copy_construct, f_hash, f_rtti
[UntypedAllocator](UntypedAllocator_requirements.html) | [default_allocator](namespacedensity.html#a06c6ce21f0d3cede79e2b503a90b731e), [basic_default_allocator](classdensity_1_1basic__void__allocator.html)
[PagedAllocator](PagedAllocator_requirements.html) | [default_allocator](namespacedensity.html#a06c6ce21f0d3cede79e2b503a90b731e), [basic_default_allocator](classdensity_1_1basic__void__allocator.html)

Benchmarks
----------

\ref function_queue_benchmarks

\ref lifo_array_benchmarks

    */

    /*! \page function_queue_benchmarks

        Benchmarks of the function queues
        ----------
        ----------

        In this single-thread tests all the function queues of density are compared to a <code>std::vector</code> and a <code>std::queue</code>
        of <code>std::function</code>s.
        In every test a function queue is created, filled by many trivial functions, and then all the functions are consumed on-by-one. Only
        in the case of <code>std::vector</code> the queue is cleared in one time, rather than removing the functions one by one while they
        are invoked. This is because there is no constant-time function to remove the first element of a <code>std::vector</code>.

        In the first test the function queues of density are instantiated with the default template parameters.
        In the second test they exploit all the available optimizations, that is:

        - \ref function_manual_clear, so that every element is actually a single pointer (plus an internal overhead pointer).
        - \ref concurrency_single, when it's applicable
        - \ref consistency_relaxed, when it's applicable

        Summary of the results
        ----------
        All the tests are compiled with Visual Studio 2017 (version 15.1), and executed on Windows 10 with an int Intel(R) Core(TM) i7-7700HQ CPU @ 2.80GHz,
        2808 Mhz, 4 cores, 8 logical processors. The benchmarks of the whole library are shuffled and interleaved at the granularity of a single invocation
        with a given cardinality, and every invocation occurs 8 times.


        Even unnecessarily paying the cost of multithreading, concurrent function queues are in general better than <code>std::queue</code> of <code>std::function</code>.
        lf_function_queue is much faster in the case of single consumer and single producers, as it can reduce considerably the number of
        atomic memory operations.

        Results of the first test
        ----------

        \image html func_queue_st_b1.png

        Results of the second test
        ----------

        \image html func_queue_st_b2.png
    */

    /*! \page test_bench

        Building
        ----------
        ----------
        The directory 'test' contains a test program that you can build with cmake.
        The directory 'bench' contains a benchmark program that you can build with cmake.
        On Windows you can also build both at once with Visual Studio 2017 using the solution test/vs2017/density_tests.sln.

        Running the tests
        ----------
        ----------
        The test program executes a number of test on all the data structures of the library. When doing massive multithread
        tests, it scales to the host machine spawning 3 threads for each logical cpu of the system.

        The test program supports the following command-line parameters:

            - rand_seed:<32-bit unsigned integer> - default is zero. Seed used to initialize the main random generator.
                If it is zero the random generator is initialized with <code>std::ranom_device</code>. In multi-thread test
                the random generator for the spawned threads is forked from the main one.

            - exceptions:<0 or 1> - default is 1. If non-zero enables the testing of exceptional execution paths. This is
                done using test allocators and test objects that throw an exception once in every point that may possibly
                throw.

            - spare_one_cpu:<0 or 1> - default is 1. If non-zero in multi-thread tests the second logical processor is not used.

            - print_progress:<0 or 1> - default is 1. If non-zero shows a percentage of the progress during queue test

            - test_allocators:<0 or 1> - default is 1. If non-zero enables special allocators that detect bugs in the memory
                management. The tests with the normal allocators are executed in any case.

            - queue_tests_cardinality:<size_t> - default is 2000. Number of elements to push in every queue tests. In multi-thread
                tests this number is partitioned among all threads.

        If the test program is built with the macro DENSITY_USER_DATA_STACK defined, the normal data stack is replaced with a test
        data stack that checks for bug in the memory management.

        Example:

        ~~~~~~~~~~~~~~
        density_test -rand_seed:22 -exceptions:1 -spare_one_cpu:1 -test_allocators:1 -queue_tests_cardinality:2000
        ~~~~~~~~~~~~~~

        Running the benchmarks
        ----------
        ----------
        The benchmark program needs to read the source files from which it is compiled. It uses the macro \__FILE__ to locate them,
        which generally has a path relative to the project file. So if you execute the program from another location, you have to
        provide a source directory.

        The benchmark program supports the following command-line parameters:

            -source:<string> - by default empty, max length = 4095. Path to prepend to the __FILE__ macro to access the source files.

   */
    // clang-format on
} // namespace density
