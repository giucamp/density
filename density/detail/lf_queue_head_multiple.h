
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

namespace density
{
    namespace detail
    {
        /** \internal Class template that implements consume operations */
        template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE,
                concurrency_cardinality PROD_CARDINALITY, consistency_model CONSISTENCY_MODEL >
            class LFQueue_Head< COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE,
            PROD_CARDINALITY, concurrency_multiple, CONSISTENCY_MODEL>
                : protected LFQueue_Tail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSISTENCY_MODEL>
        {
        private:

            using Base = LFQueue_Tail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSISTENCY_MODEL>;

        protected:

            using ControlBlock = typename Base::ControlBlock;

            LFQueue_Head() noexcept
            {
            }

            LFQueue_Head(ALLOCATOR_TYPE && i_allocator) noexcept
                : Base(std::move(i_allocator))
            {
            }

            LFQueue_Head(const ALLOCATOR_TYPE & i_allocator)
                : Base(i_allocator)
            {
            }

            LFQueue_Head(LFQueue_Head && i_source) noexcept
                : LFQueue_Head()
            {
                swap(i_source);
            }

            LFQueue_Head & operator = (LFQueue_Head && i_source) noexcept
            {
                swap(i_source);
                return *this;
            }

            void swap(LFQueue_Head & i_other) noexcept
            {
                Base::swap(i_other);

                // swap the head
                auto const tmp = i_other.m_head.load();
                i_other.m_head.store(m_head.load());
                m_head.store(tmp);
            }

            struct Consume
            {
                LFQueue_Head * m_queue = nullptr; /**< Owning queue if the Consume is not empty, undefined otherwise. */
                ControlBlock * m_control = nullptr; /**< Currently pinned control block. Independent from the empty-ness of the Consume */
                uintptr_t m_next_ptr = 0; /**< m_next member of the ControlBox of the element being consumed. The Consume is empty if and only if m_next_ptr == 0 */

                Consume() noexcept = default;

                Consume(const Consume &) = delete;
                Consume & operator = (const Consume &) = delete;

                Consume(Consume && i_source) noexcept
                    : m_queue(i_source.m_queue), m_control(i_source.m_control), m_next_ptr(i_source.m_next_ptr)
                {
                    i_source.m_control = nullptr;
                    i_source.m_next_ptr = 0;
                }

                Consume & operator = (Consume && i_source) noexcept
                {
                    swap(i_source);
                    return *this;
                }

                ~Consume()
                {
                    if (m_control != nullptr)
                    {
                        m_queue->ALLOCATOR_TYPE::unpin_page(m_control);
                    }
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
                    DENSITY_ASSERT_INTERNAL(address_is_aligned(m_control, Base::s_alloc_granularity));

                    ControlBlock * head = i_queue->m_head.load();
                    DENSITY_ASSERT_INTERNAL(address_is_aligned(head, Base::s_alloc_granularity));

                    DENSITY_TEST_ARTIFICIAL_DELAY;

                    if (head == nullptr)
                    {
                        head = init_head(i_queue);
                        if (head == nullptr)
                        {
                            return false;
                        }
                    }

                    while (DENSITY_UNLIKELY(!Base::same_page(m_control, head) || m_control == nullptr))
                    {
                        DENSITY_ASSERT_INTERNAL(m_control != head);

                        i_queue->ALLOCATOR_TYPE::pin_page(head);

                        DENSITY_TEST_ARTIFICIAL_DELAY;

                        if (m_control != nullptr)
                        {
                            i_queue->ALLOCATOR_TYPE::unpin_page(m_control);
                        }

                        m_control = head;

                        head = i_queue->m_head.load();
                        DENSITY_ASSERT_INTERNAL(address_is_aligned(head, Base::s_alloc_granularity));

                        DENSITY_TEST_ARTIFICIAL_DELAY;
                    }

                    m_control = static_cast<ControlBlock*>(head);
                    m_queue = i_queue;
                    return true;
                }

                bool is_queue_empty(const LFQueue_Head * i_queue) noexcept
                {
                    // we may do changes to the queue which are not observable from outside
                    return is_queue_empty(const_cast<LFQueue_Head *>(i_queue));
                }

                bool is_queue_empty(LFQueue_Head * i_queue) noexcept
                {
                    m_queue = i_queue;

                    auto control = m_control;
                    DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);
                    DENSITY_ASSERT_INTERNAL(address_is_aligned(control, Base::s_alloc_granularity));

                    bool is_empty = true;

                    auto next = i_queue->m_head.load(mem_relaxed);
                    for (;;)
                    {
                        DENSITY_TEST_ARTIFICIAL_DELAY;

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
                            auto const next_uint = raw_atomic_load(&control->m_next, detail::mem_relaxed);
                            next = reinterpret_cast<ControlBlock*>(next_uint & ~detail::NbQueue_AllFlags);

                            /** Check if this element is ready to be consumed */
                            if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == 0)
                            {
                                if ((next_uint & ~detail::NbQueue_InvalidNextPage) != 0)
                                {
                                    is_empty = false;
                                    break;
                                }
                                else
                                {
                                    /* We have found a zeroed ControlBlock */
                                    DENSITY_TEST_ARTIFICIAL_DELAY;
                                    next = i_queue->m_head.load(mem_relaxed);
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
                            // page switch - we don't update next, we just fix the pinning and set control = next
                            i_queue->ALLOCATOR_TYPE::pin_page(next);
                            DENSITY_TEST_ARTIFICIAL_DELAY;
                            if (control != nullptr)
                                i_queue->ALLOCATOR_TYPE::unpin_page(control);
                            DENSITY_TEST_ARTIFICIAL_DELAY;
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
                    (it has the NbQueue_Dead flag). */
                void start_consume_impl(LFQueue_Head * i_queue) noexcept
                {
                    m_queue = i_queue;

                    auto control = m_control;
                    DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);
                    DENSITY_ASSERT_INTERNAL(address_is_aligned(control, Base::s_alloc_granularity));

                    auto next = i_queue->m_head.load(mem_relaxed);
                    DENSITY_TEST_ARTIFICIAL_DELAY;
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
                            auto const next_uint = raw_atomic_load(&control->m_next, detail::mem_relaxed);
                            DENSITY_TEST_ARTIFICIAL_DELAY;
                            next = reinterpret_cast<ControlBlock*>(next_uint & ~detail::NbQueue_AllFlags);

                            /** Check if this element is ready to be consumed */
                            if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == 0)
                            {
                                if ((next_uint & ~detail::NbQueue_InvalidNextPage) != 0)
                                {
                                    /* We try to set the flag NbQueue_Busy on it */
                                    auto expected = next_uint;
                                    if (raw_atomic_compare_exchange_strong(&control->m_next, &expected, next_uint | detail::NbQueue_Busy,
                                        detail::mem_acquire, detail::mem_relaxed))
                                    {
                                        m_next_ptr = next_uint | NbQueue_Dead;
                                        DENSITY_TEST_ARTIFICIAL_DELAY;
                                        break;
                                    }
                                }
                                else
                                {
                                    /* We have found a zeroed ControlBlock */
                                    DENSITY_TEST_ARTIFICIAL_DELAY;
                                    next = i_queue->m_head.load(mem_relaxed);
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
                            else if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Dead)
                            {
                                // clean up
                                cleanup_step(control, next_uint, next);
                            }
                        }
                        else if (next != nullptr)
                        {
                            // page switch - we don't update next, we just fix the pinning and update control to be = next
                            DENSITY_TEST_ARTIFICIAL_DELAY;
                            i_queue->ALLOCATOR_TYPE::pin_page(next);
                            if (control != nullptr)
                                i_queue->ALLOCATOR_TYPE::unpin_page(control);
                            DENSITY_TEST_ARTIFICIAL_DELAY;
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
                bool cleanup_step(ControlBlock * const i_control_block, uintptr_t const i_next_uint, ControlBlock * const i_next)
                {
                    DENSITY_TEST_ARTIFICIAL_DELAY;

                    auto expected = i_control_block;
                    if (m_queue->m_head.compare_exchange_strong(expected, i_next,
                        detail::mem_seq_cst, detail::mem_relaxed))
                    {
                        DENSITY_TEST_ARTIFICIAL_DELAY;

                        if (i_next_uint & detail::NbQueue_External)
                        {
                            auto const external_block = static_cast<ExternalBlock*>(
                                address_add(i_control_block, Base::s_element_min_offset));
                            m_queue->ALLOCATOR_TYPE::deallocate(external_block->m_block, external_block->m_size, external_block->m_alignment);
                        }

                        if (Base::s_deallocate_zeroed_pages)
                        {
                            raw_atomic_store(&i_control_block->m_next, 0);
                        }

                        DENSITY_TEST_ARTIFICIAL_DELAY;

                        if (Base::same_page(i_control_block, i_next))
                        {
                            if (Base::s_deallocate_zeroed_pages)
                            {
                                auto const memset_dest = const_cast<uintptr_t*>(&i_control_block->m_next) + 1;
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
                    auto head = i_queue->m_head.load();
                    if (head == nullptr)
                    {
                        auto const initial_page = i_queue->Base::get_initial_page();

                        DENSITY_TEST_ARTIFICIAL_DELAY;

                        /* If this CAS succeeds, we have to update our local variable head. Otherwise
                            after the call we have the value of m_head stored by another concurrent consumer. */
                        if (i_queue->m_head.compare_exchange_strong(head, initial_page))
                            head = initial_page;
                    }

                    DENSITY_ASSERT_INTERNAL(address_is_aligned(head, Base::s_alloc_granularity));
                    return head;
                }

                /** Commits a consumed element. After the call the Consume is empty. */
                void commit_consume_impl() noexcept
                {
                    DENSITY_TEST_ARTIFICIAL_DELAY;

                    DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);
                    DENSITY_ASSERT_INTERNAL(m_next_ptr != 0);

                    // we expect to have NbQueue_Busy and not NbQueue_Dead...
                    DENSITY_ASSERT_INTERNAL((raw_atomic_load(&m_control->m_next, detail::mem_relaxed) &
                        (detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

                    // remove NbQueue_Busy and add NbQueue_Dead
                    DENSITY_ASSERT_INTERNAL((m_next_ptr & (detail::NbQueue_Dead | detail::NbQueue_Busy | detail::NbQueue_InvalidNextPage)) == detail::NbQueue_Dead);
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
                        DENSITY_TEST_ARTIFICIAL_DELAY;

                        auto const next_uint = raw_atomic_load(&control->m_next);
                        auto const next = reinterpret_cast<ControlBlock*>(next_uint & ~detail::NbQueue_AllFlags);
                        if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) != detail::NbQueue_Dead)
                        {
                            // the element is not dead
                            break;
                        }

                        auto expected = control;
                        if (!m_queue->m_head.compare_exchange_strong(expected, next,
                            detail::mem_seq_cst, detail::mem_relaxed))
                        {
                            // another thread is advancing the head, give up
                            DENSITY_TEST_ARTIFICIAL_DELAY;
                            break;
                        }

                        if (next_uint & detail::NbQueue_External)
                        {
                            auto const external_block = static_cast<ExternalBlock*>(
                                address_add(control, Base::s_element_min_offset));
                            m_queue->ALLOCATOR_TYPE::deallocate(external_block->m_block, external_block->m_size, external_block->m_alignment);
                        }

                        bool const is_same_page = Base::same_page(control, next);
                        DENSITY_ASSERT_INTERNAL(!is_same_page == address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
                        DENSITY_ASSERT_INTERNAL(is_same_page == (control != Base::get_end_control_block(control)));

                        static_assert(offsetof(ControlBlock, m_next) == 0, "");
                        //std::memset(control, 0, address_diff(address_of_next, control));

                        if (DENSITY_LIKELY(is_same_page))
                        {
                            if (Base::s_deallocate_zeroed_pages)
                            {
                                DENSITY_TEST_ARTIFICIAL_DELAY;
                                raw_atomic_store(&control->m_next, 0);

                                DENSITY_TEST_ARTIFICIAL_DELAY;
                                auto const memset_dest = const_cast<uintptr_t*>(&control->m_next) + 1;
                                auto const memset_size = address_diff(next, memset_dest);
                                DENSITY_ASSERT_ALIGNED(memset_dest, alignof(uintptr_t));
                                DENSITY_ASSERT_UINT_ALIGNED(memset_size, alignof(uintptr_t));
                                std::memset(memset_dest, 0, memset_size);
                            }
                            control = next;
                        }
                        else
                        {
                            DENSITY_TEST_ARTIFICIAL_DELAY;
                            m_queue->ALLOCATOR_TYPE::pin_page(next);

                            #if DENSITY_DEBUG_INTERNAL
                                auto const updated_next_uint = raw_atomic_load(&control->m_next);
                                auto const updated_next = reinterpret_cast<ControlBlock*>(updated_next_uint & ~detail::NbQueue_AllFlags);
                                DENSITY_ASSERT_INTERNAL(updated_next == next);
                            #endif

                            if (Base::s_deallocate_zeroed_pages)
                            {
                                raw_atomic_store(&control->m_next, 0);
                            }

                            if (Base::s_deallocate_zeroed_pages)
                                m_queue->ALLOCATOR_TYPE::deallocate_page_zeroed(control);
                            else
                                m_queue->ALLOCATOR_TYPE::deallocate_page(control);

                            m_queue->ALLOCATOR_TYPE::unpin_page(control);
                            control = next;
                        }
                    }

                    m_control = control;
                }

                void cancel_consume_impl() noexcept
                {
                    DENSITY_TEST_ARTIFICIAL_DELAY;

                    DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);
                    DENSITY_ASSERT_INTERNAL(m_next_ptr != 0);

                    // we expect to have NbQueue_Busy and not NbQueue_Dead...
                    DENSITY_ASSERT_INTERNAL((raw_atomic_load(&m_control->m_next, detail::mem_relaxed) &
                        (detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

                    DENSITY_ASSERT_INTERNAL((m_next_ptr & (detail::NbQueue_Dead | detail::NbQueue_Busy | detail::NbQueue_InvalidNextPage)) == detail::NbQueue_Dead);
                    raw_atomic_store(&m_control->m_next, m_next_ptr - detail::NbQueue_Dead, detail::mem_seq_cst);
                    m_next_ptr = 0;

                    clean_dead_elements();
                }

            }; // Consume

        private: // data members
            alignas(concurrent_alignment) std::atomic<ControlBlock*> m_head{nullptr};
        };

    } // namespace detail

} // namespace density
