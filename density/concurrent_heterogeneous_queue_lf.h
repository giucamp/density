
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

namespace density
{
    namespace detail
    {
        /** Computes the base2 logarithm of a size_t. If the argument is zero or is
            not a power of 2, the behavior is undefined. */
        constexpr size_t size_log2(size_t i_size) noexcept
        {
            return i_size <= 1 ? 0 : size_log2(i_size / 2) + 1;
        }

        /* Before each element there is a ControlBlock object. Since in the data member m_control_word the 2 least
            significant bit are used as flags, the address of a ControlBlock must be mutiple of 4.
            The alignas specifiers imply that this class is aligned at least at 4 bytes, but it may have
            a stricter (larger) alignment, probably 8 on 64-bit platforms (see http://en.cppreference.com/w/cpp/language/alignas). */
        template < typename COMMON_TYPE, typename RUNTIME_TYPE, size_t PAGE_SIZE>
            struct alignas(4) alignas(sync::atomic<uintptr_t>) alignas(RUNTIME_TYPE) ControlBlock
        {
            sync::atomic<uintptr_t> m_control_word; /** pointer to the next control block, plus two additional flags encoded in the least-significant bits.
                                                bit 0: exclusive access flag. The thread that succeeds in setting this flag has exclusive
                                                access on the content of the element. Other threads can always skip it.
                                                bit 1: dead element flag. The content of the element is not valid: it has been consumed,
                                                or the constructor threw an exception. Elements with this bit set don't require a the
                                                destructor to be called.
                                                The address of the next control block is given by:
                                                m_control_word.load() & (std::numeric_limits<uintptr_t>::max() - 3). */
            RUNTIME_TYPE m_type; /** Type of the element. It usually has the same size of a pointer. */

            static constexpr uintptr_t half_size_bits = size_log2(PAGE_SIZE);
            static constexpr uintptr_t half_size_mask = (static_cast<uintptr_t>(1) << half_size_bits) - 1;

            static_assert(std::numeric_limits<size_t>::radix == 2, "size_t must be binary");
            static_assert(std::numeric_limits<uintptr_t>::digits >= half_size_bits * 2, "The size of a page can't exceed 1 << ((bits in size_t) / 2)");

            DENSITY_STRONG_INLINE void lock_and_set_next_and_release(void * i_next) noexcept
            {
                DENSITY_ASSERT_INTERNAL(i_next >= this + 1 && (reinterpret_cast<uintptr_t>(i_next) & 3) == 0);
                auto const relative_address = reinterpret_cast<uintptr_t>(i_next) - reinterpret_cast<uintptr_t>(this);
                DENSITY_ASSERT_INTERNAL(relative_address <= half_size_mask);
                m_control_word.store(relative_address + 1, sync::hint_memory_order_release);
            }

            DENSITY_STRONG_INLINE void set_element_and_unlock_release(COMMON_TYPE * i_element) noexcept
            {
                #if DENSITY_DEBUG_INTERNAL
                    auto const dbg_prev_next = m_control_word.load(sync::hint_memory_order_relaxed);
                    DENSITY_ASSERT_INTERNAL((dbg_prev_next & 3) == 1 && (dbg_prev_next & ~half_size_mask) == 0);
                #endif

                DENSITY_ASSERT_INTERNAL( static_cast<void*>(i_element) >= this + 1);
                auto const relative_address = reinterpret_cast<uintptr_t>(i_element) - reinterpret_cast<uintptr_t>(this);
                DENSITY_ASSERT_INTERNAL(relative_address <= half_size_mask);
                m_control_word.fetch_add((relative_address << half_size_bits) - 1, sync::hint_memory_order_release);
            }

            DENSITY_STRONG_INLINE void set_dead_and_unlock_release() noexcept
            {
                DENSITY_ASSERT_INTERNAL((m_control_word.load(sync::hint_memory_order_relaxed) & 3) == 1);
                m_control_word.fetch_add(1, sync::hint_memory_order_release);
            }

            DENSITY_STRONG_INLINE uintptr_t get_next_from_control_word(uintptr_t i_control_word) noexcept
            {
                auto const low_part = i_control_word & (half_size_mask - 3);
                if (low_part == 0)
                {
                    // this is a link to another page
                    return i_control_word & (std::numeric_limits<uintptr_t>::max() - 3);
                }
                else
                {
                    return reinterpret_cast<uintptr_t>(this) + (low_part);
                }
            }

            struct ConsumeData
            {
                ControlBlock * const m_control;
                COMMON_TYPE * m_element;

                ConsumeData() noexcept
                    : m_control(nullptr)
                {

                }

                ConsumeData(ControlBlock * i_control_block) noexcept
                    : m_control(i_control_block), m_element(reinterpret_cast<COMMON_TYPE*>(
                        reinterpret_cast<uintptr_t>(i_control_block) + (i_control_block->m_control_word >> half_size_bits)))
                {
                    DENSITY_ASSERT_INTERNAL(address_is_aligned(m_element, i_control_block->m_type.alignment()));
                }

                bool is_valid() const noexcept { return m_control != nullptr; }

                DENSITY_STRONG_INLINE COMMON_TYPE * element_ptr() const noexcept
                {
                    return m_element;
                }

                DENSITY_STRONG_INLINE void * element_unaligned_ptr() const noexcept { return m_element; }

                DENSITY_STRONG_INLINE const RUNTIME_TYPE * type_ptr() const noexcept
                {
                    return &m_control->m_type;
                }
            };
        };

        template <typename RUNTIME_TYPE, size_t PAGE_SIZE>
            struct alignas(4) alignas(sync::atomic<uintptr_t>) alignas(RUNTIME_TYPE) ControlBlock<void, RUNTIME_TYPE, PAGE_SIZE>
        {
            sync::atomic<uintptr_t> m_control_word; /** pointer to the next control block, plus two additional flags encoded in the least-significant bits.
                                                bit 0: exclusive access flag. The thread that succeeds in setting this flag has exclusive
                                                access on the content of the element. Other threads can always skip it.
                                                bit 1: dead element flag. The content of the element is not valid: it has been consumed,
                                                or the constructor threw an exception. Elements with this bit set don't require a the
                                                destructor to be called.
                                                The address of the next control block is given by:
                                                m_control_word.load() & (std::numeric_limits<uintptr_t>::max() - 3). */
            RUNTIME_TYPE m_type; /** Type of the element. It usually has the same size of a pointer. */

            DENSITY_STRONG_INLINE void lock_and_set_next_and_release(void * i_next) noexcept
            {
                DENSITY_ASSERT_INTERNAL(i_next >= this + 1 && (reinterpret_cast<uintptr_t>(i_next) & 3) == 0);
                m_control_word.store(reinterpret_cast<uintptr_t>(i_next) + 1, sync::hint_memory_order_release);
            }

            DENSITY_STRONG_INLINE void set_element_and_unlock_release(void *) noexcept
            {
                DENSITY_ASSERT_INTERNAL((m_control_word.load(sync::hint_memory_order_relaxed) & 3) == 1);
                m_control_word.fetch_sub(1, sync::hint_memory_order_release);
            }

            DENSITY_STRONG_INLINE void set_dead_and_unlock_release() noexcept
            {
                DENSITY_ASSERT_INTERNAL((m_control_word.load(sync::hint_memory_order_relaxed) & 3) == 1);
                m_control_word.fetch_add(1, sync::hint_memory_order_release);
            }

            DENSITY_STRONG_INLINE uintptr_t get_next_from_control_word(uintptr_t i_control_word) noexcept
            {
                return i_control_word & ~static_cast<uintptr_t>(3);
            }

            struct ConsumeData
            {
                ControlBlock * const m_control;

                ConsumeData() noexcept
                    : m_control(nullptr)
                {
                }

                ConsumeData(ControlBlock * i_control_block) noexcept
                    : m_control(i_control_block)
                {
                }

                bool is_valid() const noexcept { return m_control != nullptr; }

                DENSITY_STRONG_INLINE void * element_ptr() const
                {
                    return address_upper_align(m_control + 1, m_control->m_type.alignment());
                }

                DENSITY_STRONG_INLINE void * element_unaligned_ptr() const { return m_control + 1; }

                DENSITY_STRONG_INLINE const RUNTIME_TYPE * type_ptr() const
                {
                    return &m_control->m_type;
                }
            };
        };

    }

    namespace experimental
    {
        /**
            The queue always keep at least an allocated page. Therefore the constructor allocates a page. The reason is to allow
                producer to assume that the page in which the push is tried (the last one) doesn't get deallocated while the push
                is in progress.
                The consume algorithm uses hazard pointers to safely delete the pages. Pages are deleted immediately by consumers
                when no longer needed. Using the default (and recommended) allocator, deleted pages are added to a thread-local
                free-list. When the number of pages in this free-list exceeds a fixed number, a page is added in a global lock-free
                free-list. See void_allocator for details.
                See "Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects" of Maged M. Michael for details.
                There is no requirement on the type of the elements: they can be non-trivially movable, copyable and destructible.
                */
        template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator >
            class concurrent_heterogeneous_queue_lf : private ALLOCATOR_TYPE
        {
        public:

            static_assert(std::is_same<COMMON_TYPE, typename RUNTIME_TYPE::common_type>::value,
                "COMMON_TYPE and RUNTIME_TYPE::common_type must be the same type (did you declare a type heter_cont<A,runtime_type<B>>?)");

            static_assert(ALLOCATOR_TYPE::page_size > sizeof(void*) * 8 && ALLOCATOR_TYPE::page_alignment == ALLOCATOR_TYPE::page_size,
                "The size and alignment of the pages must be the same (and not too small)");

            using allocator_type = ALLOCATOR_TYPE;
            using runtime_type = RUNTIME_TYPE;
            using common_type = COMMON_TYPE;
            using reference = typename std::add_lvalue_reference< COMMON_TYPE >::type;
            using const_reference = typename std::add_lvalue_reference< const COMMON_TYPE>::type;

            concurrent_heterogeneous_queue_lf()
            {
                auto const first_page = allocator_type::allocate_page();
                m_tail_for_producers.store(first_page);
                m_tail_for_consumers.store(first_page);
                m_head_for_consumers.store(reinterpret_cast<uintptr_t>(first_page));
                m_head_for_obliterate.store(reinterpret_cast<uintptr_t>(first_page));
            }

            /** copy not supported */
            concurrent_heterogeneous_queue_lf(const concurrent_heterogeneous_queue_lf &) = delete;

            /** copy not supported */
            concurrent_heterogeneous_queue_lf & operator = (const concurrent_heterogeneous_queue_lf &) = delete;

            /** Move constructor. Not thread safe. The source of a move operation can't be used for push and consumes. */
            concurrent_heterogeneous_queue_lf(concurrent_heterogeneous_queue_lf && i_source) noexcept
                : m_tail_for_producers(i_source.m_tail_for_producers.load()),
                  m_tail_for_consumers(i_source.m_tail_for_consumers.load()),
                  m_head_for_consumers(i_source.m_head_for_consumers.load()),
                  m_head_for_obliterate(i_source.m_head_for_obliterate.load())
            {
                i_source.m_tail_for_producers.load(nullptr);
                i_source.m_tail_for_consumers.load(nullptr);
                i_source.m_head_for_consumers.store(0);
                i_source.m_head_for_obliterate.store(0);
            }

            /** Move assignment. Not thread safe. The source of a move operation can't be used for push and consumes. */
            concurrent_heterogeneous_queue_lf & operator = (concurrent_heterogeneous_queue_lf && i_source) noexcept
            {
                if (this != &i_source)
                {
                    destroy();
                    m_tail_for_producers.store(i_source.m_tail_for_producers.load());
                    m_tail_for_consumers.store(i_source.m_tail_for_consumers.load());
                    m_head_for_consumers= i_source.store(m_head_for_consumers.load());
                    m_head_for_obliterate = i_source.store(m_head_for_obliterate.load());
                    i_source.m_tail_for_producers.load(nullptr);
                    i_source.m_tail_for_consumers.load(nullptr);
                    i_source.m_head_for_consumers.store(0);
                    i_source.m_head_for_obliterate.store(0);
                }
                return *this;
            }

            /** Adds an element at the end of the queue. The operation may require the allocation of a new page.
                This operation is thread safe. The construction of an element can run in parallel with the construction of other
                elements and with the consumption of elements. Threads synchronizes only at the beginning of the push (before the
                constructor is invoked).
                @param i_source object to be used as source to construct of new element.
                    - If this argument is an l-value, the new element copy-constructed (and the source object is left unchanged).
                    - If this argument is an r-value, the new element move-constructed (and the source object will have an undefined but valid content).

                \n\b Requires:
                    - the type ELEMENT_TYPE must copy constructible (in case of l-value) or move constructible (in case of r-value)
                    - an ELEMENT_TYPE * must be implicitly convertible to COMMON_TYPE *
                    - an COMMON_TYPE * must be convertible to an ELEMENT_TYPE * with a static_cast or a dynamic_cast
                        (this requirement is not met for example if COMMON_TYPE is a non-polymorphic (direct or indirect) virtual
                        base of ELEMENT_TYPE).

                \n\b Throws: anything that the constructor of the new element throws
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
                \n\b Complexity: constant */
            template <typename ELEMENT_TYPE>
                DENSITY_STRONG_INLINE void push(ELEMENT_TYPE && i_source)
            {
                using ElementCompleteType = typename std::decay<ELEMENT_TYPE>::type;
                emplace<ElementCompleteType>(std::forward<ELEMENT_TYPE>(i_source));
            }

            void push_by_copy(runtime_type i_runtime_type, const void * i_source)
            {
                static_assert(element_fits_in_a_page(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE)),
                    "currently ELEMENT_TYPE must fit in a page");

                auto const alignment = detail::size_max(alignof(ControlBlock), i_runtime_type.alignment());
                auto const size = uint_upper_align(size, alignof(ControlBlock));
                auto const push_data = begin_push<true>(size, alignment);

                // construct the type
                static_assert(new(push_data.type_ptr()) runtime_type(std::move(i_runtime_type)));
                new(push_data.type_ptr()) runtime_type(std::move(i_runtime_type));

                COMMON_TYPE * element;
                try
                {
                    // construct the element
                    element = i_runtime_type.copy_contruct(push_data.element_ptr(), i_source);
                }
                catch (...)
                {
                    // destroy the type (which is probably a no-operation)
                    push_data.type_ptr().RUNTIME_TYPE::~RUNTIME_TYPE();

                    // this call release the exclusive access and set the dead flag
                    cancel_push(push_data.m_control);

                    // the exception is propagated to the caller, whatever it is
                    throw;
                }

                commit_push(push_data, element);
            }

            void push_by_move(runtime_type i_runtime_type, void * i_source)
            {
                static_assert(element_fits_in_a_page(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE)),
                    "currently ELEMENT_TYPE must fit in a page");

                auto const alignment = detail::size_max(alignof(ControlBlock), i_runtime_type.alignment());
                auto const size = uint_upper_align(size, alignof(ControlBlock));
                auto const push_data = begin_push<true>(size, alignment);

                // construct the type
                static_assert(new(push_data.type_ptr()) runtime_type(std::move(i_runtime_type)));
                new(push_data.type_ptr()) runtime_type(std::move(i_runtime_type));

                COMMON_TYPE * element;
                try
                {
                    // construct the element
                    element = i_runtime_type.move_contruct(push_data.element_ptr(), i_source);
                }
                catch (...)
                {
                    // destroy the type (which is probably a no-operation)
                    push_data.type_ptr().RUNTIME_TYPE::~RUNTIME_TYPE();

                    // this call release the exclusive access and set the dead flag
                    cancel_push(push_data.m_control);

                    // the exception is propagated to the caller, whatever it is
                    throw;
                }

                commit_push(push_data, element);
            }


            template <typename COMPLETE_ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
                void emplace(CONSTRUCTION_PARAMS &&... i_args)
            {
                static_assert(std::is_convertible< COMPLETE_ELEMENT_TYPE*, COMMON_TYPE*>::value,
                    "ELEMENT_TYPE must be covariant to (i.e. must derive from) COMMON_TYPE, or COMMON_TYPE must be void");

                static_assert(element_fits_in_a_page(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE)),
                    "currently ELEMENT_TYPE must fit in a page");

                auto const alignment = detail::size_max(alignof(ControlBlock), alignof(COMPLETE_ELEMENT_TYPE));
                auto const size = uint_upper_align(sizeof(COMPLETE_ELEMENT_TYPE), alignof(ControlBlock));
                auto push_data = begin_push<true>(size, alignment);

                // construct the type - This expression can throw
                static_assert(noexcept(RUNTIME_TYPE(RUNTIME_TYPE::template make<COMPLETE_ELEMENT_TYPE>())),
                    "both runtime_type::make and the move constructor of runtime_type must be noexcept");
                new(push_data.type_ptr()) RUNTIME_TYPE(RUNTIME_TYPE::template make<COMPLETE_ELEMENT_TYPE>());

                COMMON_TYPE * element;
                try
                {
                    // construct the element
                    element = new(push_data.m_element) COMPLETE_ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_args)...);
                }
                catch (...)
                {
                    // destroy the type (which is probably a no-operation)
                    push_data.type_ptr()->RUNTIME_TYPE::~RUNTIME_TYPE();

                    // this call release the exclusive access and set the dead flag
                    cancel_push(push_data.m_control);

                    // the exception is propagated to the caller, whatever it is
                    throw;
                }

                commit_push(push_data, element);
            }

            template <typename CONSUMER_FUNC>
                bool try_consume(CONSUMER_FUNC && i_consumer_func)
            {
                auto consume_data = begin_consume();
                if (consume_data.m_control != nullptr)
                {
                    auto scope_exit = detail::at_scope_exit([this, consume_data] () noexcept {
                        consume_data.type_ptr()->destroy(consume_data.element_ptr());
                        consume_data.type_ptr()->RUNTIME_TYPE::~RUNTIME_TYPE();
                        commit_consume(consume_data);
                    });
                    i_consumer_func(*consume_data.type_ptr(), consume_data.element_ptr());
                    return true;
                }
                else
                {
                    return false;
                }
            }

            template <typename CONSUMER_FUNC>
                bool try_consume_manual_align_destroy(CONSUMER_FUNC && i_consumer_func)
            {
                auto consume_data = begin_consume();
                if (consume_data.m_control != nullptr)
                {
                    auto scope_exit = detail::at_scope_exit([this, consume_data]() noexcept {
                        consume_data.type_ptr()->RUNTIME_TYPE::~RUNTIME_TYPE();
                        commit_consume(consume_data);
                    });
                    i_consumer_func(*consume_data.type_ptr(), consume_data.unaligned_element());
                    return true;
                }
                else
                {
                    return false;
                }
            }

            /** Returns a copy of the allocator instance owned by the queue.
                \n\b Throws: anything that the copy-constructor of the allocator throws
                \n\b Complexity: constant */
            allocator_type get_allocator() const
            {
                return *static_cast<alocator_type*>(this);
            }

            /** Returns a reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            allocator_type & get_allocator_ref() noexcept
            {
                return *static_cast<alocator_type*>(this);
            }

            /** Returns a const reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            const allocator_type & get_allocator_ref() const noexcept
            {
                return *static_cast<alocator_type*>(this);
            }

        private:

            using ControlBlock = detail::ControlBlock<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE::page_size>;

            static constexpr bool element_fits_in_a_page(size_t i_size, size_t i_alignment)
            {
                return i_size + i_alignment < (ALLOCATOR_TYPE::page_size - sizeof(ControlBlock) * 2);
            }

            static bool are_same_page(void * i_first, void * i_second)
            {
                return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) &
                    ~static_cast<uintptr_t>(ALLOCATOR_TYPE::page_alignment - 1)) == 0;
            }

            /** A CompareAndSwap (CAS) function based on compare_exchange_weak.
                The difference between compare_and_set_weak and compare_exchange_weak is that compare_and_set_weak takes the expected
                value by value. Therefore, in case of failure, the caller can't get the previous value of the atomic.
                The interface compare_exchange_weak is fine in many cases, but it is not very comfortable if we are not interested in
                the previous value of the atomic. */
            template <typename TYPE>
                DENSITY_STRONG_INLINE static bool compare_and_set_weak(sync::atomic<TYPE> & io_atomic, TYPE i_expected, TYPE i_set_to,
                    sync::memory_order i_success_memory_order) noexcept
            {
                return io_atomic.compare_exchange_weak(i_expected, i_set_to, i_success_memory_order, sync::hint_memory_order_relaxed);
            }

            struct PushData
            {
                ControlBlock * m_control;
                COMMON_TYPE * m_element;

                DENSITY_STRONG_INLINE COMMON_TYPE * element_ptr() const { return m_element; }

                DENSITY_STRONG_INLINE RUNTIME_TYPE * type_ptr() const
                {
                    return &m_control->m_type;
                }
            };

            /** Allocates space for a RUNTIME_TYPE and for an element, returning a pair of pointers to them.
                The caller should construct the type and the element, and then it should call commit_push().
                If an exception is thrown during the construction, cancel_push must be called.
                If an exception is thrown by begin_push, the call has no effect.
                @param i_size size of the element in bytes. It must be a multiple of i_alignment. It may be zero.
                @param i_alignment alignment of the element. It must be >= alignof(ControlBlock), an integer power of 2
            */
            template <bool CAN_WAIT>
                PushData begin_push(size_t i_size, size_t i_alignment)
            {
                DENSITY_ASSERT(i_alignment >= alignof(ControlBlock) && is_power_of_2(i_alignment));
                DENSITY_ASSERT(uint_is_aligned(i_size, i_alignment));

                ControlBlock * control;
                void * new_element;
                void * tail;

                /* We start reading m_tail_for_producers, that is the pointer consumer threads use to make their
                   linear allocation in the page. Until we update m_tail_for_consumers, we do not need
                   any acquire/release ordering.
                   Then we compute tail, that is the next value we want to set in m_tail_for_producers, and
                   we hope that when we try to set it, m_tail_for_producers is still equal to original_tail.
                   If m_tail_for_producers is changed in the meanwhile, we retry from scratch. Note that
                   compare_exchange_weak will reload the value of m_tail_for_producers in original_tail,
                   so we load explicitly it only once. */
                auto original_tail = m_tail_for_producers.load(sync::hint_memory_order_relaxed);
                for (;;)
                {
                    // Linearly-allocate the control block and the element, updating tail
                    tail = original_tail;
                    control = static_cast<ControlBlock*>(linear_alloc(tail, sizeof(ControlBlock)));
                    new_element = linear_alloc(tail, i_size, i_alignment > alignof(ControlBlock) ? i_alignment : alignof(ControlBlock));

                    /* Check for end of page. We need to make sure that not only the ControlBlock and the element fit in the page,
                      but also an extra ControlBlock, that eventually we use as link to the next page. */
                    auto const end_of_page = (reinterpret_cast<uintptr_t>(original_tail) | static_cast<uintptr_t>(ALLOCATOR_TYPE::page_alignment - 1)) + 1;
                    auto const limit = reinterpret_cast<void*>(end_of_page - sizeof(ControlBlock));
                    if (tail > limit)
                    {
                        /* There is no place to allocate another ControlBlock after the new element.
                           We must setup this ControlBock as link for a new page. */
                        original_tail = handle_end_of_page(original_tail); // if this throws, no change is done in the queue
                        continue;
                    }

                    /* Try to update m_tail_for_producers.
                       On failure original_tail is set with the actual value of m_tail_for_producers. */
                    if (m_tail_for_producers.compare_exchange_weak(original_tail, tail, sync::hint_memory_order_relaxed, sync::hint_memory_order_relaxed))
                    {
                        break;
                    }

                    // if you CAN_WAIT is false, we can't retry
                    bool can_wait = CAN_WAIT;
                    if (!can_wait)
                    {
                        return PushData{ nullptr };
                    }
                }

                /* Now we can initialize control->m_control_word, and set the exclusive-access flag in it (the +1).
                   Other producers can allocate space in the meanwhile (moving m_tail_for_producers forward).
                  Consumers are not allowed no read after m_tail_for_consumers, which we still did not update,
                  therefore the current page can't be deallocated. */
                control->lock_and_set_next_and_release(tail);

                /* Now the 'slot' we have allocated is ready: it can be skipped (control->m_control_word) is valid,
                   but we have exclusive access on it (the first bit of control->m_control_word is set).
                   If other consumers have allocated space (i.e. modified m_tail_for_producers), now we are going to synchronize
                   with them: consumers will exit from the next loop in the exact order they succeeded in updating m_tail_for_producers.
                   Note: we have not yet constructed neither the type object, nor the element. The caller will do after thsi function returns.
                   The next CAS operation on success needs at least the release semantic (consumers must see what
                   we have written in control->m_control_word). */
                while (!compare_and_set_weak(m_tail_for_consumers, original_tail, tail, sync::hint_memory_order_relaxed))
                {
                    // this can happen only if slower consumer thread allocate space in m_tail_for_producers before a faster consumer thread
                    // sync::this_thread::yield();
                }

                /* Done. Now the caller can construct the type and the element concurrently with consumers and other producers.
                   The current page won't be deallocated until cancel_push or commit_push is called, because we have set the exclusive access
                   flag in control->m_control_word. */
                return PushData{ control, static_cast<COMMON_TYPE*>(new_element) };
            }

            /** Tries to allocate a new page. This operation may fail because many producer threads can try it concurrently, so they
                have to synchronize to avoid multiple allocations.
                Returns m_tail_for_producers.load(sync::hint_memory_order_acquire) */
            DENSITY_NO_INLINE void * handle_end_of_page(void * const i_original_tail)
            {
                /* The first thread that succeed in setting m_tail_for_producers to last_byte is the one that allocates a new page.
                   It's very important to set m_tail_for_producers to the last byte of the page and not to the end of the page, because
                   the latter is in another pages, and incoming producers go beyond the page.*/
                auto const last_byte = reinterpret_cast<uintptr_t>(i_original_tail) | static_cast<uintptr_t>(ALLOCATOR_TYPE::page_alignment - 1);
                if (i_original_tail != reinterpret_cast<void*>(last_byte) &&
                    compare_and_set_weak(m_tail_for_producers, i_original_tail, reinterpret_cast<void*>(last_byte), sync::hint_memory_order_relaxed))
                {
                    void * new_page;
                    try
                    {
                        // allocate the page - this may throw
                        new_page = ALLOCATOR_TYPE::allocate_page();
                        DENSITY_ASSERT(address_is_aligned(new_page, ALLOCATOR_TYPE::page_alignment));
                    }
                    catch (...)
                    {
                        DENSITY_ASSERT(m_tail_for_producers.load() == i_original_tail);
                        m_tail_for_producers.store(i_original_tail);
                        throw;
                    }
                    // from now on nothing can throw

                    // setup a ControlBox with the dead flag
                    auto const control = static_cast<ControlBlock*>(i_original_tail);
                    new (&control->m_type) RUNTIME_TYPE();
                    control->m_control_word.store(reinterpret_cast<uintptr_t>(new_page) + 2);

                    // now we can move the tail to the next page
                    m_tail_for_producers.store(new_page);
                    while (!compare_and_set_weak(m_tail_for_consumers, i_original_tail, new_page, sync::hint_memory_order_relaxed))
                    {
                        // sync::this_thread::yield();
                    }

                    return new_page;
                }
                else
                {
                    // sync::this_thread::yield();
                    return m_tail_for_producers.load(sync::hint_memory_order_relaxed);
                }
            }

            /** Used when a begin_push has been called, but an exception was thrown during the construction of the element
                (or the type). This function marks the element as dead, and performs a release operation on m_tail_for_consumers. */
            void cancel_push(ControlBlock * i_control_block) noexcept
            {
                /* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the
                   same time the exclusive access is removed (the first bit) */
                i_control_block->set_dead_and_unlock_release();
            }

            /** Used when a begin_push has been called, and both the type and the elements has been constructed. This
                function performs a release operation on m_tail_for_consumers. */
            DENSITY_STRONG_INLINE void commit_push(PushData i_push_data, COMMON_TYPE * i_element) noexcept
            {
                /* decrementing the size we allow the consumers to process this element (this is an atomic operation)
                   To do: this is a read-write operation. Make it a write-only op. */
                i_push_data.m_control->set_element_and_unlock_release(i_element);
            }

            typename ControlBlock::ConsumeData begin_consume() noexcept
            {
                // get exclusive access on m_head_for_consumers
                uintptr_t head_for_consumers;
                do {
                    head_for_consumers = m_head_for_consumers.fetch_or(1, sync::hint_memory_order_relaxed);
                } while (head_for_consumers & 1);

                auto const tail = reinterpret_cast<uintptr_t>(m_tail_for_consumers.load(sync::hint_memory_order_relaxed));

                for (;;)
                {
                    // access the control block to get the control_word
                    auto const control = reinterpret_cast<ControlBlock *>(head_for_consumers);
                    auto const control_word = control->m_control_word.load(sync::hint_memory_order_acquire);
                    auto const next = control->get_next_from_control_word(control_word);

                    // check of we are gone too far
                    if (head_for_consumers == tail)
                    {
                        // no element to pick
                        m_head_for_consumers.store(head_for_consumers, sync::hint_memory_order_relaxed);
                        return typename ControlBlock::ConsumeData();
                    }

                    // check if we reach an element under construction - in this case we exit
                    if ((control_word & 1) != 0)
                    {
                        // no element to pick
                        m_head_for_consumers.store(head_for_consumers, sync::hint_memory_order_relaxed);
                        return typename ControlBlock::ConsumeData();
                    }

                    if ((control_word & 3) == 0)
                    {
                        // living element, and no one has exclusive access on it
                        m_head_for_consumers.store(next, sync::hint_memory_order_relaxed);
                        return typename ControlBlock::ConsumeData(control);
                    }

                    head_for_consumers = next;
                }
            }

            void commit_consume(typename ControlBlock::ConsumeData i_consume_data) noexcept
            {
                #if DENSITY_DEBUG
                    memset(&(i_consume_data.m_control->m_type), 0xB4, sizeof(i_consume_data.m_control->m_type));
                #endif

                // mark the element as dead
                DENSITY_ASSERT_INTERNAL( (i_consume_data.m_control->m_control_word & 3) == 0 );
                i_consume_data.m_control->m_control_word.fetch_add(2, sync::hint_memory_order_release);

                uintptr_t head_for_obliterate;
                for (;;)
                {
                    // get exclusive access on m_head_for_obliterate
                    do {
                        head_for_obliterate = m_head_for_obliterate.fetch_or(1, sync::hint_memory_order_relaxed);
                    } while (head_for_obliterate & 1);

                    auto const head_for_consumers = m_head_for_consumers.load(sync::hint_memory_order_relaxed) & ~static_cast<uintptr_t>(1);
                    if (head_for_obliterate == head_for_consumers)
                    {
                        // no element to pick
                        break;
                    }

                    // access the control block to get the control_word
                    auto const control = reinterpret_cast<ControlBlock *>(head_for_obliterate);
                    auto const control_word = control->m_control_word.load(sync::hint_memory_order_acquire);
                    auto const next = control->get_next_from_control_word(control_word);

                    bool const living_element = (control_word & 2) == 0;
                    if (living_element)
                    {
                        break;
                    }

                    if (control->m_type.empty())
                    {
                        auto const page = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(control) & ~static_cast<uintptr_t>(ALLOCATOR_TYPE::page_alignment - 1));
                        DENSITY_ASSERT(address_is_aligned(page, ALLOCATOR_TYPE::page_alignment));
                        DENSITY_ASSERT(!are_same_page(page, reinterpret_cast<void*>(next)));
                        ALLOCATOR_TYPE::deallocate_page(page);
                    }

                    DENSITY_ASSERT_INTERNAL(next >= ALLOCATOR_TYPE::page_size);
                    m_head_for_obliterate.store(next);
                }

                DENSITY_ASSERT_INTERNAL(m_head_for_obliterate.load() == head_for_obliterate + 1);
                m_head_for_obliterate.fetch_sub(1);
            }

            void destroy() noexcept
            {
                auto head = m_head_for_obliterate.load(sync::hint_memory_order_release);
                auto const tail = m_tail_for_producers.load(sync::hint_memory_order_release);

                // this function is not thread safe
                DENSITY_ASSERT(head == m_head_for_consumers.load(sync::hint_memory_order_relaxed););
                DENSITY_ASSERT(tail == m_tail_for_consumers.load(sync::hint_memory_order_relaxed););

                // this operation is not thread safe, so check some invariance
                DENSITY_ASSERT( (head != 0) == (tail != nullptr) &&
                    (head != 0) == (m_tail_for_producers.load() != nullptr) );

                if (head != 0)
                {
                    while (head != tail)
                    {
                        auto const control = reinterpret_cast<ControlBlock*>(head);
                        auto const next = control->m_control_word.load(sync::hint_memory_order_relaxed);
                        DENSITY_ASSERT( (next & 1) == 0 ); // someone has exclusive access?
                        if ((next & 2) == 0)
                        {
                            // destroy the element and the type
                            auto element = address_upper_align(control + 1, control->m_type.alignment());
                            control->m_type.destroy(consume_data.element_ptr());
                            control->m_type.RUNTIME_TYPE::~RUNTIME_TYPE();
                        }
                        else
                        {
                            if (control->m_type == RUNTIME_TYPE())
                            {
                                // deallocate the page
                                auto const page = reinterpret_cast<void*>(head & ~static_cast<uintptr_t>(ALLOCATOR_TYPE::page_alignment - 1));
                                DENSITY_ASSERT(address_is_aligned(page, ALLOCATOR_TYPE::page_alignment));
                                DENSITY_ASSERT(!are_same_page(page, reinterpret_cast<void*>(dirt_next - 2)));
                                ALLOCATOR_TYPE::deallocate_page(page);
                            }
                        }
                        head = next;
                    }
                }
            }

        private:

            alignas(concurrent_alignment) sync::atomic<void*> m_tail_for_producers; /**< Pointer to the end of the last element allocated in the queue. */
            sync::atomic<void*> m_tail_for_consumers;

            alignas(concurrent_alignment) sync::atomic<uintptr_t> m_head_for_consumers;
            sync::atomic<uintptr_t> m_head_for_obliterate;
        };

    } // namespace experimental

} // namespace density

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
