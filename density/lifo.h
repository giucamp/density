
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_common.h"
#include "void_allocator.h"
#include <vector>
#include <memory>

namespace density
{
    /** Class template that provides a typeless LIFO memory management.
        Memory is allocated\freed with the member function allocate and deallocate. A living block is a block allocated,
        eventually reallocated, but not yet deallocated.
        ONLY THE MOST RECENTLY ALLOCATED LIVING BLOCK CAN BE DEALLOCATED OR REALLOCATED. If a block which is not
        the most recently allocated living block is deallocated or reallocated, the behavior is undefined.
        For simplicity, lifo_allocator does not support custom alignments: every block is guaranteed to be like
        std::max_align_t.
        Blocks allocated with an instance of lifo_allocator can't be deallocated with another instance of lifo_allocator.
        The destructor of lifo_allocator calls the member function deallocate_all to deallocate any living block.
        lifo_allocator is a stateful class template (it has non-static data members). It is uncopyable and unmovable.
        See thread_lifo_allocator for a stateless LIFO allocator. */
    template <typename UNDERLYING_VOID_ALLOCATOR = void_allocator >
        class lifo_allocator : private UNDERLYING_VOID_ALLOCATOR
    {
    public:

        /** Alignment of the memory blocks. It is guaranteed to be at least alignof(std::max_align_t). */
        static const size_t s_alignment = alignof(std::max_align_t);

        lifo_allocator() = default;

        // disable copying
        lifo_allocator(const lifo_allocator &) = delete;
        lifo_allocator & operator = (const lifo_allocator &) = delete;

        /** Destroys the allocator, deallocating any living bock*/
        ~lifo_allocator()
        {
            deallocate_all();
        }

        /** Allocates a memory block. The content of the newly allocated memory is undefined.
                @param i_block i_mem_size The size of the requested block, in bytes.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
        void * allocate(size_t i_mem_size)
        {
            auto const actual_size = (i_mem_size + s_alignment_mask) & ~s_alignment_mask;
            auto result = m_last_page->allocate(actual_size);
            if (result == nullptr)
            {
                // allocate a new page, and then allocate the requested block on it. This may throw
                result = alloc_new_page(actual_size);
            }
            return result;
        }

        /** Reallocates a memory block. Important: only the most recently allocated living block can be reallocated.
            The address of the block may change. The content of the memory block is not preserved.
            If the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, is no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_new_mem_size the new size requested for the block, in bytes.
                @return address of the resized block.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
        void * reallocate(void * i_block, size_t i_new_mem_size)
        {
            auto const new_actual_size = (i_new_mem_size + s_alignment_mask) & ~s_alignment_mask;
            if (m_last_page->reallocate(i_block, new_actual_size))
            {
                return i_block;
            }
            else
            {
                auto prev_last_page = m_last_page;
                auto size_to_copy = m_last_page->size_of_most_recent_block(i_block);
                if (size_to_copy >= new_actual_size)
                {
                    size_to_copy = new_actual_size;
                }
                // the following line may throw
                auto const new_block = alloc_new_page(new_actual_size);
                prev_last_page->free(i_block);
                return new_block;
            }
        }

        /** Reallocates a memory block. Important: only the most recently allocated living block can be reallocated.
            The address of the block may change. The content of the memory block is preserved up to the exiting extend.
            If the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, is no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_new_mem_size the new size requested for the block, in bytes.
                @return address of the resized block.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
        void * reallocate_preserve(void * i_block, size_t i_new_mem_size)
        {
            auto const new_actual_size = (i_new_mem_size + s_alignment_mask) & ~s_alignment_mask;
            if (m_last_page->reallocate(i_block, new_actual_size))
            {
                return i_block;
            }
            else
            {
                auto prev_last_page = m_last_page;
                auto size_to_copy = m_last_page->size_of_most_recent_block(i_block);
                if (size_to_copy >= new_actual_size)
                {
                    size_to_copy = new_actual_size;
                }
                // the following line may throw
                auto const new_block = alloc_new_page(new_actual_size);
                memcpy(new_block, i_block, size_to_copy);
                prev_last_page->free(i_block);
                return new_block;
            }
        }

        /** Deallocates a memory block. Important: only the most recently allocated living block can be allocated.
                @param i_block The memory block to deallocate
            \pre i_block must be the most recently allocated living block, otherwise the behavior is undefined.
            \n\b Throws: nothing. */
        void deallocate(void * i_block) DENSITY_NOEXCEPT
        {
            auto const last_page = m_last_page;
            last_page->free(i_block);
            if (last_page->is_empty())
            {
                pop_page();
            }
        }

        /** Deallocates any living block.
            \n\b Throws: nothing. */
        void deallocate_all() DENSITY_NOEXCEPT
        {
            auto const empty_page = get_empty_page();
            while (m_last_page != empty_page)
            {
                pop_page();
            }
        }

        UNDERLYING_VOID_ALLOCATOR & get_underlying_allocator() DENSITY_NOEXCEPT
        {
            return *this;
        }

        const UNDERLYING_VOID_ALLOCATOR & get_underlying_allocator() const DENSITY_NOEXCEPT
        {
            return *this;
        }

    private:

        static const size_t s_granularity = alignof(std::max_align_t);
        static const size_t s_alignment_mask = s_alignment - 1;
        static const size_t s_min_page_size = 2048;

        DENSITY_NO_INLINE void * alloc_new_page(size_t i_size)
        {
            const size_t page_size = detail::size_max(
                i_size + sizeof(PageHeader),
                detail::size_max(s_min_page_size, UNDERLYING_VOID_ALLOCATOR::s_min_block_size));

            PageHeader * const new_page = static_cast<PageHeader*>( get_underlying_allocator().allocate(page_size, s_granularity) );
            m_last_page = new(new_page) PageHeader(m_last_page,
                new_page + 1, address_add(new_page, page_size));

            /* note: the size of the first block of a page cannot be zero, otherwise is_empty() would
                return true even if this block is living. */
            void * const new_block = new_page->allocate(i_size > 0 ? i_size : s_granularity);
            DENSITY_ASSERT_INTERNAL(new_block != nullptr);
            return new_block;
        }

        void pop_page() DENSITY_NOEXCEPT
        {
            auto const last_page = m_last_page;
            auto const prev_page = last_page->prev_page();
            last_page->PageHeader::~PageHeader();
            get_underlying_allocator().deallocate(last_page, last_page->capacity() + sizeof(PageHeader), s_granularity);
            m_last_page = prev_page;
        }

        class PageHeader
        {
        public:

            PageHeader(PageHeader * const i_prev_page, void * i_start_address, void * i_end_address) DENSITY_NOEXCEPT
                : m_end_address(i_end_address), m_curr_address(i_start_address),
                  m_prev_page(i_prev_page), m_start_address(i_start_address)
            {
            }

            void * allocate(size_t i_size) DENSITY_NOEXCEPT
            {
                const auto start_of_block = m_curr_address;
                const auto end_of_block = address_add(start_of_block, i_size);
                // check for arithmetic overflow or insufficient space
                if (end_of_block >= start_of_block && end_of_block <= m_end_address)
                {
                    m_curr_address = end_of_block;
                    return start_of_block;
                }
                else
                {
                    return nullptr;
                }
            }

            bool reallocate(void * i_block, size_t i_new_size) DENSITY_NOEXCEPT
            {
                const auto start_of_block = i_block;
                const auto end_of_block = address_add(i_block, i_new_size);
                if (end_of_block >= start_of_block && end_of_block <= m_end_address)
                {
                    m_curr_address = end_of_block;
                    return true;
                }
                else
                {
                    return false;
                }
            }

            size_t size_of_most_recent_block(void * i_block) const DENSITY_NOEXCEPT
            {
                return address_diff(m_curr_address, i_block);
            }

            void free(void * i_block) DENSITY_NOEXCEPT
            {
                m_curr_address = i_block;
            }

            bool owns(void * i_address) const DENSITY_NOEXCEPT
            {
                return i_address >= m_start_address && i_address < m_end_address;
            }

            bool is_empty() const DENSITY_NOEXCEPT
            {
                return m_curr_address == m_start_address;
            }

            PageHeader * prev_page() const DENSITY_NOEXCEPT { return m_prev_page; }

            size_t capacity() const DENSITY_NOEXCEPT { return address_diff(m_end_address, m_start_address); }

        private:
            void * const m_end_address;
            void * m_curr_address;
            PageHeader * const m_prev_page;
            void * const m_start_address;
        };

        static PageHeader * get_empty_page()
        {
            static PageHeader s_empty(nullptr, nullptr, nullptr);
            return &s_empty;
        }

        bool operator == (const lifo_allocator & i_other) const DENSITY_NOEXCEPT
        {
            return this == &i_other;
        }

        bool operator != (const lifo_allocator & i_other) const DENSITY_NOEXCEPT
        {
            return this != &i_other;
        }

    private: // data members
        PageHeader * m_last_page = get_empty_page();
    };

    /** Stateless class template that provides a thread local LIFO memory management.
        Memory is allocated\freed with the member function allocate and deallocate. A living block is a block allocated,
        eventually reallocated, but not yet deallocated. Every thread has its own stack of living blocks.
        ONLY THE MOST RECENTLY ALLOCATED LIVING BLOCK CAN BE DEALLOCATED OR REALLOCATED BY A THREAD. If a block which is not
        the most recently allocated living block of the caling thread is deallocated or reallocated, the behavior is undefined.
        For simplicity, thread_lifo_allocator does not support custom alignments : every block is guaranteed to be like
        std::max_align_t.
        Blocks allocated with an instance of thread_lifo_allocator can be deallocated with another instance of thread_lifo_allocator.
        Only the calling thread matters.
        When a thread exits all its living block are deallocated. */
    template <typename UNDERLYING_VOID_ALLOCATOR = void_allocator >
        class thread_lifo_allocator
    {
    public:

        /** Alignment of the memory blocks. It is guaranteed to be at least alignof(std::max_align_t). */
        static const size_t s_alignment = lifo_allocator<UNDERLYING_VOID_ALLOCATOR>::s_alignment;

        /** Allocates a memory block. The content of the newly allocated memory is undefined.
                @param i_block i_mem_size The size of the requested block, in bytes.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
        void * allocate(size_t i_mem_size)
        {
            return t_allocator.allocate(i_mem_size);
        }

        /** Reallocates a memory block. Important: only the most recently allocated living block can be reallocated.
            The address of the block may change. The content of the memory block is preserved up to the exiting extend.
            If the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, is no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_new_mem_size the new size requested for the block, in bytes.
                @return address of the resized block.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
        void * reallocate(void * i_block, size_t i_new_mem_size)
        {
            return t_allocator.reallocate(i_block, i_new_mem_size);
        }

        /** Deallocates a memory block. Important: only the most recently allocated living block can be allocated.
                @param i_block The memory block to deallocate
            \pre i_block must be the most recently allocated living block, otherwise the behavior is undefined.
            \n\b Throws: nothing. */
        void deallocate(void * i_block) DENSITY_NOEXCEPT
        {
            t_allocator.deallocate(i_block);
        }

    private:
        static thread_local lifo_allocator<UNDERLYING_VOID_ALLOCATOR> t_allocator;
    };

    template <typename UNDERLYING_VOID_ALLOCATOR>
        thread_local lifo_allocator<UNDERLYING_VOID_ALLOCATOR> thread_lifo_allocator<UNDERLYING_VOID_ALLOCATOR>::t_allocator;

    template <typename LIFO_ALLOCATOR = thread_lifo_allocator<>>
        class lifo_buffer : LIFO_ALLOCATOR
    {
    public:

        lifo_buffer(size_t i_mem_size = 0)
        {
            m_buffer = m_block = get_allocator().allocate(i_mem_size);
            m_mem_size = i_mem_size;
        }

        lifo_buffer(size_t i_mem_size, size_t i_alignment)
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

        lifo_buffer(const LIFO_ALLOCATOR & i_allocator, size_t i_mem_size)
            : LIFO_ALLOCATOR(i_allocator), lifo_buffer(i_mem_size)
        {
        }

        lifo_buffer(const LIFO_ALLOCATOR & i_allocator, size_t i_mem_size, size_t i_alignment)
            : LIFO_ALLOCATOR(i_allocator), lifo_buffer(i_mem_size, i_alignment)
        {
        }

        lifo_buffer(const lifo_buffer &) = delete;
        lifo_buffer & operator = (const lifo_buffer &) = delete;

        ~lifo_buffer()
        {
            get_allocator().deallocate(m_buffer);
        }

        void resize(size_t i_new_size)
        {
            m_buffer = m_block = get_allocator().reallocate(m_block, i_new_size);
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
            m_block = get_allocator().reallocate(m_block, actual_block_size);
            m_buffer = address_upper_align(m_block, i_alignment);
            m_mem_size = i_new_mem_size;
        }

        void * data() const DENSITY_NOEXCEPT
        {
            return m_buffer;
        }

        size_t mem_size() const DENSITY_NOEXCEPT
        {
            return m_mem_size;
        }

        LIFO_ALLOCATOR & get_allocator() DENSITY_NOEXCEPT
        {
            return *this;
        }

        const LIFO_ALLOCATOR & get_allocator() const DENSITY_NOEXCEPT
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

            LIFO_ALLOCATOR & get_allocator() DENSITY_NOEXCEPT
                { return *this; }

            void alloc(size_t i_size)
            {
                m_elements = static_cast<TYPE*>(get_allocator().allocate(i_size * sizeof(TYPE)));
            }

            void free() DENSITY_NOEXCEPT
            {
                get_allocator().deallocate(m_elements);
            }

            TYPE * m_elements;
        };

        template <typename TYPE, typename LIFO_ALLOCATOR>
            struct LifoArrayImpl<TYPE, LIFO_ALLOCATOR, true > : public LIFO_ALLOCATOR
        {
        public:

            LIFO_ALLOCATOR & get_allocator() DENSITY_NOEXCEPT
                { return *this; }

            void alloc(size_t i_size)
            {
                // the lifo block is already aligned like std::max_align_t
                const size_t size_overhead = alignof(TYPE) - alignof(std::max_align_t);
                m_block = get_allocator().allocate(size_overhead + i_size * sizeof(TYPE));
                m_elements = static_cast<TYPE*>(address_upper_align(m_block, alignof(TYPE)));
                DENSITY_ASSERT_INTERNAL(address_diff(m_elements, m_block) <= size_overhead);
            }

            void free() DENSITY_NOEXCEPT
            {
                get_allocator().deallocate(m_block);
            }

            void * m_block;
            TYPE * m_elements;
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

		/** Constructs a lifo_array and all its elements, given an iterator range to use as source for copy-cosntruction. Elements are constructed in positional order.
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

        size_t size() const DENSITY_NOEXCEPT
        {
            return m_size;
        }

        TYPE & operator [] (size_t i_index) DENSITY_NOEXCEPT
        {
            DENSITY_ASSERT(i_index < m_size);
            return m_elements[i_index];
        }

        const TYPE & operator [] (size_t i_index) const DENSITY_NOEXCEPT
        {
            DENSITY_ASSERT(i_index < m_size);
            return m_elements[i_index];
        }

        pointer data() DENSITY_NOEXCEPT                    { return m_elements; }

        const_pointer data() const DENSITY_NOEXCEPT        { return m_elements; }

        iterator begin() DENSITY_NOEXCEPT                { return m_elements; }

        iterator end() DENSITY_NOEXCEPT                    { return m_elements + m_size; }

        const_iterator cbegin() const DENSITY_NOEXCEPT    { return m_elements; }

        const_iterator cend() const DENSITY_NOEXCEPT    { return m_elements + m_size; }

        const_iterator begin() const DENSITY_NOEXCEPT    { return m_elements; }

        const_iterator end() const DENSITY_NOEXCEPT        { return m_elements + m_size; }

        LIFO_ALLOCATOR & get_allocator() DENSITY_NOEXCEPT
        {
            return *this;
        }

        const LIFO_ALLOCATOR & get_allocator() const DENSITY_NOEXCEPT
        {
            return *this;
        }

    private:
        using detail::LifoArrayImpl<TYPE, LIFO_ALLOCATOR>::m_elements;
        const size_t m_size;
    };

} // namespace density
