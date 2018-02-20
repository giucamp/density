
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/raw_atomic.h>
#include <type_traits>

namespace density
{
    namespace detail
    {
        template<typename COMMON_TYPE> struct LfQueueControl // used by lf_heter_queue<T,...>
        {
            atomic_uintptr_t m_next; // raw atomic
            COMMON_TYPE * m_element;
        };

        template<> struct LfQueueControl<void> // used by lf_heter_queue<void,...>
        {
            atomic_uintptr_t m_next; // raw atomic
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
            LfQueue_WaitFree /**< maps to progress_wait_free, noexcept */
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

        template <typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE, typename DERIVED>
            class LFQueue_Base : public ALLOCATOR_TYPE
        {
        protected:

            using ControlBlock = LfQueueControl<COMMON_TYPE>;

            /** \internal This struct contains the result of a low-level allocation. An Allocation is empty
                if m_user_storage is nullptr. */
            struct Allocation
            {
                LfQueueControl<COMMON_TYPE> * m_control_block; /**< Control block of the allocated block */
                uintptr_t m_next_ptr; /**< Value of m_control_block->m_next */
                void * m_user_storage; /**< Pointer to the allocated space */

                Allocation() noexcept : m_user_storage(nullptr) {}

                Allocation(LfQueueControl<COMMON_TYPE> * i_control_block, uintptr_t i_next_ptr, void * i_user_storage) noexcept
                    : m_control_block(i_control_block), m_next_ptr(i_next_ptr), m_user_storage(i_user_storage) {}

                bool is_valid() const noexcept { return m_user_storage != nullptr; }
                bool is_empty() const noexcept { return m_user_storage == nullptr; }
            };

            /** Minimum alignment used for the storage of the elements.
                The storage of elements is always aligned according to the most-derived type. */
            constexpr static size_t min_alignment = alignof(void*); /* there are no particular requirements on
                                                                    the choice of this value: it just should be a very common alignment. */

            /** Head and tail pointers are alway multiple of this constant. To avoid the need of
                upper-aligning the addresses of the control-block and the runtime type, we raise it to the
                maximum alignment between ControlBlock and RUNTIME_TYPE (which are unlikely to be overaligned).
                The ControlBlock is always at offset 0 in the layout of a value or raw block. */
            constexpr static uintptr_t s_alloc_granularity = size_max(size_max(destructive_interference_size,
                alignof(ControlBlock), alignof(RUNTIME_TYPE), alignof(ExternalBlock)),
                min_alignment, size_log2(NbQueue_AllFlags + 1));

            /** Offset of the runtime_type in the layout of a value */
            constexpr static uintptr_t s_type_offset = uint_upper_align(sizeof(ControlBlock), alignof(RUNTIME_TYPE));

            /** Minimum offset of the element in the layout of a value (The actual offset is dependent on
                the alignment of the element). */
            constexpr static uintptr_t s_element_min_offset = uint_upper_align(s_type_offset + sizeof(RUNTIME_TYPE), min_alignment);

            /** Minimum offset of a row block. (The actual offset is dependent on the alignment of the block). */
            constexpr static uintptr_t s_rawblock_min_offset = uint_upper_align(sizeof(ControlBlock), size_max(min_alignment, alignof(ExternalBlock)));

            /** Offset from the beginning of the page of the end-control-block. */
            constexpr static uintptr_t s_end_control_offset = uint_lower_align(ALLOCATOR_TYPE::page_size - sizeof(ControlBlock), s_alloc_granularity);

            /** Maximum size for an element or raw block to be allocated in a page. */
            constexpr static size_t s_max_size_inpage = s_end_control_offset - s_element_min_offset;

            /** Value used to initialize the head and the tail.
                This value is designed to always cause a page overflow in the fast path.
                This mechanism allows the default constructor to be small, fast, and noexcept. */
            constexpr static uintptr_t s_invalid_control_block = s_end_control_offset;

            // some static checks
            static_assert(ALLOCATOR_TYPE::page_size > sizeof(ControlBlock) &&
                s_end_control_offset > 0 && s_end_control_offset > s_element_min_offset, "pages are too small");
            static_assert(is_power_of_2(s_alloc_granularity), "isn't destructive_interference_size a power of 2?");

            constexpr LFQueue_Base() noexcept(std::is_nothrow_default_constructible<ALLOCATOR_TYPE>::value)
		        : ALLOCATOR_TYPE()
            {
            }

            constexpr LFQueue_Base(ALLOCATOR_TYPE && i_allocator)
                    noexcept(std::is_nothrow_move_constructible<ALLOCATOR_TYPE>::value)
                : ALLOCATOR_TYPE(std::move(i_allocator))
            {
            }

            constexpr LFQueue_Base(const ALLOCATOR_TYPE & i_allocator)
                    noexcept(std::is_nothrow_copy_constructible<ALLOCATOR_TYPE>::value)
                : ALLOCATOR_TYPE(i_allocator)
            {
            }

            /** Returns whether the input addresses belong to the same page or they are both nullptr */
            static bool same_page(const void * i_first, const void * i_second) noexcept
            {
                auto const page_mask = ALLOCATOR_TYPE::page_alignment - 1;
                return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
            }

            /** Given an address, returns the end block of the page containing it. */
            static ControlBlock * get_end_control_block(void * i_address) noexcept
            {
                auto const page = address_lower_align(i_address, ALLOCATOR_TYPE::page_alignment);
                return static_cast<ControlBlock *>(address_add(page, s_end_control_offset));
            }
            static uintptr_t get_end_control_block(uintptr_t i_address) noexcept
            {
                auto const page = uint_lower_align(i_address, ALLOCATOR_TYPE::page_alignment);
                return page + s_end_control_offset;
            }

            static RUNTIME_TYPE * type_after_control(ControlBlock * i_control) noexcept
            {
                return static_cast<RUNTIME_TYPE*>(address_add(i_control, s_type_offset));
            }

            static void * get_unaligned_element(ControlBlock * i_control, bool i_is_external) noexcept
            {
                auto result = address_add(i_control, s_element_min_offset);
                if (i_is_external)
                {
                    /* i_control and s_element_min_offset are aligned to alignof(ExternalBlock), so
                        we don't need to align further */
                    result = static_cast<ExternalBlock*>(result)->m_block;
                }
                return result;
            }

            static void * get_element(detail::LfQueueControl<void> * i_control, bool i_is_external)
            {
                auto result = address_add(i_control, s_element_min_offset);
                if (i_is_external)
                {
                    /* i_control and s_element_min_offset are aligned to alignof(ExternalBlock), so
                        we don't need to align further */
                    result = static_cast<ExternalBlock*>(result)->m_block;
                }
                else
                {
                    result = address_upper_align(result, type_after_control(i_control)->alignment());
                }
                return result;
            }

            template <typename TYPE>
                static TYPE * get_element(detail::LfQueueControl<TYPE> * i_control, bool /*i_is_external*/)
            {
                return i_control->m_element;
            }

            static ControlBlock * invalid_control_block() noexcept
            {
                return reinterpret_cast<ControlBlock*>(s_invalid_control_block);
            }

            Allocation inplace_allocate(uintptr_t i_control_bits, bool i_include_type, size_t i_size, size_t i_alignment)
            {
                DENSITY_ASSERT_INTERNAL((i_control_bits & NbQueue_External) == 0); /* External blocks are
                    decided by the tail layers. The upper layers shouldn't use this flag. */
                return static_cast<DERIVED*>(this)->template try_inplace_allocate_impl<detail::
                    LfQueue_Throwing>(i_control_bits, i_include_type, i_size, i_alignment);
            }

            template <uintptr_t CONTROL_BITS, bool INCLUDE_TYPE, size_t SIZE, size_t ALIGNMENT>
                Allocation inplace_allocate()
            {
                static_assert((CONTROL_BITS & NbQueue_External) == 0, ""); /* External blocks are
                    decided by the tail layers. The upper layers shouldn't use this flag. */
                return static_cast<DERIVED*>(this)->template try_inplace_allocate_impl<detail::
                    LfQueue_Throwing, CONTROL_BITS, INCLUDE_TYPE, SIZE, ALIGNMENT>();
            }

            Allocation try_inplace_allocate(progress_guarantee i_progress_guarantee, uintptr_t i_control_bits, bool i_include_type, size_t i_size, size_t i_alignment) noexcept
            {
                DENSITY_ASSERT_INTERNAL((i_control_bits & NbQueue_External) == 0); /* External blocks are
                    decided by the tail layers. The upper layers shouldn't use this flag. */
                switch (i_progress_guarantee)
                {
                case progress_wait_free:
                    return static_cast<DERIVED*>(this)->template try_inplace_allocate_impl<detail::
                        LfQueue_WaitFree>(i_control_bits, i_include_type, i_size, i_alignment);

                case progress_lock_free:
                case progress_obstruction_free:
                    return static_cast<DERIVED*>(this)->template try_inplace_allocate_impl<detail::
                        LfQueue_LockFree>(i_control_bits, i_include_type, i_size, i_alignment);

                default:
                    DENSITY_ASSERT_INTERNAL(false); // fall-through to progress_blocking to avoid 'not all control paths return a value' warnings
                case progress_blocking:
                    return static_cast<DERIVED*>(this)->template try_inplace_allocate_impl<detail::
                        LfQueue_Blocking>(i_control_bits, i_include_type, i_size, i_alignment);
                }
            }

            /** Overload of inplace_allocate that can be used when all parameters are compile time constants */
            template <uintptr_t CONTROL_BITS, bool INCLUDE_TYPE, size_t SIZE, size_t ALIGNMENT>
                Allocation try_inplace_allocate(progress_guarantee i_progress_guarantee) noexcept
            {
                static_assert((CONTROL_BITS & NbQueue_External) == 0, ""); /* External blocks are
                    decided by the tail layers. The upper layers shouldn't use this flag. */
                switch (i_progress_guarantee)
                {
                case progress_wait_free:
                    return static_cast<DERIVED*>(this)->template try_inplace_allocate_impl<detail::
                        LfQueue_WaitFree, CONTROL_BITS, INCLUDE_TYPE, SIZE, ALIGNMENT>();

                case progress_lock_free:
                case progress_obstruction_free:
                    return static_cast<DERIVED*>(this)->template try_inplace_allocate_impl<detail::
                        LfQueue_LockFree, CONTROL_BITS, INCLUDE_TYPE, SIZE, ALIGNMENT>();

                default:
                    DENSITY_ASSERT_INTERNAL(false); // fall-through to progress_blocking to avoid 'not all control paths return a value' warnings
                case progress_blocking:
                    return static_cast<DERIVED*>(this)->template try_inplace_allocate_impl<detail::
                        LfQueue_Blocking, CONTROL_BITS, INCLUDE_TYPE, SIZE, ALIGNMENT>();
                }
            }

            /** Used by the put layers when the block can't be allocated in a page. */
            template <LfQueue_ProgressGuarantee PROGRESS_GUARANTEE>
                DENSITY_NO_INLINE Allocation external_allocate(uintptr_t i_control_bits, size_t i_size, size_t i_alignment)
                    noexcept(PROGRESS_GUARANTEE != LfQueue_Throwing)
            {
                auto guarantee = PROGRESS_GUARANTEE; // used to avoid warnings about constant conditional expressions
                DENSITY_ASSERT(guarantee == LfQueue_Throwing || guarantee == LfQueue_Blocking);

                void * external_block;
                if (guarantee == LfQueue_Throwing)
                {
                    external_block = ALLOCATOR_TYPE::allocate(i_size, i_alignment);
                }
                else
                {
                    external_block = ALLOCATOR_TYPE::try_allocate(i_size, i_alignment);
                    if (external_block == nullptr)
                    {
                        return Allocation{};
                    }
                }

                try
                {
                    /* external blocks always allocate space for the type, because it would be complicated
                        for the consumers to handle both cases*/
                    auto const inplace_put = static_cast<DERIVED*>(this)->template try_inplace_allocate_impl<PROGRESS_GUARANTEE>(
                        i_control_bits | NbQueue_External, true, sizeof(ExternalBlock), alignof(ExternalBlock));
                    if (inplace_put.m_user_storage == nullptr)
                    {
                        ALLOCATOR_TYPE::deallocate(external_block, i_size, i_alignment);
                        return Allocation{};
                    }
                    new(inplace_put.m_user_storage) ExternalBlock{external_block, i_size, i_alignment};
                    return Allocation{ inplace_put.m_control_block, inplace_put.m_next_ptr, external_block };
                }
                catch (...)
                {
                    /* if inplace_allocate fails, that means that we were able to allocate the external block,
                        but we were not able to put the struct ExternalBlock in the page (because a new page was
                        necessary, but we could not allocate it). */
                    ALLOCATOR_TYPE::deallocate(external_block, i_size, i_alignment);
                    DENSITY_INTERNAL_RETHROW_FROM_NOEXCEPT;
                }
            }

            /** Given a block with the 'busy' flag set and the 'dead' flag not set, removes the 'busy' flag.
                The member m_next_ptr of the argument must match the member m_next of the control block.
                The upper layers call this function to commit a put transaction.
                This function performs a release memory operation. */
            static void commit_put_impl(const Allocation & i_put) noexcept
            {
                // we expect to have NbQueue_Busy and not NbQueue_Dead
                DENSITY_ASSERT_INTERNAL(address_is_aligned(i_put.m_control_block, s_alloc_granularity));
                DENSITY_ASSERT_INTERNAL(
                    (i_put.m_next_ptr & ~NbQueue_AllFlags) == (raw_atomic_load(&i_put.m_control_block->m_next, mem_relaxed) & ~NbQueue_AllFlags) &&
                    (i_put.m_next_ptr & (NbQueue_Busy | NbQueue_Dead)) == NbQueue_Busy);

                // remove the flag NbQueue_Busy
                raw_atomic_store(&i_put.m_control_block->m_next, i_put.m_next_ptr - NbQueue_Busy, mem_release);
            }

            /** Given a block with the 'busy' flag set and the 'dead' flag not set, destroy the element and the
                runtime type. Then removes the 'busy' flag and adds the 'dead' flags.
                The upper layers call this function to cancel a put transaction.
                The member m_next_ptr of the argument must match the member m_next of the control block.
                This function performs a release memory operation. */
            static void cancel_put_impl(const Allocation & i_put) noexcept
            {
                // destroy the element and the type
                auto type_ptr = type_after_control(i_put.m_control_block);
                type_ptr->destroy(static_cast<COMMON_TYPE*>(i_put.m_user_storage));
                type_ptr->RUNTIME_TYPE::~RUNTIME_TYPE();

                cancel_put_nodestroy_impl(i_put);
            }

            /** Given a block with the 'busy' flag set and the 'dead' flag not set, removes the 'busy' flag and
                adds the 'dead' flags.
                The upper layers call this function to cancel a put transaction, after calling the destructor on
                the element being put and on the runtime type (if any).
                The member m_next_ptr of the argument must match the member m_next of the control block.
                This function performs a release memory operation. */
            static void cancel_put_nodestroy_impl(const Allocation & i_put) noexcept
            {
                // we expect to have NbQueue_Busy and not NbQueue_Dead
                DENSITY_ASSERT_INTERNAL(address_is_aligned(i_put.m_control_block, s_alloc_granularity));
                DENSITY_ASSERT_INTERNAL(
                    (i_put.m_next_ptr & ~NbQueue_AllFlags) == (raw_atomic_load(&i_put.m_control_block->m_next, mem_relaxed) & ~NbQueue_AllFlags) &&
                    (i_put.m_next_ptr & (NbQueue_Busy | NbQueue_Dead)) == NbQueue_Busy);

                // remove NbQueue_Busy and add NbQueue_Dead
                auto const addend = static_cast<uintptr_t>(NbQueue_Dead) - static_cast<uintptr_t>(NbQueue_Busy);
                raw_atomic_store(&i_put.m_control_block->m_next, i_put.m_next_ptr + addend, mem_release);
            }
        };

        /** \internal Class template that implements the put layer. The primary template is not defined. Partial
                specialization are defined in:
                    - lf_queue_tail_single.h
                    - lf_queue_tail_multiple_relaxed.h
                    - lf_queue_tail_multiple_seq_cst.h
                    - sp_queue_tail_multiple.h */
        template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE,
            concurrency_cardinality PROD_CARDINALITY, consistency_model CONSISTENCY_MODEL >
                class LFQueue_Tail;

        /** \internal Class template that implements the consume layer. The primary template is not defined. Partial
                specialization are defined in:
                    - lf_queue_head_single.h
                    - lf_queue_head_multiple.h */
        template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE,
            concurrency_cardinality CONSUMER_CARDINALITY, typename QUEUE_TAIL >
                class LFQueue_Head;

        enum PinResult
        {
            PinSuccessfull,
            AlreadyPinned,
            PinFailed
        };

        /** \internal Utility that provides RAII pinning\unpinning of a memory page */
        template <typename ALLOCATOR_TYPE, progress_guarantee PROGRESS_GUARANTEE>
            class PinGuard
        {
        private:
            ALLOCATOR_TYPE * const m_allocator;
            void * m_pinned_page = nullptr;

        public:
            PinGuard(ALLOCATOR_TYPE * i_allocator) noexcept
                : m_allocator(i_allocator)
            {
            }

            PinGuard(const PinGuard &) = delete;
            PinGuard & operator = (const PinGuard &) = delete;

            /** Trie to pin the page containing the provided addresss */
            PinResult pin_new(void * i_address) noexcept
            {
                auto const page = address_lower_align(i_address, ALLOCATOR_TYPE::page_alignment);
                if (page != m_pinned_page)
                {
                    if(ConstConditional(PROGRESS_GUARANTEE == progress_wait_free))
                    {
                        if (page != nullptr)
                        {
                            if(!m_allocator->try_pin_page(progress_wait_free, page))
                                return PinFailed;
                        }
                        if (m_pinned_page != nullptr)
                            m_allocator->unpin_page(progress_wait_free, m_pinned_page);
                    }
                    else
                    {
                        if (page != nullptr)
                            m_allocator->pin_page(page);
                        if (m_pinned_page != nullptr)
                            m_allocator->unpin_page(m_pinned_page);
                    }
                    m_pinned_page = page;
                    return PinSuccessfull;
                }
                else
                {
                    return AlreadyPinned;
                }
            }

            PinResult pin_new(uintptr_t i_address) noexcept
            {
                return pin_new(reinterpret_cast<void*>(i_address));
            }

            ~PinGuard()
            {
                if (m_pinned_page != nullptr)
                {
                    if (ConstConditional(PROGRESS_GUARANTEE == progress_wait_free))
                    {
                        m_allocator->unpin_page(progress_wait_free, m_pinned_page);
                    }
                    else
                    {
                        m_allocator->unpin_page(m_pinned_page);
                    }
                }
            }
        };

    } // namespace detail

} // namespace density
