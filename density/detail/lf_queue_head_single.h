
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace density
{
    namespace detail
    {
        /** \internal Class template that implements consume operations with multiple producers */
        template <
          typename COMMON_TYPE,
          typename RUNTIME_TYPE,
          typename ALLOCATOR_TYPE,
          typename QUEUE_TAIL>
        class LFQueue_Head<
          COMMON_TYPE,
          RUNTIME_TYPE,
          ALLOCATOR_TYPE,
          concurrency_single,
          QUEUE_TAIL> : protected QUEUE_TAIL
        {
          private:
            using Base = QUEUE_TAIL;

          protected:
            using ControlBlock = typename Base::ControlBlock;

            LFQueue_Head() noexcept {}

            LFQueue_Head(ALLOCATOR_TYPE && i_allocator) noexcept : Base(std::move(i_allocator)) {}

            LFQueue_Head(const ALLOCATOR_TYPE & i_allocator) : Base(i_allocator) {}

            LFQueue_Head(LFQueue_Head && i_source) noexcept : LFQueue_Head() { swap(i_source); }

            LFQueue_Head & operator=(LFQueue_Head && i_source) noexcept
            {
                swap(i_source);
                return *this;
            }

            void swap(LFQueue_Head & i_other) noexcept
            {
                Base::swap(i_other);

                // swap the head
                std::swap(m_head, i_other.m_head);
            }

            struct Consume
            {
                LFQueue_Head * m_queue =
                  nullptr; /**< Owning queue if the Consume is not empty, undefined otherwise. */
                ControlBlock * m_control =
                  nullptr; /**< Current control block. Independent from the empty-ness of the Consume */
                uintptr_t m_next_ptr =
                  0; /**< m_next member of the ControlBox of the element being consumed. The Consume is empty if and only if m_next_ptr == 0 */

                Consume() noexcept = default;

                Consume(const Consume &) = delete;
                Consume & operator=(const Consume &) = delete;

                bool empty() const noexcept { return m_next_ptr <= LfQueue_AllFlags; }

                bool external() const noexcept { return (m_next_ptr & LfQueue_External) != 0; }

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


                void swap(Consume & i_other) noexcept
                {
                    using std::swap;
                    swap(m_queue, i_other.m_queue);
                    swap(m_control, i_other.m_control);
                    swap(m_next_ptr, i_other.m_next_ptr);
                }

                /** Attaches this Consume to a queue, pinning the head. The previously pinned page is unpinned.
                    @return true if a page was pinned, false if the queue is virgin. */
                bool assign_queue(LFQueue_Head * i_queue) noexcept
                {
                    DENSITY_ASSERT_INTERNAL(
                      address_is_aligned(m_control, Base::s_alloc_granularity));

                    m_control = i_queue->m_head;
                    DENSITY_ASSERT_INTERNAL(
                      address_is_aligned(m_control, Base::s_alloc_granularity));

                    if (m_control == nullptr)
                    {
                        m_control = init_head(i_queue);
                        if (m_control == nullptr)
                        {
                            return false;
                        }
                    }
                    m_queue = i_queue;
                    return true;
                }

                bool is_queue_empty(const LFQueue_Head * i_queue) noexcept
                {
                    // we may do changes to queue which are not observable from outside
                    return is_queue_empty(const_cast<LFQueue_Head *>(i_queue));
                }

                bool is_queue_empty(LFQueue_Head * i_queue) noexcept
                {
                    m_queue = i_queue;

                    auto control = m_control;


                    DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);
                    DENSITY_ASSERT_INTERNAL(address_is_aligned(control, Base::s_alloc_granularity));

                    bool is_empty = true;

                    auto next = i_queue->m_head;
                    for (;;)
                    {
                        /*

                            - control and next are in the same page. In this case we continue iterating
                                without any pin\unpin. This is the fast-path.

                            - control and next are in distinct pages. In this case we have to switch the pinned page.

                            - next is null. The head of the queue is to be initialized. If no put has been performed on this
                                queue, no operation is done.

                            - control is nullptr. This Consume has to be initialized

                        */
                        if (DENSITY_LIKELY(Base::same_page(control, next) && control != nullptr))
                        {
                            control = next;

                            // We do an initial relaxed read. We will do a memory acquire in the CAS
                            auto const next_uint =
                              raw_atomic_load(&control->m_next, detail::mem_relaxed);
                            next = reinterpret_cast<ControlBlock *>(
                              next_uint & ~detail::LfQueue_AllFlags);

                            /** Check if this element is ready to be consumed */
                            if ((next_uint & (detail::LfQueue_Busy | detail::LfQueue_Dead)) == 0)
                            {
                                if ((next_uint & ~detail::LfQueue_InvalidNextPage) != 0)
                                {
                                    is_empty = false;
                                    break;
                                }
                                else
                                {
                                    /* We have found a zeroed ControlBlock */
                                    next                 = i_queue->m_head;
                                    bool should_continue = false;
                                    if (Base::same_page(next, control))
                                        should_continue = control < next;

                                    if (!should_continue)
                                    {
                                        // the queue is empty
                                        break;
                                    }
                                }
                            }
                        }
                        else if (next != nullptr)
                        {
                            control = next;
                        }
                        else
                        {
                            next = init_head(i_queue);
                            if (next == nullptr)
                            {
                                // the queue is virgin and empty
                                break;
                            }
                        }
                    }

                    m_control = control;

                    return is_empty;
                }

                /** Tries to start a consume operation. The Consume must be initially empty.
                    If there are no consumable elements, the Consume remains empty (m_next_ptr == 0).
                    Otherwise m_next_ptr is the value to set on the ControlBox to commit the consume
                    (it has the LfQueue_Dead flag). */
                void start_consume_impl(LFQueue_Head * i_queue) noexcept
                {
                    m_queue = i_queue;

                    auto control = m_control;
                    DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);
                    DENSITY_ASSERT_INTERNAL(address_is_aligned(control, Base::s_alloc_granularity));

                    auto next = i_queue->m_head;
                    for (;;)
                    {
                        /*

                            - control and next are in the same page. In this case we continue iterating
                                without any pin\unpin. This is the fast-path.

                            - control and next are in distinct pages. In this case we have to switch the pinned page.

                            - next is null. The head of the queue is to be initialized. If no put has been performed on this
                                queue, no operation is done.

                            - control is nullptr. This Consume has to be initialized

                        */
                        if (DENSITY_LIKELY(Base::same_page(control, next) && control != nullptr))
                        {
                            control = next;

                            // We do an initial relaxed read. We will do a memory acquire in the CAS
                            auto const next_uint =
                              raw_atomic_load(&control->m_next, detail::mem_relaxed);
                            next = reinterpret_cast<ControlBlock *>(
                              next_uint & ~detail::LfQueue_AllFlags);

                            /** Check if this element is ready to be consumed */
                            if ((next_uint & (detail::LfQueue_Busy | detail::LfQueue_Dead)) == 0)
                            {
                                if ((next_uint & ~detail::LfQueue_InvalidNextPage) != 0)
                                {
                                    /* We try to set the flag LfQueue_Busy on it */
                                    raw_atomic_store(
                                      &control->m_next,
                                      next_uint | detail::LfQueue_Busy,
                                      detail::mem_relaxed);
                                    m_next_ptr = next_uint | LfQueue_Dead;
                                    break;
                                }
                                else
                                {
                                    /* We have found a zeroed ControlBlock */
                                    next                 = i_queue->m_head;
                                    bool should_continue = false;
                                    if (Base::same_page(next, control))
                                        should_continue = control < next;

                                    if (!should_continue)
                                    {
                                        // the queue is empty
                                        break;
                                    }
                                }
                            }
                            else if (
                              (next_uint & (detail::LfQueue_Busy | detail::LfQueue_Dead)) ==
                              detail::LfQueue_Dead)
                            {
                                // clean up
                                cleanup_step(control, next_uint, next);
                            }
                        }
                        else if (next != nullptr)
                        {
                            control = next;
                        }
                        else
                        {
                            next = init_head(i_queue);
                            if (next == nullptr)
                            {
                                // the queue is virgin and empty
                                break;
                            }
                        }
                    }

                    m_control = control;
                }

                /** If m_head equals to i_control_block advance it, zeroing the memory. No pin\unpin is performed. */
                bool cleanup_step(
                  ControlBlock * const i_control_block,
                  uintptr_t const      i_next_uint,
                  ControlBlock * const i_next)
                {
                    if (m_queue->m_head == i_control_block)
                    {
                        m_queue->m_head = i_next;

                        if (i_next_uint & detail::LfQueue_External)
                        {
                            auto const external_block = static_cast<ExternalBlock *>(
                              address_add(i_control_block, Base::s_element_min_offset));
                            m_queue->ALLOCATOR_TYPE::deallocate(
                              external_block->m_block,
                              external_block->m_size,
                              external_block->m_alignment);
                        }

                        if (Base::s_deallocate_zeroed_pages)
                        {
                            raw_atomic_store(&i_control_block->m_next, uintptr_t(0));
                        }

                        if (Base::same_page(i_control_block, i_next))
                        {
                            if (Base::s_deallocate_zeroed_pages)
                            {
                                auto const memset_dest =
                                  const_cast<atomic_uintptr_t *>(&i_control_block->m_next) + 1;
                                auto const memset_size = address_diff(i_next, memset_dest);
                                std::memset(memset_dest, 0, memset_size);
                            }
                        }
                        else
                        {
                            if (Base::s_deallocate_zeroed_pages)
                                m_queue->ALLOCATOR_TYPE::deallocate_page_zeroed(i_control_block);
                            else
                                m_queue->ALLOCATOR_TYPE::deallocate_page(i_control_block);
                        }
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }

                /** Reads m_head. If it is still nullptr, tries to set it to the first page (if any) */
                static ControlBlock * init_head(LFQueue_Head * i_queue) noexcept
                {
                    // control and next are both null - initialize the head
                    if (i_queue->m_head == nullptr)
                    {
                        i_queue->m_head = i_queue->Base::get_initial_page();
                    }

                    DENSITY_ASSERT_INTERNAL(
                      address_is_aligned(i_queue->m_head, Base::s_alloc_granularity));
                    return i_queue->m_head;
                }

                /** Commits a consumed element. After the call the Consume is empty. */
                void commit_consume_impl() noexcept
                {
                    DENSITY_ASSERT_INTERNAL(m_next_ptr != 0);

                    // we expect to have LfQueue_Busy and not LfQueue_Dead...
                    DENSITY_ASSERT_INTERNAL(
                      (raw_atomic_load(&m_control->m_next, detail::mem_relaxed) &
                       (detail::LfQueue_Busy | detail::LfQueue_Dead)) == detail::LfQueue_Busy);

                    // remove LfQueue_Busy and add LfQueue_Dead
                    DENSITY_ASSERT_INTERNAL(
                      (m_next_ptr & (detail::LfQueue_Dead | detail::LfQueue_Busy |
                                     detail::LfQueue_InvalidNextPage)) == detail::LfQueue_Dead);
                    raw_atomic_store(&m_control->m_next, m_next_ptr, detail::mem_seq_cst);
                    m_next_ptr = 0;

                    clean_dead_elements();
                }

                void clean_dead_elements() noexcept
                {
                    auto control = m_control;

                    DENSITY_ASSERT_INTERNAL(control != nullptr);
                    for (;;)
                    {
                        auto const next_uint = raw_atomic_load(&control->m_next);
                        auto const next =
                          reinterpret_cast<ControlBlock *>(next_uint & ~detail::LfQueue_AllFlags);
                        if (
                          (next_uint & (detail::LfQueue_Busy | detail::LfQueue_Dead)) !=
                          detail::LfQueue_Dead)
                        {
                            // the element is not dead
                            break;
                        }

                        if (m_queue->m_head != control)
                        {
                            break;
                        }
                        m_queue->m_head = next;

                        if (next_uint & detail::LfQueue_External)
                        {
                            auto const external_block = static_cast<ExternalBlock *>(
                              address_add(control, Base::s_element_min_offset));
                            m_queue->ALLOCATOR_TYPE::deallocate(
                              external_block->m_block,
                              external_block->m_size,
                              external_block->m_alignment);
                        }

                        static_assert(offsetof(ControlBlock, m_next) == 0, "");
                        //std::memset(control, 0, address_diff(address_of_next, control));

                        auto const prev_control = control;
                        control                 = next;

                        if (Base::s_deallocate_zeroed_pages)
                        {
                            raw_atomic_store(&prev_control->m_next, uintptr_t(0));
                        }

                        bool const is_same_page = Base::same_page(control, prev_control);
                        DENSITY_ASSERT_INTERNAL(
                          !is_same_page ==
                          address_is_aligned(control, ALLOCATOR_TYPE::page_alignment));
                        DENSITY_ASSERT_INTERNAL(
                          !ConstConditional(Base::s_needs_end_control) ||
                          is_same_page ==
                            (prev_control != Base::get_end_control_block(prev_control)));
                        if (DENSITY_LIKELY(is_same_page))
                        {
                            if (Base::s_deallocate_zeroed_pages)
                            {
                                auto const memset_dest =
                                  const_cast<atomic_uintptr_t *>(&prev_control->m_next) + 1;
                                auto const memset_size = address_diff(next, memset_dest);
                                DENSITY_ASSERT_ALIGNED(memset_dest, alignof(uintptr_t));
                                DENSITY_ASSERT_UINT_ALIGNED(memset_size, alignof(uintptr_t));
                                std::memset(memset_dest, 0, memset_size);
                            }
                        }
                        else
                        {
                            if (Base::s_deallocate_zeroed_pages)
                                m_queue->ALLOCATOR_TYPE::deallocate_page_zeroed(prev_control);
                            else
                                m_queue->ALLOCATOR_TYPE::deallocate_page(prev_control);
                        }
                    }

                    m_control = control;
                }

                void cancel_consume_impl() noexcept
                {
                    DENSITY_ASSERT_INTERNAL(m_next_ptr != 0);

                    // we expect to have LfQueue_Busy and not LfQueue_Dead...
                    DENSITY_ASSERT_INTERNAL(
                      (raw_atomic_load(&m_control->m_next, detail::mem_relaxed) &
                       (detail::LfQueue_Busy | detail::LfQueue_Dead)) == detail::LfQueue_Busy);

                    DENSITY_ASSERT_INTERNAL(
                      (m_next_ptr & (detail::LfQueue_Dead | detail::LfQueue_Busy |
                                     detail::LfQueue_InvalidNextPage)) == detail::LfQueue_Dead);
                    raw_atomic_store(
                      &m_control->m_next, m_next_ptr - detail::LfQueue_Dead, detail::mem_seq_cst);
                    m_next_ptr = 0;

                    clean_dead_elements();
                }

            }; // Consume

          private: // data members
            alignas(destructive_interference_size) ControlBlock * m_head{nullptr};
        };

    } // namespace detail

} // namespace density
