
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>
#include <cstring> // for memcpy

namespace density
{
    /** Class template that provides LIFO memory management.

        @tparam VOID_ALLOCATOR Underlying allocator class, that can be stateless or stateful. It must meet the requirements
            of both \ref UntypedAllocator_concept "UntypedAllocator" and \ref PagedAllocator_concept "PagedAllocator".

        A lifo_allocator allocates memory pages from the underlying allocator to provide lifo memory management to the user.
        Memory is allocated/freed with the member function lifo_allocator::allocate and lifo_allocator::deallocate.
        A living block is a block allocated, eventually reallocated, but not yet deallocated.

        Only the most recently allocated living block can be deallocated or reallocated. If a block which is not
        the most recently allocated living block is deallocated or reallocated, the behavior is undefined.
        To simplify the implementation, lifo_allocator does not support custom alignments: every block is guaranteed 
        to be aligned like lifo_allocator::alignment.

        Blocks allocated with an instance of lifo_allocator can't be deallocated with another instance of lifo_allocator.
        When lifo_allocator is destroyed, all the living blocks are deallocated.

        lifo_allocator allocates a page from the underlying allocator when a block is requested by the user and
        there is no space in the last allocated page. Anyway, if the requested block is too big to fit in a page,
        a legacy heap block is allocated from the underlying allocator.

        lifo_allocator does not cache free pages\blocks: when a page or a block is no more used, it is immediately
        deallocated. \n
        lifo_allocator is a stateful class template (it has non-static data members). It is uncopyable and unmovable.
        See thread_lifo_allocator for a stateless LIFO allocator. \n

        Note: using a lifo_allocator directly is not recommended: using a lifo_buffer or lifo_array is easier and safer. */
    template <typename VOID_ALLOCATOR = void_allocator >
        class lifo_allocator : private VOID_ALLOCATOR
    {
    public:

        /** Alignment of the memory blocks.  */
        static constexpr size_t alignment = alignof(std::max_align_t);

        /** Max size of a single block. This size is the maximum value of a pointer, minus an implementation defined value in 
            the order of the size of a page. Requesting a bigger size to an allocation function causes undefined behavior. */
        static constexpr size_t max_size = (std::numeric_limits<uintptr_t>::max() - VOID_ALLOCATOR::page_size);

        // compile time checks
        static_assert(alignment <= VOID_ALLOCATOR::page_alignment, "The alignment of the pages is too small");

        /** Default constructor. */
        constexpr lifo_allocator() noexcept = default;

        /** Constructs passing the underlying allocator as l-value */
        lifo_allocator(const VOID_ALLOCATOR & i_underlying_allocator)
            : VOID_ALLOCATOR(i_underlying_allocator) { }

        /** Constructs passing the underlying allocator as r-value */
        lifo_allocator(VOID_ALLOCATOR && i_underlying_allocator)
            : VOID_ALLOCATOR(std::move(i_underlying_allocator)) { }

        /** Copy construction not allowed */
        lifo_allocator(const lifo_allocator &) = delete;
        
        /** Copy assignment not allowed */
        lifo_allocator & operator = (const lifo_allocator &) = delete;


        /** Allocates a memory block. The content of the newly allocated memory is undefined. The new memory block
            is aligned at least like lifo_allocator::alignment.
                @param i_size The size of the requested block, in bytes.
                @return address of the allocated block
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * allocate(size_t i_size)
        {
            // align the size
            auto const actual_size = uint_upper_align(i_size, alignment);

            // allocate
            auto const new_top = address_add(m_top, actual_size);
            auto const new_offset = address_diff(new_top, address_lower_align(m_top, VOID_ALLOCATOR::page_alignment));
            if (new_offset < VOID_ALLOCATOR::page_size)
            {
                DENSITY_ASSERT_INTERNAL(actual_size <= VOID_ALLOCATOR::page_size);
                auto const new_block = m_top;
                m_top = new_top;
                return new_block;
            }
            else
            {
                return slow_allocate(actual_size);
            }
        }

        /** Deallocates a memory block. Important: only the most recently allocated living block can be allocated.
                @param i_block The memory block to deallocate
                @param i_size Size of the block.

            \pre The behavior is undefined if either:
                - the specified block is null or it is not the most recently allocated
                - the specified size is not the one asked to the most recent reallocation of the block, or to the allocation (If no reallocation was performed)

            \n\b Throws: nothing. */
        void deallocate(void * i_block, size_t i_size) noexcept
        {
            if (same_page(i_block, m_top))
            {
                m_top = i_block;
            }
            else
            {
                slow_deallocate(i_block, i_size);
            }
        }

        /** Reallocates a memory block. Important: only the most recently allocated living block can be reallocated.
            The address of the block may change. The content of the memory block is not preserved.
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_new_mem_size the new size requested for the block, in bytes.
                @return address of the resized block.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
        {
            deallocate(i_block, i_old_size);
            return allocate(i_new_size);
        }

        /** Reallocates a memory block. Important: only the most recently allocated living block can be reallocated.
            The address of the block may change. The content of the memory block is preserved up to the exiting extend.
            If the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_new_mem_size the new size requested for the block, in bytes.
                @return address of the resized block.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * reallocate_preserve(void * i_block, size_t i_old_size, size_t i_new_size)
        {
            // align the sizes
            auto const new_actual_size = uint_upper_align(i_new_size, alignment);
            auto const old_actual_size = uint_upper_align(i_old_size, alignment);

            if (same_page(i_block, m_top))
            {
                // deallocate
                m_top = i_block;

                auto const new_block = allocate(new_actual_size);
                
                copy(i_block, old_actual_size, new_block, new_actual_size);
                return new_block;
            }
            else
            {
                if (old_actual_size < VOID_ALLOCATOR::page_size)
                {
                    // deallocate procrastinating the deallocation of the page
                    DENSITY_ASSERT_INTERNAL(!same_page(m_top, i_block));
                    auto const old_top = m_top;
                    m_top = i_block;

                    auto const new_block = allocate(new_actual_size);
                    copy(i_block, old_actual_size, new_block, new_actual_size);
                    
                    VOID_ALLOCATOR::deallocate_page(old_top);
                    return new_block;
                }
                else
                {
                    // deallocate procrastinating the deallocation of the external block
                    auto const new_block = allocate(new_actual_size);
                    copy(i_block, old_actual_size, new_block, new_actual_size);
                    
                    VOID_ALLOCATOR::deallocate(i_block, old_actual_size, alignment);
                    return new_block;
                }
            }
        }

    private:

        void copy(void * i_old_block, size_t i_old_actual_size, void * i_new_block, size_t i_new_actual_size) noexcept
        {
            if (i_old_block != i_new_block)
            {
                auto const size_to_copy = detail::size_min(i_new_actual_size, i_old_actual_size);
                DENSITY_ASSERT_INTERNAL(size_to_copy % alignment == 0); // this may help the optimizer
                memcpy(i_new_block, i_old_block, size_to_copy);
            }
        }

        /** Returns whether the input addresses belong to the same page or they are both nullptr */
        static bool same_page(const void * i_first, const void * i_second) noexcept
        {
            auto const page_mask = VOID_ALLOCATOR::page_alignment - 1;
            return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
        }

        DENSITY_NO_INLINE void * slow_allocate(size_t actual_size)
        {
            if (actual_size < VOID_ALLOCATOR::page_size)
            {
                // allocate a new page
                auto const new_page = VOID_ALLOCATOR::allocate_page();
                m_top = address_add(new_page, actual_size);
                return new_page;
            }
            else
            {
                // external block
                return VOID_ALLOCATOR::allocate(actual_size, alignment);
            }
        }

        DENSITY_NO_INLINE void slow_deallocate(void * i_block, size_t i_size) noexcept
        {
            // align the size
            auto const actual_size = uint_upper_align(i_size, alignment);
            if (actual_size < VOID_ALLOCATOR::page_size)
            {
                DENSITY_ASSERT_INTERNAL(!same_page(m_top, i_block));
                VOID_ALLOCATOR::deallocate_page(m_top);
                m_top = i_block;
            }
            else
            {
                VOID_ALLOCATOR::deallocate(i_block, actual_size, alignment);
            }
        }

    private:
        void * m_top = reinterpret_cast<void*>(VOID_ALLOCATOR::page_alignment - 1);
    };


    /** Stateless class template that provides a thread local LIFO memory management.
        Memory is allocated/freed with the member function allocate and deallocate. A living block is a block allocated,
        eventually reallocated, but not yet deallocated. Every thread has its own stack of living blocks.
        ONLY THE MOST RECENTLY ALLOCATED LIVING BLOCK CAN BE DEALLOCATED OR REALLOCATED BY A THREAD. If a block which is not
        the most recently allocated living block of the calling thread is deallocated or reallocated, the behavior is undefined.
        For simplicity, thread_lifo_allocator does not support custom alignments: every block is guaranteed to be aligned like
        std::max_align_t.
        Blocks allocated with an instance of thread_lifo_allocator can be deallocated with another instance of thread_lifo_allocator.
        Only the calling thread matters.
        When a thread exits all its living block are deallocated. \n

        Note: using thread_lifo_allocator directly is not recommended: using a lifo_buffer or lifo_array is easier and safer. */
    template <typename VOID_ALLOCATOR = void_allocator >
        class thread_lifo_allocator
    {
    public:

        /** alignment of the memory blocks. */
        static constexpr size_t page_alignment = VOID_ALLOCATOR::page_alignment;

        /** Default constructor */
        thread_lifo_allocator() = default;

        /** Allocates a memory block. The content of the newly allocated memory is undefined.
                @param i_block i_mem_size The size of the requested block, in bytes.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * allocate(size_t i_mem_size)
        {
            return get_allocator().allocate(i_mem_size);
        }

        /** Reallocates a memory block. Important: only the most recently allocated living block can be reallocated.
            The address of the block may change. The content of the memory block is not preserved.
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_new_mem_size the new size requested for the block, in bytes.
                @return address of the resized block.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * reallocate(void * i_block, size_t i_old_size, size_t i_new_mem_size)
        {
            return get_allocator().reallocate(i_block, i_old_size, i_new_mem_size);
        }

        /** Reallocates a memory block. Important: only the most recently allocated living block can be reallocated.
            The address of the block may change. The content of the memory block is preserved up to the exiting extend.
            If the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_new_mem_size the new size requested for the block, in bytes.
                @return address of the resized block.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * reallocate_preserve(void * i_block, size_t i_old_size, size_t i_new_mem_size)
        {
            return get_allocator().reallocate_preserve(i_block, i_old_size, i_new_mem_size);
        }

        /** Deallocates a memory block. Important: only the most recently allocated living block can be allocated.
                @param i_block The memory block to deallocate
            \pre i_block must be the most recently allocated living block, otherwise the behavior is undefined.
            \n\b Throws: nothing. */
        void deallocate(void * i_block, size_t i_size) noexcept
        {
            get_allocator().deallocate(i_block, i_size);
        }

    private:
        static lifo_allocator<VOID_ALLOCATOR> & get_allocator()
        {
            static thread_local lifo_allocator<VOID_ALLOCATOR> s_allocator;
            return s_allocator;
        }
    };

    /** A lifo buffer is a block of raw memory allocated from a lifo allocator. */
    template <typename LIFO_ALLOCATOR = thread_lifo_allocator<>>
        class lifo_buffer : LIFO_ALLOCATOR
    {
    public:

        /** Constructs a lifo_buffer.
            @param i_mem_size size in bytes of the memory block.
        The block is aligned at least like std::max_align_t. */
        lifo_buffer(size_t i_mem_size = 0)
        {
            m_buffer = m_block = get_allocator().allocate(i_mem_size);
            m_mem_size = i_mem_size;
        }

        /** Constructs a lifo_buffer.
            @param i_mem_size size in bytes of the memory block.
            @param i_alignment alignment of the block. It must be > 0 and an integer power of 2
        */
        lifo_buffer(size_t i_mem_size, size_t i_alignment)
        {
            DENSITY_ASSERT(i_alignment > 0 && is_power_of_2(i_alignment));

            // the lifo block is already aligned like std::max_align_t
            auto actual_block_size = i_mem_size;
            if (i_alignment > alignof(std::max_align_t))
            {
                actual_block_size += i_alignment - alignof(std::max_align_t);
            }
            m_block = get_allocator().allocate(actual_block_size);
            m_buffer = address_upper_align(m_block, i_alignment);
            m_mem_size = i_mem_size;
        }

        /** Constructs a lifo_buffer.
            @param i_allocator source to use to copy-construct the allocator
            @param i_mem_size size in bytes of the memory block.
        The block is aligned at least like std::max_align_t. */
        lifo_buffer(const LIFO_ALLOCATOR & i_allocator, size_t i_mem_size)
            : LIFO_ALLOCATOR(i_allocator)
        {
            m_buffer = m_block = get_allocator().allocate(i_mem_size);
            m_mem_size = i_mem_size;
        }

        /** Constructs a lifo_buffer.
            @param i_allocator source to use to copy-construct the allocator
            @param i_mem_size size in bytes of the memory block.
            @param i_alignment alignment of the block. It must be > 0 and an integer power of 2
        */
        lifo_buffer(const LIFO_ALLOCATOR & i_allocator, size_t i_mem_size, size_t i_alignment)
            : LIFO_ALLOCATOR(i_allocator)
        {
            // the lifo block is already aligned like std::max_align_t
            auto actual_block_size = i_mem_size;
            if (i_alignment > alignof(std::max_align_t))
            {
                actual_block_size += i_alignment - alignof(std::max_align_t);
            }
            m_block = get_allocator().allocate(actual_block_size);
            m_buffer = address_upper_align(m_block, i_alignment);
            m_mem_size = i_mem_size;
        }

        // disable the copy
        lifo_buffer(const lifo_buffer &) = delete;
        lifo_buffer & operator = (const lifo_buffer &) = delete;

        ~lifo_buffer()
        {
            get_allocator().deallocate(m_buffer, m_mem_size);
        }

        void resize(size_t i_new_size)
        {
            m_buffer = m_block = get_allocator().reallocate(m_block, m_mem_size, i_new_size);
            m_mem_size = i_new_size;
        }

        void resize(size_t i_new_mem_size, size_t i_alignment)
        {
            // the lifo block is already aligned like std::max_align_t
            auto actual_block_size = i_new_mem_size;
            if (i_alignment > alignof(std::max_align_t))
            {
                actual_block_size += i_alignment - alignof(std::max_align_t);
            }
            m_block = get_allocator().reallocate(m_block, m_mem_size, actual_block_size);
            m_buffer = address_upper_align(m_block, i_alignment);
            m_mem_size = i_new_mem_size;
        }

        void * data() const noexcept
        {
            return m_buffer;
        }

        size_t mem_size() const noexcept
        {
            return m_mem_size;
        }

        LIFO_ALLOCATOR & get_allocator() noexcept
        {
            return *this;
        }

        const LIFO_ALLOCATOR & get_allocator() const noexcept
        {
            return *this;
        }

    private:
        void * m_buffer;
        size_t m_mem_size;
        void * m_block;
    };

    namespace detail
    {
        template <typename TYPE, typename LIFO_ALLOCATOR, bool WIDE_ALIGNMENT = (alignof(TYPE) > alignof(std::max_align_t)) >
            struct LifoArrayImpl;

        template <typename TYPE, typename LIFO_ALLOCATOR>
            struct LifoArrayImpl<TYPE, LIFO_ALLOCATOR, false > : public LIFO_ALLOCATOR
        {
        public:

            LIFO_ALLOCATOR & get_allocator() noexcept
                { return *this; }

            void alloc(size_t i_size)
            {
                m_size = i_size * sizeof(TYPE);
                m_elements = static_cast<TYPE*>(get_allocator().allocate(m_size));
            }

            void free() noexcept
            {
                get_allocator().deallocate(m_elements, m_size);
            }

            TYPE * m_elements;
            size_t m_size;
        };

        template <typename TYPE, typename LIFO_ALLOCATOR>
            struct LifoArrayImpl<TYPE, LIFO_ALLOCATOR, true > : public LIFO_ALLOCATOR
        {
        public:

            LIFO_ALLOCATOR & get_allocator() noexcept
                { return *this; }

            void alloc(size_t i_size)
            {
                // the lifo block is already aligned like std::max_align_t
                const size_t size_overhead = alignof(TYPE) - alignof(std::max_align_t);
                m_actual_mem_size = size_overhead + i_size * sizeof(TYPE);
                m_block = get_allocator().allocate(m_actual_mem_size);

                m_elements = static_cast<TYPE*>(address_upper_align(m_block, alignof(TYPE)));
                DENSITY_ASSERT_INTERNAL(address_diff(m_elements, m_block) <= size_overhead);
            }

            void free() noexcept
            {
                get_allocator().deallocate(m_block, m_actual_mem_size);
            }

            void * m_block;
            TYPE * m_elements;
            size_t m_actual_mem_size;
        };
    }

    /** Simple array-like container class template suitable for LIFO memory management.
        A lifo_array contains N elements of type TYPE, where N is the size of the array, and it
        is given at construction time. Once the array has been constructed, no elements can be
        added or removed.
        Elements in a lifo_array are constructed in positional order by the constructor and destroyed
        in positional backward order by the destructor. */
    template <typename TYPE, typename LIFO_ALLOCATOR = thread_lifo_allocator<>>
        class lifo_array final : detail::LifoArrayImpl<TYPE, LIFO_ALLOCATOR>
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

        LIFO_ALLOCATOR & get_allocator() noexcept
        {
            return *this;
        }

        const LIFO_ALLOCATOR & get_allocator() const noexcept
        {
            return *this;
        }

    private:
        using detail::LifoArrayImpl<TYPE, LIFO_ALLOCATOR>::m_elements;
        const size_t m_size;
    };

} // namespace density
