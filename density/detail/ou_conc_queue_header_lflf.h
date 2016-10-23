
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
/**
        ou_conc_queue_header_lflf.h - This is a private header. Do not include it, and do not use its declarations!
        ou stands for one-use: ou_conc_queue_header does not recycle the memory freed by consumed elements
        lflf stands for lock-free-producer, lock-free-consumer
*/
#ifndef DENSITY_INCLUDING_CONC_QUEUE_DETAIL
    #error "THIS IS A PRIVATE HEADER OF DENSITY. DO NOT INCLUDE IT DIRECTLY"
#endif // ! DENSITY_INCLUDING_CONC_QUEUE_DETAIL

namespace density
{
    namespace detail
    {
        /** Header for a fixed-size disposable concurrent queue. This is a partial specialization of the class template ou_conc_queue_header.
            Warning: this is a low-level, internal, complex-to-use class. Don't use it directly, use concurrent_heterogeneous_queue and concurrent_function_queue instead.
            This class assumes that the user space starts from (this + 1) and ends to reinterpret_cast<char*>(this) + PAGE_SIZE.
            This class implements a concurrent lock-free multiple-consumers multiple-producers heterogeneous queue. This container is disposable,
            in the sense that it does not recycle the space in the buffer, like a ring buffer does. The only way to reuse the memory assigning to this class
            is destroying it and creating a new page. This behavior is the key to have a simple push algorithm.
            Every push consumes some capacity. A consume has no effect on the capacity.
                - Both head and tail are monotonic: there is no wrapping at the end of the buffer
                - The capacity is monotonic: if an element does not fit in the available space, it will never do
            This class is non-copyable and non-movable.
        */
        template <typename INTERNAL_WORD, typename RUNTIME_TYPE, INTERNAL_WORD PAGE_SIZE>
            class ou_conc_queue_header<INTERNAL_WORD, RUNTIME_TYPE, PAGE_SIZE,
                SynchronizationKind::LocklessMultiple, SynchronizationKind::LocklessMultiple>
        {
        public:

            static const INTERNAL_WORD internal_alignment = 4;

            /** Before each element there is a ControlBlock object */
            struct ControlBlock
            {
                std::atomic<INTERNAL_WORD> m_size; /** size of the element, plus two additional flags encoded in the least-significant bits.
                                                   bit 0: exclusive access flag. The thread that succeeds in setting this flag has exclusive
                                                    access on the content of the element.
                                                   bit 1: dead element flag. The content of the element is not valid: it has been consumed,
                                                   or the constructor threw an exception.
                                                   The size of the element (excluding the control block) is given by:
                                                    m_size.load() & (std::numeric_limits<INTERNAL_WORD>::max - 3). */
                RUNTIME_TYPE m_type; /** type of the element */
            };


            /** Default constructor, not thread-safe. Head and tail are initialized to sizeof(*this), because they are
                based on the address of this. Control blocks and elements are allocated from beginning from the address (this +1). */
            ou_conc_queue_header()
                : m_head(sizeof(*this)), m_tail(sizeof(*this)), m_next(nullptr)
            {
                /** The push algorithm requires the control block at m_tail has the m_size member initialized to zero */
                reinterpret_cast<ControlBlock*>(this + 1)->m_size.store(0);
            }

            ou_conc_queue_header(const ou_conc_queue_header &) = delete;
            ou_conc_queue_header & operator = (const ou_conc_queue_header &) = delete;

            /** Pushes a new element on the queue. */
            template <typename CONSTRUCTOR>
                bool push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor, INTERNAL_WORD i_size)
            {
                DENSITY_ASSERT_INTERNAL(i_size > 0 && is_uint_aligned(i_size, internal_alignment));

                INTERNAL_WORD tail;
                void * element;
                ControlBlock * control, * next_control;
                #if DENSITY_DEBUG_INTERNAL
                    INTERNAL_WORD dbg_original_tail;
                #endif

                /* The size of the control block we are going to allocate is guaranteed to be zero (see the constructor)
                    We loop until we succeed in changing the size of the control block from zero to i_size + 1.
                    The +1 in the size means that we have exclusive access to the element (needed in order to construct it),
                    Consumer threads can skip the element while we have exclusive access on it. */
                bool exclusive_access;
                do {

                    /* the tail is reloaded on every iteration, as a failure in the compare_exchange_strong means that
                        another thread has succeed, so the tail is changed. */
                    DENSITY_TEST_RANDOM_WAIT();
                    tail = m_tail.load();
                    #if DENSITY_DEBUG_INTERNAL
                        dbg_original_tail = tail;
                    #endif

                    /* linearly-allocate the control block and the element, updating tail */
                    control = static_cast<ControlBlock*>(allocate(&tail, sizeof(ControlBlock)));
                    element = allocate(&tail, i_size);

                    /* linearly-allocate the next control block, setting future_tail to the updated tail */
                    auto future_tail = tail;
                    next_control = static_cast<ControlBlock*>(allocate(&future_tail, sizeof(ControlBlock)));

                    /* If future_tail has overrun the page, we fail. So maybe we are wasting some bytes (as the current element
                        may still fit in the queue), but this allows a simpler algorithm. */
                    if (future_tail > PAGE_SIZE)
                    {
                        return false; // the new element does not fit in the queue
                    }

                    /* try to commit, setting the size of the block. This is the first change visible to the other threads.
                        This works because the first block after tail has always the size set to zero. */
                    INTERNAL_WORD expected = 0;
                    DENSITY_TEST_RANDOM_WAIT();
                    exclusive_access = control->m_size.compare_exchange_strong(expected, i_size + 1);
                } while ( !exclusive_access );

                /* after gaining exclusive access to the element after tail, we initialize the next control block to zero, to allow
                    future concurrent pushes to play the compare_exchange_strong game (the race to obtain exclusive access). */
                DENSITY_TEST_RANDOM_WAIT();
                next_control->m_size.store(0);

                /* now we can commit the tail. This allows the other pushes to skip the element we are going to construct.
                    So the duration of the contention between concurrent pushes is really minimal (two atomic stores). */
                DENSITY_ASSERT_INTERNAL(m_tail.load() == dbg_original_tail);
                DENSITY_TEST_RANDOM_WAIT();
                m_tail.store(tail);

                /* construct the type of the new element. Hopefully RUNTIME_TYPE is just a pointer.
                    We still have the exclusive access (the bit 0 set in control->m_size).*/
                DENSITY_TEST_RANDOM_WAIT();
                #if DENSITY_HANDLE_EXCEPTIONS
                    try
                #endif
                {
                    new(&control->m_type) RUNTIME_TYPE(i_source_type);
                }
                #if DENSITY_HANDLE_EXCEPTIONS
                    catch (...)
                    {
                        /* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the
                            same time the exclusive access is removed (the first bit) */
                        DENSITY_ASSERT_INTERNAL(control->m_size.load() == i_size + 1);
                        control->m_size.store(i_size + 2);
                        throw; // the exception is propagated to the caller, whatever it is.
                    }
                #endif

                /* construct the new element */
                #if LF_HANDLE_EXCEPTIONS
                    try
                #endif
                {
                    /* constructs the new element */
                    std::forward<CONSTRUCTOR>(i_constructor)(i_source_type, element);
                }
                #if LF_HANDLE_EXCEPTIONS
                    catch (...)
                    {
                        // destroy the type
                        control->m_type->RUNTIME_TYPE::~RUNTIME_TYPE();

                        /* The second bit of m_size is set to 1, to mark this element as dead. At the
                            same time the exclusive access is removed (the first bit). */
                        DENSITY_ASSERT_INTERNAL(control->m_size.load() == i_size + 1);
                        control->m_size.store(i_size + 2);
                        throw; // the exception is propagated to the caller, whatever it is.
                    }
                #endif

                /* decrementing the size we allow the consumers to process this element (this is an atomic operation) */
                DENSITY_TEST_RANDOM_WAIT();
                DENSITY_ASSERT_INTERNAL(control->m_size.load() == i_size + 1);
                --control->m_size;

                return true;
            }

            /** If this function finds an invalid element, that is an element whose construction raised an exception, it removes
                the element from the queue and returns true, even if no element was actually removed.
                If an exception is raised during the consumption of an element, the element is removed, and the exception is then
                propagated to the caller. */
            template <typename CONSUMER>
                bool try_consume(CONSUMER && i_consumer)
            {
                DENSITY_TEST_RANDOM_WAIT();
                auto head = m_head.load();
                #if DENSITY_DEBUG_INTERNAL
                    const INTERNAL_WORD dbg_original_head = head;
                #endif

                /* Try-and-repeat loop. On every iteration we skip an element. We stop when we
                    get exclusive access on a valid element. If we reach m_tail, we exit. */
                ControlBlock * control;
                void * element;
                INTERNAL_WORD dirt_size, size;
                size_t tries = 0;
                for(;;)
                {
                    // check if we have reached the tail
                    DENSITY_TEST_RANDOM_WAIT();
                    auto const tail = m_tail.load();
                    DENSITY_ASSERT_INTERNAL(tail >= head);
                    if (head >= tail)
                    {
                        DENSITY_ASSERT_INTERNAL(tail == head);

                        // no consumable element is available
                        return false;
                    }

                    /* linearly-allocate the control block, updating head */
                    control = static_cast<ControlBlock*>(allocate(&head, sizeof(ControlBlock)));

                    /* atomically load the size of the block and set the first bit to 1. If the
                        first bit was already set, we are going to repeat the loop. Otherwise we
                        have the nonexclusive access on the content of the element. */
                    DENSITY_TEST_RANDOM_WAIT();
                    dirt_size = control->m_size.fetch_or(1);

                    /* clean up the size and linearly-allocate the element */
                    size = dirt_size & (std::numeric_limits<INTERNAL_WORD>::max() - 3);
                    element = allocate(&head, size);

                    /*
                        cases for (dirt_size & 3):

                            0 -> we have got exclusive access to a living element - exit the loop
                            1 -> the element is living, but we don't have access - skip and continue
                            2 -> we have got exclusive access to a dead element - if this is the first iteration, remove it
                            3 -> dead element, but we don't have access to it - skip and continue
                    */

                    if ((dirt_size & 3) == 0)
                    {
                        /* We have obtained exclusive access on a valid element. Exit the loop. */
                        break;
                    }

                    if ((dirt_size & 1) != 0)
                    {
                        /* someone else has the exclusive access on the element, continue with the next one. */
                        tries++;
                    }
                    else
                    {
                        /* We have the exclusive access to a dead element, and this is the first iteration, we can
                            advance the head. Only the thread with exclusive access can do this, so we can do it safely */
                        DENSITY_TEST_RANDOM_WAIT();
                        if (tries == 0)
                        {
                            m_head.store(head);
                        }
                        else
                        {
                            DENSITY_ASSERT_INTERNAL((control->m_size.load() & 1) == 1);
                            #if DENSITY_DEBUG
                                const auto prev =
                            #endif
                            --control->m_size;
                            DENSITY_ASSERT_INTERNAL((prev & 1) == 0);

                            tries++;
                        }
                    }
                }

                /* we have exclusive access to a non-dead element, so we can consume it */
                DENSITY_TEST_RANDOM_WAIT();
                std::forward<CONSUMER>(i_consumer)(control->m_type, element);

                /* destroy the type (usually a nop) */
                control->m_type.RUNTIME_TYPE::~RUNTIME_TYPE();
                #if DENSITY_DEBUG
                    memset(&control->m_type, 0xB4, sizeof(control->m_type));
                #endif

                /* drop the exclusive access, and mark the element as dead */
                DENSITY_TEST_RANDOM_WAIT();
                control->m_size.store(size + 2);

                return true;
            }

            /* A page is empty if it has no living, dead or being-consumed elements. Given that no producer threads will edit the page,
                it is an unrecoverable state. It is also objective, that is all the consumer threads observe this state coherently.
                When the first page becomes empty, consumer threads may still enter it to try to consume, but if it not the last page (that
                is, producers don't try to push elements), they will surely fail to consume an element. */
            bool is_empty() const
            {
                auto const head = m_head.load();
                auto const tail = m_tail.load();
                return head == tail;
            }

        private:

            /** Allocates an object with the given size starting from *io_pos, and updates it */
            void * allocate(INTERNAL_WORD * io_pos, INTERNAL_WORD i_size)
            {
                DENSITY_ASSERT_INTERNAL(is_uint_aligned(i_size, internal_alignment));
                DENSITY_ASSERT_INTERNAL(is_uint_aligned(*io_pos, internal_alignment));
                void * res = reinterpret_cast<unsigned char*>(this) + *io_pos;
                *io_pos += i_size;
                return res;
            }

        private:
            alignas(concurrent_alignment) std::atomic<INTERNAL_WORD> m_head;
            alignas(concurrent_alignment) std::atomic<INTERNAL_WORD> m_tail;

        public:
            ou_conc_queue_header * m_next; /**< pointer to the next page, or nullptr */
        };

   } // namespace detail

} // namespace density
