
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace density
{
    namespace detail
    {
        /** \internal Class template that implements put operations */
        template <typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE>
        class LFQueue_Tail<
          COMMON_TYPE,
          RUNTIME_TYPE,
          ALLOCATOR_TYPE,
          concurrency_multiple,
          consistency_sequential>
            : public LFQueue_Base<
                COMMON_TYPE,
                RUNTIME_TYPE,
                ALLOCATOR_TYPE,
                LFQueue_Tail<
                  COMMON_TYPE,
                  RUNTIME_TYPE,
                  ALLOCATOR_TYPE,
                  concurrency_multiple,
                  consistency_sequential>>
        {
          public:
            using Base = LFQueue_Base<
              COMMON_TYPE,
              RUNTIME_TYPE,
              ALLOCATOR_TYPE,
              LFQueue_Tail<
                COMMON_TYPE,
                RUNTIME_TYPE,
                ALLOCATOR_TYPE,
                concurrency_multiple,
                consistency_sequential>>;
            using Base::cancel_put_impl;
            using Base::cancel_put_nodestroy_impl;
            using Base::commit_put_impl;
            using Base::external_allocate;
            using Base::get_element;
            using Base::get_end_control_block;
            using Base::get_unaligned_element;
            using Base::min_alignment;
            using Base::s_alloc_granularity;
            using Base::s_element_min_offset;
            using Base::s_end_control_offset;
            using Base::s_invalid_control_block;
            using Base::s_max_size_inpage;
            using Base::s_rawblock_min_offset;
            using Base::s_type_offset;
            using Base::same_page;
            using Base::type_after_control;
            using typename Base::Allocation;
            using typename Base::ControlBlock;

            /** Whether the head should zero the content of pages before deallocating. */
            constexpr static bool s_deallocate_zeroed_pages = false;

            /** Whether page switch happens only at the control block returned by get_end_control_block.
                Used only for assertions. */
            constexpr static bool s_needs_end_control = true;

            constexpr LFQueue_Tail() noexcept(std::is_nothrow_default_constructible<Base>::value)
                : m_tail(s_invalid_control_block), m_initial_page(nullptr)
            {
            }

            constexpr LFQueue_Tail(ALLOCATOR_TYPE && i_allocator) noexcept(
              std::is_nothrow_constructible<Base, ALLOCATOR_TYPE &&>::value)
                : Base(std::move(i_allocator)), m_tail(s_invalid_control_block),
                  m_initial_page(nullptr)
            {
            }

            constexpr LFQueue_Tail(const ALLOCATOR_TYPE & i_allocator) noexcept(
              std::is_nothrow_constructible<Base, const ALLOCATOR_TYPE &>::value)
                : Base(i_allocator), m_tail(s_invalid_control_block), m_initial_page(nullptr)
            {
            }

            LFQueue_Tail(LFQueue_Tail && i_source) noexcept(
              std::is_nothrow_default_constructible<
                Base>::value) // this matches the the default ctor
                : LFQueue_Tail()
            {
                static_assert(noexcept(swap(i_source)), "swap must be noexcept");
                swap(i_source);
            }

            LFQueue_Tail & operator=(LFQueue_Tail && i_source) noexcept
            {
                static_assert(noexcept(swap(i_source)), "swap must be noexcept");
                LFQueue_Tail::swap(i_source);
                return *this;
            }

            void swap(LFQueue_Tail & i_other) noexcept
            {
                // swap the allocator
                using std::swap;
                static_assert(
                  noexcept(swap(
                    static_cast<ALLOCATOR_TYPE &>(*this), static_cast<ALLOCATOR_TYPE &>(i_other))),
                  "swap must be noexcept");
                swap(static_cast<ALLOCATOR_TYPE &>(*this), static_cast<ALLOCATOR_TYPE &>(i_other));

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
                DENSITY_ASSERT(uint_is_aligned(tail, s_alloc_granularity)); // put in progress?
                if (tail != s_invalid_control_block)
                {
                    ALLOCATOR_TYPE::deallocate_page(reinterpret_cast<void *>(tail));
                }
            }

            /** Allocates a block of memory.
                The block may be allocated in the pages or in a legacy memory block, depending on the size and the alignment.
                    @tparam PROGRESS_GUARANTEE progress guarantee. If the function can't provide this guarantee, the function returns an empty Allocation
                    @param i_control_bits flags to add to the control block. Only LfQueue_Busy, LfQueue_Dead and LfQueue_External are supported
                    @param i_include_type true if this is an element value, false if it's a raw block
                    @param i_size it must a multiple of the alignment
                    @param i_alignment is must be > 0 and a power of two */
            template <LfQueue_ProgressGuarantee PROGRESS_GUARANTEE>
            Allocation try_inplace_allocate_impl(
              uintptr_t i_control_bits,
              bool      i_include_type,
              size_t    i_size,
              size_t    i_alignment) noexcept(PROGRESS_GUARANTEE != LfQueue_Throwing)
            {
                auto guarantee =
                  PROGRESS_GUARANTEE; // used to avoid warnings about constant conditional expressions

                DENSITY_ASSERT_INTERNAL(
                  (i_control_bits & ~(LfQueue_Busy | LfQueue_Dead | LfQueue_External)) == 0);
                DENSITY_ASSERT_INTERNAL(is_power_of_2(i_alignment) && (i_size % i_alignment) == 0);

                if (i_alignment < min_alignment)
                {
                    i_alignment = min_alignment;
                    i_size      = uint_upper_align(i_size, min_alignment);
                }

                auto const overhead = i_include_type ? s_element_min_offset : s_rawblock_min_offset;
                auto const required_size = overhead + i_size + (i_alignment - min_alignment);
                auto const required_units =
                  (required_size + (s_alloc_granularity - 1)) / s_alloc_granularity;

                // instantiate a pin-guard - we will use it only in case of contention
                PinGuard<ALLOCATOR_TYPE, ToDenGuarantee(PROGRESS_GUARANTEE)> scoped_pin(this);

                bool const fits_in_page =
                  required_units <
                  size_min(s_alloc_granularity, s_end_control_offset / s_alloc_granularity);
                if (fits_in_page)
                {
                    auto tail = m_tail.load(mem_relaxed);
                    for (;;)
                    {
                        static_assert(is_power_of_2(s_alloc_granularity), "");
                        auto const rest = tail & (s_alloc_granularity - 1);
                        if (rest == 0)
                        {
                            // we can try the allocation
                            auto const new_control = reinterpret_cast<ControlBlock *>(tail);
                            auto const future_tail = tail + required_units * s_alloc_granularity;
                            auto const page_start =
                              uint_lower_align(tail, ALLOCATOR_TYPE::page_alignment);
                            auto const future_tail_offset = future_tail - page_start;
                            auto       transient_tail     = tail + required_units;
#ifdef DENSITY_LOCKFREE_DEBUG
                            if (
                              tail == page_start &&
                              DENSITY_LIKELY(future_tail_offset <= s_end_control_offset))
#else
                            if (DENSITY_LIKELY(future_tail_offset <= s_end_control_offset))
#endif
                            {
                                DENSITY_ASSERT_INTERNAL(required_units < s_alloc_granularity);
                                if (m_tail.compare_exchange_weak(tail, transient_tail, mem_relaxed))
                                {
                                    raw_atomic_store(
                                      &new_control->m_next,
                                      future_tail + i_control_bits,
                                      mem_relaxed);

                                    m_tail.compare_exchange_strong(
                                      transient_tail, future_tail, mem_relaxed);

                                    auto const user_storage = address_upper_align(
                                      address_add(new_control, overhead), i_alignment);
                                    DENSITY_ASSERT_INTERNAL(
                                      reinterpret_cast<uintptr_t>(user_storage) + i_size <=
                                      future_tail);
                                    return Allocation{
                                      new_control, future_tail + i_control_bits, user_storage};
                                }
                                else
                                {
                                    if (guarantee == LfQueue_WaitFree)
                                    {
                                        return Allocation{};
                                    }
                                }
                            }
                            else
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
                        }
                        else
                        {
                            // the memory protection currently used (pinning) is based on an atomic increment, that is not wait-free
                            if (guarantee == LfQueue_WaitFree)
                            {
                                return Allocation{};
                            }

                            // an allocation is in progress, we help it
                            auto const clean_tail = tail - rest;
                            auto const incomplete_control =
                              reinterpret_cast<ControlBlock *>(clean_tail);
                            auto const next = clean_tail + rest * s_alloc_granularity;

                            auto const pin_result = scoped_pin.pin_new(incomplete_control);
                            if (pin_result != AlreadyPinned)
                            {
                                if (pin_result == PinFailed)
                                {
                                    return Allocation{};
                                }
                                auto updated_tail = m_tail.load(mem_relaxed);
                                if (updated_tail != tail)
                                {
                                    tail = updated_tail;
                                    continue;
                                }
                            }

                            // Note: NEEDS ZEROED-PAGES
                            uintptr_t expected_next = 0;
                            raw_atomic_compare_exchange_weak(
                              &incomplete_control->m_next,
                              &expected_next,
                              uintptr_t(next + LfQueue_Busy),
                              mem_relaxed);
                            if (m_tail.compare_exchange_weak(tail, next, mem_relaxed))
                            {
                                tail = next;
                            }
                        }
                    }
                }
                else
                {
                    // legacy heap allocations can only be blocking
                    if (guarantee == LfQueue_LockFree || guarantee == LfQueue_WaitFree)
                        return Allocation{};

                    return Base::template external_allocate<PROGRESS_GUARANTEE>(
                      i_control_bits, i_size, i_alignment);
                }
            }

            /** Overload of try_inplace_allocate_impl that can be used when all parameters are compile time constants */
            template <
              LfQueue_ProgressGuarantee PROGRESS_GUARANTEE,
              uintptr_t                 CONTROL_BITS,
              bool                      INCLUDE_TYPE,
              size_t                    SIZE,
              size_t                    ALIGNMENT>
            Allocation try_inplace_allocate_impl() noexcept(PROGRESS_GUARANTEE != LfQueue_Throwing)
            {
                static_assert(
                  (CONTROL_BITS & ~(LfQueue_Busy | LfQueue_Dead | LfQueue_External)) == 0, "");
                static_assert(is_power_of_2(ALIGNMENT) && (SIZE % ALIGNMENT) == 0, "");

                return try_inplace_allocate_impl<PROGRESS_GUARANTEE>(
                  CONTROL_BITS, INCLUDE_TYPE, SIZE, ALIGNMENT);
            }

            ControlBlock * get_initial_page() const noexcept { return m_initial_page.load(); }

          private:
            /** Handles a page overflow of the tail. This function may allocate a new page.
                @param i_progress_guarantee progress guarantee. If the function can't provide this guarantee, the function fails
                @param i_tail the value read from m_tail. Note that other threads may have updated m_tail
                    in then meanwhile.
                @return an updated value of tail that makes the current thread progress, or 0 in case of failure. */
            DENSITY_NO_INLINE uintptr_t
                              page_overflow(LfQueue_ProgressGuarantee i_progress_guarantee, uintptr_t const i_tail)
            {
                DENSITY_ASSERT_INTERNAL(uint_is_aligned(i_tail, s_alloc_granularity));

                // the memory protection currently used (pinning) is based on an atomic increment, that is not wait-free
                if (i_progress_guarantee == LfQueue_WaitFree)
                {
                    return 0;
                }

                auto const page_end = reinterpret_cast<uintptr_t>(
                  get_end_control_block(reinterpret_cast<void *>(i_tail)));
                if (i_tail < page_end)
                {
                    /* There is space between the (presumed) current tail and the end control block.
                        We try to pad it with a dead element. */
                    auto const uinits =
                      size_min((page_end - i_tail) / s_alloc_granularity, s_alloc_granularity - 1);
                    auto       expected_tail  = i_tail;
                    auto       transient_tail = i_tail + uinits;
                    auto const future_tail    = i_tail + uinits * s_alloc_granularity;
                    if (m_tail.compare_exchange_weak(
                          expected_tail, transient_tail, mem_relaxed, mem_relaxed))
                    {
                        // m_tail was successfully updated, now we can setup the padding element
                        auto const block = reinterpret_cast<ControlBlock *>(i_tail);
                        raw_atomic_store(&block->m_next, future_tail + LfQueue_Dead, mem_relaxed);
                        expected_tail = transient_tail;
                        if (m_tail.compare_exchange_strong(
                              expected_tail, future_tail, mem_relaxed, mem_relaxed))
                        {
                            return future_tail;
                        }
                    }

                    // we have in expected_tail an updated value of m_tail
                    return expected_tail;
                }
                else
                {
                    // get or allocate a new page
                    DENSITY_ASSERT_INTERNAL(i_tail == page_end);
                    return reinterpret_cast<uintptr_t>(get_or_allocate_next_page(
                      i_progress_guarantee, reinterpret_cast<ControlBlock *>(i_tail)));
                }
            }


            /** Tries to allocate a new page. In any case returns an update value of m_tail.
                @param i_progress_guarantee progress guarantee. If the function can't provide this guarantee, the function returns nullptr
                @param i_tail the value read from m_tail. Note that other threads may have updated m_tail
                    in then meanwhile.
                @return an updated value of tail that makes the current thread progress, or nullptr in case of failure */
            ControlBlock * get_or_allocate_next_page(
              LfQueue_ProgressGuarantee i_progress_guarantee, ControlBlock * const i_end_control)
            {
                DENSITY_ASSERT_INTERNAL(
                  i_end_control != 0 && address_is_aligned(i_end_control, s_alloc_granularity) &&
                  i_end_control == get_end_control_block(i_end_control));

                if (i_end_control != reinterpret_cast<ControlBlock *>(s_invalid_control_block))
                {
                    /* We are going to access the content of the end control, so we have to do a safe pin
                        (that is, pin the presumed tail, and then check if the tail has changed in the meanwhile). */
                    PinGuard<ALLOCATOR_TYPE, progress_lock_free> end_block(this);
                    auto const pin_result = end_block.pin_new(i_end_control);
                    if (pin_result == PinFailed)
                    {
                        return nullptr;
                    }
                    auto const updated_tail =
                      reinterpret_cast<ControlBlock *>(m_tail.load(mem_relaxed));
                    if (updated_tail != i_end_control)
                    {
                        return updated_tail;
                    }
                    // now the end control block is pinned, we can safely access it

                    // allocate and setup a new page
                    auto new_page = create_page(i_progress_guarantee);
                    if (new_page == nullptr)
                    {
                        return nullptr;
                    }

                    uintptr_t expected_next = LfQueue_InvalidNextPage;
                    if (!raw_atomic_compare_exchange_strong(
                          &i_end_control->m_next,
                          &expected_next,
                          reinterpret_cast<uintptr_t>(new_page) + LfQueue_Dead))
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

                        new_page =
                          reinterpret_cast<ControlBlock *>(expected_next & ~LfQueue_AllFlags);
                        DENSITY_ASSERT_INTERNAL(
                          new_page != nullptr &&
                          address_is_aligned(new_page, ALLOCATOR_TYPE::page_alignment));
                    }

                    auto expected_tail = reinterpret_cast<uintptr_t>(i_end_control);
                    if (m_tail.compare_exchange_strong(
                          expected_tail, reinterpret_cast<uintptr_t>(new_page)))
                        return new_page;
                    else
                        return reinterpret_cast<ControlBlock *>(expected_tail);
                }
                else
                {
                    return create_initial_page(i_progress_guarantee);
                }
            }

            DENSITY_NO_INLINE ControlBlock *
                              create_initial_page(LfQueue_ProgressGuarantee i_progress_guarantee)
            {
                // m_initial_page = initial_page = create_page()
                auto const first_page = create_page(i_progress_guarantee);
                if (first_page == nullptr)
                {
                    return nullptr;
                }

                /* note: in case of failure of the following CAS we do not give in even if we are wait-free,
                    because this is a oneshot operation, so we can't possibly stick in a loop. */
                ControlBlock * initial_page = nullptr;
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
                if (m_tail.compare_exchange_strong(tail, reinterpret_cast<uintptr_t>(initial_page)))
                {
                    return initial_page;
                }
                else
                {
                    return reinterpret_cast<ControlBlock *>(tail);
                }
            }

            ControlBlock * create_page(LfQueue_ProgressGuarantee i_progress_guarantee)
            {
                auto const new_page = static_cast<ControlBlock *>(
                  i_progress_guarantee == LfQueue_Throwing
                    ? ALLOCATOR_TYPE::allocate_page_zeroed()
                    : ALLOCATOR_TYPE::try_allocate_page_zeroed(
                        ToDenGuarantee(i_progress_guarantee)));
                if (new_page != nullptr)
                {
                    auto const new_page_end_block = get_end_control_block(new_page);
                    raw_atomic_store(
                      &new_page_end_block->m_next, uintptr_t(LfQueue_InvalidNextPage));
                }
                else
                {
                    if (i_progress_guarantee == LfQueue_Throwing)
                    {
                        throw std::bad_alloc();
                    }
                }
                return new_page;
            }

            void discard_created_page(ControlBlock * i_new_page) noexcept
            {
                auto const new_page_end_block = get_end_control_block(i_new_page);
                raw_atomic_store(&new_page_end_block->m_next, uintptr_t(0));
                ALLOCATOR_TYPE::deallocate_page_zeroed(i_new_page);
            }

          private: // data members
            alignas(destructive_interference_size) std::atomic<uintptr_t> m_tail;
            std::atomic<ControlBlock *> m_initial_page;
        };

    } // namespace detail

} // namespace density
