
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>
#include <cstring> // for memcpy
#include <type_traits>

namespace density
{
    /** Class template that provides LIFO memory management.

        @tparam UNDERLYING_ALLOCATOR Underlying allocator class, that can be stateless or stateful. It must meet the requirements
            of both \ref UntypedAllocator_concept "UntypedAllocator" and \ref PagedAllocator_concept "PagedAllocator".

        A lifo_allocator allocates memory pages from the underlying allocator to provide lifo memory management to the user.
        It is designed to be efficient, so it does not provide an high-level service to be used directly. 
            - deallocation and reallocation require the caller to specify the size of the block
            - all blocks has the same fixed guaranteed alignment
            - all blocks must be deallocated before the allocator is destroyed
        lifo_allocator handles block sizes bigger than the size of a page with legacy heap allocations.

        Memory is allocated/freed with the member functions \ref allocate and \ref deallocate.
        A living block is a block allocated, eventually reallocated, but not yet deallocated.
        Only the most recently allocated living block can be deallocated or reallocated. If a block which is not
        the most recently allocated living block is deallocated or reallocated, the behavior is undefined.
        To simplify the implementation, lifo_allocator does not support custom alignments: every block is guaranteed 
        to be aligned to \ref alignment.

        Blocks allocated with an instance of lifo_allocator can't be deallocated with another instance of lifo_allocator.
        The user must deallocate all the living blocks before the allocator is destroyed, otherwise the behvaiour is 
        undefined.

        lifo_allocator allocates a new page from the underlying allocator when a block is requested by the user and
        there is no space in the last allocated page. Anyway, if the requested block is too big to fit in a page,
        a legacy heap block is allocated from the underlying allocator.

        lifo_allocator is a stateful class template (it has non-static data members). It is uncopyable and unmovable.

        Implementation notes
        --------------------------
        A block allocation or deallocation requires only a few ALU instructions and a branch to a 
        slow path, that is taken whenever a page switch occurs. The internal state of the allocator is composed by a pointer,
        that points to the next block \ref allocate would return.
        lifo_allocator does not cache free pages\blocks: when a page or a block is no more used, it is immediately deallocated. */
    template <typename UNDERLYING_ALLOCATOR = void_allocator >
        class lifo_allocator : private UNDERLYING_ALLOCATOR
    {
    public:

        /** Alias for the template argument UNDERLYING_ALLOCATOR */
        using allocator_type = UNDERLYING_ALLOCATOR;

        /** Alignment of the memory blocks */
        static constexpr size_t alignment = alignof(std::max_align_t);

        /** Max size of a single block. This size is equal to the size of the address space, minus an implementation defined value in 
            the order of the size of a page. Requesting a bigger size to an allocation function causes undefined behavior. */
        static constexpr size_t max_block_size = (std::numeric_limits<uintptr_t>::max() - UNDERLYING_ALLOCATOR::page_size);

        // compile time checks
        static_assert(alignment <= UNDERLYING_ALLOCATOR::page_alignment, "The alignment of the pages is too small");

        /** Default constructor. */
        constexpr lifo_allocator() noexcept = default;

        /** Constructs passing the underlying allocator as l-value */
        lifo_allocator(const UNDERLYING_ALLOCATOR & i_underlying_allocator)
            : UNDERLYING_ALLOCATOR(i_underlying_allocator) { }

        /** Constructs passing the underlying allocator as r-value */
        lifo_allocator(UNDERLYING_ALLOCATOR && i_underlying_allocator)
            : UNDERLYING_ALLOCATOR(std::move(i_underlying_allocator)) { }

        /** Copy construction not allowed */
        lifo_allocator(const lifo_allocator &) = delete;
        
        /** Copy assignment not allowed */
        lifo_allocator & operator = (const lifo_allocator &) = delete;


        /** Allocates a memory block. The content of the newly allocated memory is undefined. The new memory block
            is aligned at least to \ref alignment.
                @param i_size The size of the requested block, in bytes.
                @return address of the allocated block

            \pre The behavior is undefined if either:
                - i_size > \ref max_block_size

            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * allocate(size_t i_size)
        {
            // align the size
            auto const actual_size = uint_upper_align(i_size, alignment);

            // allocate
            auto const new_top = address_add(m_top, actual_size);
            auto const new_offset = address_diff(new_top, address_lower_align(m_top, UNDERLYING_ALLOCATOR::page_alignment));
            if (new_offset < UNDERLYING_ALLOCATOR::page_size)
            {
                DENSITY_ASSERT_INTERNAL(actual_size <= UNDERLYING_ALLOCATOR::page_size);
                auto const new_block = m_top;
                m_top = new_top;
                return new_block;
            }
            else
            {
                return allocate_slow_path(actual_size);
            }
        }

        /** Deallocates the most recently allocated living memory block.
                @param i_block The memory block to deallocate
                @param i_size Size of the block.

            \pre The behavior is undefined if either:
                - the specified block is null or it is not the most recently allocated
                - the specified size is not the one asked to the most recent reallocation of the block, or to the allocation (If no reallocation was performed)

            \n\b Throws: nothing. */
        void deallocate(void * i_block, size_t i_size) noexcept
        {
            // this check detects page switches and external blocks
            if (same_page(i_block, m_top))
            {
                m_top = i_block;
            }
            else
            {
                deallocate_slow_path(i_block, i_size);
            }
        }

        /** Reallocates the most recently allocated living memory block, changing its size.
            The address of the block may change. The content of the memory block is preserved up to the existing extend.
            If the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_new_size the previous size of the block, in bytes.
                @param i_new_size the new size requested for the block, in bytes.
                @return the new address of the resized block.

            \pre The behavior is undefined if either:
                - the specified block is null or it is not the most recently allocated
                - i_old_size is not the one asked to the most recent reallocation of the block, or to the allocation (If no reallocation was performed)
                - i_new_size > max_block_size

            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
        {
            // align the sizes
            auto const new_actual_size = uint_upper_align(i_new_size, alignment);
            auto const old_actual_size = uint_upper_align(i_old_size, alignment);

            // branch depending on the old block: this check detects page switches and external blocks
            if (same_page(i_block, m_top))
            {
                // the following call sets the top only when it is sure to no throw
                auto const new_block = set_top_and_allocate(i_block, new_actual_size);
                
                copy(i_block, old_actual_size, new_block, new_actual_size);
                return new_block;
            }
            else
            {
                if (old_actual_size < UNDERLYING_ALLOCATOR::page_size)
                {
                    DENSITY_ASSERT_INTERNAL(!same_page(m_top, i_block));
                    auto const old_top = m_top;
                
                    // the following call sets the top only when it is sure to no throw
                    auto const new_block = set_top_and_allocate(i_block, new_actual_size);
                                
                    copy(i_block, old_actual_size, new_block, new_actual_size);
                    
                    UNDERLYING_ALLOCATOR::deallocate_page(old_top);
                    return new_block;
                }
                else
                {
                    // the old block is external
                    auto const new_block = allocate(new_actual_size);
                    copy(i_block, old_actual_size, new_block, new_actual_size);
                    UNDERLYING_ALLOCATOR::deallocate(i_block, old_actual_size, alignment);
                    return new_block;
                }
            }
        }

    private:

        /** Returns whether the input addresses belong to the same page or they are both nullptr */
        static bool same_page(const void * i_first, const void * i_second) noexcept
        {
            auto const page_mask = UNDERLYING_ALLOCATOR::page_alignment - 1;
            return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
        }

        DENSITY_NO_INLINE void * allocate_slow_path(size_t i_actual_size)
        {
            DENSITY_ASSERT_INTERNAL(i_actual_size % alignment == 0);
            if (i_actual_size < UNDERLYING_ALLOCATOR::page_size)
            {
                // allocate a new page
                auto const new_page = UNDERLYING_ALLOCATOR::allocate_page();
                m_top = address_add(new_page, i_actual_size);
                return new_page;
            }
            else
            {
                // external block
                return UNDERLYING_ALLOCATOR::allocate(i_actual_size, alignment);
            }
        }

        DENSITY_NO_INLINE void deallocate_slow_path(void * i_block, size_t i_size) noexcept
        {
            // align the size
            auto const actual_size = uint_upper_align(i_size, alignment);
            if (actual_size < UNDERLYING_ALLOCATOR::page_size)
            {
                DENSITY_ASSERT_INTERNAL(!same_page(m_top, i_block));
                UNDERLYING_ALLOCATOR::deallocate_page(m_top);
                m_top = i_block;
            }
            else
            {
                // external block
                UNDERLYING_ALLOCATOR::deallocate(i_block, actual_size, alignment);
            }
        }

        /** Sets has the same effect of setting m_top to i_current_top and then allocating a block.
            This function provides the strong exception guarantee. */
        void * set_top_and_allocate(void * const i_current_top, size_t const i_actual_size)
        {
            DENSITY_ASSERT_INTERNAL(i_actual_size % alignment == 0);

            // we have to procrastinate the assignment of m_top until we have allocated the page or the external block
            auto const new_top = address_add(i_current_top, i_actual_size);
            auto const new_offset = address_diff(new_top, address_lower_align(i_current_top, UNDERLYING_ALLOCATOR::page_alignment));
            if (new_offset < UNDERLYING_ALLOCATOR::page_size)
            {
                DENSITY_ASSERT_INTERNAL(i_actual_size <= UNDERLYING_ALLOCATOR::page_size);
                auto const new_block = i_current_top;
                m_top = new_top;
                return new_block;
            }
            else
            {
                // this branch does not use the value of m_top
                return allocate_slow_path(i_actual_size);
            }
        }

        void copy(void * i_old_block, size_t i_old_actual_size, void * i_new_block, size_t i_new_actual_size) noexcept
        {
            if (i_old_block != i_new_block)
            {
                auto const size_to_copy = detail::size_min(i_new_actual_size, i_old_actual_size);
                DENSITY_ASSERT_INTERNAL(size_to_copy % alignment == 0); // this may help the optimizer
                memcpy(i_new_block, i_old_block, size_to_copy);
            }
        }

    private:
        void * m_top = reinterpret_cast<void*>(UNDERLYING_ALLOCATOR::page_alignment - 1);
    };

    namespace detail
    {
        /** \internal Stateless class template that provides a thread local LIFO memory management */
        class ThreadLifoAllocator
        {
        public:

            static constexpr size_t alignment = lifo_allocator<>::alignment;
            static constexpr size_t max_block_size = lifo_allocator<>::max_block_size;

            static void * allocate(size_t i_size)
            {
                return get_allocator().allocate(i_size);
            }
            
            static void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
            {
                return get_allocator().reallocate(i_block, i_old_size, i_new_size);
            }

            static void deallocate(void * i_block, size_t i_size) noexcept
            {
                get_allocator().deallocate(i_block, i_size);
            }

        private:
            static lifo_allocator<> & get_allocator()
            {
                static thread_local lifo_allocator<> s_allocator;
                return s_allocator;
            }
        };

    } // namespace detail

    /** Class that allocates and owns a block from a thread-specific lifo allocator, called the 'data stack'. The block
        is deallocated by the destructor. 
        
        \snippet lifo_examples.cpp lifo_buffer example 1 */
    class lifo_buffer
    {
    public:

        /** Alignment guaranteed for the block */
        static constexpr size_t alignment = detail::ThreadLifoAllocator::alignment;
        
        /** Max size of a block. This size is equal to the size of the address space, minus an implementation defined value in 
            the order of the size of a page. Requesting a bigger block size causes undefined behavior. */
        static constexpr size_t max_block_size = detail::ThreadLifoAllocator::max_block_size;

        /** Allocates the block
            @param i_size size in bytes of the memory block.
            
            \pre The behavior is undefined if either:
                - i_size > \ref max_block_size */
        lifo_buffer(size_t i_size = 0)
            : m_data(detail::ThreadLifoAllocator::allocate(i_size)), m_size(i_size)
        {
        }

        /** Copy construction not allowed */
        lifo_buffer(const lifo_buffer &) = delete;

        /** Copy assignment not allowed */
        lifo_buffer & operator = (const lifo_buffer &) = delete;

        /** Deallocates the block */
        ~lifo_buffer()
        {
            detail::ThreadLifoAllocator::deallocate(m_data, m_size);
        }

        /** Changes the size of the block, preserving its existing content. 
            If the buffer grows (the new size is bigger than the previous one) the new content has undefined content.
        
            This function may reallocate the block, so its address may change.

            \pre The behavior is undefined if either:
                - i_new_size > \ref max_block_size
                - this block was not the most recently allocated one on the data stack of the calling thread

            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void resize(size_t i_new_size)
        {
            /* we must allocate before updating any data member, so that in case of exception no change remains */
            m_data = detail::ThreadLifoAllocator::reallocate(m_data, m_size, i_new_size);
            m_size = i_new_size;
        }

        /** Returns the associated memory block. */
        void * data() const noexcept
        {
            return m_data;
        }

        /** Returns the size of the associated memory block. */
        size_t size() const noexcept
        {
            return m_size;
        }

    private:
        void * m_data;
        size_t m_size;
    };

    namespace detail
    {
        template <typename TYPE, bool OVER_ALIGNED = (alignof(TYPE) > detail::ThreadLifoAllocator::alignment) >
            struct LifoArrayImpl;

        template <typename TYPE>
            struct LifoArrayImpl<TYPE, false >
        {
        public:

            void alloc(size_t i_size)
            {
                m_size = i_size * sizeof(TYPE);
                m_elements = static_cast<TYPE*>(detail::ThreadLifoAllocator::allocate(m_size));
            }

            void free() noexcept
            {
                detail::ThreadLifoAllocator::deallocate(m_elements, m_size);
            }

            TYPE * m_elements;
            size_t m_size;
        };

        template <typename TYPE>
            struct LifoArrayImpl<TYPE, true >
        {
        public:

            void alloc(size_t i_size)
            {
                // the lifo block is already aligned to ThreadLifoAllocator::alignment
                const size_t size_overhead = alignof(TYPE) - detail::ThreadLifoAllocator::alignment;
                m_actual_size = size_overhead + i_size * sizeof(TYPE);
                m_block = detail::ThreadLifoAllocator::allocate(m_actual_size);

                m_elements = static_cast<TYPE*>(address_upper_align(m_block, alignof(TYPE)));
                DENSITY_ASSERT_INTERNAL(address_diff(m_elements, m_block) <= size_overhead);
            }

            void free() noexcept
            {
                detail::ThreadLifoAllocator::deallocate(m_block, m_actual_size);
            }

            void * m_block;
            TYPE * m_elements;
            size_t m_actual_size;
        };
    }

    /** Simple array-like container class template suitable for LIFO memory management.
        A lifo_array contains N elements of type TYPE, where N is the size of the array, and it
        is given at construction time. Once the array has been constructed, no elements can be
        added or removed.
        Elements in a lifo_array are constructed in positional order by the constructor and destroyed
        in positional backward order by the destructor. 
        
        \snippet lifo_examples.cpp lifo_array example 1


        Trivially constructive types must be explicitly initialized when used as elements of lifo_array:
        \snippet lifo_examples.cpp lifo_array example 2 */
    template <typename TYPE>
        class lifo_array final : detail::LifoArrayImpl<TYPE>
    {
    public:

        using value_type = TYPE;
        using reference = TYPE &;
        using const_reference = TYPE &;
        using pointer = TYPE *;
        using const_pointer = const TYPE *;
        using iterator = TYPE *;
        using const_iterator = const TYPE *;

        /** Constructs a lifo_array and all its elements. Elements are constructed in positional order.
            @param i_size number of element of the array */
        lifo_array(size_t i_size)
                : m_size(i_size)
        {
            this->alloc(i_size);
            default_construct(std::is_trivially_default_constructible<TYPE>());
        }

        /** Constructs a lifo_array and all its elements. Elements are constructed in positional order.
            @param i_size number of element of the array
            @param i_construction_params construction parameters to use for every element of the array */
        template <typename... CONSTRUCTION_PARAMS>
            lifo_array(size_t i_size, const CONSTRUCTION_PARAMS &... i_construction_params )
                : m_size(i_size)
        {
            this->alloc(i_size);
            size_t element_index = 0;
            try
            {
                for (; element_index < i_size; element_index++)
                {
                    new(m_elements + element_index) TYPE(i_construction_params...);
                }
            }
            catch (...)
            {
                // destroy all the elements constructed till now, and then deallocate
                auto it = m_elements + element_index;
                it--;
                for (; it >= m_elements; it--)
                {
                    it->TYPE::~TYPE();
                }
                this->free();
                throw;
            }
        }

        /** Constructs a lifo_array and all its elements, given an iterator range to use as source for copy-construction. Elements are constructed in positional order.
            The size of the array is computed with std::distance(i_begin, i_end)
            @param i_begin points to the first source value
            @param i_end points to the first value that is not copied in the array. */
        template <typename INPUT_ITERATOR>
            lifo_array(INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end,
                    typename std::iterator_traits<INPUT_ITERATOR>::iterator_category = typename std::iterator_traits<INPUT_ITERATOR>::iterator_category())
                : m_size(std::distance(i_begin, i_end))
        {
            this->alloc(m_size);
            auto dest_element = m_elements;
            try
            {
                for (;i_begin != i_end; ++i_begin)
                {
                    new(dest_element) TYPE(*i_begin);
                    ++dest_element;
                }
            }
            catch (...)
            {
                // destroy all the elements constructed till now, and then deallocate
                auto it = dest_element;
                it--;
                for (; it >= m_elements; it--)
                {
                    it->TYPE::~TYPE();
                }
                this->free();
                throw;
            }
        }

        /** Destroys a lifo_array and all its elements. Elements are destroyed in reverse positional order. */
        ~lifo_array()
        {
            auto it = m_elements + m_size;
            it--;
            for (; it >= m_elements; it--)
            {
                it->TYPE::~TYPE();
            }
            this->free();
        }

        size_t size() const noexcept
        {
            return m_size;
        }

        TYPE & operator [] (size_t i_index) noexcept
        {
            DENSITY_ASSERT(i_index < m_size);
            return m_elements[i_index];
        }

        const TYPE & operator [] (size_t i_index) const noexcept
        {
            DENSITY_ASSERT(i_index < m_size);
            return m_elements[i_index];
        }

        pointer data() noexcept                    { return m_elements; }

        const_pointer data() const noexcept        { return m_elements; }

        iterator begin() noexcept                { return m_elements; }

        iterator end() noexcept                    { return m_elements + m_size; }

        const_iterator cbegin() const noexcept    { return m_elements; }

        const_iterator cend() const noexcept    { return m_elements + m_size; }

        const_iterator begin() const noexcept    { return m_elements; }

        const_iterator end() const noexcept        { return m_elements + m_size; }

    private:

        // trivially default constructible types
        void default_construct(std::true_type)
        {
        }

        // non-trivially default constructible types
        void default_construct(std::false_type)
        {
            size_t element_index = 0;
            try
            {
                for (; element_index < m_size; element_index++)
                {
                    new(m_elements + element_index) TYPE();
                }
            }
            catch (...)
            {
                // destroy all the elements constructed till now, and then deallocate
                auto it = m_elements + element_index;
                it--;
                for (; it >= m_elements; it--)
                {
                    it->TYPE::~TYPE();
                }
                this->free();
                throw;
            }
        }

    private:
        using detail::LifoArrayImpl<TYPE>::m_elements;
        const size_t m_size;
    };

} // namespace density
