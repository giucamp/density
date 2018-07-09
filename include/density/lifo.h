
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cstring> // for memcpy
#include <density/default_allocator.h>
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <iterator>
#include <type_traits>

namespace density
{
    /** Class template that provides LIFO memory management.

        @tparam UNDERLYING_ALLOCATOR Underlying allocator class, that can be stateless or stateful. It must meet the requirements
            of both \ref UntypedAllocator_concept "UntypedAllocator" and \ref PagedAllocator_concept "PagedAllocator".
        @tparam ALIGNMENT Alignment of the blocks. It must be non-zero and a power of 2.

        A lifo_allocator uses memory pages allocated from the underlying allocator to provide lifo memory management to the user.
        It is designed to be efficient, so it does not provide an high-level service. All blocks has the
        same alignment (specified by the template argument), and the size of the requested blocks must be a multiple of it.
        Deallocation and reallocation functions require the caller to specify the current size of the block.
        Blocks that are very large according to a some implementation-defined criteria, are handled with legacy heap allocations.

        A living block is a block allocated, eventually reallocated, but not yet deallocated.
        Reallocating or deallocating a block which is not the most recently allocated living block also causes undefined behavior.
        Instances of lifo_allocator are not interchangeable: blocks allocated by an instance can't be deallocated with another
        instance. All living blocks must be deallocated before the allocator is destroyed, otherwise the behavior is undefined.

        lifo_allocator is a stateful class template (it has non-static data members). It is uncopyable, unmovable and incomparable.

        The constructor of lifo_allocator is constexpr and guarantees constant initialization.

        Implementation notes
        --------------------------
        A block allocation or deallocation requires only a few ALU instructions and a branch to a
        slow path, that is taken whenever a page switch occurs. The internal state of the allocator is composed by a pointer,
        that points to the next block \ref allocate would return. A call to \ref allocate_empty just return the top of the stack,
        without altering the state of the allocator.
        lifo_allocator does not cache free pages\blocks: when a page or a block is no more used, it is immediately deallocated. */
    template <typename UNDERLYING_ALLOCATOR = default_allocator, size_t ALIGNMENT = alignof(void *)>
    class lifo_allocator : private UNDERLYING_ALLOCATOR
    {
      public:
        static_assert(ALIGNMENT > 0 && is_power_of_2(ALIGNMENT), "ALIGNMENT must be a power of 2");

        /** Alias for the template argument UNDERLYING_ALLOCATOR */
        using underlying_allocator = UNDERLYING_ALLOCATOR;

        /** Alias for the template argument ALIGNMENT */
        static constexpr size_t alignment = ALIGNMENT;

        // compile time checks
        static_assert(
          alignment <= UNDERLYING_ALLOCATOR::page_alignment,
          "The alignment of the pages is too small");

        /** Default constructor, non-throwing and suitable for constant initialization. */
        constexpr lifo_allocator() noexcept {}

        /** Constructs passing the underlying allocator as l-value */
        lifo_allocator(const UNDERLYING_ALLOCATOR & i_underlying_allocator)
            : UNDERLYING_ALLOCATOR(i_underlying_allocator)
        {
        }

        /** Constructs passing the underlying allocator as r-value */
        lifo_allocator(UNDERLYING_ALLOCATOR && i_underlying_allocator)
            : UNDERLYING_ALLOCATOR(std::move(i_underlying_allocator))
        {
        }

        /** Copy construction not allowed */
        lifo_allocator(const lifo_allocator &) = delete;

        /** Copy assignment not allowed */
        lifo_allocator & operator=(const lifo_allocator &) = delete;

        /** Allocates a memory block. The content of the newly allocated memory is undefined. The new memory block
            is aligned at least to \ref alignment.
                @param i_size The size of the requested block, in bytes.
                @return address of the allocated block

            \pre The behavior is undefined if either:
                - i_size is not a multiple of \ref alignment

            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * allocate(size_t i_size)
        {
            DENSITY_ASSERT(i_size % alignment == 0);

            auto const new_top = m_top + i_size;
            auto const new_offset =
              new_top - uint_lower_align(m_top, UNDERLYING_ALLOCATOR::page_alignment);
            if (!DENSITY_LIKELY(new_offset < UNDERLYING_ALLOCATOR::page_size))
            {
                // page overflow
                return allocate_slow_path(i_size);
            }
            else
            {
                // advance m_top
                DENSITY_ASSERT_INTERNAL(i_size <= UNDERLYING_ALLOCATOR::page_size);
                auto const new_block = reinterpret_cast<void *>(m_top);
                m_top                = new_top;
                return new_block;
            }
        }

        /** Allocates a block with size 0.

            This function is equivalent to allocate(0), but it is much faster and never throws.
            The returned block can be reallocated and deallocated.

            This function is useful to initialize block owners to an empty state in a noexcept
            context, without introducing the nullptr special case. The implementation
            of the default constructor of \ref lifo_buffer uses this function.

            \b Throws: nothing.

            <i>Implementation notes</i>: this function just returns the top of the stack, without altering the state of the allocator.

            \snippet lifo_examples.cpp lifo_allocator allocate_empty 2 */
        void * allocate_empty() noexcept { return reinterpret_cast<void *>(m_top); }

        /** Deallocates the most recently allocated living memory block.
                @param i_block The memory block to deallocate
                @param i_size Size of the block.

            \pre The behavior is undefined if either:
                - the specified block is null or it is not the most recently allocated
                - the specified size is not the one asked to the most recent reallocation of the block, or to the allocation (If no reallocation was performed)

            \n\b Throws: nothing. */
        void deallocate(void * i_block, size_t i_size) noexcept
        {
            DENSITY_ASSERT(i_block != nullptr && i_size % alignment == 0);

            // this check detects page switches and external blocks
            if (!DENSITY_LIKELY(same_page(i_block, reinterpret_cast<void *>(m_top))))
            {
                deallocate_slow_path(i_block, i_size);
            }
            else
            {
                m_top = reinterpret_cast<uintptr_t>(i_block);
            }
        }

        /** Reallocates the most recently allocated living memory block, changing its size.
            The address of the block may change. The content of the memory block is preserved up to the existing extend.
            If the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_old_size the previous size of the block, in bytes.
                @param i_new_size the new size requested for the block, in bytes.
                @return the new address of the resized block.

            \pre The behavior is undefined if either:
                - the specified block is null or it is not the most recently allocated
                - i_old_size is not the one asked to the most recent reallocation of the block, or to the allocation (if no reallocation was performed)
                - i_new_size is not a multiple of \ref alignment

            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
        {
            // branch depending on the old block: this check detects page switches and external blocks
            if (same_page(i_block, reinterpret_cast<void *>(m_top)))
            {
                // the following call sets the top only when it is sure to not throw
                auto const new_block = set_top_and_allocate(i_block, i_new_size);

                copy(i_block, i_old_size, new_block, i_new_size);
                return new_block;
            }
            else
            {
                if (
                  (m_top & (UNDERLYING_ALLOCATOR::page_alignment - 1)) == sizeof(PageHeader) &&
                  same_page(reinterpret_cast<PageHeader *>(m_top)[-1].m_prev_page, i_block))
                {
                    auto const page_to_deallocate = reinterpret_cast<void *>(m_top);
                    DENSITY_ASSERT_INTERNAL(!same_page(page_to_deallocate, i_block));

                    // the following call sets the top only when it is sure to not throw
                    auto const new_block = set_top_and_allocate(i_block, i_new_size);

                    copy(i_block, i_old_size, new_block, i_new_size);

                    UNDERLYING_ALLOCATOR::deallocate_page(page_to_deallocate);
                    return new_block;
                }
                else
                {
                    // the old block is external
                    auto const new_block = allocate(i_new_size);
                    copy(i_block, i_old_size, new_block, i_new_size);
                    UNDERLYING_ALLOCATOR::deallocate(i_block, i_old_size, alignment);
                    return new_block;
                }
            }
        }

        /** Destroys the allocator, deallocating the bottom page in case this allocator is not virgin */
        ~lifo_allocator()
        {
            if (m_top != s_virgin_top)
            {
                UNDERLYING_ALLOCATOR::deallocate_page(reinterpret_cast<void *>(m_top));
            }
        }

        /** Returns a reference to the underlying allocator */
        UNDERLYING_ALLOCATOR & underlying_allocator_ref() noexcept { return *this; }

        /** Returns a const reference to the underlying allocator */
        const UNDERLYING_ALLOCATOR & underlying_allocator_ref() const noexcept { return *this; }

      private:
        /** \internal Allocated at the beginning of every page */
        struct alignas(alignment) alignas(void *) PageHeader
        {
            void * m_prev_page; /**< pointer to a byte of the previous page */
        };

        static_assert(
          UNDERLYING_ALLOCATOR::page_size / 2 > sizeof(PageHeader), "page_size is too small");

        /** Returns whether the input addresses belong to the same page or they are both nullptr */
        static bool same_page(const void * i_first, const void * i_second) noexcept
        {
            auto const page_mask = UNDERLYING_ALLOCATOR::page_alignment - 1;
            return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) &
                    ~page_mask) == 0;
        }

        DENSITY_NO_INLINE void * allocate_slow_path(size_t i_size)
        {
            DENSITY_ASSERT_INTERNAL(i_size % alignment == 0);
            if (i_size < UNDERLYING_ALLOCATOR::page_size / 2)
            {
                // allocate a new page
                auto const new_page = UNDERLYING_ALLOCATOR::allocate_page();
                DENSITY_ASSERT_INTERNAL(new_page != nullptr);
                auto const new_header   = new (new_page) PageHeader;
                new_header->m_prev_page = reinterpret_cast<void *>(m_top);
                m_top                   = reinterpret_cast<uintptr_t>(new_header + 1) + i_size;
                return new_header + 1;
            }
            else
            {
                // external block
                return UNDERLYING_ALLOCATOR::allocate(i_size, alignment);
            }
        }

        DENSITY_NO_INLINE void deallocate_slow_path(void * i_block, size_t i_size) noexcept
        {
            DENSITY_ASSERT_INTERNAL(i_size % alignment == 0);

            if (
              (m_top & (UNDERLYING_ALLOCATOR::page_alignment - 1)) == sizeof(PageHeader) &&
              same_page(reinterpret_cast<PageHeader *>(m_top)[-1].m_prev_page, i_block))
            {
                // deallocate the top page
                auto const page_to_deallocate = reinterpret_cast<void *>(m_top);
                DENSITY_ASSERT_INTERNAL(!same_page(page_to_deallocate, i_block));
                UNDERLYING_ALLOCATOR::deallocate_page(page_to_deallocate);
                m_top = reinterpret_cast<uintptr_t>(i_block);
            }
            else
            {
                // external block
                UNDERLYING_ALLOCATOR::deallocate(i_block, i_size, alignment);
            }
        }

        /** This function has the same effect of setting m_top to i_current_top and then allocating a block.
            Provides the strong exception guarantee. */
        void * set_top_and_allocate(void * const i_current_top, size_t const i_size)
        {
            DENSITY_ASSERT_INTERNAL(i_size % alignment == 0);

            auto const current_top = reinterpret_cast<uintptr_t>(i_current_top);

            // we have to procrastinate the assignment of m_top until we have allocated the page or the external block
            auto const new_top = current_top + i_size;
            auto const new_offset =
              new_top - uint_lower_align(current_top, UNDERLYING_ALLOCATOR::page_alignment);
            if (new_offset < UNDERLYING_ALLOCATOR::page_size)
            {
                DENSITY_ASSERT_INTERNAL(i_size <= UNDERLYING_ALLOCATOR::page_size);
                auto const new_block = i_current_top;
                m_top                = new_top;
                return new_block;
            }
            else if (i_size < UNDERLYING_ALLOCATOR::page_size / 2)
            {
                // allocate a new page
                auto const new_page =
                  static_cast<PageHeader *>(UNDERLYING_ALLOCATOR::allocate_page());
                new_page->m_prev_page = reinterpret_cast<void *>(current_top);
                m_top                 = reinterpret_cast<uintptr_t>(new_page + 1) + i_size;
                return new_page + 1;
            }
            else
            {
                // external block
                auto const new_external_block = UNDERLYING_ALLOCATOR::allocate(i_size, alignment);
                m_top                         = reinterpret_cast<uintptr_t>(i_current_top);
                return new_external_block;
            }
        }

        void copy(
          void * i_old_block, size_t i_old_size, void * i_new_block, size_t i_new_size) noexcept
        {
            if (i_old_block != i_new_block)
            {
                auto const size_to_copy = detail::size_min(i_new_size, i_old_size);
                DENSITY_ASSERT_INTERNAL(size_to_copy % alignment == 0);
                DENSITY_ASSERT_ALIGNED(i_old_block, alignment);
                DENSITY_ASSERT_ALIGNED(i_new_block, alignment);
                memcpy(i_new_block, i_old_block, size_to_copy);
            }
        }

      private:
        static constexpr uintptr_t s_virgin_top =
          uint_lower_align(UNDERLYING_ALLOCATOR::page_alignment - 1, alignment);
        uintptr_t m_top = s_virgin_top; /**< pointer to the top of the stack.
                                            This variable is an integer to allow constant initialization
                                            (reinterpret_cast can't be used in compile time evaluations). */
    };

    namespace detail
    {
        /** \internal Stateless class template that provides a thread local LIFO memory management */
        template <int = 0> class ThreadLifoAllocator
        {
#ifndef DENSITY_USER_DATA_STACK

            // default data stack

          public:
            static constexpr size_t alignment =
              lifo_allocator<data_stack_underlying_allocator>::alignment;

            static void * allocate(size_t i_size) { return s_allocator.allocate(i_size); }

            static void * allocate_empty() noexcept { return s_allocator.allocate_empty(); }

            static void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
            {
                return s_allocator.reallocate(i_block, i_old_size, i_new_size);
            }

            static void deallocate(void * i_block, size_t i_size) noexcept
            {
                s_allocator.deallocate(i_block, i_size);
            }

          private:
            static thread_local lifo_allocator<data_stack_underlying_allocator> s_allocator;
#else

            // DENSITY_USER_DATA_STACK is defined: forward to the user defined data stack

          public:
            static constexpr size_t alignment = user_data_stack::alignment;

            static void * allocate(size_t i_size) { return user_data_stack::allocate(i_size); }

            static void * allocate_empty() noexcept { return user_data_stack::allocate_empty(); }

            static void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
            {
                return user_data_stack::reallocate(i_block, i_old_size, i_new_size);
            }

            static void deallocate(void * i_block, size_t i_size) noexcept
            {
                user_data_stack::deallocate(i_block, i_size);
            }

#endif
        };

#ifndef DENSITY_USER_DATA_STACK
        template <int DUMMY>
        thread_local lifo_allocator<data_stack_underlying_allocator>
          ThreadLifoAllocator<DUMMY>::s_allocator;
#endif

    } // namespace detail

    /** Class that allocates a memory block from the thread-local data stack, and owns it. The block
        is deallocated by the destructor. This class should be used only on the automatic storage.

        The data stack is a pool in which a thread can allocate and deallocate memory in LIFO order. It is handled by an
        internal \ref lifo_allocator, which in turn in built upon an instance of \ref default_allocator.
        \ref lifo_array and \ref lifo_buffer allocate on the data stack, so they must respect the LIFO order: only
        the most recently allocated block can be deallocated or reallocated.
        Instantiating \ref lifo_array and \ref lifo_buffer on the automatic storage (locally in a function) is always safe,
        as the language guarantees destruction in reverse order, even inside arrays or aggregate types.

        lifo_buffer provides a low-level service, as it allocates untyped raw memory. It allows resizing preserving
        the content, but it does so memcpy'ng the content when the address of the block changes.

        A thread may resize a lifo_buffer only if it is the most recently instantiated lifo_buffer or lifo_array, otherwise
        the behavior is undefined.

        \snippet lifo_examples.cpp lifo_buffer example 1 */
    class lifo_buffer
    {
      public:
        /** Alignment guaranteed for the block */
        static constexpr size_t alignment = detail::ThreadLifoAllocator<>::alignment;

        /** Allocates an empty block. */
        lifo_buffer() noexcept : m_data(detail::ThreadLifoAllocator<>::allocate_empty()), m_size(0)
        {
        }

        /** Allocates the block
            @param i_size size in bytes of the memory block. */
        lifo_buffer(size_t i_size)
            : m_data(detail::ThreadLifoAllocator<>::allocate(uint_upper_align(i_size, alignment))),
              m_size(i_size)
        {
        }

        /** Copy construction not allowed */
        lifo_buffer(const lifo_buffer &) = delete;

        /** Copy assignment not allowed */
        lifo_buffer & operator=(const lifo_buffer &) = delete;

        /** Deallocates the block */
        ~lifo_buffer()
        {
            detail::ThreadLifoAllocator<>::deallocate(m_data, uint_upper_align(m_size, alignment));
        }

        /** Changes the size of the block, preserving its existing content.
            If the buffer grows (the new size is bigger than the previous one) the new content has undefined content.

            @param i_new_size size in bytes of the memory block.
            @return the new address of the block

            This function may reallocate the block, so its address may change.

            \pre The behavior is undefined if either:
                - this block was not the most recently allocated one on the data stack of the calling thread

            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * resize(size_t i_new_size)
        {
            /* we must allocate before updating any data member, so that in case of exception no change remains */
            auto const block = m_data = detail::ThreadLifoAllocator<>::reallocate(
              m_data, uint_upper_align(m_size, alignment), uint_upper_align(i_new_size, alignment));
            m_size = i_new_size;
            return block;
        }

        /** Returns the address of the owned memory block. If the block is empty (that is the size is zero), the
            return value is undefined (may be nullptr, or any address), but it's still aligned to \ref alignment. */
        void * data() const noexcept { return m_data; }

        /** Returns the size of the owned memory block. */
        size_t size() const noexcept { return m_size; }

      private:
        void * m_data;
        size_t m_size;
    };

    namespace detail
    {
        template <
          typename TYPE,
          bool OVER_ALIGNED = (alignof(TYPE) > detail::ThreadLifoAllocator<>::alignment)>
        class LifoArrayImpl;

        template <typename TYPE> class LifoArrayImpl<TYPE, false>
        {
          public:
            static size_t compute_mem_size(size_t i_element_count) noexcept
            {
                auto const mem_size  = i_element_count * sizeof(TYPE);
                bool already_aligned = sizeof(TYPE) % detail::ThreadLifoAllocator<>::alignment == 0;
                if (already_aligned)
                    return mem_size;
                else
                    return uint_upper_align(mem_size, detail::ThreadLifoAllocator<>::alignment);
            }

            LifoArrayImpl(size_t i_element_count)
                : m_elements(static_cast<TYPE *>(
                    detail::ThreadLifoAllocator<>::allocate(compute_mem_size(i_element_count)))),
                  m_size(i_element_count)
            {
            }

            ~LifoArrayImpl()
            {
                detail::ThreadLifoAllocator<>::deallocate(m_elements, compute_mem_size(m_size));
            }

          protected:
            TYPE * const m_elements;
            size_t const
              m_size; /**< number of elements. Note: the term 'size' for the number of elements in the container
                                 was a bad choice, because usually it indicates the number of bytes. */
        };

        template <typename TYPE> class LifoArrayImpl<TYPE, true>
        {
          public:
            static constexpr size_t size_overhead =
              alignof(TYPE) - detail::ThreadLifoAllocator<>::alignment;

            static size_t compute_mem_size(size_t i_element_count) noexcept
            {
                auto const mem_size  = i_element_count * sizeof(TYPE) + size_overhead;
                bool already_aligned = sizeof(TYPE) % detail::ThreadLifoAllocator<>::alignment == 0;
                if (already_aligned)
                    return mem_size;
                else
                    return uint_upper_align(mem_size, detail::ThreadLifoAllocator<>::alignment);
            }

            LifoArrayImpl(size_t i_size) : m_size(i_size)
            {
                auto const mem_size = compute_mem_size(m_size);
                m_block             = detail::ThreadLifoAllocator<>::allocate(mem_size);
                m_elements = static_cast<TYPE *>(address_upper_align(m_block, alignof(TYPE)));
                DENSITY_ASSERT_INTERNAL(
                  address_diff(m_elements, m_block) <= size_overhead &&
                  m_elements + m_size <= address_add(m_block, mem_size));
            }

            ~LifoArrayImpl()
            {
                auto const mem_size = compute_mem_size(m_size);
                detail::ThreadLifoAllocator<>::deallocate(m_block, mem_size);
            }

          protected:
            void * m_block;
            TYPE * m_elements;
            size_t const
              m_size; /**< number of elements. Note: the term 'size' for the number of elements in the container
                                 was a bad choice, because usually it indicates the number of bytes. */
        };
    } // namespace detail

    /** Array container that allocates its elements from the thread-local data stack, and owns it. The block
        is deallocated by the destructor. This class should be used only on the automatic storage.

        @tparam TYPE Element type.

        The data stack is a pool in which a thread can allocate and deallocate memory in LIFO order. It is handled by an
        internal \ref lifo_allocator, which in turn in built upon an instance of \ref default_allocator.
        \ref lifo_array and \ref lifo_buffer allocate on the data stack, so they must respect the LIFO order: only
        the most recently allocated block can be deallocated or reallocated.
        Instantiating \ref lifo_array and \ref lifo_buffer on the automatic storage (locally in a function) is always safe,
        as the language guarantees destruction in reverse order, even inside arrays or aggregate types.

        Exactly like raw arrays and <code>std::array</code>, the default constructor of \ref lifo_array default-constructs the elements. 
        So if elements have POD type, they are not initialized. In contrast, <code>std::vector</code> value-initializes its elements, 
        so POD types are initialized to zero.

        \snippet lifo_examples.cpp lifo_array example 2

        The size of a lifo_array is fixed at construction time and is immutable.
        Elements are constructed in positional order and destroyed in reverse order.

        \snippet lifo_examples.cpp lifo_array example 1 */
    template <typename TYPE> class lifo_array final : detail::LifoArrayImpl<TYPE>
    {
        struct PrivateTag
        {
        };

      public:
        using value_type      = TYPE;
        using reference       = TYPE &;
        using const_reference = TYPE &;
        using pointer         = TYPE *;
        using const_pointer   = const TYPE *;
        using size_type       = size_t;
        using difference_type = ptrdiff_t;

        class iterator
        {
          public:
            using difference_type   = ptrdiff_t;
            using value_type        = TYPE;
            using pointer           = TYPE *;
            using reference         = TYPE &;
            using iterator_category = std::random_access_iterator_tag;

            iterator(PrivateTag, TYPE * i_ptr) noexcept : m_curr(i_ptr) {}

            iterator() noexcept                 = default;
            iterator(const iterator &) noexcept = default;
            iterator & operator=(const iterator &) noexcept = default;

            TYPE * operator->() const noexcept { return m_curr; }

            TYPE & operator*() const & noexcept { return *m_curr; }

            TYPE && operator*() const && noexcept { return *m_curr; }

            iterator & operator++() noexcept
            {
                m_curr++;
                return *this;
            }

            iterator operator++(int) noexcept
            {
                iterator copy(*this);
                         operator++();
                return copy;
            }

            iterator & operator+=(difference_type i_diff) noexcept
            {
                m_curr += i_diff;
                return *this;
            }

            iterator & operator--() noexcept
            {
                m_curr--;
                return *this;
            }

            iterator operator--(int) noexcept
            {
                iterator copy(*this);
                         operator++();
                return copy;
            }

            iterator & operator-=(difference_type i_diff) noexcept
            {
                m_curr -= i_diff;
                return *this;
            }

            friend difference_type
              operator-(const iterator & i_first, const iterator & i_second) noexcept
            {
                return i_first.m_curr - i_second.m_curr;
            }

            // relational operators

            friend bool operator<(const iterator & i_first, const iterator & i_second) noexcept
            {
                return i_first.m_curr < i_second.m_curr;
            }

            friend bool operator<=(const iterator & i_first, const iterator & i_second) noexcept
            {
                return i_first.m_curr <= i_second.m_curr;
            }

            friend bool operator==(const iterator & i_first, const iterator & i_second) noexcept
            {
                return i_first.m_curr == i_second.m_curr;
            }

            friend bool operator!=(const iterator & i_first, const iterator & i_second) noexcept
            {
                return i_first.m_curr != i_second.m_curr;
            }

            friend bool operator>(const iterator & i_first, const iterator & i_second) noexcept
            {
                return i_first.m_curr > i_second.m_curr;
            }

            friend bool operator>=(const iterator & i_first, const iterator & i_second) noexcept
            {
                return i_first.m_curr >= i_second.m_curr;
            }

          private:
            TYPE * m_curr{};
        };

        class const_iterator
        {
          public:
            using difference_type   = ptrdiff_t;
            using value_type        = const TYPE;
            using pointer           = const TYPE *;
            using reference         = const TYPE &;
            using iterator_category = std::random_access_iterator_tag;

            const_iterator(PrivateTag, const TYPE * i_ptr) noexcept : m_curr(i_ptr) {}

            const_iterator() noexcept                       = default;
            const_iterator(const const_iterator &) noexcept = default;
            const_iterator & operator=(const const_iterator &) noexcept = default;

            const TYPE * operator->() const noexcept { return m_curr; }

            const TYPE & operator*() const & noexcept { return *m_curr; }

            const TYPE && operator*() const && noexcept { return *m_curr; }

            const_iterator & operator++() noexcept
            {
                m_curr++;
                return *this;
            }

            const_iterator operator++(int) noexcept
            {
                const_iterator copy(*this);
                               operator++();
                return copy;
            }

            const_iterator & operator+=(difference_type i_diff) noexcept
            {
                m_curr += i_diff;
                return *this;
            }

            const_iterator & operator--() noexcept
            {
                m_curr--;
                return *this;
            }

            const_iterator operator--(int) noexcept
            {
                const_iterator copy(*this);
                               operator++();
                return copy;
            }

            const_iterator & operator-=(difference_type i_diff) noexcept
            {
                m_curr -= i_diff;
                return *this;
            }

            friend difference_type
              operator-(const const_iterator & i_first, const const_iterator & i_second) noexcept
            {
                return i_first.m_curr - i_second.m_curr;
            }

            // relational operators

            friend bool
              operator<(const const_iterator & i_first, const const_iterator & i_second) noexcept
            {
                return i_first.m_curr < i_second.m_curr;
            }

            friend bool
              operator<=(const const_iterator & i_first, const const_iterator & i_second) noexcept
            {
                return i_first.m_curr <= i_second.m_curr;
            }

            friend bool
              operator==(const const_iterator & i_first, const const_iterator & i_second) noexcept
            {
                return i_first.m_curr == i_second.m_curr;
            }

            friend bool
              operator!=(const const_iterator & i_first, const const_iterator & i_second) noexcept
            {
                return i_first.m_curr != i_second.m_curr;
            }

            friend bool
              operator>(const const_iterator & i_first, const const_iterator & i_second) noexcept
            {
                return i_first.m_curr > i_second.m_curr;
            }

            friend bool
              operator>=(const const_iterator & i_first, const const_iterator & i_second) noexcept
            {
                return i_first.m_curr >= i_second.m_curr;
            }

          private:
            const TYPE * m_curr{};
        };

        /** Constructs a lifo_array and all its elements. If elements are POD types, they are not initialized.
            @param i_size number of elements of the array */
        lifo_array(size_t i_size) : detail::LifoArrayImpl<TYPE>(i_size)
        {
            default_construct(std::is_pod<TYPE>());
        }

        /** Constructs all the elements with the provided parameter pack.
            @param i_size number of elements of the array
            @param i_construction_params construction parameters to use for every element of the array

            \snippet lifo_examples.cpp lifo_array constructor 3 */
        template <typename... CONSTRUCTION_PARAMS>
        lifo_array(size_t i_size, const CONSTRUCTION_PARAMS &... i_construction_params)
            : detail::LifoArrayImpl<TYPE>(i_size)
        {
            size_t element_index = 0;
            try
            {
                for (; element_index < i_size; element_index++)
                {
                    new (m_elements + element_index) TYPE(i_construction_params...);
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
                throw;
            }
        }

        /** Initializes the array from an input range. The size of the array is computed with <code>std::distance(i_begin, i_end)</code>.
            @param i_begin iterator pointing to the first source value
            @param i_end iterator pointing to the first value that is not copied in the array.

            \snippet lifo_examples.cpp lifo_array constructor 2 */
        template <typename INPUT_ITERATOR>
        lifo_array(
          INPUT_ITERATOR i_begin,
          INPUT_ITERATOR i_end,
          typename std::iterator_traits<INPUT_ITERATOR>::iterator_category =
            typename std::iterator_traits<INPUT_ITERATOR>::iterator_category())
            : detail::LifoArrayImpl<TYPE>(std::distance(i_begin, i_end))
        {
            auto dest_element = m_elements;
            try
            {
                for (; i_begin != i_end; ++i_begin)
                {
                    DENSITY_ASSERT_INTERNAL(dest_element != nullptr); // hint for the optimizer
                    new (dest_element) TYPE(*i_begin);
                    ++dest_element;
                }
            }
            catch (...)
            {
                // destroy all the elements constructed until now. The destructor of the base class will deallocate the block
                auto count_to_destroy = dest_element - m_elements;
                while (count_to_destroy > 0)
                {
                    --dest_element;
                    --count_to_destroy;
                    dest_element->TYPE::~TYPE();
                }
                throw;
            }
        }

        /** Copy construction not allowed */
        lifo_array(const lifo_array &) = delete;

        /** Copy assignment not allowed */
        lifo_array & operator=(const lifo_array &) = delete;

        /** Destroys a lifo_array and all its elements. Elements are destroyed in reverse positional order. */
        ~lifo_array() { destroy_elements(std::is_trivially_destructible<TYPE>()); }

        /** Returns the number of elements */
        size_t size() const noexcept { return m_size; }

        /** Returns a reference to the i-th element.

            \pre The behavior is undefined if either:
                - i_index >= \ref size
        */
        TYPE & operator[](size_t i_index) noexcept
        {
            DENSITY_ASSERT(i_index < m_size);
            return m_elements[i_index];
        }

        /** Returns a const reference to the i-th element.

            \pre The behavior is undefined if either:
                - i_index >= \ref size
        */
        const TYPE & operator[](size_t i_index) const noexcept
        {
            DENSITY_ASSERT(i_index < m_size);
            return m_elements[i_index];
        }

        /** Returns a pointer to the first element. */
        pointer data() noexcept { return m_elements; }

        /** Returns a pointer to the first element. */
        const_pointer data() const noexcept { return m_elements; }

        iterator begin() noexcept { return iterator(PrivateTag{}, m_elements); }

        iterator end() noexcept { return iterator(PrivateTag{}, m_elements + m_size); }

        const_iterator cbegin() const noexcept { return const_iterator(PrivateTag{}, m_elements); }

        const_iterator cend() const noexcept
        {
            return const_iterator(PrivateTag{}, m_elements + m_size);
        }

        const_iterator begin() const noexcept { return const_iterator(PrivateTag{}, m_elements); }

        const_iterator end() const noexcept
        {
            return const_iterator(PrivateTag{}, m_elements + m_size);
        }

      private:
        // trivially default constructible types
        void default_construct(std::true_type) {}

        // non-trivially default constructible types
        void default_construct(std::false_type)
        {
            size_t element_index = 0;
            try
            {
                for (; element_index < m_size; element_index++)
                {
                    auto const dest = m_elements + element_index;
                    DENSITY_ASSERT_INTERNAL(dest != nullptr); // hint for the optimizer
                    new (dest) TYPE();
                }
            }
            catch (...)
            {
                // destroy all the elements constructed until now. The destructor of the base class will deallocate the block
                auto count_to_destroy = element_index;
                while (count_to_destroy > 0)
                {
                    --count_to_destroy;
                    m_elements[count_to_destroy].TYPE::~TYPE();
                }
                throw;
            }
        }

        void destroy_elements(std::true_type) noexcept {}

        void destroy_elements(std::false_type) noexcept
        {
            auto it = m_elements + m_size;
            it--;
            for (; it >= m_elements; it--)
            {
                it->TYPE::~TYPE();
            }
        }

      private:
        using detail::LifoArrayImpl<TYPE>::m_elements;
        using detail::LifoArrayImpl<TYPE>::m_size;
    };

    /*! \page lifo_array_benchmarks

        Benchmarks of %lifo_array
        ----------
        ----------

        In this tests \ref lifo_array is compared with a legacy heap array, the function
        [_alloca](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/alloca), and the function
        [_malloca](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/malloca) (a Microsoft-specific safer version).
        A comparison with <code>std::vector</code> wouldn't be fair because <code>std::vector</code> value-initializes the elements
        even if they are pod types, while %lifo_array doesn't.

        The benchmark is composed by two tests:

        - An array of <code>float</code>s is allocated, the first element is zeroed, and then the array is deallocated.
        - An array of <code>double</code>s is allocated, the first element is zeroed, and then the array is deallocated. This test is an head-to-head
            between <code>alloca</code> and <code>%lifo_array</code>.

        Summary of the results
        ----------
        All the tests are compiled with Visual Studio 2017 (version 15.1), and executed on Windows 10 with an int Intel(R) Core(TM) i7-7700HQ CPU @ 2.80GHz,
        2808 Mhz, 4 cores, 8 logical processors. The benchmarks of the whole library are shuffled and interleaved at the granularity of a single invocation
        with a given cardinality, and every invocation occurs 8 times.

        The results are quite noisy: there are two separate classes of players, but no clear winner in each of them.

        - <code>_malloca</code> is roughly the same to the array on the heap (actually many times it's slower). The reason is that this
            function allocates on the heap sizes bigger than 1024 bytes, and the tests allocate up to 30-40 kibibytes. Note that <code>_malloca</code>
            needs a deallocation function to be called ([_freea](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/freea)), and that it
            can still possibly cause a stack overflow, though much more rarely than <code>alloca</code>.

        - <code>alloca</code> and <code>%lifo_array</code> are very close, with the first being ~80% of times slightly faster if the size an element is
            not a multiple of lifo_allocator::alignment (currently = <code>alignof(void*)</code>). Otherwise <code>%lifo_array</code> can skip some
            ALU instructions that align the size of the block. I'm not sure that this is the reason, but this case it's often slightly faster than
            <code>alloca</code>. Anyway both <code>alloca</code> and <code>%lifo_array</code> are very fast.


        Generated code for %lifo_array
        ----------

        Visual Stdio 2017 translates the source code of the first test:

        ~~~~~~~~~~~~~~
        lifo_array<char> chars(i_cardinality);
        volatile char c = 0;
        chars[0] = c;
        ~~~~~~~~~~~~~~

        to this x64 code:

        ~~~~~~~~~~~~~~
            lifo_array<char> chars(i_cardinality);
        00007FF636BD8C5F  mov         rax,qword ptr gs:[58h]  
        00007FF636BD8C68  lea         rdi,[rdx+7]  
        00007FF636BD8C6C  and         rdi,0FFFFFFFFFFFFFFF8h  
        00007FF636BD8C70  mov         ebx,128h  
        00007FF636BD8C75  add         rbx,qword ptr [rax]  
        00007FF636BD8C78  mov         rdx,qword ptr [rbx]  
        00007FF636BD8C7B  mov         rcx,rdx  
        00007FF636BD8C7E  and         rcx,0FFFFFFFFFFFF0000h  
        00007FF636BD8C85  lea         r8,[rdi+rdx]  
        00007FF636BD8C89  mov         rax,r8  
        00007FF636BD8C8C  sub         rax,rcx  
        00007FF636BD8C8F  cmp         rax,0FFF0h  
        00007FF636BD8C95  jb          <lambda_b8a4ec06313d93817536a7cf8b449dcc>::operator()+57h (07FF636BD8CA7h)  
        00007FF636BD8C97  mov         rdx,rdi  
        00007FF636BD8C9A  mov         rcx,rbx  
        00007FF636BD8C9D  call        density::lifo_allocator<density::basic_default_allocator<65536>,8>::allocate_slow_path (07FF636BD9220h)  
        00007FF636BD8CA2  mov         rdx,rax  
        00007FF636BD8CA5  jmp         <lambda_b8a4ec06313d93817536a7cf8b449dcc>::operator()+5Ah (07FF636BD8CAAh)  
        00007FF636BD8CA7  mov         qword ptr [rbx],r8  
                    volatile char c = 0;
        00007FF636BD8CAA  mov         byte ptr [rsp+30h],0  
                    chars[0] = c;
        00007FF636BD8CAF  movzx       eax,byte ptr [c]  
        00007FF636BD8CB4  mov         byte ptr [rdx],al  
                }, __LINE__);
        00007FF636BD8CB6  mov         rax,qword ptr [rbx]  
        00007FF636BD8CB9  xor         rax,rdx  
        00007FF636BD8CBC  test        rax,0FFFFFFFFFFFF0000h  
        00007FF636BD8CC2  je          <lambda_b8a4ec06313d93817536a7cf8b449dcc>::operator()+89h (07FF636BD8CD9h)  
        00007FF636BD8CC4  mov         r8,rdi  
        00007FF636BD8CC7  mov         rcx,rbx  
        00007FF636BD8CCA  mov         rbx,qword ptr [rsp+38h]  
        00007FF636BD8CCF  add         rsp,20h  
        00007FF636BD8CD3  pop         rdi  
        00007FF636BD8CD4  jmp         density::lifo_allocator<density::basic_default_allocator<65536>,8>::deallocate_slow_path (07FF636BD9190h)  
        00007FF636BD8CD9  mov         qword ptr [rbx],rdx  
        00007FF636BD8CDC  mov         rbx,qword ptr [rsp+38h]
        ~~~~~~~~~~~~~~

        The branch after <code>cmp</code> and the one after <code>test</code> skip the slow paths. They are always taken unless
        a page switch is required, or the requested block does not fit in a page (in which case the block is allocated in the
        heap). The branch predictor of the cpu should do a good job here.

        Note that the first instructions align the size of the block to 8 bytes.

        The source code of second test:

        ~~~~~~~~~~~~~~
        lifo_array<double> chars(i_cardinality);
        volatile double c = 0;
        chars[0] = c;
        ~~~~~~~~~~~~~~

        is translated to:

        ~~~~~~~~~~~~~~
        00007FF636BD907F  mov         rax,qword ptr gs:[58h]  
        00007FF636BD9088  lea         rdi,[rdx*8]  
        00007FF636BD9090  mov         ebx,128h  
        00007FF636BD9095  add         rbx,qword ptr [rax]  
        00007FF636BD9098  mov         rdx,qword ptr [rbx]  
        00007FF636BD909B  mov         rcx,rdx  
        00007FF636BD909E  and         rcx,0FFFFFFFFFFFF0000h  
        00007FF636BD90A5  lea         r8,[rdx+rdi]  
        00007FF636BD90A9  mov         rax,r8  
        00007FF636BD90AC  sub         rax,rcx  
        00007FF636BD90AF  cmp         rax,0FFF0h  
        00007FF636BD90B5  jb          <lambda_c7840347498da9fa856125956fe6e478>::operator()+57h (07FF636BD90C7h)  
        00007FF636BD90B7  mov         rdx,rdi  
        00007FF636BD90BA  mov         rcx,rbx  
        00007FF636BD90BD  call        density::lifo_allocator<density::basic_default_allocator<65536>,8>::allocate_slow_path (07FF636BD9220h)  
        00007FF636BD90C2  mov         rdx,rax  
        00007FF636BD90C5  jmp         <lambda_c7840347498da9fa856125956fe6e478>::operator()+5Ah (07FF636BD90CAh)  
        00007FF636BD90C7  mov         qword ptr [rbx],r8  
        00007FF636BD90CA  xorps       xmm0,xmm0  
                    volatile double c = 0;
        00007FF636BD90CD  movsd       mmword ptr [rsp+30h],xmm0  
                    chars[0] = c;
        00007FF636BD90D3  movsd       xmm1,mmword ptr [c]  
                }, __LINE__);
        00007FF636BD90D9  mov         rax,qword ptr [rbx]  
        00007FF636BD90DC  xor         rax,rdx  
                    chars[0] = c;
        00007FF636BD90DF  movsd       mmword ptr [rdx],xmm1  
                }, __LINE__);
        00007FF636BD90E3  test        rax,0FFFFFFFFFFFF0000h  
        00007FF636BD90E9  je          <lambda_c7840347498da9fa856125956fe6e478>::operator()+90h (07FF636BD9100h)  
        00007FF636BD90EB  mov         r8,rdi  
        00007FF636BD90EE  mov         rcx,rbx  
        00007FF636BD90F1  mov         rbx,qword ptr [rsp+38h]  
        00007FF636BD90F6  add         rsp,20h  
        00007FF636BD90FA  pop         rdi  
        00007FF636BD90FB  jmp         density::lifo_allocator<density::basic_default_allocator<65536>,8>::deallocate_slow_path (07FF636BD9190h)  
        00007FF636BD9100  mov         qword ptr [rbx],rdx  
        00007FF636BD9103  mov         rbx,qword ptr [rsp+38h]

        ~~~~~~~~~~~~~~

        In this case the size of the array is always aligned to 8 bytes, so there is some less ALU instructions.

        Results of the first test
        ----------

        \image html lifo_array_b1_1.png
        \image html lifo_array_b1_2.png
        \image html lifo_array_b1_3.png


        Results of the second test
        ----------

        \image html lifo_array_b2_1.png
        \image html lifo_array_b2_2.png
        \image html lifo_array_b2_3.png
    */

} // namespace density
