
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <assert.h>
#include <type_traits>
#include <ostream>
#include <limits>
#include <cstddef>
#include <memory>

#define DENSITY_VERSION            0x0007

#if !defined(NDEBUG)
	#define DENSITY_DEBUG                    1
    #define DENSITY_DEBUG_INTERNAL           1
#else
    #define DENSITY_DEBUG                    0
    #define DENSITY_DEBUG_INTERNAL           0
#endif

#if DENSITY_DEBUG
	#define DENSITY_ASSERT(bool_expr)              assert((bool_expr))
#else
    #define DENSITY_ASSERT(bool_expr)
#endif

#if DENSITY_DEBUG_INTERNAL
    #define DENSITY_ASSERT_INTERNAL(bool_expr)     assert((bool_expr))
#else
    #define DENSITY_ASSERT_INTERNAL(bool_expr)
#endif

#ifdef _MSC_VER
    #define DENSITY_NO_INLINE                    __declspec(noinline)
    #define DENSITY_STRONG_INLINE                __forceinline
#else
    #define DENSITY_NO_INLINE
    #define DENSITY_STRONG_INLINE
#endif

namespace density
{
    namespace detail
    {
        /* DeferenceVoidPtr<TYPE>::apply(void *) returns *static_cast<TYPE*>(i_ptr). If TYPE is void, returns void */
        template <typename TYPE> struct DeferenceVoidPtr
        {
            using type = TYPE &;
            inline static type apply(void * i_ptr)
                { return *static_cast<TYPE*>(i_ptr); }
        };
        template <> struct DeferenceVoidPtr<void>
        {
            using type = void;
            inline static type apply(void * /*i_ptr*/) {}
        };

        // size_max: avoid including <algorithm> just to use std::max<size_t>
        inline size_t size_max(size_t i_first, size_t i_second) noexcept
        {
            return i_first > i_second ? i_first : i_second;
        }
    }

                // address functions

    /** Returns true whether the given unsigned integer number is a power of 2 (1, 2, 4, 8, ...)
        @param i_number must be > 0, otherwise the behavior is undefined */
    inline bool is_power_of_2(size_t i_number) noexcept
    {
        DENSITY_ASSERT(i_number > 0);
        return (i_number & (i_number - 1)) == 0;
    }

    /** Returns true whether the given address has the specified alignment
        @param i_address address to be checked
        @i_alignment must be > 0 and a power of 2 */
    inline bool is_address_aligned(const void * i_address, size_t i_alignment) noexcept
    {
        DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));
        return (reinterpret_cast<uintptr_t>(i_address) & (i_alignment - 1)) == 0;
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
        return reinterpret_cast< void * >( uint_pointer + i_offset );
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
        return reinterpret_cast< void * >( uint_pointer - i_offset );
    }

    /** Computes the unsigned difference between two pointers. The first must be above or equal to the second.
        @param i_end_address first address
        @param i_start_address second address
        @return i_end_address minus i_start_address    */
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

    /** Returns true whether the given pair of pointers enclose a valid array of objects of the type. This function is intended to validate
            an input array.
        @param i_objects_start inclusive lower bound of the array
        @param i_objects_end exclusive upper bound of the array
        @return true if and only if all the following conditions are true:
            - i_objects_start <= i_objects_end
            - the difference (in bytes) between i_objects_end and i_objects_start is a multiple of the size of TYPE
            - both i_objects_start and i_objects_end respects the alignment for TYPE. */
    template <typename TYPE>
        inline bool is_valid_range(const TYPE * i_objects_start, const TYPE * i_objects_end) noexcept
    {
        if (i_objects_start > i_objects_end)
        {
            return false;
        }
        if( !is_address_aligned(i_objects_start, std::alignment_of<TYPE>::value) )
        {
            return false;
        }
        if( !is_address_aligned(i_objects_end, std::alignment_of<TYPE>::value))
        {
            return false;
        }
        const uintptr_t diff = reinterpret_cast<uintptr_t>(i_objects_end) -  reinterpret_cast<uintptr_t>(i_objects_start);
        if (diff % sizeof(TYPE) != 0)
        {
            return false;
        }
        return true;
    }

    namespace detail
    {
        struct AlignmentHeader
        {
            void * m_block;
        };
    }

    /** Allocates aligned memory using the provided allocator. This function just allocates (no constructors are called). Throws std::bad_alloc if allocation fails.
            @param i_allocator allocator to use
            @param i_size size of the requested memory block, in bytes
            @param i_alignment alignment of the requested memory block, in bytes. Must be >0 and a power of 2
            @param i_alignment_offset offset of the block to be aligned. The alignment is guaranteed only at i_alignment_offset
                from the beginning of the block.
            @return address of the new memory block */
    template <typename ALLOCATOR>
        void * aligned_alloc(ALLOCATOR & i_allocator, size_t i_size, size_t i_alignment, size_t i_alignment_offset )
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));

        if (i_alignment <= std::alignment_of<void*>::value)
        {
            typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<void *> other_alloc(i_allocator);
            return other_alloc.allocate((i_size + sizeof(void*) - 1) / sizeof(void*));
        }
        else
        {
            size_t const extra_size = (i_alignment >= sizeof(detail::AlignmentHeader) ? i_alignment : sizeof(detail::AlignmentHeader));
            size_t const actual_size = i_size + extra_size;

            typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(i_allocator);
            void * const complete_block = char_alloc.allocate(actual_size);

            void * const user_block = address_lower_align(address_add(complete_block, extra_size), i_alignment, i_alignment_offset);
            detail::AlignmentHeader & header = *(static_cast<detail::AlignmentHeader*>(user_block) - 1);
            header.m_block = complete_block;

            // done
            return user_block;
        }
    }

    /** Frees an address allocated with aligned_alloc. This function just deallocates (no destructors are called). It never throws.
            @param i_allocator allocator to use. Must be the same passed to aligned_alloc, otherwise the behavior is undefined.
            @param i_block block to free (returned by aligned_alloc). If it's not a valid block the behavior is undefined.
            @param i_size size of the block to free, in bytes. Must be the same passed to aligned_alloc, otherwise the behavior is undefined.
            @param i_alignment alignment of the memory block. Must be the same passed to aligned_alloc, otherwise the behavior is undefined. */
    template <typename ALLOCATOR>
        void aligned_free(ALLOCATOR & i_allocator, void * i_block, size_t i_size, size_t i_alignment ) noexcept
    {
        if (i_alignment <= std::alignment_of<void*>::value)
        {
            typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<void *> other_alloc(i_allocator);
            other_alloc.deallocate(static_cast<void**>(i_block), (i_size + sizeof(void*) - 1) / sizeof(void*));
        }
        else if (i_block != nullptr)
        {
            {
                size_t const extra_size = detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
                size_t const actual_size = i_size + extra_size;

                detail::AlignmentHeader * header = static_cast<detail::AlignmentHeader*>(i_block) - 1;

                typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> char_alloc(i_allocator);
                char_alloc.deallocate(static_cast<char*>(header->m_block), actual_size);
            }
        }
    }


    class AllocatorUtils
    {
    public:

        template <typename CHAR_ALLOCATOR>
            static void * aligned_allocate(CHAR_ALLOCATOR & i_char_allocator, size_t i_size, size_t i_alignment, size_t i_alignment_offset )
        {
            static_assert(std::is_same<typename CHAR_ALLOCATOR::value_type, char>::value, "The valuetype of the allocator must be char");

            DENSITY_ASSERT(is_power_of_2(i_alignment));

            size_t const extra_size = detail::size_max(i_alignment, sizeof(detail::AlignmentHeader));
            size_t const actual_size = i_size + extra_size;

            void * const complete_block = i_char_allocator.allocate(actual_size);

            void * const user_block = address_lower_align(address_add(complete_block, extra_size), i_alignment, i_alignment_offset);
            detail::AlignmentHeader & header = *(static_cast<detail::AlignmentHeader*>(user_block) - 1);
            header.m_block = complete_block;

            return user_block;
        }

        template <typename CHAR_ALLOCATOR>
            static void aligned_deallocate(CHAR_ALLOCATOR & i_char_allocator, void * i_block, size_t i_size ) noexcept
        {
            static_assert(std::is_same<typename CHAR_ALLOCATOR::value_type, char>::value, "The valuetype of the allocator must be char");

            if (i_block != nullptr)
            {
                detail::AlignmentHeader * header = static_cast<detail::AlignmentHeader*>(i_block) - 1;

                const size_t complete_size = address_diff(address_add(i_block, i_size), header->m_block);

                i_char_allocator.deallocate(static_cast<char*>(header->m_block), complete_size);
            }
        }
    };

    /** Finds the aligned storage for a block with the specified size and alignment, such that it is
            >= *io_top_pointer, and sets *io_top_pointer to the end of the block. The actual pointed memory is not read\written.
        @param io_top_pointer pointer to the current address, which is incremented to make space for the new block. After
            the function exits, *io_top_pointer will point to the first address after the new block.
        @param i_size size (in bytes) of the block to allocate
        @param i_alignment alignment (in bytes) of the block to allocate (must be a positive power of 2).
        @return address of the new block. */
    inline void * linear_alloc(void * * io_top_pointer, size_t i_size, size_t i_alignment)
    {
        DENSITY_ASSERT(is_power_of_2(i_alignment));

        auto top = *io_top_pointer;
        auto new_block = top = address_upper_align(top, i_alignment);
        top = address_add(top, i_size);
        *io_top_pointer = top;
        return new_block;
    }

    inline void * linear_alloc(void * & io_curr_ptr, size_t i_size, size_t i_alignment)
    {
        DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));

        const auto alignment_mask = i_alignment - 1;

        auto ptr = reinterpret_cast<uintptr_t>(io_curr_ptr);
        const auto original_ptr = ptr;

        ptr += alignment_mask;
        ptr &= ~alignment_mask;
        void * result = reinterpret_cast<void*>(ptr);
        ptr += i_size;

        if (ptr < original_ptr)
        {
            io_curr_ptr = reinterpret_cast<void*>(std::numeric_limits<uintptr_t>::max());
        }

        io_curr_ptr = reinterpret_cast<void*>(ptr);
        return result;
    }

    namespace detail
    {
        template <typename BASE_CLASS, typename... TYPES>
            struct AllCovariant
        {
            static const bool value = true;
        };

        template <typename BASE_CLASS, typename FIRST_TYPE, typename... OTHER_TYPES>
            struct AllCovariant<BASE_CLASS, FIRST_TYPE, OTHER_TYPES...>
        {
            static const bool value = std::is_convertible<FIRST_TYPE*, BASE_CLASS*>::value &&
                AllCovariant<BASE_CLASS, OTHER_TYPES...>::value;
        };
    }

    /*! \page wid_list_iter_bench Widget list benchmarks

    These tests iterate an existing list of widgets many times, and do something with every of them. These are the test with the more variable results: array_any seems to perform better, but it's hard to tell how much.

    ptr_vector is a std::vector of std::unique_ptr's, den_list is a array_any<Widget>. They are created before the test runs, with this code:

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
            array_any<Widget> list;
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

    \section list_iter_bench_sec2 Set some viriables on every widget

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
			array_any<void> res;
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
    - using a \ref density::small_queue_function< RET_VAL(PARAMS...)> "small_queue_function"
    - using a \ref density::queue_function< RET_VAL(PARAMS...)> "queue_function"

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
