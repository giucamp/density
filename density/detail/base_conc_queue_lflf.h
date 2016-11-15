
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#ifndef DENSITY_INCLUDING_CONC_QUEUE_DETAIL
    #error "THIS IS A PRIVATE HEADER OF DENSITY. DO NOT INCLUDE IT DIRECTLY"
#endif // ! DENSITY_INCLUDING_CONC_QUEUE_DETAIL

namespace density
{
    namespace detail
    {
        namespace conc_queue
        {
            /** A CompareAndSwap (CAS) function based on compare_exchange_weak.
                The difference between compare_and_set_weak and compare_exchange_weak is that compare_and_set_weak takes the expected
				value by value. Therefore, in case of failure, so the caller can't see the previous value of the atomic.
				The interface compare_exchange_weak is fine in many cases, but it is not very comfortable if we are not interested in 
				the previous value of the atomic. */
            template <typename TYPE>
                DENSITY_STRONG_INLINE bool compare_and_set_weak(sync::atomic<TYPE> & io_atomic, TYPE i_expected, TYPE i_set_to, 
					sync::memory_order i_success_memory_order) noexcept
            {
                return io_atomic.compare_exchange_weak(i_expected, i_set_to, i_success_memory_order, sync::hint_memory_order_relaxed);
            }

            /*inline size_t get_rand(size_t i_max)
            {
                static thread_local std::mt19937 rand{ std::random_device()() };
                return std::uniform_int_distribution<size_t>(0, i_max)(rand);
            }*/

            #define DENSITY_TEST_RANDOM_WAIT() //if(get_rand(7) == 3) { std::this_thread::sleep_for(std::chrono::nanoseconds(get_rand(65336) ) ); }

            #define DENSITY_STATS(expr)

            /*inline bool are_same_page(void * i_first, void * i_second)
            {
                return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) &
                    ~static_cast<uintptr_t>(VOID_ALLOCATOR::page_alignment - 1)) == 0;
            }*/

            inline void * allocate(void * * io_ptr, size_t i_size)
            {
                auto const res = *io_ptr;
                *reinterpret_cast<unsigned char**>(io_ptr) += i_size;
                return res;
            }

            inline void * allocate(void * * io_ptr, size_t i_size, size_t i_alignment)
            {
                *io_ptr = address_upper_align(*io_ptr, i_alignment);
                auto const res = *io_ptr;
                *reinterpret_cast<unsigned char**>(io_ptr) += i_size;
                return res;
            }

            /* Before each element there is a ControlBlock object. Since in the data member m_next the 2 least
                significant bit are used as flags, the address of a ControlBlock must be mutiple of 4.
                The alignas specifiers imply that this class is aligned at least at 4 bytes, but it may have
                a stricter (larger) alignment, probably 8 on 64-bit platforms (see http://en.cppreference.com/w/cpp/language/alignas). */
            template <typename RUNTIME_TYPE>
                struct alignas(4) alignas(sync::atomic<uintptr_t>) alignas(RUNTIME_TYPE) ControlBlock
            {
                sync::atomic<uintptr_t> m_next; /** pointer to the next control block, plus two additional flags encoded in the least-significant bits.
                                        bit 0: exclusive access flag. The thread that succeeds in setting this flag has exclusive
                                        access on the content of the element. Other threads can always skip it.
                                        bit 1: dead element flag. The content of the element is not valid: it has been consumed,
                                        or the constructor threw an exception. Elements with this bit set don't require a the
                                        destructor to be called.
                                        The address of the next control block is given by:
                                            m_next.load() & (std::numeric_limits<uintptr_t>::max() - 3). */
                RUNTIME_TYPE m_type; /** Type of the element. It usually has the same size of a pointer. */
            };

            /** Tail represent the tail of a concurrent heterogeneous queue. This is the forward declaration of the
                primary template, which is not defined. For every value of SynchronizationKind there is a partial specialization. */
            template < typename VOID_ALLOCATOR, typename RUNTIME_TYPE, SynchronizationKind SYNC_KIND>
                class Tail;

            /** Head represent the head of a concurrent heterogeneous queue. This is the forward declaration of the
                primary template, which is not defined. For every value of SynchronizationKind there is a partial specialization. */
            template < typename VOID_ALLOCATOR, typename RUNTIME_TYPE, SynchronizationKind SYNC_KIND>
                class Head;

            /** Partial specialization of Tail with SYNC_KIND = SynchronizationKind::LocklessMultiple. */
            template < typename VOID_ALLOCATOR, typename RUNTIME_TYPE>
                class alignas(concurrent_alignment) Tail<VOID_ALLOCATOR, RUNTIME_TYPE, SynchronizationKind::LocklessMultiple>
            {
            public:

                using ControlBlock = ControlBlock<RUNTIME_TYPE>;

                static constexpr bool element_fits_in_a_page(size_t i_size, size_t i_alignment)
                {
                    return i_size + i_alignment < (VOID_ALLOCATOR::page_size - sizeof(ControlBlock) * 2);
                }

                void initialize(VOID_ALLOCATOR * i_allocator, void * i_first_page)
                {
                    m_allocator = i_allocator;
                    m_tail_for_alloc.store(i_first_page);
                    m_tail_for_consumers.store(i_first_page);
                }

                const sync::atomic<void*> * get_tail_for_consumers() const { return &m_tail_for_consumers; }

                struct PushData
                {
                    ControlBlock * m_control;
                    void * m_element;

                    DENSITY_STRONG_INLINE void * element() const { return m_element; }

                    DENSITY_STRONG_INLINE const RUNTIME_TYPE * type() const
                    {
                        return &m_control->m_type();
                    }
                };

                /** Allocates space for a RUNTIME_TYPE and for an element, returning a pair of pointers to them.
                    The caller should construct the type and the element, and then it should call commit_push().
                    If an exception is thrown during the construction, cancel_push must be called.
                    If an exception is thrown by begin_push, the call has no effect. */
                template <bool CAN_WAIT>
                    PushData begin_push(size_t i_size, size_t i_alignment)
                {
                    ControlBlock * control;
                    void * new_element;
					void * tail;

					/* We start reading m_tail_for_alloc, that is the pointer consumer threads use to make their
						linear allocation in the page. Until we update m_tail_for_consumers, we do not need
						any acquire/release ordering.
						Then we compute tail, that is the next value we want to set in m_tail_for_alloc, and
						we hope that when we try to set it, m_tail_for_alloc is still equal to original_tail.
						If m_tail_for_alloc is changed in the meanwhile, we retry from scratch. Note that
						compare_exchange_weak will reload the value of m_tail_for_alloc in original_tail,
						so we load explicitly it only once. */
                    auto original_tail = m_tail_for_alloc.load(sync::hint_memory_order_relaxed);
					for(;;)
                    {
                        // Linearly-allocate the control block and the element, updating tail
                        tail = original_tail;
                        control = static_cast<ControlBlock*>(allocate(&tail, sizeof(ControlBlock)));
                        new_element = allocate(&tail, i_size, i_alignment > alignof(ControlBlock) ? i_alignment : alignof(ControlBlock));

                        /* Check for end of page. We need to make sure that not only the ControlBlock and the element fit in the page,
                            but also an extra ControlBlock, that eventually we use as link to the next page. */
                        auto const end_of_page = ( reinterpret_cast<uintptr_t>(original_tail) | static_cast<uintptr_t>(VOID_ALLOCATOR::page_alignment - 1) ) + 1;
                        auto const limit = reinterpret_cast<void*>(end_of_page - sizeof(ControlBlock) );
                        if (tail > limit)
                        {
                            /* There is no place to allocate another ControlBlock after the new element.
                                We must setup this ControlBock as link for a new page. */
                            original_tail = handle_end_of_page(original_tail); // if this throws, no change is done in the queue
                            continue;
                        }

                        /* Try to update m_tail_for_alloc.
							On failure original_tail is set with the actual value of m_tail_for_alloc. */
                        if (m_tail_for_alloc.compare_exchange_weak(original_tail, tail, sync::hint_memory_order_relaxed, sync::hint_memory_order_relaxed))
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

                    /* Now we can initialize control->m_next, and set the exclusive-access flag in it (the +1).
                       Other producers can allocate space in the meanwhile (moving m_tail_for_alloc forward).
                       Consumers are not allowed no read after m_tail_for_consumers, which we still did not update,
                       therefore the current page can't be deallocated. */
                    control->m_next.store(reinterpret_cast<uintptr_t>(tail) + 1, sync::hint_memory_order_relaxed);

                    /** Now the 'slot' we have allocated is ready: it can be skipped (control->m_next) is valid,
                        but we have exclusive access on it (the first bit of control->m_next is set).
                        If other consumers have allocated space (i.e. modified m_tail_for_alloc), now we are going to synchronize
                        with them: consumers will exit from the next loop in the exact order they succeeded in updating m_tail_for_alloc.
                        Note: we have not yet constructed neither the type object, nor the element. The caller will do after thsi function returns.
                        The next CAS operation on success needs at least the release semantic (consumers must see what
                        we have written in control->m_next). */
                    while (!compare_and_set_weak(m_tail_for_consumers, original_tail, tail, sync::hint_memory_order_release))
                    {
                        // this can happen only if slower consumer thread allocate space in m_tail_for_alloc before a faster consumer thread
                        std::this_thread::yield();
                    }

                    /* Done. Now the caller can construct the type and the element concurrently with consumers and other producers.
                       The current page won't be deallocated until cancel_push or commit_push is called, because we have set the exclusive access
                       flag in control->m_next. */
                    return PushData{control, new_element};
                }

                /** Tries to allocate a new page. This operation may fail because many producer threads can try it concurrently, so they
                    have to synchronize to avoid multiple allocations.
                    Returns m_tail_for_alloc.load(sync::hint_memory_order_acquire) */
                DENSITY_NO_INLINE void * handle_end_of_page(void * const i_original_tail)
                {
                    /* The first thread that succeed in setting m_tail_for_alloc to last_byte is the one that allocates a new page.
                        It's very important to set m_tail_for_alloc to the last byte of the page and not to the end of the page, because
                        the latter is in another pages, and incoming producers go beyond the page.*/
                    auto const last_byte = reinterpret_cast<uintptr_t>(i_original_tail) | static_cast<uintptr_t>(VOID_ALLOCATOR::page_alignment - 1);
                    if (i_original_tail != reinterpret_cast<void*>(last_byte) &&
                        compare_and_set_weak(m_tail_for_alloc, i_original_tail, reinterpret_cast<void*>(last_byte), sync::hint_memory_order_relaxed))
                    {
                        void * new_page;
                        try
                        {
                            // allocate the page - this may throw
                            new_page = m_allocator->allocate_page();
                            DENSITY_ASSERT(is_address_aligned(new_page, VOID_ALLOCATOR::page_alignment));
                        }
                        catch (...)
                        {
                            DENSITY_ASSERT(m_tail_for_alloc.load() == i_original_tail);
                            m_tail_for_alloc.store(i_original_tail);
                            throw;
                        }
                        // from now on nothing can throw

                        // setup a ControlBox with the dead flag
                        auto const control = static_cast<ControlBlock*>(i_original_tail);
                        new (&control->m_type) RUNTIME_TYPE();
                        control->m_next.store(reinterpret_cast<uintptr_t>(new_page) + 2);

                        // now we can move the tail to the next page
                        m_tail_for_alloc.store(new_page);
                        while (!compare_and_set_weak(m_tail_for_consumers, i_original_tail, new_page, sync::hint_memory_order_relaxed))
                        {
                            std::this_thread::yield();
                        }

                        return new_page;
                    }
                    else
                    {
                        std::this_thread::yield();
                        return m_tail_for_alloc.load(sync::hint_memory_order_relaxed);
                    }
                }

                void cancel_push(ControlBlock * i_control_block) noexcept
                {
                    /* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the
                       same time the exclusive access is removed (the first bit) */
                    DENSITY_ASSERT_INTERNAL((i_control_block->m_next.load(sync::hint_memory_order_relaxed) & 3) == 1);
                    i_control_block->m_next.fetch_xor(3, sync::hint_memory_order_relaxed);

                    /** Consumers should see what we have done (while producers are not interested). So we do a
                        release operation on m_tail_for_consumers. */
                    m_tail_for_consumers.fetch_add(0, sync::hint_memory_order_release);
                }

                void commit_push(PushData i_push_data) noexcept
                {
                    /* decrementing the size we allow the consumers to process this element (this is an atomic operation)
                        To do: this is a read-write operation. Make it a write-only op. */
                    DENSITY_TEST_RANDOM_WAIT();
                    DENSITY_ASSERT_INTERNAL((i_push_data.m_control->m_next.load() & 3) == 1);
                    i_push_data.m_control->m_next.fetch_sub(1, sync::hint_memory_order_relaxed);

                    /** Consumers should see what we have done (while producers are not interested). So we do a
                        release operation on m_tail_for_consumers. */
                    m_tail_for_consumers.fetch_add(0, sync::hint_memory_order_release);
                }


            private:
                sync::atomic<void*> m_tail_for_alloc; /**< Pointer to the end of the last element allocated in the queue. */
                sync::atomic<void*> m_tail_for_consumers;
                VOID_ALLOCATOR * m_allocator; /**< The allocator is a subobject (a base class) of the queue. If the push algorithm
                                              were implemented in the queue, this pointer would be unnecessary. With this pointer
                                              we need an extra indirection to access the to allocator. Anyway, thanks to the
                                              page-based memory management, we don't need to access the allocator, so the extra
                                              indirection is paid not so often. (Furthermore this class probably fit in a cache line). */
            };

            /** Partial specialization of Tail with SYNC_KIND = SynchronizationKind::LocklessMultiple. */
            template < typename VOID_ALLOCATOR, typename RUNTIME_TYPE>
                class alignas(concurrent_alignment) Head<VOID_ALLOCATOR, RUNTIME_TYPE, SynchronizationKind::LocklessMultiple>
            {
            public:

                using ControlBlock = ControlBlock<RUNTIME_TYPE>;

                void initialize(VOID_ALLOCATOR * i_allocator, void * i_first_page, const sync::atomic<void*> * i_tail_for_consumers)
                {
                    m_allocator = i_allocator;
                    m_tail_for_consumers = i_tail_for_consumers;
                    m_head.store(reinterpret_cast<uintptr_t>(i_first_page));
                }

                struct ConsumeData
                {
                    ControlBlock * m_control;

                    DENSITY_STRONG_INLINE void * element() const
                    {
                        return address_upper_align(m_control + 1, m_control->m_type.alignment());
                    }

                    DENSITY_STRONG_INLINE void * element_unaligned() const { return m_control + 1; }

                    DENSITY_STRONG_INLINE const RUNTIME_TYPE & type() const
                    {
                        return m_control->m_type;
                    }
                };

                ConsumeData begin_consume() noexcept
                {
                    // Get exclusive access on m_head, setting it to zero
                    uintptr_t head;
                    do {
                        head = m_head.fetch_and(0, sync::hint_memory_order_acquire);
                        if (head == 0)
                        {
                            std::this_thread::yield();
                        }
                    } while (head == 0);

                    // this is the value of head we are going to set when we have finished
                    auto good_head = head;

                    unsigned skip_count = 0;

                    for(;;)
                    {
                        // check if we have reached the end of the queue
                        auto const tail_for_consume = m_tail_for_consumers->load(sync::hint_memory_order_acquire);
                        if (reinterpret_cast<void*>(head) == tail_for_consume)
                        {
                            m_head.store(good_head, sync::hint_memory_order_release);
                            return ConsumeData{ nullptr };
                        }

                        auto const control = reinterpret_cast<ControlBlock*>(head);
                        auto const dirt_next = control->m_next.fetch_or(1);
                        if ((dirt_next & 1) == 0)
                        {
                            // exclusive access
                            if ((dirt_next & 2) == 0)
                            {
                                // living object
                                m_head.store(good_head, sync::hint_memory_order_release);
                                return ConsumeData{ control };
                            }
                            else
                            {
                                // dead element
                                if (skip_count == 0)
                                {
                                    DENSITY_ASSERT_INTERNAL( (dirt_next & 3) == 2 );

                                    #if DENSITY_DEBUG_INTERNAL
                                        control->m_next.store(37);
                                    #endif
                                    if (control->m_type.empty())
                                    {
                                        auto const page = reinterpret_cast<void*>(head & ~static_cast<uintptr_t>(VOID_ALLOCATOR::page_alignment - 1));
                                        DENSITY_ASSERT(is_address_aligned(page, VOID_ALLOCATOR::page_alignment));
                                        m_allocator->deallocate_page(page);
                                    }
                                    good_head = head = dirt_next - 2;
                                    continue;
                                }
                                else
                                {
                                    // unlock
                                    control->m_next.store(dirt_next);
                                }
                            }
                        }

                        // Move to the next element, which may be in another page
                        head = dirt_next & ~static_cast<uintptr_t>(3);
                        skip_count++;
                    }
                }

                void commit_consume(ConsumeData i_consume_data) noexcept
                {
                    #if DENSITY_DEBUG
                        memset(&(i_consume_data.m_control->m_type), 0xB4, sizeof(i_consume_data.m_control->m_type));
                    #endif

                    i_consume_data.m_control->m_next.fetch_xor(3, sync::hint_memory_order_relaxed);

                    m_head.fetch_add(0, sync::hint_memory_order_release);
                }

            private:
                sync::atomic<uintptr_t> m_head;
                const sync::atomic<void*> * m_tail_for_consumers;
                VOID_ALLOCATOR * m_allocator; /**< The allocator is a subobject (a base class) of the queue. If the consume algorithm
                                              were implemented in the queue, this pointer would be unnecessary. With this pointer
                                              we need an extra indirection to access the to allocator. Anyway, thanks to the
                                              page-based memory management, we don't need to access the allocator, so the extra
                                              indirection is paid not so often. (Furthermore this class probably fit in a cache line). */
            };
        }

    } // namespace detail

} // namespace density
