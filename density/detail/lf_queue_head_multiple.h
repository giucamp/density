
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
        template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE, typename QUEUE_TAIL >
            class LFQueue_Head<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, concurrency_multiple, QUEUE_TAIL> : protected QUEUE_TAIL
        {
        private:
            using Base = QUEUE_TAIL;
            
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
                
            private:
                uintptr_t m_next_ptr = 0; /**< m_next member of the ControlBox of the element being consumed. The Consume is empty if and only if m_next_ptr == 0 */
            public:

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

                bool empty() const noexcept
                {
                    return m_next_ptr <= NbQueue_AllFlags;
                }

                bool external() const noexcept
                {
                    return (m_next_ptr & NbQueue_External) != 0;
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

                bool begin_iteration(LFQueue_Head * i_queue) noexcept
                {
                    DENSITY_ASSERT_ALIGNED(m_control, Base::s_alloc_granularity);

                    ControlBlock * head = i_queue->m_head.load();
                    DENSITY_ASSERT_ALIGNED(head, Base::s_alloc_granularity);

                    if (head == nullptr)
                    {
                        head = init_head(i_queue);
                        if (head == nullptr)
                        {
                            m_next_ptr = 0;
                            return true;
                        }
                    }

                    while (!Base::same_page(m_control, head))
                    {
                        DENSITY_ASSERT_INTERNAL(m_control != head);

                        i_queue->ALLOCATOR_TYPE::pin_page(head);

                        if (m_control != nullptr)
                        {
                            i_queue->ALLOCATOR_TYPE::unpin_page(m_control);
                        }

                        m_control = head;

                        head = i_queue->m_head.load();
                        DENSITY_ASSERT_INTERNAL(address_is_aligned(head, Base::s_alloc_granularity));
                    }

                    m_queue = i_queue;
                    m_control = static_cast<ControlBlock*>(head);
                    m_next_ptr = raw_atomic_load(&m_control->m_next, mem_relaxed);
                    return true;
                }

                /** Attaches this Consume to a queue, pinning the head. The previously pinned page is unpinned.
                    @return true if a page was pinned, false if the queue is virgin. */
                bool assign_queue(LFQueue_Head * i_queue) noexcept
                {
                    begin_iteration(i_queue);
                    return !empty();
                }

                bool is_queue_empty(const LFQueue_Head * i_queue) noexcept
                {
                    // we may do changes to the queue which are not observable from outside
                    return is_queue_empty(const_cast<LFQueue_Head *>(i_queue));
                }

                bool is_queue_empty(LFQueue_Head * i_queue) noexcept
                {
                    DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);

                    begin_iteration(i_queue);
                    while (!empty())
                    {
                        if ((m_next_ptr & (NbQueue_Busy | NbQueue_Dead | NbQueue_InvalidNextPage)) == 0)
                        {
                            return false;
                        }
                        move_next();
                    }
                    return true;
                }

                bool move_next() noexcept
                {
                    DENSITY_ASSERT_INTERNAL(address_is_aligned(m_control, Base::s_alloc_granularity));

                    auto next = reinterpret_cast<ControlBlock*>(m_next_ptr & ~NbQueue_AllFlags);
                    if (!Base::same_page(m_control, next))
                    {
                        DENSITY_ASSERT_INTERNAL(next != nullptr);
                        m_queue->ALLOCATOR_TYPE::pin_page(next);

                        auto const potentially_different_next_ptr = raw_atomic_load(&m_control->m_next, mem_relaxed);

                        m_queue->ALLOCATOR_TYPE::unpin_page(m_control);

                        if (potentially_different_next_ptr == 0)
                        {
                            /* the control block has been zeroed in the meanwhile, we have to restart */
                            m_control = next;
                            begin_iteration(m_queue);
                            return true;
                        }
                    }

                    m_control = next;
                    m_next_ptr = raw_atomic_load(&m_control->m_next, mem_relaxed);
                    return true;
                }

                /** Tries to start a consume operation. The Consume must be initially empty.
                    If there are no consumable elements, the Consume remains empty (m_next_ptr == 0).
                    Otherwise m_next_ptr is the value to set on the ControlBox to commit the consume
                    (it has the NbQueue_Dead flag). */
                void start_consume_impl(LFQueue_Head * i_queue) noexcept
                {
                    DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);

                    begin_iteration(i_queue);
                    while (!empty())
                    {
                        if ((m_next_ptr & (NbQueue_Busy | NbQueue_Dead | NbQueue_InvalidNextPage)) == 0)
                        {
                            /* We try to set the flag NbQueue_Busy on it */
                            if (raw_atomic_compare_exchange_strong(&m_control->m_next, &m_next_ptr, m_next_ptr | NbQueue_Busy,
                                mem_acquire, mem_relaxed))
                            {
                                m_next_ptr |= NbQueue_Dead;
                                break;
                            }
                        }
                        else if ( (m_next_ptr & (NbQueue_Busy | NbQueue_Dead)) == NbQueue_Dead)
                        {
                            advance_head();
                        }
                        move_next();
                    }
                }

                /** Commits a consumed element. After the call the Consume is empty. */
                void commit_consume_impl() noexcept
                {
                    DENSITY_ASSERT_INTERNAL(!empty());
                    DENSITY_ASSERT_INTERNAL((m_next_ptr & (NbQueue_Busy | NbQueue_Dead | NbQueue_InvalidNextPage)) == NbQueue_Dead);
                    DENSITY_ASSERT_INTERNAL(raw_atomic_load(&m_control->m_next, mem_relaxed) == m_next_ptr - NbQueue_Dead + NbQueue_Busy);
                    DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);

                    // remove NbQueue_Busy and add NbQueue_Dead
                    raw_atomic_store(&m_control->m_next, m_next_ptr, mem_seq_cst);
                    m_next_ptr = 0;

                    clean_dead_elements();
                }

                void cancel_consume_impl() noexcept
                {
                    DENSITY_ASSERT_INTERNAL(!empty());
                    DENSITY_ASSERT_INTERNAL((m_next_ptr & (NbQueue_Busy | NbQueue_Dead | NbQueue_InvalidNextPage)) == NbQueue_Dead);
                    DENSITY_ASSERT_INTERNAL(raw_atomic_load(&m_control->m_next, mem_relaxed) == m_next_ptr - NbQueue_Dead + NbQueue_Busy);
                    DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);

                    // remove NbQueue_Busy and add NbQueue_Dead
                    raw_atomic_store(&m_control->m_next, m_next_ptr - NbQueue_Dead, mem_seq_cst);
                    m_next_ptr = 0;
                }

                void clean_dead_elements() noexcept
                {
                    while ((m_next_ptr & (NbQueue_Busy | NbQueue_Dead)) == NbQueue_Dead)
                    {
                        advance_head();
                        move_next();
                    }
                }

                /** If m_head equals to m_control advance it to the next block, zeroing the memory.
                    This function assumes that the current block is dead. */
                bool advance_head()
                {
                    auto next = reinterpret_cast<ControlBlock*>(m_next_ptr & ~NbQueue_AllFlags);

                    auto expected = m_control;
                    if (m_queue->m_head.compare_exchange_strong(expected, next,
                        mem_seq_cst, mem_relaxed))
                    {
                        if (m_next_ptr & NbQueue_External)
                        {
                            auto const external_block = static_cast<ExternalBlock*>(
                                address_add(m_control, Base::s_element_min_offset));
                            m_queue->ALLOCATOR_TYPE::deallocate(external_block->m_block, external_block->m_size, external_block->m_alignment);
                        }

                        bool const is_same_page = Base::same_page(m_control, next);
                        DENSITY_ASSERT_INTERNAL(is_same_page != address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
                        DENSITY_ASSERT_INTERNAL(!ConstConditional(Base::s_needs_end_control) || is_same_page == (m_control != Base::get_end_control_block(m_control)));

                        static_assert(offsetof(ControlBlock, m_next) == 0, "");

                        if (is_same_page)
                        {
                            if (Base::s_deallocate_zeroed_pages)
                            {
                                raw_atomic_store(&m_control->m_next, uintptr_t(0));

                                auto const memset_dest = const_cast<atomic_uintptr_t*>(&m_control->m_next) + 1;
                                auto const memset_size = address_diff(next, memset_dest);
                                DENSITY_ASSERT_ALIGNED(memset_dest, alignof(uintptr_t));
                                DENSITY_ASSERT_UINT_ALIGNED(memset_size, alignof(uintptr_t));
                                std::memset(memset_dest, 0, memset_size);
                            }
                        }
                        else
                        {
                            /** the member m_next is zeroed even if s_deallocate_zeroed_pages is false, and before
                                deallocating the page, to allow a safe-pinning to the other consumers.
                                That is, if a consumers pins a page that is pointed by a m_next, and if after the pin
                                the m_next is still not zeroed, it can be sure that the allocator will not reuse the
                                page, even if it gets deallocated. If the consumer does not read again the member m_next
                                after pinning, it can't be sure that the page is recycled between the read of m_next and 
                                the pin. */
                            raw_atomic_store(&m_control->m_next, uintptr_t(0));

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

                /** Reads m_head. If it is still nullptr, tries to set it to the first page (if any) */
                static ControlBlock * init_head(LFQueue_Head * i_queue) noexcept
                {
                    // control and next are both null - initialize the head
                    auto head = i_queue->m_head.load();
                    if (head == nullptr)
                    {
                        auto const initial_page = i_queue->Base::get_initial_page();

                        /* If this CAS succeeds, we have to update our local variable head. Otherwise
                        after the call we have the value of m_head stored by another concurrent consumer. */
                        if (i_queue->m_head.compare_exchange_strong(head, initial_page))
                            head = initial_page;
                    }

                    DENSITY_ASSERT_INTERNAL(address_is_aligned(head, Base::s_alloc_granularity));
                    return head;
                }

            }; // Consume

        private: // data members
            alignas(destructive_interference_size) std::atomic<ControlBlock*> m_head{nullptr};
        };

    } // namespace detail

} // namespace density
