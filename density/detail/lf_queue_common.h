
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace density
{
    namespace detail
    {
        template<typename COMMON_TYPE> struct LfQueueControl // used by lf_heter_queue<T,...>
        {
            uintptr_t m_next; // raw atomic
            COMMON_TYPE * m_element;
        };

        template<> struct LfQueueControl<void> // used by lf_heter_queue<void,...>
        {
            uintptr_t m_next; // raw atomic
        };

        enum NbQueue_Flags : uintptr_t
        {
            NbQueue_Busy = 1, /**< set on LfQueueControl::m_next when a thread is producing or consuming an element */
            NbQueue_Dead = 2,  /**< set on LfQueueControl::m_next when an element is not consumable.
                               If NbQueue_Dead is set, then NbQueue_Busy is meaningless.
                               This flag is not revertible: once it is set, it can't be removed. */
            NbQueue_External = 4,  /**< set on LfQueueControl::m_next in case of external allocation */
            NbQueue_InvalidNextPage = 8,  /**< initial value for the pointer to the next page */
            NbQueue_AllFlags = NbQueue_Busy | NbQueue_Dead | NbQueue_External | NbQueue_InvalidNextPage
        };

        /** \internal Internally we do not distinguish between progress_lock_free and progress_obstruction_free, and furthermore
            in the implementation functions we need to know if we are inside a try function (and we can't throw) or
            inside a non-try function (possibly throwing, and always with blocking progress guarantee) */
        enum LfQueue_ProgressGuarantee
        {
            LfQueue_Throwing, /**< maps to progress_blocking, allows exceptions */
            LfQueue_Blocking, /**< maps to progress_blocking, noexcept */
            LfQueue_LockFree, /**< maps to progress_lock_free and progress_obstruction_free, noexcept */
            LfQueue_WaitFree /**< maps to progress_lock_free and progress_wait_free, noexcept */
        };

        constexpr LfQueue_ProgressGuarantee ToLfGuarantee(progress_guarantee i_progress_guarantee, bool i_can_throw)
        {
            return i_can_throw ? LfQueue_Throwing : (
                i_progress_guarantee == progress_blocking ? LfQueue_Blocking : (
                (i_progress_guarantee == progress_lock_free || i_progress_guarantee == progress_obstruction_free) ? LfQueue_LockFree : LfQueue_WaitFree
            ));
        }

        constexpr progress_guarantee ToDenGuarantee(LfQueue_ProgressGuarantee i_progress_guarantee)
        {
            return (i_progress_guarantee == LfQueue_Throwing || i_progress_guarantee == LfQueue_Blocking) ? progress_blocking : (
                i_progress_guarantee == LfQueue_LockFree ? progress_lock_free : progress_wait_free
            );
        }

        /** \internal Class template that implements the low-level interface for put transactions */
        template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE,
            concurrency_cardinality PROD_CARDINALITY, consistency_model CONSISTENCY_MODEL >
                class LFQueue_Tail;

        /** \internal Class template that implements the low-level interface for consume operations */
        template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE,
            concurrency_cardinality CONSUMER_CARDINALITY, typename QUEUE_TAIL >
                class LFQueue_Head;

        /** \internal This struct contains the result of a low-level allocation. */
        template<typename COMMON_TYPE> struct LfBlock
        {
            LfQueueControl<COMMON_TYPE> * m_control_block;
            uintptr_t m_next_ptr;
            void * m_user_storage;

            LfBlock() noexcept : m_user_storage(nullptr) {}

            LfBlock(LfQueueControl<COMMON_TYPE> * i_control_block, uintptr_t i_next_ptr, void * i_user_storage) noexcept
                : m_control_block(i_control_block), m_next_ptr(i_next_ptr), m_user_storage(i_user_storage) {}
        };

    } // namespace detail

} // namespace density
