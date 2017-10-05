
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
        It is designed to be efficient, so it does not provide an high-level service to be used directly. All blocks has the 
        same fixed guaranteed alignment, and the size of the requested blocks must be a multiple of the alignment.
        Deallocation and reallocation functions require the caller to specify the size of the block.
        
        lifo_allocator handles block sizes bigger than the size of a page with legacy heap allocations.

        All blocks must be deallocated before the allocator is destroyed, otherwise the behavior is undefined.        

        Memory is allocated/freed with the member functions \ref allocate and \ref deallocate.
        A living block is a block allocated, eventually reallocated, but not yet deallocated.
        Only the most recently allocated living block can be deallocated or reallocated. If a block which is not
        the most recently allocated living block is deallocated or reallocated, the behavior is undefined.
        To simplify the implementation, lifo_allocator does not support custom alignments: every block is guaranteed 
        to be aligned to \ref alignment.

        Blocks allocated with an instance of lifo_allocator can't be deallocated with another instance of lifo_allocator.
        The user must deallocate all the living blocks before the allocator is destroyed, otherwise the behavior is 
        undefined.

        lifo_allocator allocates a new page from the underlying allocator when a block is requested by the user and
        there is no space in the last allocated page. Anyway, if the requested block is too big to fit in a page,
        a legacy heap block is allocated from the underlying allocator.

        lifo_allocator is a stateful class template (it has non-static data members). It is uncopyable and unmovable.

        The constructor of lifo_allocator is constexpr and guarantees constant initialization.

        Implementation notes
        --------------------------
        A block allocation or deallocation requires only a few ALU instructions and a branch to a 
        slow path, that is taken whenever a page switch occurs. The internal state of the allocator is composed by a pointer,
        that points to the next block \ref allocate would return.
        lifo_allocator does not cache free pages\blocks: when a page or a block is no more used, it is immediately deallocated. */
    template <typename UNDERLYING_ALLOCATOR = void_allocator>
        class lifo_allocator : private UNDERLYING_ALLOCATOR
    {
    public:

        /** Alias for the template argument UNDERLYING_ALLOCATOR */
        using underlying_allocator = UNDERLYING_ALLOCATOR;

        /** Alignment of the memory blocks. The value is implementation defined */
        static constexpr size_t alignment = alignof(void*);

        /** Max size of a single block. This size is equal to the size of the address space, minus an implementation defined value in 
            the order of the size of a page. Requesting a bigger size to an allocation function causes undefined behavior. */
        static constexpr size_t max_block_size = (std::numeric_limits<uintptr_t>::max() - UNDERLYING_ALLOCATOR::page_size);

        // compile time checks
        static_assert(alignment <= UNDERLYING_ALLOCATOR::page_alignment, "The alignment of the pages is too small");

        /** Default constructor, suitable for constant initialization. */
        constexpr lifo_allocator() noexcept {}

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
                - i_size is not a multiple of \ref alignment
                - i_size > \ref max_block_size

            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * allocate(size_t i_size)
        {
            DENSITY_ASSERT(i_size % alignment == 0);

            auto const new_top = m_top + i_size;
            auto const new_offset = new_top - uint_lower_align(m_top, UNDERLYING_ALLOCATOR::page_alignment);
            if (new_offset >= UNDERLYING_ALLOCATOR::page_size)
            {
                return allocate_slow_path(i_size);
            }
            else
            {
                DENSITY_ASSERT_INTERNAL(i_size <= UNDERLYING_ALLOCATOR::page_size);
                auto const new_block = reinterpret_cast<void*>(m_top);
                m_top = new_top;
                return new_block;                
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
            DENSITY_ASSERT(i_block != nullptr && i_size % alignment == 0);

            // this check detects page switches and external blocks
            if (!same_page(i_block, reinterpret_cast<void*>(m_top)))
            {
                deallocate_slow_path(i_block, i_size);
            }
            else
            {
                m_top = reinterpret_cast<uintptr_t>(i_block);
            }
        }

        /** Reallocates the most recently allocated living memory block, changing its size.
            The address of the block may change. The content of the memory block is preserved up to the existing extend:
            if the memory block is moved to another address, its content is copied with memcopy.
                @param i_block block to be resized. After the call, if no exception is thrown, this pointer must be discarded,
                    because it may point to invalid memory.
                @param i_old_size the previous size of the block, in bytes.
                @param i_new_size the new size requested for the block, in bytes.
                @return the new address of the resized block.

            \pre The behavior is undefined if either:
                - the specified block is null or it is not the most recently allocated
                - i_old_size is not the one asked to the most recent reallocation of the block, or to the allocation (If no reallocation was performed)
                - i_new_size is not a multiple of \ref alignment
                - i_new_size > \ref max_block_size

            \n\b Throws: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
        {
            // branch depending on the old block: this check detects page switches and external blocks
            if (same_page(i_block, reinterpret_cast<void*>(m_top)))
            {
                // the following call sets the top only when it is sure to not throw
                auto const new_block = set_top_and_allocate(i_block, i_new_size);
                
                copy(i_block, i_old_size, new_block, i_new_size);
                return new_block;
            }
            else
            {
                if (i_old_size < UNDERLYING_ALLOCATOR::page_size)
                {
                    auto const page_to_deallocate = reinterpret_cast<void*>(m_top);
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

    private:

        /** Returns whether the input addresses belong to the same page or they are both nullptr */
        static bool same_page(const void * i_first, const void * i_second) noexcept
        {
            auto const page_mask = UNDERLYING_ALLOCATOR::page_alignment - 1;
            return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
        }

        DENSITY_NO_INLINE void * allocate_slow_path(size_t i_size)
        {
            DENSITY_ASSERT_INTERNAL(i_size % alignment == 0);
            if (i_size < UNDERLYING_ALLOCATOR::page_size)
            {
                // allocate a new page
                auto const new_page = UNDERLYING_ALLOCATOR::allocate_page();
                m_top = reinterpret_cast<uintptr_t>(new_page) + i_size;
                return new_page;
            }
            else
            {
                // external block
                return UNDERLYING_ALLOCATOR::allocate(i_size, alignment);
            }
        }

        DENSITY_NO_INLINE void deallocate_slow_path(void * i_block, size_t i_size) noexcept
        {
            // align the size
            auto const actual_size = uint_upper_align(i_size, alignment);
            if (actual_size < UNDERLYING_ALLOCATOR::page_size)
            {
                auto const page_to_deallocate = reinterpret_cast<void*>(m_top);
                DENSITY_ASSERT_INTERNAL(!same_page(page_to_deallocate, i_block));
                UNDERLYING_ALLOCATOR::deallocate_page(page_to_deallocate);
                m_top = reinterpret_cast<uintptr_t>(i_block);
            }
            else
            {
                // external block
                UNDERLYING_ALLOCATOR::deallocate(i_block, actual_size, alignment);
            }
        }

        /** Sets has the same effect of setting m_top to i_current_top and then allocating a block.
            This function provides the strong exception guarantee. */
        void * set_top_and_allocate(void * const i_current_top, size_t const i_size)
        {
            DENSITY_ASSERT_INTERNAL(i_size % alignment == 0);

            auto const current_top = reinterpret_cast<uintptr_t>(i_current_top);

            // we have to procrastinate the assignment of m_top until we have allocated the page or the external block
            auto const new_top = current_top + i_size;
            auto const new_offset = new_top - uint_lower_align(current_top, UNDERLYING_ALLOCATOR::page_alignment);
            if (new_offset < UNDERLYING_ALLOCATOR::page_size)
            {
                DENSITY_ASSERT_INTERNAL(i_size <= UNDERLYING_ALLOCATOR::page_size);
                auto const new_block = i_current_top;
                m_top = new_top;
                return new_block;
            }
            else
            {
                // this branch does not use the value of m_top
                return allocate_slow_path(i_size);
            }
        }

        void copy(void * i_old_block, size_t i_old_size, void * i_new_block, size_t i_new_size) noexcept
        {
            if (i_old_block != i_new_block)
            {
                auto const size_to_copy = detail::size_min(i_new_size, i_old_size);
                DENSITY_ASSERT_INTERNAL(size_to_copy % alignment == 0); // this may help the optimizer
                memcpy(i_new_block, i_old_block, size_to_copy);
            }
        }

    private:
        uintptr_t m_top = UNDERLYING_ALLOCATOR::page_alignment - 1; /**< pointer to the top of the stack. This variable is an integer
                                                                    to allow constant initialization (reinterpret_cast can't be used in
                                                                    compile time evaluations). */
    };

    namespace detail
    {
        /** \internal Stateless class template that provides a thread local LIFO memory management */
        template <int=0> class ThreadLifoAllocator
        {
        public:

            static constexpr size_t alignment = lifo_allocator<data_stack_underlying_allocator>::alignment;
            static constexpr size_t max_block_size = lifo_allocator<data_stack_underlying_allocator>::max_block_size;

            static void * allocate(size_t i_size)
            {
                return s_allocator.allocate(i_size);
            }
            
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
        };

        template <int DUMMY>
            thread_local lifo_allocator<data_stack_underlying_allocator> ThreadLifoAllocator<DUMMY>::s_allocator;

    } // namespace detail

    /** Class that allocates a memory block from the thread-local data stack, and owns it. The block
        is deallocated by the destructor. This class should be used only on the automatic storage.

        The data stack is a pool in which a thread can allocate and deallocate memory in LIFO order. It is handled by an 
        internal lifo allocator, which in turn in built upon an \ref instance of data_stack_underlying_allocator.
        \ref lifo_array and \ref lifo_buffer allocate on the data stack, so they must respect the LIFO order: only
        the most recently allocated block can be deallocated or reallocated.
        Instantiating \ref lifo_array and \ref lifo_buffer on the automatic storage (locally in a function) is always safe,
        as the language guarantees destruction in reverse order, even inside arrays or aggregate types.

        lifo_buffer provides a low-level service, as it allocates untyped raw memory. It allows resizing preserving 
        the content, but it does so memcpy'ng the content when the address of the block changes.

        \snippet lifo_examples.cpp lifo_buffer example 1 */
    class lifo_buffer
    {
    public:

        /** Alignment guaranteed for the block */
        static constexpr size_t alignment = detail::ThreadLifoAllocator<>::alignment;
        
        /** Max size of a block. This size is equal to the size of the address space, minus an implementation defined value in 
            the order of the size of a page. Requesting a bigger block size causes undefined behavior. */
        static constexpr size_t max_block_size = detail::ThreadLifoAllocator<>::max_block_size;

        /** Allocates the block
            @param i_size size in bytes of the memory block.
            
            \pre The behavior is undefined if either:
                - i_size > \ref max_block_size */
        lifo_buffer(size_t i_size = 0)
            : m_data(detail::ThreadLifoAllocator<>::allocate(uint_upper_align(i_size, alignment))), m_size(i_size)
        {
        }

        /** Copy construction not allowed */
        lifo_buffer(const lifo_buffer &) = delete;

        /** Copy assignment not allowed */
        lifo_buffer & operator = (const lifo_buffer &) = delete;

        /** Deallocates the block */
        ~lifo_buffer()
        {
            detail::ThreadLifoAllocator<>::deallocate(m_data, uint_upper_align(m_size, alignment));
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
            m_data = detail::ThreadLifoAllocator<>::reallocate(m_data, 
                uint_upper_align(m_size, alignment), 
                uint_upper_align(i_new_size, alignment));
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
        template <typename TYPE, bool OVER_ALIGNED = (alignof(TYPE) > detail::ThreadLifoAllocator<>::alignment) >
            class LifoArrayImpl;

        template <typename TYPE>
            class LifoArrayImpl<TYPE, false >
        {
        public:

            static size_t compute_mem_size(size_t i_element_count) noexcept
            {
                auto const mem_size = i_element_count * sizeof(TYPE);
                bool already_aligned = sizeof(TYPE) % detail::ThreadLifoAllocator<>::alignment == 0;
                if (already_aligned)
                    return mem_size;
                else
                    return uint_upper_align(mem_size, detail::ThreadLifoAllocator<>::alignment);
            }

            LifoArrayImpl(size_t i_element_count)
                : m_elements(static_cast<TYPE*>(  detail::ThreadLifoAllocator<>::allocate(compute_mem_size(i_element_count)) )),
                  m_size(i_element_count)
            {
            }

            ~LifoArrayImpl()
            {
                detail::ThreadLifoAllocator<>::deallocate(m_elements, compute_mem_size(m_size));
            }

        protected:
            TYPE * const m_elements;
            size_t const m_size;
        };

        template <typename TYPE>
            class LifoArrayImpl<TYPE, true >
        {
        public:

            static constexpr size_t size_overhead = alignof(TYPE) - detail::ThreadLifoAllocator<>::alignment;

            static size_t compute_mem_size(size_t i_element_count) noexcept
            {
                auto const mem_size = i_element_count * sizeof(TYPE) + size_overhead;
                bool already_aligned = sizeof(TYPE) % detail::ThreadLifoAllocator<>::alignment == 0;
                if (already_aligned)
                    return mem_size;
                else
                    return uint_upper_align(mem_size, detail::ThreadLifoAllocator<>::alignment);
            }

            LifoArrayImpl(size_t i_size)
                : m_size(i_size)
            {
                auto const mem_size = compute_mem_size(m_size);
                m_block = detail::ThreadLifoAllocator<>::allocate(mem_size);
                m_elements = static_cast<TYPE*>(address_upper_align(m_block, alignof(TYPE)));
                DENSITY_ASSERT_INTERNAL(address_diff(m_elements, m_block) <= size_overhead &&
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
            size_t const m_size;
        };
    }

    /** Array container that allocates its elements from the thread-local data stack, and owns it. The block
        is deallocated by the destructor. This class should be used only on the automatic storage.

        @tparam TYPE Element type.
        
        The data stack is a pool in which a thread can allocate and deallocate memory in LIFO order. It is handled by an 
        internal lifo allocator, which in turn in built upon an \ref instance of data_stack_underlying_allocator.
        \ref lifo_array and \ref lifo_buffer allocate on the data stack, so they must respect the LIFO order: only
        the most recently allocated block can be deallocated or reallocated.
        Instantiating \ref lifo_array and \ref lifo_buffer on the automatic storage (locally in a function) is always safe,
        as the language guarantees destruction in reverse order, even inside arrays or aggregate types.

        If elements are POD types, they are not initialized by the default constructor:

        \snippet lifo_examples.cpp lifo_array example 2

        The size of a \ref lifo_array is fixed at construction time, with no resize function provided.
        Elements are constructed in positional order and destroyed in reverse order.

        \snippet lifo_examples.cpp lifo_array example 1 */
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

        /** Constructs a lifo_array and all its elements. If elements are POD types, they are not initialized.
            @param i_size number of elements of the array */
        lifo_array(size_t i_size)
                : detail::LifoArrayImpl<TYPE>(i_size)
        {
            default_construct(std::is_pod<TYPE>());
        }

        /** Constructs all the elements with the provided parameter pack.
            @param i_size number of elements of the array
            @param i_construction_params construction parameters to use for every element of the array
            
            \snippet lifo_examples.cpp lifo_array constructor 3 */
        template <typename... CONSTRUCTION_PARAMS>
            lifo_array(size_t i_size, const CONSTRUCTION_PARAMS &... i_construction_params )
                : detail::LifoArrayImpl<TYPE>(i_size)
        {
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
                throw;
            }
        }

        /** Initializes the array from an input range. The size of the array is computed with <code>std::distance(i_begin, i_end)</code>.
            @param i_begin iterator pointing to the first source value
            @param i_end iterator pointing to the first value that is not copied in the array. 
            
            \snippet lifo_examples.cpp lifo_array constructor 2 */
        template <typename INPUT_ITERATOR>
            lifo_array(INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end,
                    typename std::iterator_traits<INPUT_ITERATOR>::iterator_category = typename std::iterator_traits<INPUT_ITERATOR>::iterator_category())
                : detail::LifoArrayImpl<TYPE>(std::distance(i_begin, i_end))
        {
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
                throw;
            }
        }

        /** Destroys a lifo_array and all its elements. Elements are destroyed in reverse positional order. */
        ~lifo_array()
        {
            destroy_elements(std::is_trivially_destructible<TYPE>());
        }

        /** Returns the number of elements */
        size_t size() const noexcept
        {
            return m_size;
        }

        /** Returns a reference to the i-th element.
        
            \pre The behavior is undefined if either:
                - i_index >= \ref size
        */
        TYPE & operator [] (size_t i_index) noexcept
        {
            DENSITY_ASSERT(i_index < m_size);
            return m_elements[i_index];
        }

        /** Returns a const reference to the i-th element.
        
            \pre The behavior is undefined if either:
                - i_index >= \ref size
        */
        const TYPE & operator [] (size_t i_index) const noexcept
        {
            DENSITY_ASSERT(i_index < m_size);
            return m_elements[i_index];
        }

        /** Returns a pointer to the first element. */
        pointer data() noexcept                     { return m_elements; }

        /** Returns a pointer to the first element. */
        const_pointer data() const noexcept         { return m_elements; }

        iterator begin() noexcept                   { return m_elements; }

        iterator end() noexcept                     { return m_elements + m_size; }

        const_iterator cbegin() const noexcept      { return m_elements; }

        const_iterator cend() const noexcept        { return m_elements + m_size; }

        const_iterator begin() const noexcept       { return m_elements; }

        const_iterator end() const noexcept         { return m_elements + m_size; }

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
                throw;
            }
        }

        void destroy_elements(std::true_type) noexcept
        {

        }

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

        Visual Stdio 2017 translates the source code of first test:

        ~~~~~~~~~~~~~~
        lifo_array<char> chars(i_cardinality);
        volatile char c = 0;
        chars[0] = c;
        ~~~~~~~~~~~~~~

        to this machine code:

        ~~~~~~~~~~~~~~
                    lifo_array<char> chars(i_cardinality);
         mov         rax,qword ptr gs:[58h]  
                    lifo_array<char> chars(i_cardinality);
         lea         rdi,[rdx+7]  
         and         rdi,0FFFFFFFFFFFFFFF8h  
         mov         ebx,128h  
         add         rbx,qword ptr [rax]  
         mov         rdx,qword ptr [rbx]  
         mov         rcx,rdx  
         and         rcx,0FFFFFFFFFFFF0000h  
         lea         r8,[rdi+rdx]  
         mov         rax,r8  
         sub         rax,rcx  
         cmp         rax,0FFF0h  
         jb          <lambda_b8a4ec06313d93817536a7cf8b449dcc>::operator()+57h (07FF72B438037h)  
         mov         rdx,rdi  
         mov         rcx,rbx  
         call        density::lifo_allocator<density::basic_void_allocator<65536> >::allocate_slow_path (07FF72B4385B0h)  
         mov         rdx,rax  
         jmp         <lambda_b8a4ec06313d93817536a7cf8b449dcc>::operator()+5Ah (07FF72B43803Ah)  
         mov         qword ptr [rbx],r8  
                    volatile char c = 0;
         mov         byte ptr [rsp+30h],0  
                    chars[0] = c;
         movzx       eax,byte ptr [c]  
         mov         byte ptr [rdx],al  
                }
         mov         rax,qword ptr [rbx]  
         xor         rax,rdx  
         test        rax,0FFFFFFFFFFFF0000h  
         je          <lambda_b8a4ec06313d93817536a7cf8b449dcc>::operator()+89h (07FF72B438069h)  
         mov         r8,rdi  
         mov         rcx,rbx  
         mov         rbx,qword ptr [rsp+38h]  
         add         rsp,20h  
         pop         rdi  
         jmp         density::lifo_allocator<density::basic_void_allocator<65536> >::deallocate_slow_path (07FF72B438520h)  
         mov         qword ptr [rbx],rdx  
         mov         rbx,qword ptr [rsp+38h]  
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
                    lifo_array<double> chars(i_cardinality);
         mov         rax,qword ptr gs:[58h]  
                    lifo_array<double> chars(i_cardinality);
         lea         rdi,[rdx*8]  
         mov         ebx,128h  
         add         rbx,qword ptr [rax]  
         mov         rdx,qword ptr [rbx]  
         mov         rcx,rdx  
         and         rcx,0FFFFFFFFFFFF0000h  
         lea         r8,[rdx+rdi]  
         mov         rax,r8  
         sub         rax,rcx  
         cmp         rax,0FFF0h  
         jb          <lambda_c7840347498da9fa856125956fe6e478>::operator()+57h (07FF74CD28457h)  
         mov         rdx,rdi  
         mov         rcx,rbx  
         call        density::lifo_allocator<density::basic_void_allocator<65536> >::allocate_slow_path (07FF74CD285B0h)  
         mov         rdx,rax  
         jmp         <lambda_c7840347498da9fa856125956fe6e478>::operator()+5Ah (07FF74CD2845Ah)  
         mov         qword ptr [rbx],r8  
         xorps       xmm0,xmm0  
                    volatile double c = 0;
         movsd       mmword ptr [rsp+30h],xmm0  
                    chars[0] = c;
         movsd       xmm1,mmword ptr [c]  
                }
         mov         rax,qword ptr [rbx]  
         xor         rax,rdx  
                    chars[0] = c;
         movsd       mmword ptr [rdx],xmm1  
                }
         test        rax,0FFFFFFFFFFFF0000h  
         je          <lambda_c7840347498da9fa856125956fe6e478>::operator()+90h (07FF74CD28490h)  
         mov         r8,rdi  
         mov         rcx,rbx  
         mov         rbx,qword ptr [rsp+38h]  
         add         rsp,20h  
                }
         pop         rdi  
         jmp         density::lifo_allocator<density::basic_void_allocator<65536> >::deallocate_slow_path (07FF74CD28520h)  
         mov         qword ptr [rbx],rdx  
         mov         rbx,qword ptr [rsp+38h]  

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
