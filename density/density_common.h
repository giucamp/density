
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
