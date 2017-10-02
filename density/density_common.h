
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <type_traits>
#include <limits>
#include <cstddef>
#include <utility>
#include <atomic> // for std::memory_order
#include <new>

#define DENSITY_VERSION            0x00100000

#include <density/density_config.h>

/** namespace density */
namespace density
{
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
        DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));
        return (reinterpret_cast<uintptr_t>(i_address) & (i_alignment - 1)) == 0;
    }

    /** Returns true whether the given unsigned integer has the specified alignment
        @param i_uint integer to be checked
        @param i_alignment must be > 0 and a power of 2 */
    template <typename UINT>
        inline bool uint_is_aligned(UINT i_uint, UINT i_alignment) noexcept
    {
        static_assert(std::numeric_limits<UINT>::is_integer && !std::numeric_limits<UINT>::is_signed, "UINT mus be an unsigned integer");
        DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));
        return (i_uint & (i_alignment - 1)) == 0;
    }

    /** Adds an offset to a pointer.
        @param i_address source address
        @param i_offset number to add to the address
        @return i_address plus i_offset */
    inline void * address_add( void * i_address, size_t i_offset ) noexcept
    {
        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );
        return reinterpret_cast< void * >( uint_pointer + i_offset );
    }
    inline const void * address_add( const void * i_address, size_t i_offset ) noexcept
    {
        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );
        return reinterpret_cast< const void * >( uint_pointer + i_offset );
    }

    /** Subtracts an offset from a pointer
        @param i_address source address
        @param i_offset number to subtract from the address
        @return i_address minus i_offset */
    inline void * address_sub( void * i_address, size_t i_offset ) noexcept
    {
        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );
        DENSITY_ASSERT( uint_pointer >= i_offset );
        return reinterpret_cast< void * >( uint_pointer - i_offset );
    }
    inline const void * address_sub( const void * i_address, size_t i_offset ) noexcept
    {
        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );
        DENSITY_ASSERT( uint_pointer >= i_offset );
        return reinterpret_cast< const void * >( uint_pointer - i_offset );
    }

    /** Computes the unsigned difference between two pointers. The first must be above or equal to the second.
        @param i_end_address first address
        @param i_start_address second address
        @return i_end_address minus i_start_address */
    inline uintptr_t address_diff( const void * i_end_address, const void * i_start_address ) noexcept
    {
        DENSITY_ASSERT( i_end_address >= i_start_address );

        const uintptr_t end_uint_pointer = reinterpret_cast<uintptr_t>( i_end_address );
        const uintptr_t start_uint_pointer = reinterpret_cast<uintptr_t>( i_start_address );

        return end_uint_pointer - start_uint_pointer;
    }

    /** Returns the biggest aligned address lesser than or equal to a given address
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2.
        @return the aligned address */
    inline void * address_lower_align( void * i_address, size_t i_alignment ) noexcept
    {
        DENSITY_ASSERT( i_alignment > 0 && is_power_of_2( i_alignment ) );

        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

        const size_t mask = i_alignment - 1;

        return reinterpret_cast< void * >( uint_pointer & ~mask );
    }
    inline const void * address_lower_align( const void * i_address, size_t i_alignment ) noexcept
    {
        DENSITY_ASSERT( i_alignment > 0 && is_power_of_2( i_alignment ) );

        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

        const size_t mask = i_alignment - 1;

        return reinterpret_cast< void * >( uint_pointer & ~mask );
    }

    /** Returns    the biggest address lesser than the first parameter, such that i_address + i_alignment_offset is aligned
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2
        @param i_alignment_offset alignment offset
        @return the result address */
    inline void * address_lower_align( void * i_address, size_t i_alignment, size_t i_alignment_offset ) noexcept
    {
        void * address = address_add( i_address, i_alignment_offset );

        address = address_lower_align( address, i_alignment );

        address = address_sub( address, i_alignment_offset );

        return address;
    }
    inline const void * address_lower_align( const void * i_address, size_t i_alignment, size_t i_alignment_offset ) noexcept
    {
        const void * address = address_add( i_address, i_alignment_offset );

        address = address_lower_align( address, i_alignment );

        address = address_sub( address, i_alignment_offset );

        return address;
    }

    /** Returns the smallest aligned address greater than or equal to a given address
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2.
        @return the aligned address */
    inline void * address_upper_align( void * i_address, size_t i_alignment ) noexcept
    {
        DENSITY_ASSERT( i_alignment > 0 && is_power_of_2( i_alignment ) );

        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

        const size_t mask = i_alignment - 1;

        return reinterpret_cast< void * >( ( uint_pointer + mask ) & ~mask );
    }
    inline const void * address_upper_align( const void * i_address, size_t i_alignment ) noexcept
    {
        DENSITY_ASSERT( i_alignment > 0 && is_power_of_2( i_alignment ) );

        const uintptr_t uint_pointer = reinterpret_cast<uintptr_t>( i_address );

        const size_t mask = i_alignment - 1;

        return reinterpret_cast< void * >( ( uint_pointer + mask ) & ~mask );
    }

    template <typename UINT>
        constexpr UINT uint_upper_align(UINT i_uint, size_t i_alignment) noexcept
    {
        static_assert(std::numeric_limits<UINT>::is_integer && !std::numeric_limits<UINT>::is_signed, "UINT must be an unsigned integer");
        return (i_uint + (i_alignment - 1)) & ~(i_alignment - 1);
    }

    template <typename UINT>
        constexpr UINT uint_lower_align(UINT i_uint, size_t i_alignment) noexcept
    {
        static_assert(std::numeric_limits<UINT>::is_integer && !std::numeric_limits<UINT>::is_signed, "UINT must be an unsigned integer");
        return i_uint & ~(i_alignment - 1);
    }

    /** Returns the smallest address greater than the first parameter, such that i_address + i_alignment_offset is aligned
        @param i_address address to be aligned
        @param i_alignment alignment required from the pointer. It must be an integer power of 2
        @param i_alignment_offset alignment offset
        @return the result address */
    inline void * address_upper_align( void * i_address, size_t i_alignment, size_t i_alignment_offset ) noexcept
    {
        void * address = address_add( i_address, i_alignment_offset );

        address = address_upper_align( address, i_alignment );

        address = address_sub( address, i_alignment_offset );

        return address;
    }
    inline const void * address_upper_align( const void * i_address, size_t i_alignment, size_t i_alignment_offset ) noexcept
    {
        const void * address = address_add( i_address, i_alignment_offset );

        address = address_upper_align( address, i_alignment );

        address = address_sub( address, i_alignment_offset );

        return address;
    }

    /** Returns whether two memory ranges overlap */
    inline bool address_overlap( const void * i_first, size_t i_first_size, const void * i_second, size_t i_second_size ) noexcept
    {
        if( i_first < i_second )
            return address_add( i_first, i_first_size ) > i_second;
        else
            return address_add( i_second, i_second_size ) > i_first;
    }

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
        constexpr inline size_t size_max(size_t i_first, size_t i_second, size_t i_third, size_t i_fourth) noexcept
        {
            return size_max(size_max(i_first, i_second, i_third), i_fourth);
        }

        struct AlignmentHeader
        {
            void * m_block;
        };

        inline bool mem_equal(const void * i_start, size_t i_size, unsigned char i_value) noexcept
        {
            auto const chars = static_cast<const unsigned char *>(i_start);
            for (size_t char_index = 0; char_index < i_size; char_index++)
            {
                if (chars[char_index] != i_value)
                {
                    return false;
                }
            }
            return true;
        }

        constexpr auto mem_relaxed = !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_relaxed;
        constexpr auto mem_acquire = !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_acquire;
        constexpr auto mem_release = !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_release;
        constexpr auto mem_acq_rel = !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_acq_rel;
        constexpr auto mem_seq_cst = !enable_relaxed_atomics ? std::memory_order_seq_cst : std::memory_order_seq_cst;

        /** Computes the base2 logarithm of a size_t. If the argument is zero or is
            not a power of 2, the behavior is undefined. */
        constexpr size_t size_log2(size_t i_size) noexcept
        {
            return i_size <= 1 ? 0 : size_log2(i_size / 2) + 1;
        }

        /** \internal Utility that provides RAII pinning\unpinning of a memory page */
        template <typename ALLOCATOR_TYPE>
            class PinGuard
        {
        private:
            ALLOCATOR_TYPE * const m_allocator;
            void * m_pinned_page = nullptr;

        public:
            PinGuard(ALLOCATOR_TYPE * i_allocator) noexcept
                : m_allocator(i_allocator)
            {
            }

            PinGuard(ALLOCATOR_TYPE * i_allocator, void * i_address) noexcept
                : m_allocator(i_allocator), m_pinned_page(address_lower_align(i_address, ALLOCATOR_TYPE::page_alignment))
            {
                if (m_pinned_page != nullptr)
                    m_allocator->pin_page(m_pinned_page);
            }

            PinGuard(const PinGuard &) = delete;
            PinGuard & operator = (const PinGuard &) = delete;

            bool pin_new(void * i_address) noexcept
            {
                auto const page = address_lower_align(i_address, ALLOCATOR_TYPE::page_alignment);
                if (page != m_pinned_page)
                {
                    if (m_pinned_page != nullptr)
                        m_allocator->unpin_page(m_pinned_page);
                    m_pinned_page = page;
                    if (m_pinned_page != nullptr)
                        m_allocator->pin_page(m_pinned_page);
                    return true;
                }
                else
                {
                    return false;
                }
            }

            ~PinGuard()
            {
                if (m_pinned_page != nullptr)
                    m_allocator->unpin_page(m_pinned_page);
            }
        };


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
    }



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

        \exception std::bad_alloc if the allocation fails

        The content of the newly allocated block is undefined. */
    inline void * aligned_allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0)
    {
        DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));
        DENSITY_ASSERT(i_alignment_offset <= i_size);

        // if this function is inlined, and i_alignment is constant, the allocator should simplify much of this function
        void * user_block;
        if (i_alignment <= alignof(std::max_align_t) && i_alignment_offset == 0)
        {
            user_block = operator new (i_size);
        }
        else
        {
            // reserve an additional space in the block equal to the max(i_alignment, sizeof(AlignmentHeader) - sizeof(void*) )
            size_t const extra_size = detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
            size_t const actual_size = i_size + extra_size;
            auto const complete_block = operator new (actual_size);
            user_block = address_lower_align(address_add(complete_block, extra_size), i_alignment, i_alignment_offset);
            detail::AlignmentHeader & header = *(static_cast<detail::AlignmentHeader*>(user_block) - 1);
            header.m_block = complete_block;
            DENSITY_ASSERT_INTERNAL(user_block >= complete_block &&
                address_add(user_block, i_size) <= address_add(complete_block, actual_size));
        }
        return user_block;
    }

    /** Uses the global operator new to try to allocate a memory block with at least the specified size and alignment
        Currently only blocking allocations are supported: if i_progress_guarantee is not progress_blocking, this function
        always returns nullptr.
            @param i_progress_guarantee progress guarantee to provide
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

        \exception none

        The content of the newly allocated block is undefined. */
    inline void * try_aligned_allocate(progress_guarantee i_progress_guarantee,
        size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0) noexcept
    {
        DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));
        DENSITY_ASSERT(i_alignment_offset <= i_size);

        if (i_progress_guarantee != progress_blocking)
        {
            return nullptr;
        }

        // if this function is inlined, and i_alignment is constant, the allocator should simplify much of this function
        void * user_block;
        if (i_alignment <= alignof(std::max_align_t) && i_alignment_offset == 0)
        {
            user_block = operator new (i_size, std::nothrow);
        }
        else
        {
            // reserve an additional space in the block equal to the max(i_alignment, sizeof(AlignmentHeader) - sizeof(void*) )
            size_t const extra_size = detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
            size_t const actual_size = i_size + extra_size;
            auto const complete_block = operator new (actual_size, std::nothrow);
            if (complete_block != nullptr)
            {
                user_block = address_lower_align(address_add(complete_block, extra_size), i_alignment, i_alignment_offset);
                detail::AlignmentHeader & header = *(static_cast<detail::AlignmentHeader*>(user_block) - 1);
                header.m_block = complete_block;
                DENSITY_ASSERT_INTERNAL(user_block >= complete_block &&
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

        If i_block is nullptr, the call has no effect. */
    inline void aligned_deallocate(void * i_block, size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0) noexcept
    {
        DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));

        if (i_alignment <= alignof(std::max_align_t) && i_alignment_offset == 0)
        {
            #if __cplusplus >= 201402L
                operator delete (i_block, i_size); // since C++14
            #else
                (void)i_size;
                operator delete (i_block);
            #endif
        }
        else
        {
            if(i_block != nullptr)
            {
                const auto & header = *( static_cast<detail::AlignmentHeader*>(i_block) - 1 );
                #if __cplusplus >= 201402L // since C++14
                    size_t const extra_size = detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
                    size_t const actual_size = i_size + extra_size;
                    operator delete (header.m_block, actual_size);
                #else
                    (void)i_size;
                    operator delete (header.m_block);
                #endif
            }
        }
    }

    /*!

    \page intro Intro

Density Overview
----------------
Density is a C++11 header-only library focused on paged memory management. providing a rich set of highly configurable heterogeneous queues.
concurrency strategy|function queue|heterogeneous queue|Consumers cardinality|Producers cardinality
--------------- |------------------ |--------------------|--------------------|--------------------
single threaded   |[function_queue](classdensity_1_1function__queue.html)      |[heter_queue](classdensity_1_1heter__queue.html)| - | -
locking         |[conc_function_queue](classdensity_1_1conc__function__queue.html) |[conc_hetr_queue](classdensity_1_1conc__heter__queue.html)|multiple|multiple
lock-free       |[lf_function_queue](classdensity_1_1lf__function__queue.html) |[lf_hetr_queue](classdensity_1_1lf__heter__queue.html)|configurable|configurable
spin-locking    |[sp_function_queue](classdensity_1_1sp__function__queue.html) |[sp_hetr_queue](classdensity_1_1sp__heter__queue.html)|configurable|configurable

All function queues have a common interface, and so all heterogeneous queues. Some queues have specific extensions: an example is heter_queue supporting iteration, or lock-free and spin-locking queues supporting try_* functions with parametric progress guarantee.
A lifo memory management system is provided too, built upon the paged management.

[![Build Status](https://travis-ci.org/giucamp/density.svg?branch=master)](https://travis-ci.org/giucamp/density)

About function queues
--------------
A function queue is an heterogeneous FIFO pseudo-container that stores callable objects, each with a different type, but all invokable with the same signature.
Foundamentaly a funcion queue is a queue of `std::function`-like objects that uses an in-page linear allocation for the storage of the captures.

\snippet misc_examples.cpp function_queue example 1

If the return type of the signature is `void`, the function `try_consume` returns a boolean indicating whether a function has been called. Otherwise it returns an `optional` containing the return value of the function, or empty if no function was present.
Any callable object can be pushed on the queue: a lambda, the result of an `std::bind`, a (possibly local) class defining an operator (), a pointer to member function or variable. The actual signature of the callable does not need to match the one of the queue, as long as an implicit conversion exists:

\snippet misc_examples.cpp function_queue example 3

All queues in density have a very simple implementation: a *tail pointer*, an *head pointer*, and a set of memory pages. Paging is a kind of quantization of memory: all pages have a fixed size (by default slightly less than 64 kibibyte) and a fixed alignment (by default 64 kibibyte), so that the allocator can be simple and efficient. 
Values are arranged in the queue by *linear allocation*: allocating a value means just adding its size to the tail pointer. If the new tail pointer would point outside the last page, a new page is allocated, and the linear allocation is performed from there. In case of very huge values, only a pointer is allocated in the pages, and the actual storage for the value is allocated in the heap.


Transactional puts and raw allocations
------------------------------------

The functions `queue::push` and `function_queue::emplace` append a callable at the end of the queue. This is the quick way of doing a *put transaction*. We can have more control breaking it using the **start_** put functions:

\snippet misc_examples.cpp function_queue example 4

The start_* put functions return a [put_transaction](classdensity_1_1heter__queue_1_1put__transaction.html) by which the caller can:

 - access or alter the object being pushed before it becomes observable
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
During a put or a consume operation an heterogeneous or function queue is not in a consistent state *for the caller thread*. So accessing the queue in any way, in the between of a start_* function and the cancel/commit, causes undefined behavior. Also accessing the queue from the constructor of an element, or from the invocation of a callable of a function queue, causes undefined behavior.
Anyway in this time span the queue is not in an inconsistent state for the other threads, provided that the queue is concurrency-enabled. This may appear weird, but think to the queues protected by a mutex (`conc_function_queue` and `conc_heter_queue`): reentrancy would mean a double lock by the same thread, which causes undefined behaviour (unless the mutex is recursive, which is not). On the other hand other threads can access the queue: they will eventually block in a lock. Single threaded queues also exploit non-reentrancy to do some minor optimizations (internally they set the transaction as committed when it is started, so that the commit does not write the control bit again).
Anyway, especially for consumers, reentrancy is sometimes necessary: a callable object, during the invocation, may need to push another callable object to the same queue. For every put or consume function, in every queue, there is a reentrant variant.

\snippet conc_func_queue_examples.cpp conc_function_queue try_reentrant_consume example 1

When doing reentrant operations,`conc_function_queue` and `conc_heter_queue` lock the mutex once when starting, and once when committing or canceling. In the middle of the operation the mutex is not locked.

Relaxed guarantees
------------
Function queues use type erasure to handle callable objects of heterogeneous types. By default two operations are captured for each element type: invoke-destroy, and just-destroy.
The third template parameter of all function queues is an enum of type [function_type_erasure](namespacedensity.html#a80100b808e35e98df3ffe74cc2293309) that controls the type erasure: the value function_manual_clear excludes the second operation, so that the function queue will not be able to destroy a callable without invoking it. This gives a performance benefit, at the price that the queue can't be cleared, and that the user must ensure that the queue is empty when destroyed.
Internally, in manual-clean mode, the layout of a value in the function queue is composed by:

 - an overhead pointer (that points to the next value, and keeps the state of the value in the least significant bits)
 - the runtime type, actually a pointer to the invoke-destroy function
 - the eventual capture

So if you put a captureless lambda or a pointer to a function, you are advancing the tail pointer by the space required by 2 pointers.
Anyway lock-free queues and spin-locking queues align their values to [density::concurrent_alignment](namespacedensity.html#ae8f72b2dd386b61bf0bc4f30478c2941), so they are less dense than the other queues.

All queues but `function_queue` and `heter_queue` are concurrency enabled. By default they allow multiple producers and multiple consumers.
The class templates [lf_function_queue](classdensity_1_1lf__function__queue.html), [lf_hetr_queue](classdensity_1_1lf__heter__queue.html), [sp_function_queue](classdensity_1_1sp__function__queue.html) and [sp_hetr_queue](classdensity_1_1sp__heter__queue.html) allow to specify, with 2 independent template arguments of type [concurrency_cardinality](namespacedensity.html#aeef74ec0c9bea0ed2bc9802697c062cb), whether multiple threads are allowed to produce, and whether multiple threads are allowed to consume:

\snippet misc_examples.cpp lf_function_queue cardinality example

When dealing with a multiple producers, the tail pointer is an atomic variable. Otherwise it is a plain variable.
When dealing with a multiple consumers, the head pointer is an atomic variable. Otherwise it is a plain variable.

The class templates [lf_function_queue](classdensity_1_1lf__function__queue.html) and [lf_hetr_queue](classdensity_1_1lf__heter__queue.html) allow a further optimization with a template argument of type [consistency_model](namespacedensity.html#ad5d59321f5f1b9a040c6eb9bc500a051): by default the queue is sequential consistent (that is all threads observe the operations happening in the same order). If \ref consistency_relaxed is specified, this guarantee is removed, with a great performance benefit.

For all queues, the functions `try_consume` and `try_reentrant_consume` have 2 variants:

- one returning a consume operation. If the queue was empty, an empty consume is returned.
- one taking a reference to a consume as parameter and returning a boolean, raccomanded if a thread performs many consecutive consumes.

There is no functional difference between the two consumes. Anyway,
currently only for lock-free and spin-locking queues supporting multi-producers, the second consume can be much faster. The reason has to do with the way they ensure that a consumer does not deallocate a page while another consumer is reading it.
When a consumer needs to access a page, it increments a ref-count in the page (it *pins* the page), to notify to the allocator that it is using it. When it has finished, the consumer decrements the ref-count (it *unpins* the page).
If a thread performs many consecutive consumes, it will ends up doing many atomic increments and decrements of the same page (that is a somewhat expensive operation). Since pin\unpin logic is encapsulated in the consume_operation, if the consumer thread keeps the consume_operation alive, pinning and unpinning will be performed only in case of page switch.
Note: a forgotten consume_operation which has pinned a page prevents the page from being recycled by the page allocator, even if it was deallocated by other consumers.  

Heterogeneous queues
--------------------
Every function queue is actually an adaptor for the corresponding heterogeneous pseudo-container.  Heterogeneous queues have put and consume functions, just like function queues, but elements are not required to be callable objects.

\snippet heterogeneous_queue_examples.cpp heter_queue put example 1

The first 3 template parameters are the same for all the heterogeneous queues.

Template parameter            |Meaning  |Constraints|Default  |
------------------------------|---------|-----------|---------|
typename `COMMON_TYPE`|Common type of elements|Must decay to itself (see [std::decay](http://en.cppreference.com/w/cpp/types/decay))|`void`|
typename `RUNTIME_TYPE`|Type eraser type|Must model [RuntimeType](RuntimeType_concept.html)|[runtime_type](classdensity_1_1runtime__type.html)|
typename `ALLOCATOR_TYPE`|Allocator|Must model both [PageAllocator](PagedAllocator_concept.html) and [UntypedAllocator](UntypedAllocator_concept.html)|[void_allocator](namespacedensity.html#a06c6ce21f0d3cede79e2b503a90b731e)|

An element can be pushed on a queue if its address is is implicitly convertible to `COMMON_TYPE*`. By default any type is allowed in the queue.
The `RUNTIME_TYPE` type allows much more customization than the [function_type_erasure](namespacedensity.html#a80100b808e35e98df3ffe74cc2293309) template parameter of fumnction queues. Even using the buillt-in [runtime_type](classdensity_1_1runtime__type.html) you can select which operations the elements of the queue should support, and add your own.

\snippet misc_examples.cpp runtime_type example 2
\snippet misc_examples.cpp runtime_type example 3

Output:

~~~~~~~~~~~~~~
ObjectA::update(0.0166667)
ObjectB::update(0.0166667)
ObjectB::update(0.0166667)
~~~~~~~~~~~~~~

Function queues benchmarks
--------------------
All the tests are done on an Intel(R) Core(TM) i7-7700HQ CPU @ 2.80GHz, 2808 Mhz, 4 cores, 8 logical processors running Windows 10, and compiled with Visual Studio 2017.

In this singlethreaded test <code>std::vector</code> and <code>std::queue</code> are compared to density function queues. Even unnecessarily paying the cost of multithreading,
all concurrent function queues are better than a <code>std::queue</code> of <code>std::function</code>.

Note that the vector is being cleared at the end, because otherwise it would be too slow.

\image html bench.png width=8cm

This is the same test as before, but function queues uses \ref function_manual_clear, \ref concurrency_single and \ref consistency_relaxed to speedup.

\image html bench_2.png width=8cm

Lifo data structures
--------------
Density provides two lifo data structures: lifo_array and lifo_buffer.

[lifo_array](classdensity_1_1lifo__array.html) is a modern C++ version of the variable length automatic arrays of C99. It is is an alternative to the non-standard [alloca](http://man7.org/linux/man-pages/man3/alloca.3.html) function, much more C++ish.

		void dijkstra_path_find(const GraphNode * i_nodes, size_t i_node_count, size_t i_initial_node_index)
		{
			lifo_array<float> min_distance(i_node_count, std::numeric_limits<float>::max() );
			min_distance[i_initial_node_index] = 0.f;

			// ...

The size of the array is provided at construction time, and cannot be changed. lifo_array allocates its element contiguously in memory in the **data stack** of the calling thread. 
The data stack is a thread-local storage managed by a lifo allocator.  By "lifo" (last-in-first-out) we mean that only the most recently allocated block can be deallocated. If the lifo constraint is violated, the behaviour of the program is undefined (defining DENSITY_DEBUG as non-zero in density_common.h enables a debug check, but beaware of the [one definition rule](https://en.wikipedia.org/wiki/One_Definition_Rule)). 
If you declare a lifo_array locally to a function, you don't have to worry about the lifo-constraint, because C++ is designed so that automatic objects are destroyed in lifo order. Even a lifo_array as non-static member of a struct/class allocated on the callstack is safe.

A [lifo_buffer](classdensity_1_1lifo__buffer.html) is not a container, it's more low-level. It provides a dynamic raw storage. Unlike lifo_array, lifo_buffer allows reallocation (that is changing the size and the alignment of the buffer), but only of the last created lifo_buffer (otherwise the behavior is undefined).

~~~~~~~~~~~~~~

    void string_io()
    {
        using namespace std;
        vector<string> strings{ "string", "long string", "very long string", "much longer string!!!!!!" };
        uint32_t len = 0;

		// for each string, write a length-chars pair on a temporary file
        #ifndef _MSC_VER
            auto file = tmpfile();
        #else
            FILE * file = nullptr;
            tmpfile_s(&file);
        #endif
        for (const auto & str : strings)
        {
            len = static_cast<uint32_t>( str.length() + 1);
            fwrite(&len, sizeof(len), 1, file);
            fwrite(str.c_str(), 1, len, file);
        }

		// now we read what we have written
        rewind(file);
        lifo_buffer buff;
        while (fread(&len, sizeof(len), 1, file) == 1)
        {
            buff.resize(len);
            fread(buff.data(), len, 1, file);
            cout << static_cast<char*>(buff.data()) << endl;
        }

        fclose(file);
    }

~~~~~~~~~~~~~~

Concepts
----------

Concept     | Modeled in density by
------------|--------------
[RuntimeType](RuntimeType_concept.html) | [runtime_type](classdensity_1_1runtime__type.html)
[UntypedAllocator](UntypedAllocator_concept.html) | [void_allocator](namespacedensity.html#a06c6ce21f0d3cede79e2b503a90b731e), [basic_void_allocator](classdensity_1_1basic__void__allocator.html)
[PagedAllocator](PagedAllocator_concept.html) | [void_allocator](namespacedensity.html#a06c6ce21f0d3cede79e2b503a90b731e), [basic_void_allocator](classdensity_1_1basic__void__allocator.html)
 
[Github Repository](https://github.com/giucamp/density)

    */


    /*!

    \page func_queue_bench Function queue benchmarks

    These tests create a queue, fill it with many lambda functions, and then call and remove every function. These tests have been performed on a program built with Visual Studio 2015 (update 2).
    5 snippets doing that are compared:
    - using a std::vector of std::function's
    - using a std::vector of std::function's with an initial reserve. In general one does not know the maximum size of the queue, so this may be considered a 'cheat'
    - using a std::queue (which uses a std::deque)
    - using a \ref density::small_function_queue< RET_VAL(PARAMS...)> "small_function_queue"
    - using a \ref density::function_queue< RET_VAL(PARAMS...)> "function_queue"

    \section func_queue_bench_sec1 No capture
    In the first test there is no captured state (function object are small).
    \image html push___consume__no_capture.png width=10cm

    \section func_queue_bench_sec2 Capture of 46 bytes
    In the second test the capture state has the biggest size possible with std::function still not allocating heap memory.
    \image html push___consume__middle_capture__46_bytes_.png width=10cm

    \section func_queue_bench_sec3 Capture of 64 bytes
    In the third test the capture state is bigger than the inline storage of std::function.
    \image html push___consume__big_capture__64_bytes_.png width=10cm

    \page lifo_array_bench Automatic variable-length array benchmarks

    This test just creates an automatic dynamic sized array. The class virtual is defined by this code:
    \code{.cpp}
    struct Virtual
    {
        virtual ~Virtual() {}
    };
    \endcode
    \image html create_array.png width=10cm
    */
    /*! \page runtime_type_sample A sample with runtime_type
    \include runtime_type_sample.cpp
    */
    /*! \page lifo_sample A lifo sample
    \include lifo_sample.cpp
    */
    /*! \page function_queue_sample A sample with function queues
    \include function_queue_sample.cpp
    */

} // namespace density
