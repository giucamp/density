
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace density
{
    namespace detail
    {
        /** \internal Partial specialization of LFQueue_Tail for multi-threaded producers, with no sequential consistency. */
        template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE>
            class LFQueue_Tail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, concurrency_multiple, consistency_relaxed>
                : public LFQueue_Base<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, 
                    LFQueue_Tail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, concurrency_multiple, consistency_relaxed> >
        {
        public:

            using Base = LFQueue_Base<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE,
                LFQueue_Tail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, concurrency_multiple, consistency_relaxed> >;
            using typename Base::ControlBlock;
            using typename Base::Allocation;
            using Base::same_page;
            using Base::min_alignment;
            using Base::s_alloc_granularity;
            using Base::s_type_offset;
            using Base::s_element_min_offset;
            using Base::s_rawblock_min_offset;
            using Base::s_end_control_offset;
            using Base::s_max_size_inpage;
            using Base::s_invalid_control_block;
            using Base::get_end_control_block;
            using Base::type_after_control;
            using Base::get_unaligned_element;
            using Base::get_element;
            using Base::invalid_control_block;
            using Base::commit_put_impl;
            using Base::cancel_put_impl;
            using Base::cancel_put_nodestroy_impl;
            using Base::external_allocate;

            /** Whether the head should zero the content of pages before deallocating. */
            constexpr static bool s_deallocate_zeroed_pages = true;

            /** Whether page switch happens only at the control block returned by get_end_control_block.
                Used only for assertions. */
            constexpr static bool s_needs_end_control = true;

            constexpr LFQueue_Tail() 
                    noexcept(std::is_nothrow_default_constructible<Base>::value)
                : m_tail(s_invalid_control_block),
                  m_initial_page(0)
            {
            }

            constexpr LFQueue_Tail(ALLOCATOR_TYPE && i_allocator)
                    noexcept(std::is_nothrow_constructible<Base, ALLOCATOR_TYPE &&>::value)
                : Base(std::move(i_allocator)),
                  m_tail(s_invalid_control_block),
                  m_initial_page(0)
            {
            }

            constexpr LFQueue_Tail(const ALLOCATOR_TYPE & i_allocator)
                    noexcept(std::is_nothrow_constructible<Base, const ALLOCATOR_TYPE &>::value)
                : Base(i_allocator),
                  m_tail(s_invalid_control_block),
                  m_initial_page(0)
            {
            }

            LFQueue_Tail(LFQueue_Tail && i_source)
                    noexcept(std::is_nothrow_default_constructible<Base>::value) // this matches the the default ctor
                : LFQueue_Tail()
            {
                static_assert(noexcept(swap(i_source)), "swap must be noexcept");
                swap(i_source);
            }

            LFQueue_Tail & operator = (LFQueue_Tail && i_source) noexcept
            {
                static_assert(noexcept(swap(i_source)), "swap must be noexcept");
                LFQueue_Tail::swap(i_source);
                return *this;
            }

            void swap(LFQueue_Tail & i_other) noexcept
            {
                // swap the allocator
                using std::swap;
                static_assert(noexcept(
                    swap(static_cast<ALLOCATOR_TYPE&>(*this), static_cast<ALLOCATOR_TYPE&>(i_other))
                    ), "swap must be noexcept");
                swap(static_cast<ALLOCATOR_TYPE&>(*this), static_cast<ALLOCATOR_TYPE&>(i_other));

                // swap m_tail
                auto const tmp = i_other.m_tail.load();
                i_other.m_tail.store(m_tail.load());
                m_tail.store(tmp);

                // swap m_initial_page
                auto const tmp1 = i_other.m_initial_page.load();
                i_other.m_initial_page.store(m_initial_page.load());
                m_initial_page.store(tmp1);
            }

            ~LFQueue_Tail()
            {
                auto const tail = m_tail.load();
                if (tail != s_invalid_control_block)
                {
                    ALLOCATOR_TYPE::deallocate_page(reinterpret_cast<void*>(tail));
                }
            }

            /** Allocates a block of memory.
                The block may be allocated in the pages or in a legacy memory block, depending on the size and the alignment.
                    @tparam PROGRESS_GUARANTEE progress guarantee. If the function can't provide this guarantee, the function returns an empty Allocation
                    @param i_control_bits flags to add to the control block. Only NbQueue_Busy, NbQueue_Dead and NbQueue_External are supported
                    @param i_include_type true if this is an element value, false if it's a raw allocation
                    @param i_size it must a multiple of the alignment
                    @param i_alignment is must be > 0 and a power of two */
            template<LfQueue_ProgressGuarantee PROGRESS_GUARANTEE>
                Allocation try_inplace_allocate_impl(uintptr_t i_control_bits, bool i_include_type, size_t i_size, size_t i_alignment)
                    noexcept(PROGRESS_GUARANTEE != LfQueue_Throwing)
            {
                auto guarantee = PROGRESS_GUARANTEE; // used to avoid warnings about constant conditional expressions

                DENSITY_ASSERT_INTERNAL((i_control_bits & ~(NbQueue_Busy | NbQueue_Dead | NbQueue_External)) == 0);
                DENSITY_ASSERT_INTERNAL(is_power_of_2(i_alignment) && (i_size % i_alignment) == 0);

                if (i_alignment < min_alignment)
                {
                    i_alignment = min_alignment;
                    i_size = uint_upper_align(i_size, min_alignment);
                }

                auto tail = m_tail.load(mem_relaxed);
                for (;;)
                {
                    DENSITY_ASSERT_INTERNAL(tail != 0 && uint_is_aligned(tail, s_alloc_granularity));

                    // allocate space for the control block (and possibly the runtime type)
                    auto new_tail = tail + (i_include_type ? s_element_min_offset : s_rawblock_min_offset);

                    // allocate the user space
                    new_tail = uint_upper_align(new_tail, i_alignment);
                    auto const user_storage = reinterpret_cast<void*>(new_tail);
                    new_tail = uint_upper_align(new_tail + i_size, s_alloc_granularity);

                    // check for page overflow
                    auto const page_start = uint_lower_align(tail, ALLOCATOR_TYPE::page_alignment);
                    DENSITY_ASSERT_INTERNAL(new_tail > page_start);
                    auto const new_tail_offset = new_tail - page_start;
                    if (DENSITY_LIKELY(new_tail_offset <= s_end_control_offset))
                    {
                        /* No page overflow occurs with the new tail we have computed. */
                        if (m_tail.compare_exchange_weak(tail, new_tail, mem_relaxed, mem_relaxed))
                        {
                            /* At this point this thread has truncated the queue, because it has allocated
                               an element, but the member m_next of its control point is still zeroed (the
                               initial content of a newly allocated page).
                               Other threads may still put elements, but they will be not visible to the consumers
                               until the next store is completed. This is one of the reasons why this class 
                               is not sequential consistent. 
                               
                               The next store is safe because the zeroed block is a barrier that consumers will
                               not get over, so this page can't be deallocated. If this block does not have the 
                               dead 'flag', the access to this page is safe until the element is commited or canceled. */

                            // initialize the control block
                            auto const new_block = reinterpret_cast<ControlBlock*>(tail);
                            DENSITY_ASSERT_INTERNAL(raw_atomic_load(&new_block->m_next, mem_relaxed) == 0);
                            auto const next_ptr = new_tail + i_control_bits;
                            raw_atomic_store(&new_block->m_next, next_ptr, mem_relaxed);

                            // succesfull
                            DENSITY_ASSERT_INTERNAL(new_block < get_end_control_block(new_block));
                            return { new_block, next_ptr, user_storage };
                        }
                        else
                        {
                            if (guarantee == LfQueue_WaitFree)
                            {
                                // don't retry
                                return Allocation{};
                            }
                        }
                    }
                    else if (i_size + (i_alignment - min_alignment) <= s_max_size_inpage) // if this allocation may fit in a page
                    {
                        tail = page_overflow(guarantee, tail);
                        if (guarantee != LfQueue_Throwing)
                        {
                            if (tail == 0)
                            {
                                return Allocation{};
                            }
                        }
                        else
                        {
                            DENSITY_ASSERT_INTERNAL(tail != 0);
                        }
                    }
                    else // this allocation would never fit in a page, allocate an external block
                    {
                        // legacy heap allocations can only be blocking 
                        if (guarantee == LfQueue_LockFree || guarantee == LfQueue_WaitFree)
                            return Allocation{};
                        
                        return Base::template external_allocate<PROGRESS_GUARANTEE>(i_control_bits, i_size, i_alignment);
                    }
                }
            }

            /** Overload of inplace_allocate that can be used when all parameters are compile time constants */
            template <LfQueue_ProgressGuarantee PROGRESS_GUARANTEE, uintptr_t CONTROL_BITS, bool INCLUDE_TYPE, size_t SIZE, size_t ALIGNMENT>
                Allocation try_inplace_allocate_impl()
                    noexcept(PROGRESS_GUARANTEE != LfQueue_Throwing)
            {
                static_assert((CONTROL_BITS & ~(NbQueue_Busy | NbQueue_Dead | NbQueue_External)) == 0, "");
                static_assert(is_power_of_2(ALIGNMENT) && (SIZE % ALIGNMENT) == 0, "");

                return try_inplace_allocate_impl<PROGRESS_GUARANTEE>(CONTROL_BITS, INCLUDE_TYPE, SIZE, ALIGNMENT);
            }

            /** This function is used by the consume layer to initialize the head on the first allocated page*/
            ControlBlock * get_initial_page() const noexcept
            {
                return reinterpret_cast<ControlBlock *>(m_initial_page.load());
            }

        private:

            /** Handles a page overflow of the tail. This function may allocate a new page.
                @param i_progress_guarantee progress guarantee. If the function can't provide this guarantee, the function fails
                @param i_tail the value read from m_tail. Note that other threads may have updated m_tail
                    in then meanwhile.
                @return an updated value of tail that makes the current thread progress, or nullptr in case of failure to 
                    allocate a page. */
            DENSITY_NO_INLINE uintptr_t page_overflow(LfQueue_ProgressGuarantee i_progress_guarantee, uintptr_t i_tail)
            {
                auto const page_end = get_end_control_block(i_tail);
                if (i_tail < page_end)
                {
                    /* There is space between the (presumed) current tail and the end control block.
                        We try to pad it with a dead element. */
                    auto expected_tail = i_tail;
                    if (m_tail.compare_exchange_weak(expected_tail, page_end, mem_relaxed, mem_relaxed))
                    {
                        // m_tail was successfully updated, now we can setup the padding element
                        auto const block = reinterpret_cast<ControlBlock*>(i_tail);
                        raw_atomic_store(&block->m_next, page_end + NbQueue_Dead, mem_release);
                        return page_end;
                    }
                    else
                    {
                        // failed to allocate the padding, reenter the main loop
                        return expected_tail;
                    }
                }
                else
                {
                    // get or allocate a new page
                    DENSITY_ASSERT_INTERNAL(i_tail == page_end);
                    return get_or_allocate_next_page(i_progress_guarantee, i_tail);
                }
            }


            /** Tries to allocate a new page. In any case returns an update value of m_tail.
                @param i_progress_guarantee progress guarantee. If the function can't provide this guarantee, the function returns nullptr
                @param i_tail the value read from m_tail. Note that other threads may have updated m_tail
                    in then meanwhile.
                @return an updated value of tail that makes the current thread progress, or 0 in case of failure. */
            uintptr_t get_or_allocate_next_page(LfQueue_ProgressGuarantee const i_progress_guarantee, uintptr_t i_end_control)
            {
                DENSITY_ASSERT_INTERNAL(i_end_control != 0 &&
                    uint_is_aligned(i_end_control, s_alloc_granularity) &&
                    i_end_control == get_end_control_block(i_end_control));

                if (i_end_control != s_invalid_control_block)
                {
                    /* We are going to access the content of the end control, so we have to do a safe pin
                        (that is, pin the presumed tail, and then check if the tail has changed in the meanwhile). */

                    // try pin
                    PinGuard<ALLOCATOR_TYPE, progress_lock_free> end_block(this);
                    auto const pin_result = end_block.pin_new(i_end_control);
                    if (pin_result == PinFailed) // the pinning can fail only in wait-freedom
                    {
                        return 0;
                    }

                    // check if the tail has changed in the meanwhile
                    auto const updated_tail = m_tail.load(mem_relaxed);
                    if (updated_tail != i_end_control)
                    {
                        return updated_tail;
                    }
                    // now the end control block is pinned, we can safely access it

                    // allocate and setup a new page
                    auto new_page = create_page(i_progress_guarantee);
                    if (new_page == 0)
                    {
                        return 0;
                    }

                    auto const end_control = reinterpret_cast<ControlBlock*>(i_end_control);
                    uintptr_t expected_next = NbQueue_InvalidNextPage;
                    if (!raw_atomic_compare_exchange_strong(&end_control->m_next, &expected_next,
                        new_page + NbQueue_Dead))
                    {
                        /* Some other thread has already linked a new page. We discard the page we
                            have just allocated. */
                        discard_created_page(new_page);

                        /* So i_end_control->m_next may now be the pointer to the next page
                            or 0 (if the page has been consumed in the meanwhile). */
                        if (expected_next == 0)
                        {
                            return updated_tail;
                        }

                        new_page = expected_next & ~NbQueue_AllFlags;
                        DENSITY_ASSERT_INTERNAL(new_page != 0 && uint_is_aligned(new_page, ALLOCATOR_TYPE::page_alignment));
                    }

                    auto expected_tail = i_end_control;
                    if (m_tail.compare_exchange_strong(expected_tail, new_page))
                        return new_page;
                    else
                        return expected_tail;
                }
                else
                {
                    return create_initial_page(i_progress_guarantee);
                }
            }

            DENSITY_NO_INLINE uintptr_t create_initial_page(LfQueue_ProgressGuarantee const i_progress_guarantee)
            {
                // m_initial_page = initial_page = create_page()
                auto const first_page = create_page(i_progress_guarantee);
                if (first_page == 0)
                {
                    return 0;
                }

                /* note: in case of failure of the following CAS we do not give in even if we are wait-free,
                    because this is a oneshot operation, so we can't possibly stick in a loop. */
                uintptr_t initial_page = 0;
                if (m_initial_page.compare_exchange_strong(initial_page, first_page))
                {
                    initial_page = first_page;
                }
                else
                {
                    discard_created_page(first_page);
                }

                // m_tail = initial_page;
                auto tail = s_invalid_control_block;
                if (m_tail.compare_exchange_strong(tail, initial_page))
                {
                    return initial_page;
                }
                else
                {
                    return tail;
                }
            }

            uintptr_t create_page(LfQueue_ProgressGuarantee const i_progress_guarantee)
            {
                auto const new_page = static_cast<ControlBlock*>(
                    i_progress_guarantee == LfQueue_Throwing ? ALLOCATOR_TYPE::allocate_page_zeroed() :
                    ALLOCATOR_TYPE::try_allocate_page_zeroed(ToDenGuarantee(i_progress_guarantee)) );
                if (new_page != nullptr)
                {
                    auto const new_page_end_block = get_end_control_block(new_page);
                    raw_atomic_store(&new_page_end_block->m_next, uintptr_t(NbQueue_InvalidNextPage));
                }
                else
                {
                    if (i_progress_guarantee == LfQueue_Throwing)
                    {
                        throw std::bad_alloc();
                    }
                }
                return reinterpret_cast<uintptr_t>(new_page);
            }

            void discard_created_page(uintptr_t i_new_page) noexcept
            {
                auto const new_page = reinterpret_cast<void*>(i_new_page);
                auto const new_page_end_block = get_end_control_block(new_page);
                raw_atomic_store(&new_page_end_block->m_next, uintptr_t(0));
                ALLOCATOR_TYPE::deallocate_page_zeroed(new_page);
            }

        private: // data members
            alignas(destructive_interference_size) std::atomic<uintptr_t> m_tail;
            std::atomic<uintptr_t> m_initial_page;
        };

    } // namespace detail

} // namespace density
