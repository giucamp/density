
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace density
{
    namespace detail
    {
        /** \internal Class template that implements consume operations with a single producer */
        template <typename RUNTIME_TYPE, typename ALLOCATOR_TYPE, typename QUEUE_TAIL>
        class LFQueue_Head<RUNTIME_TYPE, ALLOCATOR_TYPE, concurrency_single, QUEUE_TAIL>
            : protected QUEUE_TAIL
        {
          private:
            using Base = QUEUE_TAIL;

          protected:
            using ControlBlock = typename Base::ControlBlock;

            constexpr LFQueue_Head() noexcept {}

            constexpr LFQueue_Head(ALLOCATOR_TYPE && i_allocator) noexcept
                : Base(std::move(i_allocator))
            {
            }

            constexpr LFQueue_Head(const ALLOCATOR_TYPE & i_allocator) noexcept : Base(i_allocator)
            {
            }

            template <typename BUSY_WAIT>
            constexpr LFQueue_Head(
              ALLOCATOR_TYPE && i_allocator, BUSY_WAIT && i_busy_wait, std::true_type) noexcept
                : Base(std::move(i_allocator), std::forward<BUSY_WAIT>(i_busy_wait))
            {
            }
            template <typename BUSY_WAIT>
            constexpr LFQueue_Head(
              const ALLOCATOR_TYPE & i_allocator, BUSY_WAIT && i_busy_wait, std::true_type) noexcept
                : Base(i_allocator, std::forward<BUSY_WAIT>(i_busy_wait))
            {
            }

            template <typename BUSY_WAIT>
            constexpr LFQueue_Head(
              ALLOCATOR_TYPE && i_allocator, BUSY_WAIT &&, std::false_type) noexcept
                : Base(std::move(i_allocator))
            {
            }
            template <typename BUSY_WAIT>
            constexpr LFQueue_Head(
              const ALLOCATOR_TYPE & i_allocator, BUSY_WAIT &&, std::false_type) noexcept
                : Base(i_allocator)
            {
            }

            LFQueue_Head(LFQueue_Head && i_source) noexcept : LFQueue_Head()
            {
                swap(*this, i_source);
            }

            LFQueue_Head & operator=(LFQueue_Head && i_source) noexcept
            {
                swap(*this, i_source);
                return *this;
            }

            // this function is not required to be thread-safe
            friend void swap(LFQueue_Head & i_first, LFQueue_Head & i_second) noexcept
            {
                // swap the base
                swap(static_cast<Base &>(i_first), static_cast<Base &>(i_second));

                // swap the head
                std::swap(i_first.m_head, i_second.m_head);
            }

            struct Consume
            {
                LFQueue_Head * m_queue =
                  nullptr; /**< Owning queue if the Consume is not empty, undefined otherwise. */
                ControlBlock * m_control =
                  nullptr; /**< Currently pinned control block. Independent from the empty-ness of the Consume */
                uintptr_t m_next_ptr =
                  0; /**< m_next member of the ControlBox of the element being consumed. The Consume is empty if and 
                          only if m_next_ptr <= LfQueue_AllFlags */

                Consume() noexcept = default;

                Consume(const Consume &) = delete;
                Consume & operator=(const Consume &) = delete;

                Consume(Consume && i_source) noexcept
                    : m_queue(i_source.m_queue), m_control(i_source.m_control),
                      m_next_ptr(i_source.m_next_ptr)
                {
                    i_source.m_control  = nullptr;
                    i_source.m_next_ptr = 0;
                }

                Consume & operator=(Consume && i_source) noexcept
                {
                    swap(i_source);
                    return *this;
                }

                bool empty() const noexcept { return m_next_ptr <= LfQueue_AllFlags; }

                bool external() const noexcept { return (m_next_ptr & LfQueue_External) != 0; }

                void swap(Consume & i_other) noexcept
                {
                    using std::swap;
                    swap(m_queue, i_other.m_queue);
                    swap(m_control, i_other.m_control);
                    swap(m_next_ptr, i_other.m_next_ptr);
                }

                bool begin_iteration(LFQueue_Head * i_queue) noexcept
                {
                    DENSITY_ASSUME_ALIGNED(m_control, Base::s_alloc_granularity);

                    DENSITY_ASSUME_ALIGNED(i_queue->m_head, Base::s_alloc_granularity);

                    if (i_queue->m_head == nullptr)
                    {
                        i_queue->m_head = i_queue->Base::get_initial_page();
                        if (i_queue->m_head == nullptr)
                        {
                            m_next_ptr = 0;
                            return true;
                        }
                    }

                    m_queue    = i_queue;
                    m_control  = static_cast<ControlBlock *>(i_queue->m_head);
                    m_next_ptr = raw_atomic_load(&m_control->m_next, mem_acquire);
                    return true;
                }

                bool is_queue_empty(const LFQueue_Head * i_queue) noexcept
                {
                    // we may do changes to the queue which are not observable from outside
                    return is_queue_empty(const_cast<LFQueue_Head *>(i_queue));
                }

                bool is_queue_empty(LFQueue_Head * i_queue) noexcept
                {
                    DENSITY_ASSERT_INTERNAL(empty());

                    begin_iteration(i_queue);
                    while (!empty())
                    {
                        if (
                          (m_next_ptr & (LfQueue_Busy | LfQueue_Dead | LfQueue_InvalidNextPage)) ==
                          0)
                        {
                            return false;
                        }
                        move_next();
                    }
                    return true;
                }

                bool move_next() noexcept
                {
                    DENSITY_ASSUME_ALIGNED(m_control, Base::s_alloc_granularity);

                    m_control  = reinterpret_cast<ControlBlock *>(m_next_ptr & ~LfQueue_AllFlags);
                    m_next_ptr = raw_atomic_load(&m_control->m_next, mem_acquire);
                    return true;
                }

                /** Tries to start a consume operation. The Consume must be initially empty.
                    If there are no consumable elements, the Consume remains empty (m_next_ptr <= LfQueue_AllFlags).
                    Otherwise m_next_ptr is the value to set on the ControlBox to commit the consume
                    (it has the LfQueue_Dead flag). */
                void start_consume_impl(LFQueue_Head * i_queue) noexcept
                {
                    DENSITY_ASSERT_INTERNAL(empty());

                    begin_iteration(i_queue);
                    while (!empty())
                    {
                        if (
                          (m_next_ptr & (LfQueue_Busy | LfQueue_Dead | LfQueue_InvalidNextPage)) ==
                          0)
                        {
                            /* found an element to consume, set LfQueue_Busy 
                                todo: the single consumer may probably avoid to do any further atomic
                                    operation on m_control->m_next */

                            raw_atomic_store(
                              &m_control->m_next, m_next_ptr | LfQueue_Busy, mem_relaxed);
                            m_next_ptr |=
                              LfQueue_Dead; /* this is what we will write in m_control->m_next
                                when (and if) we will commit the consume */
                            break;
                        }
                        else if ((m_next_ptr & (LfQueue_Busy | LfQueue_Dead)) == LfQueue_Dead)
                        {
                            advance_head();
                        }
                        move_next();
                    }
                }

#if DENSITY_DEBUG_INTERNAL
                void consume_dbg_checks()
                {
                    DENSITY_ASSERT_INTERNAL(!empty());
                    DENSITY_ASSERT_INTERNAL(
                      (m_next_ptr & (LfQueue_Busy | LfQueue_Dead | LfQueue_InvalidNextPage)) ==
                      LfQueue_Dead);
                    DENSITY_ASSERT_INTERNAL(
                      raw_atomic_load(&m_control->m_next, mem_relaxed) ==
                      m_next_ptr - LfQueue_Dead + LfQueue_Busy);
                }
#endif

                /** Commits a consumed element. After the call the Consume is empty. */
                void commit_consume_impl() noexcept
                {
#if DENSITY_DEBUG_INTERNAL
                    consume_dbg_checks();
#endif

                    // remove LfQueue_Busy and add LfQueue_Dead
                    raw_atomic_store(&m_control->m_next, m_next_ptr, mem_release);

                    clean_dead_elements();

                    m_next_ptr = 0;
                }

                void cancel_consume_impl() noexcept
                {
#if DENSITY_DEBUG_INTERNAL
                    consume_dbg_checks();
#endif

                    // remove LfQueue_Busy and add LfQueue_Dead
                    raw_atomic_store(&m_control->m_next, m_next_ptr - LfQueue_Dead, mem_release);
                    m_next_ptr = 0;
                }

                void clean_dead_elements() noexcept
                {
                    while ((m_next_ptr & (LfQueue_Busy | LfQueue_Dead)) == LfQueue_Dead)
                    {
                        advance_head();
                        move_next();
                    }
                }

                /** If m_head equals to m_control advance it to the next block, zeroing the memory.
                    This function assumes that the current block is dead. */
                bool advance_head()
                {
                    auto next = reinterpret_cast<ControlBlock *>(m_next_ptr & ~LfQueue_AllFlags);

                    if (m_queue->m_head == m_control)
                    {
                        m_queue->m_head = next;
                        if (m_next_ptr & LfQueue_External)
                        {
                            auto const external_block = static_cast<ExternalBlock *>(
                              address_add(m_control, Base::s_element_min_offset));
                            m_queue->ALLOCATOR_TYPE::deallocate(
                              external_block->m_block,
                              external_block->m_size,
                              external_block->m_alignment);
                        }

                        bool const is_same_page = Base::same_page(m_control, next);
                        DENSITY_ASSERT_INTERNAL(
                          is_same_page != address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
                        DENSITY_ASSERT_INTERNAL(
                          !ConstConditional(Base::s_needs_end_control) ||
                          is_same_page == (m_control != Base::get_end_control_block(m_control)));

                        static_assert(offsetof(ControlBlock, m_next) == 0, "");

                        if (Base::s_deallocate_zeroed_pages)
                        {
                            raw_atomic_store(&m_control->m_next, uintptr_t(0));
                        }

                        if (is_same_page)
                        {
                            if (Base::s_deallocate_zeroed_pages)
                            {
                                auto const memset_dest =
                                  const_cast<uintptr_t *>(&m_control->m_next) + 1;
                                auto const memset_size = address_diff(next, memset_dest);
                                DENSITY_ASSUME_ALIGNED(memset_dest, alignof(uintptr_t));
                                DENSITY_ASSUME_UINT_ALIGNED(memset_size, alignof(uintptr_t));
                                std::memset(memset_dest, 0, memset_size);
                            }
                        }
                        else
                        {
                            if (Base::s_deallocate_zeroed_pages)
                                m_queue->ALLOCATOR_TYPE::deallocate_page_zeroed(m_control);
                            else
                                m_queue->ALLOCATOR_TYPE::deallocate_page(m_control);
                        }
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }

            }; // Consume

          private: // data members
            alignas(destructive_interference_size) ControlBlock * m_head{nullptr};
        };

    } // namespace detail

} // namespace density
