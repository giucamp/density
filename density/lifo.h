
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_common.h"
#include "runtime_type.h"
#include "void_allocator.h"
#include <cstring> // for memcpy

namespace density
{
    /** Class template that provides a typeless LIFO memory management.

		@tparam VOID_ALLOCATOR Underlying allocator class, that can be stateless or stateful. It must model both the
			\ref UntypedAllocator_concept "UntypedAllocator" and \ref PagedAllocator_concept "PagedAllocator" concepts.

		Memory is allocated\freed with the member function lifo_allocator::allocate and lifo_allocator::deallocate.
		A living block is a block allocated, eventually reallocated, but not yet deallocated. \n
        ONLY THE MOST RECENTLY ALLOCATED LIVING BLOCK CAN BE DEALLOCATED OR REALLOCATED. If a block which is not
        the most recently allocated living block is deallocated or reallocated, the behavior is undefined.
        To simpify the implementation, lifo_allocator does not support custom alignments: every block is guaranteed to be like
        std::max_align_t. \n
        Blocks allocated with an instance of lifo_allocator can't be deallocated with another instance of lifo_allocator.
        The destructor of lifo_allocator calls the member function deallocate_all to deallocate any living block. \n
		lifo_allocator allocates a page from the underlying allocator when a block is requested by the user and
		there is no space in the last allocated page. Anyway, if the requested block is too big to fit in a page,
		an untyped block is allocated from the underlying allocator. \n
		lifo_allocator does not cache free pages\blocks: when a page or a block is no more used, it is immediately
		deallocated. \n
		lifo_allocator is a stateful class template (it has non-static data members). It is uncopyable and unmovable.
        See thread_lifo_allocator for a stateless LIFO allocator. \n

		Note: using a lifo_allocator directly is not recommended: using a lifo_buffer or lifo_array is easier and safer. */
    template <typename VOID_ALLOCATOR = void_allocator >
		class lifo_allocator : private VOID_ALLOCATOR
	{
	public:

		/** alignment of the memory blocks. It is guaranteed to be at least alignof(std::max_align_t). */
		static const size_t s_alignment = alignof(std::max_align_t);

		/** Default constructor. */
		lifo_allocator() = default;

		/** Constructor from an allocator */
		lifo_allocator(const VOID_ALLOCATOR & i_underlying_allocator)
			: VOID_ALLOCATOR(i_underlying_allocator) { }

        // disable copying
        lifo_allocator(const lifo_allocator &) = delete;
        lifo_allocator & operator = (const lifo_allocator &) = delete;

        /** Destroys the allocator, deallocating any living bock */
        ~lifo_allocator()
        {
            deallocate_all();
        }

        /** Allocates a memory block. The content of the newly allocated memory is undefined. The new memory block
            is aligned at least like std::max_align_t.
                @param i_mem_size The size of the requested block, in bytes.
                @return address of the allocated block
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
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
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
                auto const new_block = alloc_new_page(new_actual_size); // <- this line may throw
                prev_last_page->free(i_block);
                return new_block;
            }
        }

        /** Reallocates a memory block. Important: only the most recently allocated living block can be reallocated.
            The address of the block may change. The content of the memory block is preserved up to the exiting extend.
            If the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
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
                std::memcpy(new_block, i_block, size_to_copy);
                prev_last_page->free(i_block);
                return new_block;
            }
        }

        /** Deallocates a memory block. Important: only the most recently allocated living block can be allocated.
                @param i_block The memory block to deallocate
            \pre i_block must be the most recently allocated living block, otherwise the behavior is undefined.
            \n\b Throws: nothing. */
        void deallocate(void * i_block) noexcept
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
        void deallocate_all() noexcept
        {
            auto const empty_page = get_empty_page();
            while (m_last_page != empty_page)
            {
                pop_page();
            }
        }

		/** Returns a reference to the underlying allocator. */
        VOID_ALLOCATOR & get_underlying_allocator() noexcept
        {
            return *this;
        }

		/** Returns a const reference to the underlying allocator. */
        const VOID_ALLOCATOR & get_underlying_allocator() const noexcept
        {
            return *this;
        }

    private:

        static const size_t granularity = alignof(std::max_align_t);
        static const size_t s_alignment_mask = s_alignment - 1;
        static const size_t s_min_page_size = 2048;

        DENSITY_NO_INLINE void * alloc_new_page(size_t i_needed_size)
        {
            const size_t allocator_page_size = VOID_ALLOCATOR::page_size();

            auto needed_page_size = i_needed_size + sizeof(PageHeader);
            void * page_address;
            if (needed_page_size <= allocator_page_size)
            {
                page_address = get_underlying_allocator().allocate_page();
                needed_page_size = allocator_page_size;
            }
            else
            {
                page_address = get_underlying_allocator().allocate(needed_page_size);
            }

            m_last_page = new(page_address) PageHeader(m_last_page,
                address_add(page_address, sizeof(PageHeader)), address_add(page_address, needed_page_size));

            /* note: the size of the first block of a page cannot be zero, otherwise is_empty() would
                return true even if this block is living. */
            void * const new_block = m_last_page->allocate(i_needed_size > 0 ? i_needed_size : granularity);
            DENSITY_ASSERT_INTERNAL(new_block != nullptr);
            return new_block;
        }

        void pop_page() noexcept
        {
            const size_t allocator_page_size = VOID_ALLOCATOR::page_size();

            auto const last_page = m_last_page;
            auto const prev_page = last_page->prev_page();
            auto const last_page_size = last_page->capacity() + sizeof(PageHeader);
            last_page->PageHeader::~PageHeader();
            if (last_page_size <= allocator_page_size)
            {
				get_underlying_allocator().deallocate_page(last_page);
            }
            else
            {
				get_underlying_allocator().deallocate(last_page, last_page_size);
            }
            m_last_page = prev_page;
        }

        class PageHeader
        {
        public:

            PageHeader(PageHeader * const i_prev_page, void * i_start_address, void * i_end_address) noexcept
                : m_end_address(i_end_address), m_curr_address(i_start_address),
                  m_prev_page(i_prev_page), m_start_address(i_start_address)
            {
            }

            void * allocate(size_t i_size) noexcept
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

            bool reallocate(void * i_block, size_t i_new_size) noexcept
            {
                const auto start_of_block = i_block;
                const auto end_of_block = address_add(i_block, i_new_size);
                // check for arithmetic overflow or insufficient space
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

            size_t size_of_most_recent_block(void * i_block) const noexcept
            {
                return address_diff(m_curr_address, i_block);
            }

            void free(void * i_block) noexcept
            {
                m_curr_address = i_block;
            }

            bool owns(void * i_address) const noexcept
            {
                return i_address >= m_start_address && i_address < m_end_address;
            }

            bool is_empty() const noexcept
            {
                return m_curr_address == m_start_address;
            }

            PageHeader * prev_page() const noexcept { return m_prev_page; }

            size_t capacity() const noexcept { return address_diff(m_end_address, m_start_address); }

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

        bool operator == (const lifo_allocator & i_other) const noexcept
        {
            return this == &i_other;
        }

        bool operator != (const lifo_allocator & i_other) const noexcept
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
        When a thread exits all its living block are deallocated. \n

		Note: using thread_lifo_allocator directly is not recommended: using a lifo_buffer or lifo_array is easier and safer. */
    template <typename VOID_ALLOCATOR = void_allocator >
        class thread_lifo_allocator
    {
    public:

        /** alignment of the memory blocks. It is guaranteed to be at least alignof(std::max_align_t). */
        static size_t page_alignment() noexcept { return VOID_ALLOCATOR::page_alignment; }

		/** Default constructor */
		thread_lifo_allocator() = default;

		// disable the copy
		thread_lifo_allocator(const thread_lifo_allocator & ) = delete;
		thread_lifo_allocator & operator = (const thread_lifo_allocator &) = delete;

        /** Allocates a memory block. The content of the newly allocated memory is undefined.
                @param i_block i_mem_size The size of the requested block, in bytes.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
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
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
        void * reallocate(void * i_block, size_t i_new_mem_size)
        {
            return get_allocator().reallocate(i_block, i_new_mem_size);
        }

		/** Reallocates a memory block. Important: only the most recently allocated living block can be reallocated.
            The address of the block may change. The content of the memory block is preserved up to the exiting extend.
            If the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_new_mem_size the new size requested for the block, in bytes.
                @return address of the resized block.
            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
        void * reallocate_preserve(void * i_block, size_t i_new_mem_size)
        {
            return get_allocator().reallocate_preserve(i_block, i_new_mem_size);
        }

        /** Deallocates a memory block. Important: only the most recently allocated living block can be allocated.
                @param i_block The memory block to deallocate
            \pre i_block must be the most recently allocated living block, otherwise the behavior is undefined.
            \n\b Throws: nothing. */
        void deallocate(void * i_block) noexcept
        {
            get_allocator().deallocate(i_block);
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
                m_elements = static_cast<TYPE*>(get_allocator().allocate(i_size * sizeof(TYPE)));
            }

            void free() noexcept
            {
                get_allocator().deallocate(m_elements);
            }

            TYPE * m_elements;
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
                m_block = get_allocator().allocate(size_overhead + i_size * sizeof(TYPE));
                m_elements = static_cast<TYPE*>(address_upper_align(m_block, alignof(TYPE)));
                DENSITY_ASSERT_INTERNAL(address_diff(m_elements, m_block) <= size_overhead);
            }

            void free() noexcept
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

    template <typename RUNTIME_TYPE = runtime_type<>, typename LIFO_ALLOCATOR = thread_lifo_allocator<> >
        class lifo_any : private LIFO_ALLOCATOR
    {
    public:

        lifo_any() noexcept
            : m_block(nullptr), m_object(nullptr)
                { }

		lifo_any(const lifo_any&) = delete;
		lifo_any & operator = (const lifo_any&) = delete;

        template <typename TARGET_TYPE>
            lifo_any(TARGET_TYPE && i_source)
                : m_type(RUNTIME_TYPE::template make<TARGET_TYPE>())
        {
            assign_impl(i_source);
        }

        template <typename TARGET_TYPE>
            lifo_any & operator = (TARGET_TYPE && i_source)
        {
            clear_impl();
            m_type = RUNTIME_TYPE::template make<TARGET_TYPE>();
            assign_impl(i_source);
            return *this;
        }

        void clear() noexcept
        {
            clear_impl();
            m_type.clear();
            m_block = nullptr;
            m_object = nullptr;
        }

        bool operator == (const lifo_any & i_other) const noexcept
        {
            return m_type == i_other.m_type &&
                m_type.template get_feature<type_features::equals>()(m_object, i_other.m_object);
        }

        bool operator != (const lifo_any & i_other) const noexcept
        {
            return m_type != i_other.m_type ||
                m_type.template get_feature<type_features::less>()(m_object, i_other.m_object);
        }

        const RUNTIME_TYPE & type() const noexcept { return m_type; }

        const typename RUNTIME_TYPE::base_type * data() const noexcept
        {
            return m_object;
        }

        typename RUNTIME_TYPE::base_type * data() noexcept
        {
            return m_object;
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

        template <typename TARGET_TYPE>
            typename RUNTIME_TYPE::base_type * construct_impl(void * i_dest, TARGET_TYPE && i_source, std::true_type)
        {
            typename RUNTIME_TYPE::base_type * base_ptr = &i_source;
            return m_type.move_construct(i_dest, base_ptr);
        }

        template <typename TARGET_TYPE>
            typename RUNTIME_TYPE::base_type * construct_impl(void * i_dest, const TARGET_TYPE & i_source, std::false_type)
        {
            const typename RUNTIME_TYPE::base_type * base_ptr = &i_source;
            return m_type.copy_construct(i_dest, base_ptr);
        }

        template <typename TARGET_TYPE>
            void assign_impl(TARGET_TYPE && i_source)
        {
            auto const size = m_type.alignment();
            auto const alignment = m_type.alignment();
            void * aligned_block;
            if (alignment <= alignof(std::max_align_t))
            {
                aligned_block = m_block = get_allocator().allocate(size);
            }
            else
            {
                // the lifo block is already aligned like std::max_align_t
                const size_t size_overhead = alignment - alignof(std::max_align_t);
                m_block = get_allocator().allocate(size_overhead + size);
                aligned_block = address_upper_align(m_block, alignment);
                DENSITY_ASSERT_INTERNAL(address_diff(aligned_block, m_block) <= size_overhead);
            }
            m_object = construct_impl(aligned_block, std::forward<TARGET_TYPE>(i_source),
                typename std::is_rvalue_reference<TARGET_TYPE&&>::type() );
        }

        void clear_impl() noexcept
        {
            m_type.destroy(m_object);
            get_allocator().deallocate(m_block);
        }

    private:
        RUNTIME_TYPE m_type;
        void * m_block;
        typename RUNTIME_TYPE::base_type * m_object;
    };

} // namespace density
