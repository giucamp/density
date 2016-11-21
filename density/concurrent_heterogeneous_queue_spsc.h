
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

namespace density
{
    namespace experimental
    {
        /**
            The queue always keep at least an allocated page. Therefore the constructor allocates a page. The reason is to allow
                producer to assume that the page in which the push is tried (the last one) doesn't get deallocated while the push
                is in progress.
                The consume algorithm uses hazard pointers to safely delete the pages. Pages are deleted immediately by consumers
                when no longer needed. Using the default (and recommended) allocator, deleted pages are added to a thread-local
                free-list. When the number of pages in this free-list exceeds a fixed number, a page is added in a global lock-free
                free-list. See void_allocator for details.
                See "Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects" of Maged M. Michael for details.
                There is no requirement on the type of the elements: they can be non-trivially movable, copyable and destructible.
                */
        template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator >
            class concurrent_heterogeneous_queue_spsc : private ALLOCATOR_TYPE
        {
        public:

            static_assert(std::is_same<COMMON_TYPE, typename RUNTIME_TYPE::common_type>::value,
                "COMMON_TYPE and RUNTIME_TYPE::common_type must be the same type (did you declare a type heter_cont<A,runtime_type<B>>?)");

            static_assert(std::is_same<COMMON_TYPE, void>::value, "Currently only fully-heterogeneous elements are supported");
                /* Reason: casting from a derived class is often a trivial operation (the value of the pointer does not change),
                    but sometimes it may require offsetting the pointer, or it may also require a memory indirection. Most containers
                    in this library store the result of this cast implicitly in the overhead data of the elements, but currently this container doesn't. */

            static_assert(ALLOCATOR_TYPE::page_size > sizeof(void*) * 8 && ALLOCATOR_TYPE::page_alignment == ALLOCATOR_TYPE::page_size,
                "The size and alignment of the pages must be the same (and not too small)");

            using allocator_type = ALLOCATOR_TYPE;
            using runtime_type = RUNTIME_TYPE;
            using value_type = COMMON_TYPE;
            using reference = typename std::add_lvalue_reference< COMMON_TYPE >::type;
            using const_reference = typename std::add_lvalue_reference< const COMMON_TYPE>::type;

            concurrent_heterogeneous_queue_spsc()
            {
                auto const first_page = allocator_type::allocate_page();
                m_tail_for_alloc.store(first_page);
                m_tail_for_consumers.store(first_page);
                m_head.store(reinterpret_cast<uintptr_t>(first_page));
            }

            /** Adds an element at the end of the queue. The operation may require the allocation of a new page.
                This operation is thread safe. The construction of an element can run in parallel with the construction of other
                elements and with the consumption of elements. Threads synchronizes only at the beginning of the push (before the
                constructor is invoked).
                @param i_source object to be used as source to construct of new element.
                    - If this argument is an l-value, the new element copy-constructed (and the source object is left unchanged).
                    - If this argument is an r-value, the new element move-constructed (and the source object will have an undefined but valid content).

                \n\b Requires:
                    - the type ELEMENT_TYPE must copy constructible (in case of l-value) or move constructible (in case of r-value)
                    - an ELEMENT_TYPE * must be implicitly convertible to COMMON_TYPE *
                    - an COMMON_TYPE * must be convertible to an ELEMENT_TYPE * with a static_cast or a dynamic_cast
                        (this requirement is not met for example if COMMON_TYPE is a non-polymorphic (direct or indirect) virtual
                        base of ELEMENT_TYPE).

                \n\b Throws: anything that the constructor of the new element throws
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
                \n\b Complexity: constant */
            template <typename ELEMENT_TYPE>
                DENSITY_STRONG_INLINE void push(ELEMENT_TYPE && i_source)
            {
                using ElementCompleteType = typename std::decay<ELEMENT_TYPE>::type;
                emplace<ElementCompleteType>(std::forward<ELEMENT_TYPE>(i_source));
            }

            void push_by_copy(runtime_type i_runtime_type, const void * i_source)
            {
                static_assert(element_fits_in_a_page(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE)),
                    "currently ELEMENT_TYPE must fit in a page");

                auto push_data = begin_push<true>(i_runtime_type.size(), i_runtime_type.alignment());

                // construct the type
                static_assert(new(push_data.type_ptr()) runtime_type(std::move(i_runtime_type)));
                new(push_data.type_ptr()) runtime_type(std::move(i_runtime_type));

                try
                {
                    // construct the element
                    new(push_data.m_element) COMPLETE_ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_args)...);
                }
                catch (...)
                {
                    // destroy the type (which is probably a no-operation)
                    push_data.type_ptr().RUNTIME_TYPE::~RUNTIME_TYPE();

                    // this call release the exclusive access and set the dead flag
                    cancel_push(push_data.m_control);

                    // the exception is propagated to the caller, whatever it is
                    throw;
                }

                commit_push(push_data);
            }


            template <typename COMPLETE_ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
                void emplace(CONSTRUCTION_PARAMS &&... i_args)
            {
                static_assert(std::is_convertible< COMPLETE_ELEMENT_TYPE*, COMMON_TYPE*>::value,
                    "ELEMENT_TYPE must be covariant to (i.e. must derive from) COMMON_TYPE, or COMMON_TYPE must be void");

                static_assert(element_fits_in_a_page(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE)),
                    "currently ELEMENT_TYPE must fit in a page");

                auto push_data = begin_push<true>(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE));

                // construct the type - This expression can throw
                static_assert(noexcept(RUNTIME_TYPE(RUNTIME_TYPE::template make<COMPLETE_ELEMENT_TYPE>())),
                    "both runtime_type::make and the move constructor of runtime_type must be noexcept");
                new(push_data.type_ptr()) RUNTIME_TYPE(RUNTIME_TYPE::template make<COMPLETE_ELEMENT_TYPE>());

                try
                {
                    // construct the element
                    new(push_data.m_element) COMPLETE_ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_args)...);
                }
                catch (...)
                {
                    // destroy the type (which is probably a no-operation)
                    push_data.type_ptr()->RUNTIME_TYPE::~RUNTIME_TYPE();

                    // this call release the exclusive access and set the dead flag
                    cancel_push(push_data.m_control);

                    // the exception is propagated to the caller, whatever it is
                    throw;
                }

                commit_push(push_data);
            }

            template <typename CONSUMER_FUNC>
                bool try_consume(CONSUMER_FUNC && i_consumer_func)
            {
                auto consume_data = begin_consume();
                if (consume_data.m_control != nullptr)
                {
                    auto scope_exit = detail::at_scope_exit([this, consume_data] () noexcept {
                        consume_data.type_ptr()->destroy(consume_data.element_ptr());
                        consume_data.type_ptr()->RUNTIME_TYPE::~RUNTIME_TYPE();
                        commit_consume(consume_data);
                    });
                    i_consumer_func(*consume_data.type_ptr(), consume_data.element_ptr());
                    return true;
                }
                else
                {
                    return false;
                }
            }

            template <typename CONSUMER_FUNC>
                bool try_consume_manual_align_destroy(CONSUMER_FUNC && i_consumer_func)
            {
                auto consume_data = begin_consume();
                if (consume_data.m_control != nullptr)
                {
                    auto scope_exit = detail::at_scope_exit([this, consume_data]() noexcept {
                        consume_data.type_ptr()->RUNTIME_TYPE::~RUNTIME_TYPE();
                        commit_consume(consume_data);
                    });
                    i_consumer_func(*consume_data.type_ptr(), consume_data.unaligned_element());
                    return true;
                }
                else
                {
                    return false;
                }
            }

            /** Returns a copy of the allocator instance owned by the queue.
                \n\b Throws: anything that the copy-constructor of the allocator throws
                \n\b Complexity: constant */
            allocator_type get_allocator() const
            {
                return *static_cast<alocator_type*>(this);
            }

            /** Returns a reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            allocator_type & get_allocator_ref() noexcept
            {
                return *static_cast<alocator_type*>(this);
            }

            /** Returns a const reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            const allocator_type & get_allocator_ref() const noexcept
            {
                return *static_cast<alocator_type*>(this);
            }

        private:

            /* Before each element there is a ControlBlock object. Since in the data member m_next the 2 least
                significant bit are used as flags, the address of a ControlBlock must be mutiple of 4.
                The alignas specifiers imply that this class is aligned at least at 4 bytes, but it may have
                a stricter (larger) alignment, probably 8 on 64-bit platforms (see http://en.cppreference.com/w/cpp/language/alignas). */
            struct alignas(4) alignas(sync::atomic<uintptr_t>) alignas(RUNTIME_TYPE) ControlBlock
            {
                uintptr_t m_next; /** pointer to the next control block, plus two additional flags encoded in the least-significant bits.
                                        bit 0: not used
                                        bit 1: dead element flag. The content of the element is not valid: it has been consumed,
                                        or the constructor threw an exception. Elements with this bit set don't require a the
                                        destructor to be called.
                                        The address of the next control block is given by:
                                        m_next & (std::numeric_limits<uintptr_t>::max() - 3). */
                RUNTIME_TYPE m_type; /** Type of the element. It usually has the same size of a pointer. */
            };

            static constexpr bool element_fits_in_a_page(size_t i_size, size_t i_alignment)
            {
                return i_size + i_alignment < (ALLOCATOR_TYPE::page_size - sizeof(ControlBlock) * 2);
            }

            static bool are_same_page(void * i_first, void * i_second)
            {
                return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) &
                    ~static_cast<uintptr_t>(ALLOCATOR_TYPE::page_alignment - 1)) == 0;
            }

            /** A CompareAndSwap (CAS) function based on compare_exchange_weak.
                The difference between compare_and_set_weak and compare_exchange_weak is that compare_and_set_weak takes the expected
                value by value. Therefore, in case of failure, the caller can't get the previous value of the atomic.
                The interface compare_exchange_weak is fine in many cases, but it is not very comfortable if we are not interested in
                the previous value of the atomic. */
            template <typename TYPE>
                DENSITY_STRONG_INLINE static bool compare_and_set_weak(sync::atomic<TYPE> & io_atomic, TYPE i_expected, TYPE i_set_to,
                    sync::memory_order i_success_memory_order) noexcept
            {
                return io_atomic.compare_exchange_weak(i_expected, i_set_to, i_success_memory_order, sync::hint_memory_order_relaxed);
            }

            struct PushData
            {
                ControlBlock * m_control;
                void * m_element;

                DENSITY_STRONG_INLINE void * element_ptr() const { return m_element; }

                DENSITY_STRONG_INLINE RUNTIME_TYPE * type_ptr() const
                {
                    return &m_control->m_type;
                }
            };

            /** Allocates space for a RUNTIME_TYPE and for an element, returning a pair of pointers to them.
                The caller should construct the type and the element, and then it should call commit_push().
                If an exception is thrown during the construction, cancel_push must be called.
                If an exception is thrown by begin_push, the call has no effect. */
            template <bool CAN_WAIT>
                PushData begin_push(size_t i_size, size_t i_alignment)
            {
                ControlBlock * control;
                void * new_element;
                void * tail;

                for (;;)
                {
                    tail = m_tail.load(sync::hint_memory_order_relaxed);

                    control = static_cast<ControlBlock*>(linear_alloc(tail, sizeof(ControlBlock)));
                    new_element = linear_alloc(tail, i_size, i_alignment > alignof(ControlBlock) ? i_alignment : alignof(ControlBlock));

                    /* Check for end of page. We need to make sure that not only the ControlBlock and the element fit in the page,
                       but also an extra ControlBlock, that eventually we use as link to the next page. */
                    auto const end_of_page = (reinterpret_cast<uintptr_t>(original_tail) | static_cast<uintptr_t>(ALLOCATOR_TYPE::page_alignment - 1)) + 1;
                    auto const limit = reinterpret_cast<void*>(end_of_page - sizeof(ControlBlock));
                    if (tail > limit)
                    {
                        /* There is no place to allocate another ControlBlock after the new element.
                        We must setup this ControlBock as link for a new page. */
                        handle_end_of_page(); // if this throws, no change is done in the queue
                        continue;
                    }
                }

                /* Now we can initialize control->m_next. Consumers are not allowed no read after m_tail, which we still did not update,
                    therefore the current page can't be deallocated. */
                control->m_next = reinterpret_cast<uintptr_t>(tail);

                /* Done. Now the caller can construct the type and the element concurrently with consumers and other producers.
                   The current page won't be deallocated until cancel_push or commit_push is called, because we have set the exclusive access
                   flag in control->m_next. */
                return PushData{ control, new_element };
            }

            /** Tries to allocate a new page. This operation may fail because many producer threads can try it concurrently, so they
                have to synchronize to avoid multiple allocations.
                Returns m_tail_for_alloc.load(sync::hint_memory_order_acquire) */
            DENSITY_NO_INLINE void handle_end_of_page()
            {
                // allocate the page - this may throw
                auto const new_page = allocate_page();
                DENSITY_ASSERT(address_is_aligned(new_page, ALLOCATOR_TYPE::page_alignment));

                // from now on nothing can throw

                // setup a ControlBox with the dead flag
                auto tail = m_tail.load(sync::hint_memory_order_relaxed);
                auto const control = static_cast<ControlBlock*>(tail);
                new (&control->m_type) RUNTIME_TYPE();
                control->m_next = reinterpret_cast<uintptr_t>(new_page) + 2;

                // now we can move the tail to the next page
                tail.store(new_page, sync::hint_memory_order_release);
            }

            /** Used when a begin_push has been called, but an exception was thrown during the construction of the element
                (or the type). This function marks the element as dead, and performs a release operation on m_tail_for_consumers. */
            void cancel_push(ControlBlock * i_control_block) noexcept
            {
                /* The second bit of m_size is set to 1, meaning that the state of the element is invalid */
                DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 0);
                i_control_block->m_next |= 2;

                /** Consumers should see what we have done (while producers are not interested). So we do a
                    release operation on m_tail_for_consumers. */
                m_tail.fetch_add(0, sync::hint_memory_order_release);
            }

            /** Used when a begin_push has been called, and both the type and the elements has been constructed. This
                function performs a release operation on m_tail_for_consumers. */
            void commit_push(PushData i_push_data) noexcept
            {
                /* decrementing the size we allow the consumers to process this element (this is an atomic operation)
                   To do: this is a read-write operation. Make it a write-only op. */
                DENSITY_ASSERT_INTERNAL((i_push_data.m_control->m_next & 3) == 1);
                i_push_data.m_control->m_next--;

                /** Consumers should see what we have done (while producers are not interested). So we do a
                    release operation on m_tail_for_consumers. */
                m_tail.fetch_add(0, sync::hint_memory_order_release);
            }

            struct ConsumeData
            {
                ControlBlock * m_control;

                DENSITY_STRONG_INLINE void * element_ptr() const
                {
                    return address_upper_align(m_control + 1, m_control->m_type.alignment());
                }

                DENSITY_STRONG_INLINE void * element_unaligned_ptr() const { return m_control + 1; }

                DENSITY_STRONG_INLINE const RUNTIME_TYPE * type_ptr() const
                {
                    return &m_control->m_type;
                }
            };

            ConsumeData begin_consume() noexcept
            {
                // Get exclusive access on m_head, setting it to zero
                auto head = m_head.load(sync::hint_memory_order_relaxed);

                // Check if we have reached the end of the queue
                auto const tail = m_tail.load(sync::hint_memory_order_acquire);
                if (reinterpret_cast<void*>(head) == tail)
                {
                    return ConsumeData{ nullptr };
                }

                /** Now we loop the elements from the head on, trying to get exclusive access on one. If we find a
                    dead element soon after the head, we obliterate it: that is, we move the head after it. */
                auto good_head = head; // this is the value of head we are going to set when we have finished
                for(;;)
                {


                    // Try to get exclusive access on the element, setting the first bit in m_next
                    auto const control = reinterpret_cast<ControlBlock*>(head);
                    auto const dirt_next = control->m_next.fetch_or(1, sync::hint_memory_order_relaxed);
                    if ((dirt_next & 1) == 0)
                    {
                        // we have the exclusive access on the element
                        bool const living_element = (dirt_next & 2) == 0;
                        if (living_element)
                        {
                            // release the head and return it
                            m_head.store(good_head, sync::hint_memory_order_release);
                            return ConsumeData{ control };
                        }
                        else
                        {
                            // This is a dead element. We can obliterate only if it is the first after the head
                            bool const can_obliterate = good_head == head;
                            if (can_obliterate)
                            {
                                DENSITY_ASSERT_INTERNAL( (dirt_next & 3) == 2 );
                                #if DENSITY_DEBUG_INTERNAL
                                    control->m_next.store(37);
                                #endif

                                // Check if this is a link ControlBlock, in which case we are leaving the page
                                if (control->m_type.empty())
                                {
                                    auto const page = reinterpret_cast<void*>(head & ~static_cast<uintptr_t>(ALLOCATOR_TYPE::page_alignment - 1));
                                    DENSITY_ASSERT(address_is_aligned(page, ALLOCATOR_TYPE::page_alignment));
                                    DENSITY_ASSERT(!are_same_page(page, reinterpret_cast<void*>(dirt_next - 2)));
                                    deallocate_page(page);
                                }
                                good_head = head = dirt_next - 2;
                                continue;
                            }
                            else
                            {
                                /* this is a dead element, but we can't move the head after it, because there are
                                    non-dead elements before it */
                                control->m_next.store(dirt_next, sync::hint_memory_order_release);
                            }
                        }
                    }

                    // Move to the next element, which may be in another page
                    head = dirt_next & ~static_cast<uintptr_t>(3);
                }
            }

            void commit_consume(ConsumeData i_consume_data) noexcept
            {
                #if DENSITY_DEBUG
                    memset(&(i_consume_data.m_control->m_type), 0xB4, sizeof(i_consume_data.m_control->m_type));
                #endif

                i_consume_data.m_control->m_next.fetch_xor(3, sync::hint_memory_order_relaxed);

                m_head.fetch_add(0, sync::hint_memory_order_release);
            }

        private:
            alignas(concurrent_alignment) sync::atomic<void*> m_tail;
            alignas(concurrent_alignment) sync::atomic<uintptr_t> m_head;
        };

    } // namespace experimental

} // namespace density

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
