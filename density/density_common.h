
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <type_traits>
#include <ostream>
#include <limits>
#include <cstddef>
#include <utility>

#define DENSITY_VERSION            0x0008
/**
    Breaking changes from version 0x0007:
        - The \ref PagedAllocator_concept "PagedAllocator" concept previously required page_size and page_alignment
            to be static functions (to avoid using the constexpr keyword). In version 0x0008 page_size and page_alignment
            are required to be static constexpr variables.
*/

#include <density/density_config.h>

namespace density
{
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

        /** Computes the base2 logarithm of a size_t. If the argument is zero or is
            not a power of 2, the behavior is undefined. */
        constexpr size_t size_log2(size_t i_size) noexcept
        {
            return i_size <= 1 ? 0 : size_log2(i_size / 2) + 1;
        }

		struct SizeAndAlignment { size_t m_size, m_alignment; };

		constexpr static detail::SizeAndAlignment adjust_alignment(detail::SizeAndAlignment i_input, size_t i_min_alignment) noexcept
		{
			return i_input.m_alignment >= i_min_alignment ? i_input :
				detail::SizeAndAlignment{ uint_upper_align(i_input.m_size, min_alignment), i_min_alignment };
		}

        /*template <typename SCOPE_EXIT_ACTION>
            class ScopeExit
        {
        public:

            ScopeExit(SCOPE_EXIT_ACTION && i_scope_exit_action)
                : m_scope_exit_action(std::move(i_scope_exit_action))
            {
                static_assert(noexcept(m_scope_exit_action()), "The scope exit action must be noexcept");
            }

            ~ScopeExit()
            {
                m_scope_exit_action();
            }

            ScopeExit(const ScopeExit &) = delete;
            ScopeExit & operator = (const ScopeExit &) = delete;

            ScopeExit(ScopeExit &&) noexcept = default;
            ScopeExit & operator = (ScopeExit &&) noexcept = default;

        private:
            SCOPE_EXIT_ACTION m_scope_exit_action;
        };

        template <typename SCOPE_EXIT_ACTION>
            inline ScopeExit<SCOPE_EXIT_ACTION> at_scope_exit(SCOPE_EXIT_ACTION && i_scope_exit_action)
        {
            return ScopeExit<SCOPE_EXIT_ACTION>(std::move(i_scope_exit_action));
        }*/
    }

	enum concurrency_guarantee
	{
		obstruction_free,
		conditionally_lockfree,
		conditionally_waitfree
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

    /** Returns    the biggest address lesser than te first parameter, such that i_address + i_alignment_offset is aligned
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
        inline UINT uint_upper_align(UINT i_uint, size_t i_alignment) noexcept
    {
        static_assert(std::numeric_limits<UINT>::is_integer && !std::numeric_limits<UINT>::is_signed, "UINT mus be an unsigned integer");
        DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));
        auto const mask = i_alignment - 1;
        return (i_uint + mask) & ~mask;
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

    /** Allocates an object with the given size and alignment starting from a reference pointer, and update
            it to the end of the allocation.
        @param io_top_pointer pointer to the current address, which is incremented to make space for the new block
        @param i_size size (in bytes) of the block to allocate
        @param i_alignment alignment (in bytes) of the block to allocate (must be a positive power of 2).
        @return address of the new block. */
    inline void * linear_alloc(void * & io_top_pointer, size_t i_size, size_t i_alignment)
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment) && uint_is_aligned(i_size, i_alignment));
        auto top = io_top_pointer;
        auto const new_block = top = address_upper_align(top, i_alignment);
        top = address_add(top, i_size);
        io_top_pointer = top;
        return new_block;
    }

    /** Allocates an object with the given size starting from a reference pointer, and update
            it to the end of the allocation.
        @param io_top_pointer pointer to the current address, which is incremented to make space for the new block
        @param i_size size (in bytes) of the block to allocate
        @param i_alignment alignment (in bytes) of the block to allocate (must be a positive power of 2).
        @return address of the new block. */
    inline void * linear_alloc(void * & io_top_pointer, size_t i_size)
    {
        auto top = io_top_pointer;
        auto const new_block = top;
        top = address_add(top, i_size);
        io_top_pointer = top;
        return new_block;
    }

    /** unvoid_t<T> and unvoid<T>::type: alias for T if T is not a (possibly cv) void, unspecified POD type otherwise. **/
    template <typename TYPE>
        struct unvoid { using type = TYPE; };
    template <>
        struct unvoid<void> { struct type {}; };
    template <>
        struct unvoid<const void> { struct type {}; };
    template <>
        struct unvoid<volatile void> { struct type {}; };
    template <>
        struct unvoid<const volatile void> { struct type {}; };
    template <typename TYPE>
        using unvoid_t = typename unvoid<TYPE>::type;


    /*! \page wid_list_iter_bench Widget list benchmarks

    These tests iterate an existing list of widgets many times, and do something with every of them. These are the test with the more variable results: heterogeneous_array seems to perform better, but it's hard to tell how much.

    ptr_vector is a std::vector of std::unique_ptr's, den_list is a heterogeneous_array<Widget>. They are created before the test runs, with this code:

    \code{.cpp}
        static auto ptr_vector = []() {
            vector<unique_ptr<Widget>> res;
            for (size_t i = 0; i < 3000; i++)
            {
                switch (i % 3)
                {
                    case 0: res.push_back(make_unique<Widget>()); break;
                    case 1: res.push_back(make_unique<TextWidget>()); break;
                    case 2: res.push_back(make_unique<ImageWidget>()); break;
                    default: break;
                }
            }
            return res;
        }();

        static auto den_list = []() {
            heterogeneous_array<Widget> list;
            for (size_t i = 0; i < 3000; i++)
            {
                switch (i % 3)
                {
                    case 0: list.push_back(Widget()); break;
                    case 1: list.push_back(TextWidget()); break;
                    case 2: list.push_back(ImageWidget()); break;
                    default: break;
                }
            }
            return list;
        }();
    \endcode

    \section list_iter_bench_sec1 Call a virtual function on every widget

    For every widget a virtual function is called. Widget are defined with this code:
    \code{.cpp}
        struct Widget { int var[8]; virtual void f() { memset(var, 0, sizeof(var) ); } };
        struct TextWidget : Widget { int var[3]; void f() { memset(var, 0, sizeof(var)); }  };
        struct ImageWidget : Widget { int var[8]; void f() { memset(var, 0, sizeof(var)); } };
    \endcode
    \image html iterate_dense_list_and_call_virtual_func.png width=10cm

    \section list_iter_bench_sec2 Set some variables on every widget

    For every widget some variable is set. Widget are defined with this code:
    \code{.cpp}
        struct Widget { int a, b, c, d, e, f, g, h; };
        struct TextWidget : Widget { char str[8]; };
        struct ImageWidget : Widget { float a, b, c; };
    \endcode
    \image html iterate_dense_list_and_set_variables.png width=10cm

    \page any_bench Comparison benchmarks with boost::any
    This test just instances an homogeneous queue and fill it with int's
    \image html queue_push.png width=10cm

    This test iterates an homogeneous container and get the std::type_info for every element. The containers are created by this code:

    \code{.cpp}
        static auto any_vector = []() {
            vector<boost::any> res;
            for (size_t i = 0; i < 3000; i++)
            {
                res.push_back(static_cast<int>(i));
            }
            return res;
        }();

        static auto den_list = []() {
            heterogeneous_array<void> res;
            for (size_t i = 0; i < 3000; i++)
            {
                res.push_back(static_cast<int>(i));
            }
            return res;
        }();
    \endcode
    \image html iterate_an_heterogeneous_list_and_print_type_name.png width=10cm

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
    /*! \page producer_consumer_sample A producer-consumer sample
    \include producer_consumer_sample.cpp
    */

} // namespace density
