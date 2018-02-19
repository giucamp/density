
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace density
{
    namespace detail
    {
        template <typename BUSY_WAIT_FUNC>
            class SpinlockMutex : private BUSY_WAIT_FUNC
        {
        public:

            SpinlockMutex() noexcept
            {
                m_lock.clear();
            }

            SpinlockMutex(const BUSY_WAIT_FUNC & i_busy_wait)
                : BUSY_WAIT_FUNC(i_busy_wait)
            {
                m_lock.clear();
            }

            bool try_lock() noexcept
            {
                return !m_lock.test_and_set(mem_acquire);
            }

            void lock() noexcept
            {
                BUSY_WAIT_FUNC & busy_wait = *this;
                while (m_lock.test_and_set(mem_acquire))
                {
                    busy_wait();
                }
            }

            void unlock() noexcept
            {
                m_lock.clear(mem_release);
            }

            ~SpinlockMutex()
            {
                DENSITY_ASSERT_INTERNAL(m_lock.test_and_set() == false);
            }

        private:
            std::atomic_flag m_lock;
        };

        /** \internal Class template that implements put operations for spin-locking queues */
        template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE, typename BUSY_WAIT_FUNC>
            class SpQueue_TailMultiple : public LFQueue_Base<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, 
                SpQueue_TailMultiple<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, BUSY_WAIT_FUNC>>
        {
        public:

            using Base = LFQueue_Base<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE,
                SpQueue_TailMultiple<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, BUSY_WAIT_FUNC>>;
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
            constexpr static bool s_deallocate_zeroed_pages = false;

            constexpr static bool s_needs_end_control = true;

            constexpr SpQueue_TailMultiple() 
                    noexcept(std::is_nothrow_default_constructible<Base>::value)
                : m_tail(invalid_control_block()),
                  m_initial_page(nullptr)
            {
            }

            constexpr SpQueue_TailMultiple(ALLOCATOR_TYPE && i_allocator)
                    noexcept(std::is_nothrow_constructible<Base, ALLOCATOR_TYPE &&>::value)
                : Base(std::move(i_allocator)),
                  m_tail(invalid_control_block()),
                  m_initial_page(nullptr)
            {
            }

            SpQueue_TailMultiple(const ALLOCATOR_TYPE & i_allocator)
                : Base(i_allocator),
                  m_tail(invalid_control_block()),
                  m_initial_page(nullptr)
            {
            }

            SpQueue_TailMultiple(SpQueue_TailMultiple && i_source) noexcept
                : SpQueue_TailMultiple()
            {
                swap(i_source);
            }

            SpQueue_TailMultiple & operator = (SpQueue_TailMultiple && i_source) noexcept
            {
                SpQueue_TailMultiple::swap(i_source);
                return *this;
            }

            void swap(SpQueue_TailMultiple & i_other) noexcept
            {
                // swap the allocator
                using std::swap;
                swap(static_cast<ALLOCATOR_TYPE&>(*this), static_cast<ALLOCATOR_TYPE&>(i_other));

                // swap m_tail
                swap(m_tail, i_other.m_tail);

                // swap m_initial_page
                auto const tmp1 = i_other.m_initial_page.load();
                i_other.m_initial_page.store(m_initial_page.load());
                m_initial_page.store(tmp1);
            }

            ~SpQueue_TailMultiple()
            {
                if (m_tail != invalid_control_block())
                {
                    ALLOCATOR_TYPE::deallocate_page(m_tail);
                }
            }

            /** Allocates a block of memory.
                The block may be allocated in the pages or in a legacy memory block, depending on the size and the alignment.
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
                
                std::unique_lock<decltype(m_mutex)> lock(m_mutex, std::defer_lock);
                if (guarantee == LfQueue_Throwing || guarantee == LfQueue_Blocking)
                {
                    lock.lock();
                }
                else
                {
                    if (!lock.try_lock())
                        return Allocation{};
                }

                auto tail = m_tail;
                for (;;)
                {
                    DENSITY_ASSERT_INTERNAL(tail != nullptr && address_is_aligned(tail, s_alloc_granularity));

                    // allocate space for the control block (and possibly the runtime type)
                    void * address = address_add(tail, i_include_type ? s_element_min_offset : s_rawblock_min_offset);

                    // allocate space for the element
                    address = address_upper_align(address, i_alignment);
                    void * const user_storage = address;
                    address = address_add(address, i_size);
                    address = address_upper_align(address, s_alloc_granularity);
                    auto const new_tail = static_cast<ControlBlock*>(address);

                    // check for page overflow
                    auto const new_tail_offset = address_diff(new_tail, address_lower_align(tail, ALLOCATOR_TYPE::page_alignment));
                    if (DENSITY_LIKELY(new_tail_offset <= s_end_control_offset))
                    {
                        /* note: while control_block->m_next is zero, no consumers may ever read this
                        variable. So this does not need to be atomic store. */
                        //new_tail->m_next = 0;
                        /* edit: clang5 thread sanitizer has reported a data race between this write and the read:
                        auto const next_uint = raw_atomic_load(&control->m_next, mem_relaxed);
                        in start_consume_impl (detail\lf_queue_head_multiple.h).
                        Making the store atomic.... */
                        raw_atomic_store(&new_tail->m_next, uintptr_t(0));

                        auto const control_block = tail;
                        auto const next_ptr = reinterpret_cast<uintptr_t>(new_tail) + i_control_bits;
                        DENSITY_ASSERT_INTERNAL(raw_atomic_load(&control_block->m_next, mem_relaxed) == 0);
                        raw_atomic_store(&control_block->m_next, next_ptr, mem_release);

                        DENSITY_ASSERT_INTERNAL(control_block < get_end_control_block(tail));
                        m_tail = new_tail;
                        return { control_block, next_ptr, user_storage };
                    }
                    else if (i_size + (i_alignment - min_alignment) <= s_max_size_inpage) // if this allocation may fit in a page
                    {
                        tail = page_overflow(PROGRESS_GUARANTEE, tail);
                        if (guarantee != LfQueue_Throwing)
                        {
                            if (tail == 0)
                            {
                                return Allocation();
                            }
                        }
                        else
                        {
                            DENSITY_ASSERT_INTERNAL(tail != 0);
                        }
                        m_tail = tail;
                    }
                    else // this allocation would never fit in a page, allocate an external block
                    {
                        // legacy heap allocations can only be blocking 
                        if (guarantee == LfQueue_LockFree || guarantee == LfQueue_WaitFree)
                            return Allocation();
                        
                        lock.unlock(); // this avoids recursive locks
                        return Base::template external_allocate<PROGRESS_GUARANTEE>(i_control_bits, i_size, i_alignment);
                    }
                }
            }

            /** Overload of try_inplace_allocate_impl that can be used when all parameters are compile time constants */
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
                return m_initial_page.load();
            }

        private:

            /** Handles a page overflow of the tail. This function may allocate a new page.
                @param i_progress_guarantee progress guarantee. If the function can't provide this guarantee, the function fails
                @param i_tail the value read from m_tail. Note that other threads may have updated m_tail
                    in then meanwhile.
                @return the new tail, or nullptr in case of failure. */
            DENSITY_NO_INLINE ControlBlock * page_overflow(LfQueue_ProgressGuarantee i_progress_guarantee, ControlBlock * const i_tail)
            {
                auto const page_end = get_end_control_block(i_tail);
                if (i_tail < page_end)
                {
                    /* There is space between the (presumed) current tail and the end control block.
                        We try to pad it with a dead element. */

                    DENSITY_ASSERT_INTERNAL(m_tail == i_tail);
                    m_tail = i_tail;

                    auto const block = static_cast<ControlBlock*>(i_tail);
                    raw_atomic_store(&block->m_next, reinterpret_cast<uintptr_t>(page_end) + NbQueue_Dead, mem_release);
                    return page_end;
                }
                else
                {
                    // get or allocate a new page
                    DENSITY_ASSERT_INTERNAL(i_tail == page_end);
                    return get_or_allocate_next_page(i_progress_guarantee, i_tail);
                }
            }


            /** Tries to allocate a new page. Returns the new value of m_tail.
                @param i_progress_guarantee progress guarantee. If the function can't provide this guarantee, the function returns nullptr
                @param i_tail the value read from m_tail.
                @return an updated value of tail, that makes the current thread progress. */
            ControlBlock * get_or_allocate_next_page(LfQueue_ProgressGuarantee i_progress_guarantee, ControlBlock * const i_end_control)
            {
                DENSITY_ASSERT_INTERNAL(i_end_control != nullptr &&
                    address_is_aligned(i_end_control, s_alloc_granularity) &&
                    i_end_control == get_end_control_block(i_end_control));

                if (i_end_control != invalid_control_block())
                {
                    // allocate and setup a new page
                    auto new_page = create_page(i_progress_guarantee);
                    if (new_page == nullptr)
                    {
                        return nullptr;
                    }

                    raw_atomic_store(&i_end_control->m_next, reinterpret_cast<uintptr_t>(new_page) + NbQueue_Dead);

                    m_tail = new_page;

                    return m_tail;
                }
                else
                {
                    return create_initial_page(i_progress_guarantee);
                }
            }

            ControlBlock * create_initial_page(LfQueue_ProgressGuarantee i_progress_guarantee)
            {
                // m_initial_page = initial_page = create_page()
                auto const initial_page = create_page(i_progress_guarantee);
                if (initial_page == nullptr)
                {
                    return nullptr;
                }
                DENSITY_ASSERT_INTERNAL(m_initial_page.load() == nullptr);
                m_initial_page.store(initial_page);

                // m_tail = initial_page;
                DENSITY_ASSERT_INTERNAL(m_tail == invalid_control_block());
                m_tail = initial_page;

                return m_tail;
            }

            ControlBlock * create_page(LfQueue_ProgressGuarantee i_progress_guarantee)
            {
                auto const new_page = static_cast<ControlBlock *>(
                    i_progress_guarantee == LfQueue_Throwing ? ALLOCATOR_TYPE::allocate_page() :
                    ALLOCATOR_TYPE::try_allocate_page(ToDenGuarantee(i_progress_guarantee)) );
                if (new_page)
                {
                    auto const new_page_end_block = get_end_control_block(new_page);
                    raw_atomic_store(&new_page_end_block->m_next, uintptr_t(NbQueue_InvalidNextPage));

                    raw_atomic_store(&new_page->m_next, uintptr_t(0), mem_release);
                }
                return new_page;
            }

        private: // data members
            alignas(destructive_interference_size) SpinlockMutex<BUSY_WAIT_FUNC> m_mutex;
            ControlBlock * m_tail;
            std::atomic<ControlBlock*> m_initial_page;
        };

    } // namespace detail

} // namespace density
