
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/raw_atomic.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>
#include <type_traits>
#include <limits>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

#include <density/detail/lf_queue_common.h>
#include <density/detail/lf_queue_tail_single.h>
#include <density/detail/lf_queue_tail_multiple_relaxed.h>
#include <density/detail/lf_queue_tail_multiple_seq_cst.h>
#include <density/detail/lf_queue_head_single.h>
#include <density/detail/lf_queue_head_multiple.h>

namespace density
{
    /** Concurrent lock-free heterogeneous FIFO container-like class template. lf_heter_queue is a concurrent version
        of heter_queue that uses lock free algorithms for both put transactions and put operations.

        @tparam COMMON_TYPE Common type of all the elements. An object of type E can be pushed on the queue only if E* is
            implicitly convertible to COMMON_TYPE*. If COMMON_TYPE is void (the default), any type can be put in the queue.
            Otherwise it should be an user-defined-type, and only types deriving from it can be added.
        @tparam RUNTIME_TYPE Runtime-type object used to handle the actual complete type of each element.
                This type must meet the requirements of \ref RuntimeType_concept "RuntimeType". The default is runtime_type.
        @tparam ALLOCATOR_TYPE Allocator type to be used. This type must meet the requirements of both \ref UntypedAllocator_concept
                "UntypedAllocator" and \ref PagedAllocator_concept "PagedAllocator". The default is density::void_allocator.
        @tparam PROD_CARDINALITY specifies whether multiple threads can do put transactions concurrently. Must be a member of density::concurrency_cardinality.
        @tparam CONSUMER_CARDINALITY specifies whether multiple threads can do consume operations concurrently. Must be a member of density::concurrency_cardinality.
        @tparam CONSISTENCY_MODEL Specifies whether the queue is linearizable. Must be a member of density::consistency_model.
            Implementation note: Currently this parameter affects only put operations, and only in case of multiple producers:
            in case of relaxed consistency model, for a small amount of time, during the first phase of a put transaction, the
            queue is truncated, so any thread can successfully put further elements, but those elements are not observable to any
            thread, even the one who did the put.

        \n <b>Thread safeness</b>: A thread doing put operations and another thread doing consumes don't need to be synchronized.
                If PROD_CARDINALITY is concurrency_multiple, multiple threads are allowed to put without any synchronization.
                If CONSUMER_CARDINALITY is concurrency_multiple, multiple threads are allowed to consume without any synchronization.
        \n <b>Exception safeness</b>: Any function of lf_heter_queue is noexcept or provides the strong exception guarantee.

        This class template uses lock-free algorithms for both put operations and consume operations. Anyway, for the overall
        put or consume to be lock-free, if a memory operation is necessary, it must be lock-free too. The default allocator,
        density::void_allocator, can manage pages in lock freedom within its current capacity (i.e. the memory it has managed until
        now). This capacity is composed by all the allocated, pinned, thread-owned and free pages.
        If the capacity must exceed its previous peek, and all the memory regions are exhausted, void_allocator must request a
        new memory region to the system. In this case it can't guarantee lock-freedom.
        The static functions void_allocator::reserve_lockfree_page_memory and
        void_allocator::try_reserve_lockfree_page_memory can be used to reserve a capacity in advance.
        \n void_allocator delegates legacy memory operations to the system. Since the storage of elements whose
        size exceeds a fixed limit can't be allocated in a page, they require a legacy memory allocation, and in this case the put
        can't be lock-free.

        This class template provides all the put functions provided by heter_queue, and furthermore it adds the try_ variants, that:
        - Don't throw in case of failure allocating memory. Anyway they pass through any exception thrown by the constructor of the
            element and the constructor of the runtime type.
        - Allow to specify a progress guarantee to be respected by the overall operation. For example, if the lock-free guarantee is
            requested, but it requires a memory operation that the allocator is not able to complete in lock-freedom, the put fails.
            In the current implementation, wait-free put operation may fail even in isolation, because page pinning is lock-free but not wait-free.

        Also raw allocation functions have the try_ versions. They do not throw in case of failure, but just return null.

        This table shows all the put functions supported by lf_heter_queue:

        <table>
        <caption id="multi_row">Put functions</caption>
        <tr>
            <th style="width:150px">Group</th>
            <th style="width:100px">Normal</th>
            <th style="width:100px">Transactional</th>
            <th style="width:100px">Reentrant</th>
            <th style="width:100px">Transactional-Reentrant</th>
            <th style="width:130px">Type binding</th>
            <th style="width:130px">Constructor</th>
        </tr>
        <tr>
            <td>push</td>
            <td>@code push @endcode</td>
            <td>@code start_push @endcode</td>
            <td>@code reentrant_push @endcode</td>
            <td>@code start_reentrant_push @endcode</td>
            <td>Compile time</td>
            <td>Copy/Move</td>
        </tr>
        <tr>
            <td>emplace</td>
            <td>@code emplace @endcode</td>
            <td>@code start_emplace @endcode</td>
            <td>@code reentrant_emplace @endcode</td>
            <td>@code start_reentrant_emplace @endcode</td>
            <td>Compile time</td>
            <td>Any</td>
        </tr>
        <tr>
            <td>dynamic push</td>
            <td>@code dyn_push @endcode</td>
            <td>@code start_dyn_push @endcode</td>
            <td>@code reentrant_dyn_push @endcode</td>
            <td>@code start_reentrant_dyn_push @endcode</td>
            <td>Runtime</td>
            <td>Default</td>
        </tr>
        <tr>
            <td>dynamic push (copy)</td>
            <td>@code dyn_push_copy @endcode</td>
            <td>@code start_dyn_push_copy @endcode</td>
            <td>@code reentrant_dyn_push_copy @endcode</td>
            <td>@code start_reentrant_dyn_push_copy @endcode</td>
            <td>Runtime</td>
            <td>Copy</td>
        </tr>
        <tr>
            <td>dynamic push (move)</td>
            <td>@code dyn_push_move @endcode</td>
            <td>@code start_dyn_push_move @endcode</td>
            <td>@code reentrant_dyn_push_move @endcode</td>
            <td>@code start_reentrant_dyn_push_move @endcode</td>
            <td>Runtime</td>
            <td>Move</td>
        </tr>
        <tr>
            <td>try push</td>
            <td>@code try_push @endcode</td>
            <td>@code try_start_push @endcode</td>
            <td>@code try_reentrant_push @endcode</td>
            <td>@code try_start_reentrant_push @endcode</td>
            <td>Compile time</td>
            <td>Copy/Move</td>
        </tr>
        <tr>
            <td>try emplace</td>
            <td>@code try_emplace @endcode</td>
            <td>@code try_start_emplace @endcode</td>
            <td>@code try_reentrant_emplace @endcode</td>
            <td>@code try_start_reentrant_emplace @endcode</td>
            <td>Compile time</td>
            <td>Any</td>
        </tr>
        <tr>
            <td>try dynamic push</td>
            <td>@code try_dyn_push @endcode</td>
            <td>@code try_start_dyn_push @endcode</td>
            <td>@code try_reentrant_dyn_push @endcode</td>
            <td>@code try_start_reentrant_dyn_push @endcode</td>
            <td>Runtime</td>
            <td>Default</td>
        </tr>
        <tr>
            <td>try dynamic push (copy)</td>
            <td>@code try_dyn_push_copy @endcode</td>
            <td>@code try_start_dyn_push_copy @endcode</td>
            <td>@code try_reentrant_dyn_push_copy @endcode</td>
            <td>@code try_start_reentrant_dyn_push_copy @endcode</td>
            <td>Runtime</td>
            <td>Copy</td>
        </tr>
        <tr>
            <td>try dynamic push (move)</td>
            <td>@code try_dyn_push_move @endcode</td>
            <td>@code try_start_dyn_push_move @endcode</td>
            <td>@code try_reentrant_dyn_push_move @endcode</td>
            <td>@code try_start_reentrant_dyn_push_move @endcode</td>
            <td>Runtime</td>
            <td>Move</td>
        </tr>
        </table>
    */
    template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator,
            concurrency_cardinality PROD_CARDINALITY = concurrency_multiple,
            concurrency_cardinality CONSUMER_CARDINALITY = concurrency_multiple,
            consistency_model CONSISTENCY_MODEL = consistency_sequential>
        class lf_heter_queue : private detail::LFQueue_Head< COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, CONSUMER_CARDINALITY,
                detail::LFQueue_Tail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSISTENCY_MODEL> >
    {
    private:
        using Base = detail::LFQueue_Head< COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, CONSUMER_CARDINALITY,
                detail::LFQueue_Tail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSISTENCY_MODEL> >;
        using ControlBlock = typename Base::ControlBlock;
        using Block = typename Base::Block;
        using Consume = typename Base::Consume;

        /** This type is used to make some functions of the inner classes accessible only by the queue */
        enum class PrivateType {};

    public:

        /** Minimum alignment used for the storage of the elements. The storage of elements is always aligned according to the most-derived type. */
        constexpr static size_t min_alignment = Base::min_alignment;

        using common_type = COMMON_TYPE;
        using runtime_type = RUNTIME_TYPE;
        using value_type = std::pair<const runtime_type &, common_type* const>;
        using allocator_type = ALLOCATOR_TYPE;
        using pointer = value_type *;
        using const_pointer = const value_type *;
        using reference = value_type;
        using const_reference = const value_type&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        /** Whether multiple threads can do put operations on the same queue without any further synchronization. */
        static constexpr bool concurrent_puts = PROD_CARDINALITY == concurrency_multiple;

        /** Whether multiple threads can do consume operations on the same queue without any further synchronization. */
        static constexpr bool concurrent_consumes = CONSUMER_CARDINALITY == concurrency_multiple;

        /** Whether puts and consumes can be done concurrently without any further synchronization. In any case unsynchronized concurrency is
            constrained by concurrent_puts and concurrent_consumes. */
        static constexpr bool concurrent_put_consumes = true;

        /** Whether this queue is sequential consistent. */
        static constexpr bool is_seq_cst = CONSISTENCY_MODEL == consistency_sequential;

        static_assert(std::is_same<COMMON_TYPE, typename RUNTIME_TYPE::common_type>::value,
            "COMMON_TYPE and RUNTIME_TYPE::common_type must be the same type (did you try to use a type like heter_cont<A,runtime_type<B>>?)");

        static_assert(std::is_same<COMMON_TYPE, typename std::decay<COMMON_TYPE>::type>::value,
            "COMMON_TYPE can't be cv-qualified, an array or a reference");

        static_assert(is_power_of_2(ALLOCATOR_TYPE::page_alignment) &&
            ALLOCATOR_TYPE::page_alignment >= ALLOCATOR_TYPE::page_size &&
            (ALLOCATOR_TYPE::page_alignment % min_alignment) == 0,
            "The alignment of the pages must be a power of 2, greater or equal to the size of the pages, and a multiple of min_alignment");

        static_assert(ALLOCATOR_TYPE::page_size > (min_alignment + alignof(ControlBlock)) * 4, "Invalid page size");

        /** Default constructor. The allocator is default-constructed.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: nothing.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
                This constructor does not allocate memory and never throws.

        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue default_construct example 1 */
        lf_heter_queue() noexcept = default;

        /** Constructor with allocator parameter. The allocator is copy-constructed.
            @param i_source_allocator source used to copy-construct the allocator.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: whatever the copy constructor of the allocator throws.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
                This constructor does not allocate memory. It throws anything the copy constructor of the allocator throws.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue construct_copy_alloc example 1 */
        lf_heter_queue(const ALLOCATOR_TYPE & i_source_allocator)
                noexcept (std::is_nothrow_copy_constructible<ALLOCATOR_TYPE>::value)
            : Base(i_source_allocator)
        {
        }

        /** Constructor with allocator parameter. The allocator is move-constructed.
            @param i_source_allocator source used to move-construct the allocator.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: nothing.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
                This constructor does not allocate memory. It throws anything the move constructor of the allocator throws.

        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue construct_move_alloc example 1 */
        lf_heter_queue(ALLOCATOR_TYPE && i_source_allocator) noexcept
            : Base(std::move(i_source_allocator))
        {
            static_assert(std::is_nothrow_move_constructible<ALLOCATOR_TYPE>::value, "");
        }

        /** Move constructor. The allocator is move-constructed from the one of the source.
                @param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: nothing.
            \n <i>Implementation notes</i>:
                - After the call the source is left empty.

        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue move_construct example 1 */
        lf_heter_queue(lf_heter_queue && i_source) noexcept = default;

        /** Move assignment. The allocator is move-assigned from the one of the source.
                @param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.

            <b>Complexity</b>: Unspecified.
            \n <b>Effects on iterators</b>: Any iterator pointing to this queue or to the source queue is invalidated.
            \n <b>Throws</b>: Nothing.

            \n <i>Implementation notes</i>:
                - After the call the source is left empty.
                - The complexity is linear in the number of elements in this queue.

        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue move_assign example 1 */
        lf_heter_queue & operator = (lf_heter_queue && i_source) noexcept
        {
            Base::swap(i_source);
            return *this;
        }

        /** Returns a copy of the allocator

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue get_allocator example 1 */
        allocator_type get_allocator() noexcept(std::is_nothrow_copy_constructible<allocator_type>::value)
        {
            return *this;
        }

        /** Returns a reference to the allocator

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue get_allocator_ref example 1 */
        allocator_type & get_allocator_ref() noexcept
        {
            return *this;
        }

        /** Returns a const reference to the allocator

        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue get_allocator_ref example 2 */
        const allocator_type & get_allocator_ref() const noexcept
        {
            return *this;
        }

        /** Swaps two queues.

        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue swap example 1 */
        friend void swap(lf_heter_queue & i_first, lf_heter_queue & i_second) noexcept
        {
            i_first.Base::swap(i_second);
        }

        /** Destructor.

            <b>Complexity</b>: linear.
            \n <b>Effects on iterators</b>: any iterator pointing to this queue is invalidated.
            \n <b>Throws</b>: Nothing. */
        ~lf_heter_queue()
        {
            clear();

            Consume consume;
            if (consume.assign_queue(this))
            {
                consume.clean_dead_elements();
            }
        }

        /** Returns whether the queue contains no elements.

            <b>Complexity</b>: Unspecified.
            \n <b>Throws</b>: Nothing.

        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue empty example 1 */
        bool empty() const noexcept
        {
            return Consume().is_queue_empty(this);
        }

        /** Deletes all the elements in the queue.

            <b>Complexity</b>: linear.
            \n <b>Effects on iterators</b>: any iterator is invalidated
            \n <b>Throws</b>: Nothing.

        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue clear example 1 */
        void clear() noexcept
        {
            consume_operation consume;
            while(try_start_consume(consume))
            {
                consume.commit();
            }
        }

        /** Move-only class template that can be bound to a put transaction, otherwise it's empty.

            @tparam ELEMENT_COMPLETE_TYPE Complete type of elements that can be handled by a transaction, or void.
                ELEMENT_COMPLETE_TYPE must decay to itself (it can't be cv-qualified).

            Transactional put functions on lf_heter_queue return a non-empty put_transaction that can be
            used to allocate raw memory in the queue, inspect or alter the element while it is still not observable
            in the queue, and commit or cancel the push.

            A put_transaction is empty when:
                - it is default constructed
                - it is used as source for a move construction or move assignment
                - after a cancel or a commit

            Calling \ref put_transaction::raw_allocate "raw_allocate", \ref put_transaction::raw_allocate_copy "raw_allocate_copy",
            \ref put_transaction::commit "commit", \ref put_transaction::cancel "cancel", \ref put_transaction::element_ptr "element_ptr",
            \ref put_transaction::element "element" or \ref put_transaction::complete_type "complete_type" on an empty put_transaction
            triggers undefined behavior.

            A void put_transaction can be move constructed/assigned from any put_transaction. A typed put_transaction
            can be move constructed/assigned only from a put_transaction with the same ELEMENT_COMPLETE_TYPE. */
        template <typename ELEMENT_COMPLETE_TYPE = void>
            class put_transaction
        {
            static_assert(std::is_same<ELEMENT_COMPLETE_TYPE, typename std::decay<ELEMENT_COMPLETE_TYPE>::type>::value, "");

        public:

            /** Constructs an empty put transaction */
            put_transaction() noexcept = default;

            /** Copy construction is not allowed.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction copy_construct example 1 */
            put_transaction(const put_transaction &) = delete;

            /** Copy assignment is not allowed.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction copy_assign example 1 */
            put_transaction & operator = (const put_transaction &) = delete;

            /** Move constructs a put_transaction, transferring the state from the source.
                    @param i_source source to move from. It is left in a valid but indeterminate state.


            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction move_construct example 1

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction move_construct example 2 */
            template <typename OTHERTYPE, typename = typename std::enable_if<
                    std::is_same<OTHERTYPE, ELEMENT_COMPLETE_TYPE>::value || std::is_void<ELEMENT_COMPLETE_TYPE>::value >::type >
                put_transaction(put_transaction<OTHERTYPE> && i_source) noexcept
                    : m_put(i_source.m_put), m_queue(i_source.m_queue)
            {
                i_source.m_put.m_user_storage = nullptr;
            }

            /** Move assigns a put_transaction, transferring the state from the source.
                @param i_source source to move from. It is left in a valid but indeterminate state.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction move_assign example 1
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction move_assign example 2 */
            template <typename OTHERTYPE, typename = typename std::enable_if<
                    std::is_same<OTHERTYPE, ELEMENT_COMPLETE_TYPE>::value || std::is_void<ELEMENT_COMPLETE_TYPE>::value >::type >
                put_transaction & operator = (put_transaction<OTHERTYPE> && i_source) noexcept
            {
                using std::swap;
                swap(m_put, i_source.m_put);
                swap(m_queue, i_source.m_queue);
                return *this;
            }

            /** Swaps two instances of put_transaction.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction swap example 1 */
            friend void swap(put_transaction & i_first, put_transaction & i_second) noexcept
            {
                using std::swap;
                swap(i_first.m_put, i_second.m_put);
                swap(i_first.m_queue, i_second.m_queue);
            }

            /** Allocates a memory block associated to the element being added in the queue. The block may be allocated contiguously with
                the elements in the memory pages. If the block does not fit in one page, the block is allocated using non-paged memory services
                of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_size size in bytes of the block to allocate.
                @param i_alignment alignment of the block to allocate. It must be a non-zero power of 2, and less than or
                        equal to i_size.

                \pre The behavior is undefined if either:
                    - this transaction is empty
                    - the alignment is not valid
                    - the size is not a multiple of the alignment

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction raw_allocate example 1*/
            void * raw_allocate(size_t i_size, size_t i_alignment)
            {
                DENSITY_ASSERT(!empty());
                auto push_data = m_queue->inplace_allocate(detail::NbQueue_Dead, false, i_size, i_alignment);
                return push_data.m_user_storage;
            }

            /** Allocates a memory block associated to the element being added in the queue, and copies the content from a range
                of iterators. The block may be allocated contiguously with the elements in the memory pages. If the block does not
                fit in one page, the block is allocated using non-paged memory services of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_begin first element to be copied
                @param i_end first element not to be copied

                \n <b>Requires</b>:
                    - INPUT_ITERATOR must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
                    - the value type must be trivially destructible

                \pre The behavior is undefined if either:
                    - this transaction is empty
                    - i_end is not reachable from i_begin

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects)

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction raw_allocate_copy example 1*/
            template <typename INPUT_ITERATOR>
                typename std::iterator_traits<INPUT_ITERATOR>::value_type *
                    raw_allocate_copy(INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end)
            {
                using ValueType = typename std::iterator_traits<INPUT_ITERATOR>::value_type;
                static_assert(std::is_trivially_destructible<ValueType>::value,
                    "raw_allocate_copy provides a raw memory inplace allocation that does not invoke destructors when deallocating");

                auto const count_s = std::distance(i_begin, i_end);
                auto const count = static_cast<size_t>(count_s);
                DENSITY_ASSERT(static_cast<decltype(count_s)>(count) == count_s);

                auto const elements = static_cast<ValueType*>(raw_allocate(sizeof(ValueType) * count, alignof(ValueType)));
                for (auto curr = elements; i_begin != i_end; ++i_begin, ++curr)
                    new(curr) ValueType(*i_begin);
                return elements;
            }

            /** Allocates a memory block associated to the element being added in the queue, and copies the content from a range.
                The block may be allocated contiguously with the elements in the memory pages. If the block does not
                fit in one page, the block is allocated using non-paged memory services of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_source_range to be copied

                \n <b>Requires</b>:
                    - The iterators of the range must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
                    - the value type must be trivially destructible

                \pre The behavior is undefined if either:
                    - this transaction is empty

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction raw_allocate_copy example 2 */
            template <typename INPUT_RANGE>
                auto raw_allocate_copy(const INPUT_RANGE & i_source_range)
                    -> decltype(raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range)))
            {
                return raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range));
            }

            /** Tries to allocates a memory block associated to the element being added in the queue. The block may be allocated contiguously with
                the elements in the memory pages. If the block does not fit in one page, the block is allocated using non-paged memory services
                of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                \n If the operation can't be completed with the specified progress guarantee, this function returns nullptr to indicate
                a failure, and has no observable effects. This function fails if:
                - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                    the blocking progress guarantee indicates an out of memory, but no exception is thrown.
                - there is a contention between threads, and the wait-free progress guarantee is requested
                - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

                @param i_progress_guarantee progress guarantee to respect
                @param i_size size in bytes of the block to allocate.
                @param i_alignment alignment of the block to allocate. It must be a non-zero power of 2, and less than or
                        equal to i_size.
                @return pointer to the allocated storage, or null in case of failure

                \pre The behavior is undefined if either:
                    - this transaction is empty
                    - the alignment is not valid
                    - the size is not a multiple of the alignment

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: nothing.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction try_raw_allocate example 1*/
            void * try_raw_allocate(progress_guarantee i_progress_guarantee, size_t i_size, size_t i_alignment) noexcept
            {
                DENSITY_ASSERT(!empty());
                auto push_data = m_queue->try_inplace_allocate(i_progress_guarantee, detail::NbQueue_Dead, false, i_size, i_alignment);
                return push_data.m_user_storage;
            }

            /** Tries to allocate a memory block associated to the element being added in the queue, and copies the content from a range
                of iterators. The block may be allocated contiguously with the elements in the memory pages. If the block does not
                fit in one page, the block is allocated using non-paged memory services of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                \n If the operation can't be completed with the specified progress guarantee, this function returns nullptr to indicate
                a failure, and has no observable effects. This function fails if:
                - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                    the blocking progress guarantee indicates an out of memory, but no exception is thrown.
                - there is a contention between threads, and the wait-free progress guarantee is requested
                - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

                @param i_progress_guarantee progress guarantee to respect
                @param i_begin first element to be copied
                @param i_end first element not to be copied
                @return pointer to the allocated array, or null in case of failure

                \n <b>Requires</b>:
                    - INPUT_ITERATOR must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
                    - the value type must be trivially destructible

                \pre The behavior is undefined if either:
                    - this transaction is empty
                    - i_end is not reachable from i_begin

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: what the copy constructor of the element throws (conditionally noexcept)
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects)

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction try_raw_allocate_copy example 1*/
            template <typename INPUT_ITERATOR>
                typename std::iterator_traits<INPUT_ITERATOR>::value_type *
                    try_raw_allocate_copy(progress_guarantee i_progress_guarantee, INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end)
                        noexcept(std::is_nothrow_copy_constructible<typename std::iterator_traits<INPUT_ITERATOR>::value_type>::value)
            {
                using ValueType = typename std::iterator_traits<INPUT_ITERATOR>::value_type;
                static_assert(std::is_trivially_destructible<ValueType>::value,
                    "raw_allocate_copy provides a raw memory inplace allocation that does not invoke destructors when deallocating");

                auto const count_s = std::distance(i_begin, i_end);
                auto const count = static_cast<size_t>(count_s);
                DENSITY_ASSERT(static_cast<decltype(count_s)>(count) == count_s);

                auto const elements = static_cast<ValueType*>(try_raw_allocate(i_progress_guarantee, sizeof(ValueType) * count, alignof(ValueType)));
                if (elements != nullptr)
                {
                    for (auto curr = elements; i_begin != i_end; ++i_begin, ++curr)
                        new(curr) ValueType(*i_begin);
                }
                return elements;
            }

            /** Tries top allocate a memory block associated to the element being added in the queue, and copies the content from a range.
                The block may be allocated contiguously with the elements in the memory pages. If the block does not
                fit in one page, the block is allocated using non-paged memory services of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                \n If the operation can't be completed with the specified progress guarantee, this function returns nullptr to indicate
                a failure, and has no observable effects. This function fails if:
                - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                    the blocking progress guarantee indicates an out of memory, but no exception is thrown.
                - there is a contention between threads, and the wait-free progress guarantee is requested
                - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

                @param i_progress_guarantee progress guarantee to respect
                @param i_source_range to be copied
                @return pointer to the allocated array, or null in case of failure

                \n <b>Requires</b>:
                    - The iterators of the range must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
                    - the value type must be trivially destructible

                \pre The behavior is undefined if either:
                    - this transaction is empty

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: what the copy constructor of the element throws (conditionally noexcept)
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction try_raw_allocate_copy example 2 */
            template <typename INPUT_RANGE>
                auto try_raw_allocate_copy(progress_guarantee i_progress_guarantee, const INPUT_RANGE & i_source_range)
                    noexcept(noexcept(std::declval<put_transaction>().try_raw_allocate_copy(i_progress_guarantee, std::begin(i_source_range), std::end(i_source_range))))
                        -> decltype(try_raw_allocate_copy(i_progress_guarantee, std::begin(i_source_range), std::end(i_source_range)))
            {
                return try_raw_allocate_copy(i_progress_guarantee,
                    std::begin(i_source_range), std::end(i_source_range));
            }

            /** Makes the effects of the transaction observable. This object becomes empty.

                \pre The behavior is undefined if either:
                    - this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
                DENSITY_ASSERT(!empty());
                Base::commit_put_impl(m_put);
                m_put.m_user_storage = nullptr;
            }

            /** Cancel the transaction. This object becomes empty.

                \pre The behavior is undefined if either:
                    - this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction cancel example 1 */
            void cancel() noexcept
            {
                DENSITY_ASSERT(!empty());
                Base::cancel_put_impl(m_put);
                m_put.m_user_storage = nullptr;
            }

            /** Returns true whether this object is not currently bound to a transaction.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction empty example 1 */
            bool empty() const noexcept
            {
                return m_put.m_user_storage == nullptr;
            }

            /** Returns true whether this object is bound to a transaction. Same to !consume_operation::empty.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction operator_bool example 1 */
            explicit operator bool() const noexcept
            {
                return m_put.m_user_storage != nullptr;
            }

            /** Returns a pointer to the target queue if a transaction is bound, otherwise returns nullptr

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction queue example 1 */
            lf_heter_queue * queue() const noexcept
            {
                return m_put.m_user_storage != nullptr ? m_queue : nullptr;
            }

            /** Returns a pointer to the object being added.
                \n <i>Notes</i>:
                - The object is constructed when the transaction is started, so this function always returns a
                    pointer to a valid object.
                - This function returns a pointer to the common_type subobject of the element. Non-dynamic
                    transactional put function (start_push, start_emplace, start_reentrant_push, start_reentrant_emplace)
                    return a typed_put_transaction or a reentrant_put_transaction, that provide the function element(),
                    which is a better alternative to this function

                \pre The behavior is undefined if either:
                    - this transaction is empty

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction element_ptr example 1

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction element_ptr example 2 */
            common_type * element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return m_put.m_user_storage;
            }

            /** Returns a reference to the element being added. This function can be used to modify the element
                    before the commit.
                \n <i>Note</i>: An element is observable in the queue only after commit has been called on the
                    put_transaction. The element is constructed at the begin of the transaction, so the
                    returned object is always valid.

                \n <b>Requires</b>:
                    - ELEMENT_COMPLETE_TYPE must not be void

                \pre The behavior is undefined if:
                    - this transaction is empty

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue typed_put_transaction element example 1 */
            #ifndef DOXYGEN_DOC_GENERATION
            template <typename EL = ELEMENT_COMPLETE_TYPE>
                typename std::enable_if<!std::is_void<EL>::value && std::is_same<EL, ELEMENT_COMPLETE_TYPE>::value, EL>::type &
            #else
                ELEMENT_COMPLETE_TYPE &
            #endif
                    element() const noexcept
            {
                return *static_cast<ELEMENT_COMPLETE_TYPE *>(element_ptr());
            }

            /** Returns the type of the object being added.

                \pre The behavior is undefined if either:
                    - this transaction is empty

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction complete_type example 1 */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return *Base::type_after_control(m_put.m_control_block);
            }

            /** If this transaction is empty the destructor has no side effects. Otherwise it cancels it.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue put_transaction destroy example 1 */
            ~put_transaction()
            {
                if (m_put.m_user_storage != nullptr)
                {
                    Base::cancel_put_impl(m_put);
                }
            }

            /** \internal - private function, usable only within the library */
            put_transaction(PrivateType, lf_heter_queue * i_queue, const Block & i_put,
                    std::false_type /*i_is_void*/, COMMON_TYPE * i_element) noexcept
                : m_put(i_put), m_queue(i_queue)
            {
                m_put.m_user_storage = i_element;
                m_put.m_control_block->m_element = i_element;
            }

            /** \internal - private function, usable only within the library */
            put_transaction(PrivateType, lf_heter_queue * i_queue, const Block & i_put,
                    std::true_type /*i_is_void*/, void *) noexcept
                : m_put(i_put), m_queue(i_queue)
            {
            }

        private:
            Block m_put;
            lf_heter_queue * m_queue;
            template <typename OTHERTYPE> friend class put_transaction;
        };

        /** Move-only class that can be bound to a consume operation, otherwise it's empty.


        Consume functions on lf_heter_queue return a non-empty consume_operation that can be
            used to inspect or alter the element while it is not observable in the queue, and
            commit or cancel the consume.

            A consume_operation is empty when:
                - it is default constructed
                - it is used as source for a move construction or move assignment
                - after a cancel or a commit

            Calling \ref consume_operation::commit "commit", \ref consume_operation::commit_nodestroy "commit_nodestroy",
            \ref consume_operation::cancel "cancel", \ref consume_operation::element_ptr "element_ptr",
            \ref consume_operation::unaligned_element_ptr "unaligned_element_ptr",
            \ref consume_operation::element "element" or \ref consume_operation::complete_type "complete_type" on
            an empty consume_operation triggers undefined behavior. */
        class consume_operation
        {
        public:

            /** Constructs an empty consume_operation

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation default_construct example 1 */
            consume_operation() noexcept = default;

            /** Copy construction is not allowed

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation copy_construct example 1 */
            consume_operation(const consume_operation &) = delete;

            /** Copy assignment is not allowed

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation copy_assign example 1 */
            consume_operation & operator = (const consume_operation &) = delete;

            /** Move constructor. The source is left in a valid but indeterminate state.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation move_construct example 1 */
            consume_operation(consume_operation && i_source) noexcept = default;

            /** Move assignment. The source is left in a valid but indeterminate state.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation move_assign example 1 */
            consume_operation & operator = (consume_operation && i_source) noexcept = default;

            /** Destructor: cancel the operation (if any).

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation destroy example 1 */
            ~consume_operation()
            {
                if (m_consume_data.m_next_ptr != 0)
                {
                    m_consume_data.cancel_consume_impl();
                }
            }

            /** Swaps two instances of consume_operation.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation swap example 1 */
            friend void swap(consume_operation & i_first, consume_operation & i_second) noexcept
            {
                i_first.m_consume_data.swap(i_second.m_consume_data);
            }

            /** Returns true whether this object does not hold the state of an operation.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation empty example 1 */
            bool empty() const noexcept
            {
                return m_consume_data.m_next_ptr == 0;
            }

            /** Returns true whether this object does not hold the state of an operation. Same to !consume_operation::empty.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation operator_bool example 1 */
            explicit operator bool() const noexcept
            {
                return m_consume_data.m_next_ptr != 0;
            }

            /** Returns a pointer to the target queue if a transaction is bound, otherwise returns nullptr

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation queue example 1 */
            lf_heter_queue * queue() const noexcept
            {
                return static_cast<lf_heter_queue*>(m_consume_data.m_queue);
            }

            /** Destroys the element, making the consume irreversible. This comnsume_operation becomes empty.

                \pre The behavior is undefined if either:
                    - this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
                DENSITY_ASSERT(!empty());

                auto const & type = complete_type();
                auto const element = element_ptr();
                type.destroy(element);

                type.RUNTIME_TYPE::~RUNTIME_TYPE();

                m_consume_data.commit_consume_impl();
            }

            /** Destroys the element, making the consume irreversible. This comnsume_operation becomes empty.
                The caller should destroy the element before calling this function.
                This object becomes empty.

                \pre The behavior is undefined if either:
                    - this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing.

            Note: this function may be used to combine a feature of the runtime type on the element with the
                destruction of the element. For example a function queue may improve the performances using a feature
                like invoke_destroy to do both the function call and the destruction of the capture in a single call,
                making a single pseudo v-call instead of two.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation commit_nodestroy example 1 */
            void commit_nodestroy() noexcept
            {
                DENSITY_ASSERT(!empty());

                bool destroy_type = !std::is_trivially_destructible<RUNTIME_TYPE>::value;
                if (destroy_type)
                {
                    auto const & type = complete_type();
                    type.RUNTIME_TYPE::~RUNTIME_TYPE();
                }

                m_consume_data.commit_consume_impl();
            }

             /** Cancel the operation. This consume_operation becomes empty.

                \pre The behavior is undefined if either:
                    - this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation cancel example 1 */
            void cancel() noexcept
            {
                DENSITY_ASSERT(!empty());
                m_consume_data.cancel_consume_impl();
            }

            /** Returns the type of the element being consumed.

                \pre The behavior is undefined if this consume_operation is empty.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation complete_type example 1 */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return *Base::type_after_control(m_consume_data.m_control);
            }

            /** Returns a pointer that, if properly aligned to the alignment of the element type, points to the element.
                The returned address is guaranteed to be aligned to min_alignment

                \pre The behavior is undefined if this consume_operation is empty, that is it has been used as source for a move operation.
                \post The returned address is aligned at least on lf_heter_queue::min_alignment.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation unaligned_element_ptr example 1 */
            void * unaligned_element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return Base::get_unaligned_element(m_consume_data.m_control);
            }

            /** Returns a pointer to the element being consumed.

                This call is equivalent to: <code>address_upper_align(unaligned_element_ptr(), complete_type().alignment());</code>

                \pre The behavior is undefined if this consume_operation is empty, that is it has been committed or used as source for a move operation.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation element_ptr example 1 */
            COMMON_TYPE * element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return Base::get_element(m_consume_data.m_control);
            }

            /** Returns a reference to the element being consumed.

                \pre The behavior is undefined if this consume_operation is empty, that is it has been committed or used as source for a move operation.
                \pre The behavior is undefined if COMPLETE_ELEMENT_TYPE is not exactly the complete type of the element.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue consume_operation element example 1 */
            template <typename COMPLETE_ELEMENT_TYPE>
                COMPLETE_ELEMENT_TYPE & element() const noexcept
            {
                DENSITY_ASSERT(!empty() && complete_type().template is<COMPLETE_ELEMENT_TYPE>());
                return *static_cast<COMPLETE_ELEMENT_TYPE*>(Base::get_element(m_consume_data.m_control));
            }

            /** \internal - private function, usable only within the library */
            consume_operation(PrivateType, lf_heter_queue * i_queue) noexcept
            {
                m_consume_data.start_consume_impl(i_queue);
            }

            /** \internal - private function, usable only within the library */
            bool start_consume_impl(PrivateType, lf_heter_queue * i_queue)
            {
                if(m_consume_data.m_next_ptr != 0)
                {
                    m_consume_data.cancel_consume_impl();
                }

                m_consume_data.start_consume_impl(i_queue);

                return m_consume_data.m_next_ptr != 0;
            }

        private:
            Consume m_consume_data;
        };

        /** Appends at the end of the queue an element of type <code>ELEMENT_TYPE</code>, copy-constructing or move-constructing
                it from the source.

            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed
                - If this argument is an r-value, the new element move-constructed

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue push example 1 */
        template <typename ELEMENT_TYPE>
            void push(ELEMENT_TYPE && i_source)
        {
            return emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Appends at the end of the queue an element of type <code>ELEMENT_TYPE</code>, inplace-constructing it from
                a perfect forwarded parameter pack.
            \n <i>Note</i>: the template argument ELEMENT_TYPE can't be deduced from the parameters so it must explicitly specified.

            @param i_construction_params construction parameters for the new element.

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be constructible with <code>std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...</code>
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            void emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            start_emplace<ELEMENT_TYPE>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
        }

        /** Adds at the end of the queue an element of a type known at runtime, default-constructing it.

            @param i_type type of the new element.

            \n <b>Requires</b>:
                - The function <code>RUNTIME_TYPE::default_construct</code> must be invokable. If
                  <code>RUNTIMETYPE</code> is runtime_type this means that <code>default_construct</code> must be
                  included in the feature list.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue dyn_push example 1 */
        void dyn_push(const runtime_type & i_type)
        {
            start_dyn_push(i_type).commit();
        }

        /** Appends at the end of the queue an element of a type known at runtime, copy-constructing it from the source.

            @param i_type type of the new element.
            @param i_source pointer to the subobject of type <code>COMMON_TYPE</code> of an object or subobject of type <code>ELEMENT_TYPE</code>.
                <i>Note</i>: be careful using void pointers: only the compiler knows how to casts from/to a base to/from a derived class.

            \n <b>Requires</b>:
                - The function <code>RUNTIME_TYPE::copy_construct</code> must be invokable. If
                  <code>RUNTIMETYPE</code> is runtime_type this means that <code>copy_construct</code> must be
                  included in the feature list.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue dyn_push_copy example 1 */
        void dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            start_dyn_push_copy(i_type, i_source).commit();
        }

        /** Adds at the end of the queue an element of a type known at runtime, move-constructing it from the source.

            @param i_type type of the new element
            @param i_source pointer to the subobject of type <code>COMMON_TYPE</code> of an object or subobject of type <code>ELEMENT_TYPE</code>
                <i>Note</i>: be careful using void pointers: casts from/to a base to/from a derived class can be done only
                by the type system of the language.

            \n <b>Requires</b>:
                - The function <code>RUNTIME_TYPE::move_construct</code> must be invokable. If
                  <code>RUNTIMETYPE</code> is runtime_type this means that <code>move_construct</code> must be
                  included in the feature list.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue dyn_push_move example 1 */
        void dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            start_dyn_push_move(i_type, i_source).commit();
        }

        /** Begins a transaction that appends an element of type <code>ELEMENT_TYPE</code>, copy-constructing
                or move-constructing it from the source.
            \n This function allocates the required space, constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed.
                - If this argument is an r-value, the new element move-constructed.
            @return The associated transaction object.

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_push example 1 */
        template <typename ELEMENT_TYPE>
            put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_push(ELEMENT_TYPE && i_source)
        {
            return start_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Begins a transaction that appends an element of a type <code>ELEMENT_TYPE</code>,
            inplace-constructing it from a perfect forwarded parameter pack.
            \n This function allocates the required space, constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_construction_params construction parameters for the new element.
            @return The associated transaction object.

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be constructible with <code>std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...</code>
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            put_transaction<ELEMENT_TYPE> start_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
                "ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");

            auto push_data = Base::template inplace_allocate<detail::NbQueue_Busy, true, detail::size_of<ELEMENT_TYPE>::value, alignof(ELEMENT_TYPE)>();

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(runtime_type::template make<ELEMENT_TYPE>());

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = new (push_data.m_user_storage) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();

                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return put_transaction<ELEMENT_TYPE>(PrivateType(),
                this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Begins a transaction that appends an element of a type known at runtime, default-constructing it.
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_type type of the new element.
            @return The associated transaction object.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_dyn_push example 1 */
        put_transaction<> start_dyn_push(const runtime_type & i_type)
        {
            auto push_data = Base::inplace_allocate(detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.default_construct(push_data.m_user_storage);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();

                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return put_transaction<void>(PrivateType(), this, push_data, std::is_void<COMMON_TYPE>(), element);
        }


        /** Begins a transaction that appends an element of a type known at runtime, copy-constructing it from the source..
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_type type of the new element.
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
                <i>Note</i>: be careful using void pointers: casts from/to a base to/from a derived class can be done only
                by the type system of the language.
            @return The associated transaction object.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_dyn_push_copy example 1 */
        put_transaction<> start_dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            auto push_data = Base::inplace_allocate(detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.copy_construct(push_data.m_user_storage, i_source);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return put_transaction<void>(PrivateType(), this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Begins a transaction that appends an element of a type known at runtime, move-constructing it from the source..
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_type type of the new element.
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
                <i>Note</i>: be careful using void pointers: casts from/to a base to/from a derived class can be done only
                by the type system of the language.
            @return The associated transaction object.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_dyn_push_move example 1 */
        put_transaction<> start_dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            auto push_data = Base::inplace_allocate(detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.move_construct(push_data.m_user_storage, i_source);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return put_transaction<void>(PrivateType(), this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Tries to append at the end of the queue an element of type <code>ELEMENT_TYPE</code>, copy-constructing or move-constructing
            it from the source.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns false to indicate
            a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            @param i_progress_guarantee progress guarantee to respect
            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed
                - If this argument is an r-value, the new element move-constructed
            @return whether the put operation was successful


            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and the runtime type throws (conditionally noexcept)
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_push example 1 */
        template <typename ELEMENT_TYPE>
            bool try_push(progress_guarantee i_progress_guarantee, ELEMENT_TYPE && i_source)
                noexcept(noexcept(std::declval<lf_heter_queue>().template try_emplace<typename std::decay<ELEMENT_TYPE>::type>(i_progress_guarantee, std::forward<ELEMENT_TYPE>(i_source))))
        {
            return try_emplace<typename std::decay<ELEMENT_TYPE>::type>(i_progress_guarantee, std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Tries to append at the end of the queue an element of type <code>ELEMENT_TYPE</code>, inplace-constructing it from
            a perfect forwarded parameter pack.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns false to indicate
            a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            \n <i>Note</i>: the template argument ELEMENT_TYPE can't be deduced from the parameters so it must explicitly specified.

            @param i_progress_guarantee progress guarantee to respect
            @param i_construction_params construction parameters for the new element.
            @return whether the put operation was successful

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be constructible with <code>std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...</code>
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and the runtime type throws (conditionally noexcept)
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            bool try_emplace(progress_guarantee i_progress_guarantee, CONSTRUCTION_PARAMS && ... i_construction_params)
                noexcept(noexcept(std::declval<lf_heter_queue>().template try_start_emplace<ELEMENT_TYPE>(i_progress_guarantee,
                    std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...)))
        {
            auto tranasction = try_start_emplace<ELEMENT_TYPE>(i_progress_guarantee,
                std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
            if (!tranasction)
                return false;
            tranasction.commit();
            return true;
        }

        /** Tries to add at the end of the queue an element of a type known at runtime, default-constructing it.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns false to indicate
            a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            @param i_progress_guarantee progress guarantee to respect
            @param i_type type of the new element.
            @return whether the put operation was successful

            \n <b>Requires</b>:
                - The function <code>RUNTIME_TYPE::default_construct</code> must be invokable. If
                  <code>RUNTIMETYPE</code> is runtime_type this means that <code>default_construct</code> must be
                  included in the feature list.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and the runtime type throws
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_dyn_push example 1 */
        bool try_dyn_push(progress_guarantee i_progress_guarantee, const runtime_type & i_type)
        {
            auto tranasction = try_start_dyn_push(i_progress_guarantee, i_type);
            if (!tranasction)
                return false;
            tranasction.commit();
            return true;
        }

        /** Tries to add at the end of the queue an element of a type known at runtime, copy-constructing it from the source.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns false to indicate
            a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            @param i_progress_guarantee progress guarantee to respect
            @param i_type type of the new element.
            @param i_source pointer to the subobject of type <code>COMMON_TYPE</code> of an object or subobject of type <code>ELEMENT_TYPE</code>.
                <i>Note</i>: be careful using void pointers: only the compiler knows how to casts from/to a base to/from a derived class.
            @return whether the put operation was successful

            \n <b>Requires</b>:
                - The function <code>RUNTIME_TYPE::copy_construct</code> must be invokable. If
                  <code>RUNTIMETYPE</code> is runtime_type this means that <code>copy_construct</code> must be
                  included in the feature list.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and the runtime type throws
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_dyn_push_copy example 1 */
        bool try_dyn_push_copy(progress_guarantee i_progress_guarantee, const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            auto tranasction = try_start_dyn_push_copy(i_progress_guarantee, i_type, i_source);
            if (!tranasction)
                return false;
            tranasction.commit();
            return true;
        }

        /** Tries add at the end of the queue an element of a type known at runtime, move-constructing it from the source.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns false to indicate
            a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            @param i_progress_guarantee progress guarantee to respect
            @param i_type type of the new element
            @param i_source pointer to the subobject of type <code>COMMON_TYPE</code> of an object or subobject of type <code>ELEMENT_TYPE</code>
                <i>Note</i>: be careful using void pointers: casts from/to a base to/from a derived class can be done only
                by the type system of the language.
            @return whether the put operation was successful

            \n <b>Requires</b>:
                - The function <code>RUNTIME_TYPE::move_construct</code> must be invokable. If
                  <code>RUNTIMETYPE</code> is runtime_type this means that <code>move_construct</code> must be
                  included in the feature list.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and the runtime type throws
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_dyn_push_move example 1 */
        bool try_dyn_push_move(progress_guarantee i_progress_guarantee, const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            auto tranasction = try_start_dyn_push_move(i_progress_guarantee, i_type, i_source);
            if (!tranasction)
                return false;
            tranasction.commit();
            return true;
        }

        /** Tries to begin a transaction that appends an element of type <code>ELEMENT_TYPE</code>, copy-constructing or move-constructing it
            from the source.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns an empty transaction to
            indicate  a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            \n This function allocates the required space, constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_progress_guarantee progress guarantee to respect
            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed.
                - If this argument is an r-value, the new element move-constructed.
            @return The associated transaction object.

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and the runtime type throws (conditionally noexcept)
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_push example 1 */
        template <typename ELEMENT_TYPE>
            put_transaction<typename std::decay<ELEMENT_TYPE>::type> try_start_push(progress_guarantee i_progress_guarantee, ELEMENT_TYPE && i_source)
                noexcept(noexcept(std::declval<lf_heter_queue>().template try_start_emplace<typename std::decay<ELEMENT_TYPE>::type>(i_progress_guarantee, std::forward<ELEMENT_TYPE>(i_source))))
        {
            return try_start_emplace<typename std::decay<ELEMENT_TYPE>::type>(i_progress_guarantee, std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Tries to begin a transaction that appends an element of a type <code>ELEMENT_TYPE</code>,
            inplace-constructing it from a perfect forwarded parameter pack.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns an empty transaction to
            indicate  a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            \n This function allocates the required space, constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_progress_guarantee progress guarantee to respect
            @param i_construction_params construction parameters for the new element.
            @return The associated transaction object.

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be constructible with <code>std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...</code>
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and of the runtime type throws (conditionally noexcept)
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            put_transaction<ELEMENT_TYPE> try_start_emplace(progress_guarantee i_progress_guarantee, CONSTRUCTION_PARAMS && ... i_construction_params)
                noexcept(noexcept(ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...)) &&
                    noexcept(runtime_type(runtime_type::template make<ELEMENT_TYPE>())) )
        {
            static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
                "ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");

            auto push_data = Base::template try_inplace_allocate<detail::NbQueue_Busy, true, detail::size_of<ELEMENT_TYPE>::value, alignof(ELEMENT_TYPE)>(i_progress_guarantee);
            if (push_data.m_user_storage == nullptr)
            {
                return put_transaction<ELEMENT_TYPE>();
            }

            bool is_noexcept = std::is_nothrow_constructible<ELEMENT_TYPE, CONSTRUCTION_PARAMS...>::value &&
                noexcept(runtime_type(runtime_type::template make<ELEMENT_TYPE>()));

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;

            if (is_noexcept)
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(runtime_type::template make<ELEMENT_TYPE>());

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = new (push_data.m_user_storage) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
            }
            else
            {
                try
                {
                    auto const type_storage = Base::type_after_control(push_data.m_control_block);
                    DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                    type = new (type_storage) runtime_type(runtime_type::template make<ELEMENT_TYPE>());

                    DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                    element = new (push_data.m_user_storage) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
                }
                catch (...)
                {
                    if (type != nullptr)
                        type->RUNTIME_TYPE::~RUNTIME_TYPE();

                    Base::cancel_put_nodestroy_impl(push_data);
                    #ifdef _MSC_VER
                        #pragma warning(push)
                        #pragma warning(disable:4297) // function assumed not to throw an exception but does
                    #elif defined(__GNUG__) && !defined(__clang__)
                        #pragma GCC diagnostic push
                        #pragma GCC diagnostic ignored "-Wterminate"
                    #endif
                    throw;
                    #ifdef _MSC_VER
                        #pragma warning(pop)
                    #elif defined(__GNUG__) && !defined(__clang__)
                        #pragma GCC diagnostic pop
                    #endif
                }
            }

            return put_transaction<ELEMENT_TYPE>(PrivateType(),
                this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Tries to begin a transaction that appends an element of a type known at runtime, default-constructing it.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns an empty transaction to
            indicate  a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_progress_guarantee progress guarantee to respect
            @param i_type type of the new element.
            @return The associated transaction object.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and the runtime type throws
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_dyn_push example 1 */
        put_transaction<> try_start_dyn_push(progress_guarantee i_progress_guarantee, const runtime_type & i_type)
        {
            auto push_data = Base::try_inplace_allocate(i_progress_guarantee, detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());
            if (push_data.m_user_storage == nullptr)
            {
                return put_transaction<>();
            }

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.default_construct(push_data.m_user_storage);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();

                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return put_transaction<void>(PrivateType(), this, push_data, std::is_void<COMMON_TYPE>(), element);
        }


        /** Tries to begin a transaction that appends an element of a type known at runtime, copy-constructing it from the source.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns an empty transaction to
            indicate  a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_progress_guarantee progress guarantee to respect
            @param i_type type of the new element.
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
                <i>Note</i>: be careful using void pointers: casts from/to a base to/from a derived class can be done only
                by the type system of the language.
            @return The associated transaction object.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and the runtime type throws
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_dyn_push_copy example 1 */
        put_transaction<> try_start_dyn_push_copy(progress_guarantee i_progress_guarantee, const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            auto push_data = Base::try_inplace_allocate(i_progress_guarantee, detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());
            if (push_data.m_user_storage == nullptr)
            {
                return put_transaction<>();
            }

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.copy_construct(push_data.m_user_storage, i_source);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return put_transaction<void>(PrivateType(), this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Tries to begin a transaction that appends an element of a type known at runtime, move-constructing it from the source.
            \n If the put operation can't be completed with the specified progress guarantee, this function returns an empty transaction to
            indicate  a failure, and has no observable effects. This function fails if:
            - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                the blocking progress guarantee indicates an out of memory, but no exception is thrown.
            - there is a contention between threads, and the wait-free progress guarantee is requested
            - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
            If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
            Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
            function is called in this timespan by the same thread, the behavior is undefined.

            @param i_progress_guarantee progress guarantee to respect
            @param i_type type of the new element.
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
                <i>Note</i>: be careful using void pointers: casts from/to a base to/from a derived class can be done only
                by the type system of the language.
            @return The associated transaction object.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: what the construction of the element and the runtime type throws
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_dyn_push_move example 1 */
        put_transaction<> try_start_dyn_push_move(progress_guarantee i_progress_guarantee, const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            auto push_data = Base::try_inplace_allocate(i_progress_guarantee, detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());
            if (push_data.m_user_storage == nullptr)
            {
                return put_transaction<>();
            }

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.move_construct(push_data.m_user_storage, i_source);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return put_transaction<void>(PrivateType(), this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Removes and destroy the first element of the queue, if the queue is not empty. Otherwise it has no effect.
            This function discards the element. Use a consume function if you want to access the element before it
            gets destroyed.

            @return whether an element was actually removed.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: nothing
        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_pop example 1 */
        bool try_pop() noexcept
        {
            if (auto operation = try_start_consume())
            {
                operation.commit();
                return true;
            }
            return false;
        }

        /** Tries to start a consume operation.
            @return a consume_operation which is empty if there are no elements to consume

            A non-empty consume must be committed for the consume to have effect.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_consume example 1 */
        consume_operation try_start_consume() noexcept
        {
            return consume_operation(PrivateType(), this);
        }

        /** Tries to start a consume operation using an existing consume_operation object.
            @param i_consume reference to a consume_operation to be used. If it is non-empty
                it gets canceled before trying to start the new consume.
            @return whether i_consume is non-empty after the call, that is whether the queue was
                not empty.

            A non-empty consume must be committed for the consume to have effect.

            This overload is similar to the one taking no arguments and returning a consume_operation,
            but it's more efficient if the next consumable element is in the same page of the last
            element i_consume has visited because in this case it doesn't perform page pinning.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_consume_ example 1 */
        bool try_start_consume(consume_operation & i_consume) noexcept
        {
            return i_consume.start_consume_impl(PrivateType(), this);
        }


        /** Move-only class template that can be bound to a reentrant put transaction, otherwise it's empty.

            @tparam ELEMENT_COMPLETE_TYPE Complete type of elements that can be handled by a transaction, or void.
                ELEMENT_COMPLETE_TYPE must decay to itself (it can't be cv-qualified).

            Reentrant transactional put functions on lf_heter_queue return a non-empty reentrant_put_transaction that can be
            used to allocate raw memory in the queue, inspect or alter the element while it is still not observable
            in the queue, and commit or cancel the push.

            A reentrant_put_transaction is empty when:
                - it is default constructed
                - it is used as source for a move construction or move assignment
                - after a cancel or a commit

            Calling \ref reentrant_put_transaction::raw_allocate "raw_allocate", \ref reentrant_put_transaction::raw_allocate_copy "raw_allocate_copy",
            \ref reentrant_put_transaction::commit "commit", \ref reentrant_put_transaction::cancel "cancel", \ref reentrant_put_transaction::element_ptr "element_ptr",
            \ref reentrant_put_transaction::element "element" or \ref reentrant_put_transaction::complete_type "complete_type" on an empty reentrant_put_transaction
            triggers undefined behavior.

            A void reentrant_put_transaction can be move constructed/assigned from any reentrant_put_transaction. A typed reentrant_put_transaction
            can be move constructed/assigned only from a reentrant_put_transaction with the same ELEMENT_COMPLETE_TYPE. */
        template <typename ELEMENT_COMPLETE_TYPE = void>
            class reentrant_put_transaction
        {
            static_assert(std::is_same<ELEMENT_COMPLETE_TYPE, typename std::decay<ELEMENT_COMPLETE_TYPE>::type>::value, "");

        public:

            /** Constructs an empty put transaction */
            reentrant_put_transaction() noexcept = default;

            /** Copy construction is not allowed.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction copy_construct example 1 */
            reentrant_put_transaction(const reentrant_put_transaction &) = delete;

            /** Copy assignment is not allowed.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction copy_assign example 1 */
            reentrant_put_transaction & operator = (const reentrant_put_transaction &) = delete;

            /** Move constructs a reentrant_put_transaction, transferring the state from the source.
                    @param i_source source to move from. It is left in a valid but indeterminate state.


            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction move_construct example 1

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction move_construct example 2 */
            template <typename OTHERTYPE, typename = typename std::enable_if<
                    std::is_same<OTHERTYPE, ELEMENT_COMPLETE_TYPE>::value || std::is_void<ELEMENT_COMPLETE_TYPE>::value >::type >
                reentrant_put_transaction(reentrant_put_transaction<OTHERTYPE> && i_source) noexcept
                    : m_put(i_source.m_put), m_queue(i_source.m_queue)
            {
                i_source.m_put.m_user_storage = nullptr;
            }

            /** Move assigns a reentrant_put_transaction, transferring the state from the source.
                @param i_source source to move from. It is left in a valid but indeterminate state.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction move_assign example 1
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction move_assign example 2 */
            template <typename OTHERTYPE, typename = typename std::enable_if<
                    std::is_same<OTHERTYPE, ELEMENT_COMPLETE_TYPE>::value || std::is_void<ELEMENT_COMPLETE_TYPE>::value >::type >
                reentrant_put_transaction & operator = (reentrant_put_transaction<OTHERTYPE> && i_source) noexcept
            {
                using std::swap;
                swap(m_put, i_source.m_put);
                swap(m_queue, i_source.m_queue);
                return *this;
            }

            /** Swaps two instances of reentrant_put_transaction.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction swap example 1 */
            friend void swap(reentrant_put_transaction & i_first, reentrant_put_transaction & i_second) noexcept
            {
                using std::swap;
                swap(i_first.m_put, i_second.m_put);
                swap(i_first.m_queue, i_second.m_queue);
            }

            /** Allocates a memory block associated to the element being added in the queue. The block may be allocated contiguously with
                the elements in the memory pages. If the block does not fit in one page, the block is allocated using non-paged memory services
                of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_size size in bytes of the block to allocate.
                @param i_alignment alignment of the block to allocate. It must be a non-zero power of 2, and less than or
                        equal to i_size.

                \pre The behavior is undefined if either:
                    - this transaction is empty
                    - the alignment is not valid
                    - the size is not a multiple of the alignment

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction raw_allocate example 1*/
            void * raw_allocate(size_t i_size, size_t i_alignment)
            {
                DENSITY_ASSERT(!empty());
                auto push_data = m_queue->inplace_allocate(detail::NbQueue_Dead, false, i_size, i_alignment);
                return push_data.m_user_storage;
            }

            /** Allocates a memory block associated to the element being added in the queue, and copies the content from a range
                of iterators. The block may be allocated contiguously with the elements in the memory pages. If the block does not
                fit in one page, the block is allocated using non-paged memory services of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_begin first element to be copied
                @param i_end first element not to be copied

                \n <b>Requires</b>:
                    - INPUT_ITERATOR must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
                    - the value type must be trivially destructible

                \pre The behavior is undefined if either:
                    - this transaction is empty
                    - i_end is not reachable from i_begin

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects)

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction raw_allocate_copy example 1*/
            template <typename INPUT_ITERATOR>
                typename std::iterator_traits<INPUT_ITERATOR>::value_type *
                    raw_allocate_copy(INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end)
            {
                using ValueType = typename std::iterator_traits<INPUT_ITERATOR>::value_type;
                static_assert(std::is_trivially_destructible<ValueType>::value,
                    "raw_allocate_copy provides a raw memory inplace allocation that does not invoke destructors when deallocating");

                auto const count_s = std::distance(i_begin, i_end);
                auto const count = static_cast<size_t>(count_s);
                DENSITY_ASSERT(static_cast<decltype(count_s)>(count) == count_s);

                auto const elements = static_cast<ValueType*>(raw_allocate(sizeof(ValueType) * count, alignof(ValueType)));
                for (auto curr = elements; i_begin != i_end; ++i_begin, ++curr)
                    new(curr) ValueType(*i_begin);
                return elements;
            }

            /** Allocates a memory block associated to the element being added in the queue, and copies the content from a range.
                The block may be allocated contiguously with the elements in the memory pages. If the block does not
                fit in one page, the block is allocated using non-paged memory services of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_source_range to be copied

                \n <b>Requires</b>:
                    - The iterators of the range must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
                    - the value type must be trivially destructible

                \pre The behavior is undefined if either:
                    - this transaction is empty

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction raw_allocate_copy example 2 */
            template <typename INPUT_RANGE>
                auto raw_allocate_copy(const INPUT_RANGE & i_source_range)
                    -> decltype(raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range)))
            {
                return raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range));
            }

            /** Tries to allocates a memory block associated to the element being added in the queue. The block may be allocated contiguously with
                the elements in the memory pages. If the block does not fit in one page, the block is allocated using non-paged memory services
                of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                \n If the operation can't be completed with the specified progress guarantee, this function returns nullptr to indicate
                a failure, and has no observable effects. This function fails if:
                - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                    the blocking progress guarantee indicates an out of memory, but no exception is thrown.
                - there is a contention between threads, and the wait-free progress guarantee is requested
                - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

                @param i_progress_guarantee progress guarantee to respect
                @param i_size size in bytes of the block to allocate.
                @param i_alignment alignment of the block to allocate. It must be a non-zero power of 2, and less than or
                        equal to i_size.
                @return pointer to the allocated storage, or null in case of failure

                \pre The behavior is undefined if either:
                    - this transaction is empty
                    - the alignment is not valid
                    - the size is not a multiple of the alignment

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: nothing.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction try_raw_allocate example 1*/
            void * try_raw_allocate(progress_guarantee i_progress_guarantee, size_t i_size, size_t i_alignment) noexcept
            {
                DENSITY_ASSERT(!empty());
                auto push_data = m_queue->try_inplace_allocate(i_progress_guarantee, detail::NbQueue_Dead, false, i_size, i_alignment);
                return push_data.m_user_storage;
            }

            /** Tries to allocate a memory block associated to the element being added in the queue, and copies the content from a range
                of iterators. The block may be allocated contiguously with the elements in the memory pages. If the block does not
                fit in one page, the block is allocated using non-paged memory services of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                \n If the operation can't be completed with the specified progress guarantee, this function returns nullptr to indicate
                a failure, and has no observable effects. This function fails if:
                - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                    the blocking progress guarantee indicates an out of memory, but no exception is thrown.
                - there is a contention between threads, and the wait-free progress guarantee is requested
                - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

                @param i_progress_guarantee progress guarantee to respect
                @param i_begin first element to be copied
                @param i_end first element not to be copied
                @return pointer to the allocated array, or null in case of failure

                \n <b>Requires</b>:
                    - INPUT_ITERATOR must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
                    - the value type must be trivially destructible

                \pre The behavior is undefined if either:
                    - this transaction is empty
                    - i_end is not reachable from i_begin

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: what the copy constructor of the element throws (conditionally noexcept)
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects)

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction try_raw_allocate_copy example 1*/
            template <typename INPUT_ITERATOR>
                typename std::iterator_traits<INPUT_ITERATOR>::value_type *
                    try_raw_allocate_copy(progress_guarantee i_progress_guarantee, INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end)
                        noexcept(std::is_nothrow_copy_constructible<typename std::iterator_traits<INPUT_ITERATOR>::value_type>::value)
            {
                using ValueType = typename std::iterator_traits<INPUT_ITERATOR>::value_type;
                static_assert(std::is_trivially_destructible<ValueType>::value,
                    "raw_allocate_copy provides a raw memory inplace allocation that does not invoke destructors when deallocating");

                auto const count_s = std::distance(i_begin, i_end);
                auto const count = static_cast<size_t>(count_s);
                DENSITY_ASSERT(static_cast<decltype(count_s)>(count) == count_s);

                auto const elements = static_cast<ValueType*>(try_raw_allocate(i_progress_guarantee, sizeof(ValueType) * count, alignof(ValueType)));
                if (elements != nullptr)
                {
                    for (auto curr = elements; i_begin != i_end; ++i_begin, ++curr)
                        new(curr) ValueType(*i_begin);
                }
                return elements;
            }

            /** Tries to allocate a memory block associated to the element being added in the queue, and copies the content from a range.
                The block may be allocated contiguously with the elements in the memory pages. If the block does not
                fit in one page, the block is allocated using non-paged memory services of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                \n If the operation can't be completed with the specified progress guarantee, this function returns nullptr to indicate
                a failure, and has no observable effects. This function fails if:
                - a memory allocation is necessary but the allocator can't complete it with the specified progress guarantee. A failure with
                    the blocking progress guarantee indicates an out of memory, but no exception is thrown.
                - there is a contention between threads, and the wait-free progress guarantee is requested
                - the algorithm would perform some non wait-free operations (like page pinning), and the wait-free progress guarantee is requested

                @param i_progress_guarantee progress guarantee to respect
                @param i_source_range to be copied
                @return pointer to the allocated array, or null in case of failure

                \n <b>Requires</b>:
                    - The iterators of the range must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
                    - the value type must be trivially destructible

                \pre The behavior is undefined if either:
                    - this transaction is empty

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: what the copy constructor of the element throws (conditionally noexcept)
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction try_raw_allocate_copy example 2 */
            template <typename INPUT_RANGE>
                auto try_raw_allocate_copy(progress_guarantee i_progress_guarantee, const INPUT_RANGE & i_source_range)
                    noexcept(noexcept(std::declval<reentrant_put_transaction>().try_raw_allocate_copy(i_progress_guarantee, std::begin(i_source_range), std::end(i_source_range))))
                        -> decltype(try_raw_allocate_copy(i_progress_guarantee, std::begin(i_source_range), std::end(i_source_range)))
            {
                return try_raw_allocate_copy(i_progress_guarantee,
                    std::begin(i_source_range), std::end(i_source_range));
            }

            /** Makes the effects of the transaction observable. This object becomes empty.

                \pre The behavior is undefined if either:
                    - this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
                DENSITY_ASSERT(!empty());
                Base::commit_put_impl(m_put);
                m_put.m_user_storage = nullptr;
            }

            /** Cancel the transaction. This object becomes empty.

                \pre The behavior is undefined if either:
                    - this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction cancel example 1 */
            void cancel() noexcept
            {
                DENSITY_ASSERT(!empty());
                Base::cancel_put_impl(m_put);
                m_put.m_user_storage = nullptr;
            }

            /** Returns true whether this object does not hold the state of a transaction.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction empty example 1 */
            bool empty() const noexcept
            {
                return m_put.m_user_storage == nullptr;
            }

            /** Returns true whether this object is bound to a transaction. Same to !consume_operation::empty.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction operator_bool example 1 */
            explicit operator bool() const noexcept
            {
                return m_put.m_user_storage != nullptr;
            }

            /** Returns a pointer to the target queue if a transaction is bound, otherwise returns nullptr

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction queue example 1 */
            lf_heter_queue * queue() const noexcept
            {
                return m_put.m_user_storage != nullptr ? m_queue : nullptr;
            }

            /** Returns a pointer to the object being added.
                \n <i>Notes</i>:
                - The object is constructed when the transaction is started, so this function always returns a
                    pointer to a valid object.
                - This function returns a pointer to the common_type subobject of the element. Non-dynamic
                    transactional put function (start_push, start_emplace, start_reentrant_push, start_reentrant_emplace)
                    return a typed_put_transaction or a reentrant_put_transaction, that provide the function element(),
                    which is a better alternative to this function

                \pre The behavior is undefined if either:
                    - this transaction is empty

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction element_ptr example 1

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction element_ptr example 2 */
            common_type * element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return m_put.m_user_storage;
            }

            /** Returns a reference to the element being added. This function can be used to modify the element
                    before calling the commit.
                \n <i>Note</i>: An element is observable in the queue only after commit has been called on the
                    reentrant_put_transaction. The element is constructed at the begin of the transaction, so the
                    returned object is always valid.

                \n <b>Requires</b>:
                    - ELEMENT_COMPLETE_TYPE must not be void

                \pre The behavior is undefined if:
                    - this transaction is empty

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue typed_put_transaction element example 1 */
            #ifndef DOXYGEN_DOC_GENERATION
            template <typename EL = ELEMENT_COMPLETE_TYPE>
                typename std::enable_if<!std::is_void<EL>::value && std::is_same<EL, ELEMENT_COMPLETE_TYPE>::value, EL>::type &
            #else
                ELEMENT_COMPLETE_TYPE &
            #endif
                    element() const noexcept
            {
                return *static_cast<ELEMENT_COMPLETE_TYPE *>(element_ptr());
            }

            /** Returns the type of the object being added.

                \pre The behavior is undefined if either:
                    - this transaction is empty

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction complete_type example 1 */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return *Base::type_after_control(m_put.m_control_block);
            }

            /** If this transaction is empty the destructor has no side effects. Otherwise it cancels it.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_put_transaction destroy example 1 */
            ~reentrant_put_transaction()
            {
                if (m_put.m_user_storage != nullptr)
                {
                    Base::cancel_put_impl(m_put);
                }
            }

            /** \internal - private function, usable only within the library */
            reentrant_put_transaction(PrivateType, lf_heter_queue * i_queue, const Block & i_put,
                    std::false_type /*i_is_void*/, COMMON_TYPE * i_element) noexcept
                : m_put(i_put), m_queue(i_queue)
            {
                m_put.m_user_storage = i_element;
                m_put.m_control_block->m_element = i_element;
            }

            /** \internal - private function, usable only within the library */
            reentrant_put_transaction(PrivateType, lf_heter_queue * i_queue, const Block & i_put,
                    std::true_type /*i_is_void*/, void *) noexcept
                : m_put(i_put), m_queue(i_queue)
            {
            }

        private:
            Block m_put;
            lf_heter_queue * m_queue = nullptr;
            template <typename OTHERTYPE> friend class reentrant_put_transaction;
        };


        /** Move-only class that can be bound to a reentrant consume operation, otherwise it's empty.

        Reentrant consume functions on lf_heter_queue return a non-empty reentrant_consume_operation that can be
            used to inspect or alter the element while it is not observable in the queue, and
            commit or cancel the consume.

            A reentrant_consume_operation is empty when:
                - it is default constructed
                - it is used as source for a move construction or move assignment
                - after a cancel or a commit

            Calling \ref reentrant_consume_operation::commit "commit", \ref reentrant_consume_operation::commit_nodestroy "commit_nodestroy",
            \ref reentrant_consume_operation::cancel "cancel", \ref reentrant_consume_operation::element_ptr "element_ptr",
            \ref reentrant_consume_operation::unaligned_element_ptr "unaligned_element_ptr",
            \ref reentrant_consume_operation::element "element" or \ref reentrant_consume_operation::complete_type "complete_type" on
            an empty reentrant_consume_operation triggers undefined behavior. */
        class reentrant_consume_operation
        {
        public:

            /** Constructs an empty reentrant_consume_operation

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation default_construct example 1 */
            reentrant_consume_operation() noexcept = default;

            /** Copy construction is not allowed

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation copy_construct example 1 */
            reentrant_consume_operation(const reentrant_consume_operation &) = delete;

            /** Copy assignment is not allowed

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation copy_assign example 1 */
            reentrant_consume_operation & operator = (const reentrant_consume_operation &) = delete;

            /** Move constructor. It is left in a valid but indeterminate state.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation move_construct example 1 */
            reentrant_consume_operation(reentrant_consume_operation && i_source) noexcept = default;

            /** Move assignment. It is left in a valid but indeterminate state.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation move_assign example 1 */
            reentrant_consume_operation & operator = (reentrant_consume_operation && i_source) noexcept = default;

            /** Destructor: cancel the operation (if any).

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation destroy example 1 */
            ~reentrant_consume_operation()
            {
                if (m_consume_data.m_next_ptr != 0)
                {
                    m_consume_data.cancel_consume_impl();
                }
            }

            /** Swaps two instances of reentrant_consume_operation.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation swap example 1 */
            friend void swap(reentrant_consume_operation & i_first, reentrant_consume_operation & i_second) noexcept
            {
                i_first.m_consume_data.swap(i_second.m_consume_data);
            }

            /** Returns true whether this object does not hold the state of an operation.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation empty example 1 */
            bool empty() const noexcept { return m_consume_data.m_next_ptr == 0; }

            /** Returns true whether this object does not hold the state of an operation. Same to !reentrant_consume_operation::empty.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation operator_bool example 1 */
            explicit operator bool() const noexcept
            {
                return m_consume_data.m_next_ptr != 0;
            }

            /** Returns a pointer to the target queue if a transaction is bound, otherwise returns nullptr

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation queue example 1 */
            lf_heter_queue * queue() const noexcept { return static_cast<lf_heter_queue*>(m_consume_data.m_queue); }

            /** Destroys the element, making the consume irreversible. This comnsume_operation becomes empty.

                \pre The behavior is undefined if either:
                    - this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
                DENSITY_ASSERT(!empty());

                auto const & type = complete_type();
                auto const element = element_ptr();
                type.destroy(element);

                type.RUNTIME_TYPE::~RUNTIME_TYPE();

                m_consume_data.commit_consume_impl();
            }

            /** Destroys the element, making the consume irreversible. This comnsume_operation becomes empty.
                The caller should destroy the element before calling this function.
                This object becomes empty.

                \pre The behavior is undefined if either:
                    - this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing.

            Note: this function may be used to combine a feature of the runtime type on the element with the
                destruction of the element. For example a function queue may improve the performances using a feature
                like invoke_destroy to do both the function call and the destruction of the capture in a single call,
                making a single pseudo v-call instead of two.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation commit_nodestroy example 1 */
            void commit_nodestroy() noexcept
            {
                DENSITY_ASSERT(!empty());

                bool destroy_type = !std::is_trivially_destructible<RUNTIME_TYPE>::value;
                if (destroy_type)
                {
                    auto const & type = complete_type();
                    type.RUNTIME_TYPE::~RUNTIME_TYPE();
                }

                m_consume_data.commit_consume_impl();
            }

             /** Cancel the operation. This reentrant_consume_operation becomes empty.

                \pre The behavior is undefined if either:
                    - this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation cancel example 1 */
            void cancel() noexcept
            {
                DENSITY_ASSERT(!empty());
                m_consume_data.cancel_consume_impl();
            }

            /** Returns the type of the element being consumed.

                \pre The behavior is undefined if this reentrant_consume_operation is empty.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation complete_type example 1 */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return *Base::type_after_control(m_consume_data.m_control);
            }

            /** Returns a pointer that, if properly aligned to the alignment of the element type, points to the element.
                The returned address is guaranteed to be aligned to min_alignment

                \pre The behavior is undefined if this reentrant_consume_operation is empty, that is it has been used as source for a move operation.
                \post The returned address is aligned at least on lf_heter_queue::min_alignment.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation unaligned_element_ptr example 1 */
            void * unaligned_element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return Base::get_unaligned_element(m_consume_data.m_control);
            }

            /** Returns a pointer to the element being consumed.

                This call is equivalent to: <code>address_upper_align(unaligned_element_ptr(), complete_type().alignment());</code>

                \pre The behavior is undefined if this reentrant_consume_operation is empty, that is it has been committed or used as source for a move operation.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation element_ptr example 1 */
            COMMON_TYPE * element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return Base::get_element(m_consume_data.m_control);
            }

            /** Returns a reference to the element being consumed.

                \pre The behavior is undefined if this reentrant_consume_operation is empty, that is it has been committed or used as source for a move operation.
                \pre The behavior is undefined if COMPLETE_ELEMENT_TYPE is not exactly the complete type of the element.

                \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_consume_operation element example 1 */
            template <typename COMPLETE_ELEMENT_TYPE>
                COMPLETE_ELEMENT_TYPE & element() const noexcept
            {
                DENSITY_ASSERT(!empty() && complete_type().template is<COMPLETE_ELEMENT_TYPE>());
                return *static_cast<COMPLETE_ELEMENT_TYPE*>(Base::get_element(m_consume_data.m_control));
            }

            /** \internal - private function, usable only within the library */
            reentrant_consume_operation(PrivateType, lf_heter_queue * i_queue) noexcept
            {
                m_consume_data.start_consume_impl(i_queue);
            }

            /** \internal - private function, usable only within the library */
            bool start_consume_impl(PrivateType, lf_heter_queue * i_queue)
            {
                if(m_consume_data.m_next_ptr != 0)
                {
                    m_consume_data.cancel_consume_impl();
                }

                m_consume_data.start_consume_impl(i_queue);

                return m_consume_data.m_next_ptr != 0;
            }

        private:
            Consume m_consume_data;
        };

        /** Same to lf_heter_queue::push, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            void reentrant_push(ELEMENT_TYPE && i_source)
        {
            return reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to lf_heter_queue::emplace, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            void reentrant_emplace(CONSTRUCTION_PARAMS &&... i_construction_params)
        {
            start_reentrant_emplace<ELEMENT_TYPE>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
        }

        /** Same to lf_heter_queue::dyn_push, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_dyn_push example 1 */
        void reentrant_dyn_push(const runtime_type & i_type)
        {
            start_reentrant_dyn_push(i_type).commit();
        }

        /** Same to lf_heter_queue::dyn_push_copy, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_dyn_push_copy example 1 */
        void reentrant_dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_copy(i_type, i_source).commit();
        }

        /** Same to lf_heter_queue::dyn_push_move, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue reentrant_dyn_push_move example 1 */
        void reentrant_dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_move(i_type, i_source).commit();
        }

        /** Same to lf_heter_queue::start_push, but allows reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            reentrant_put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_reentrant_push(ELEMENT_TYPE && i_source)
        {
            return start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to lf_heter_queue::start_emplace, but allows reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            reentrant_put_transaction<ELEMENT_TYPE> start_reentrant_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
                "ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");

            auto push_data = Base::template inplace_allocate<detail::NbQueue_Busy, true, detail::size_of<ELEMENT_TYPE>::value, alignof(ELEMENT_TYPE)>();

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(runtime_type::template make<ELEMENT_TYPE>());

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = new (push_data.m_user_storage) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return reentrant_put_transaction<ELEMENT_TYPE>(PrivateType(),
                this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Same to lf_heter_queue::start_dyn_push, but allows reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_reentrant_dyn_push example 1 */
        reentrant_put_transaction<> start_reentrant_dyn_push(const runtime_type & i_type)
        {
            auto push_data = Base::inplace_allocate(detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.default_construct(push_data.m_user_storage);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return reentrant_put_transaction<void>(PrivateType(),
                this, push_data, std::is_void<COMMON_TYPE>(), element);
        }


        /** Same to lf_heter_queue::start_dyn_push_copy, but allows reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_reentrant_dyn_push_copy example 1 */
        reentrant_put_transaction<> start_reentrant_dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            auto push_data = Base::inplace_allocate(detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.copy_construct(push_data.m_user_storage, i_source);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return reentrant_put_transaction<void>(PrivateType(),
                this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Same to lf_heter_queue::start_dyn_push_move, but allows reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue start_reentrant_dyn_push_move example 1 */
        reentrant_put_transaction<> start_reentrant_dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            auto push_data = Base::inplace_allocate(detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.move_construct(push_data.m_user_storage, i_source);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return reentrant_put_transaction<void>(PrivateType(),
                this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Same to lf_heter_queue::try_push, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            bool try_reentrant_push(progress_guarantee i_progress_guarantee, ELEMENT_TYPE && i_source)
                noexcept(noexcept(std::declval<lf_heter_queue>().template try_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(i_progress_guarantee, std::forward<ELEMENT_TYPE>(i_source))))
        {
            return try_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(i_progress_guarantee, std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to lf_heter_queue::try_emplace, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            bool try_reentrant_emplace(progress_guarantee i_progress_guarantee, CONSTRUCTION_PARAMS && ... i_construction_params)
                noexcept(noexcept(std::declval<lf_heter_queue>().template try_start_reentrant_emplace<ELEMENT_TYPE>(i_progress_guarantee,
                    std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...)))
        {
            auto tranasction = try_start_reentrant_emplace<ELEMENT_TYPE>(i_progress_guarantee,
                std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
            if (!tranasction)
                return false;
            tranasction.commit();
            return true;
        }

        /** Same to lf_heter_queue::try_dyn_push, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_reentrant_dyn_push example 1 */
        bool try_reentrant_dyn_push(progress_guarantee i_progress_guarantee, const runtime_type & i_type)
        {
            auto tranasction = try_start_reentrant_dyn_push(i_progress_guarantee, i_type);
            if (!tranasction)
                return false;
            tranasction.commit();
            return true;
        }

        /** Same to lf_heter_queue::try_dyn_push_copy, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_reentrant_dyn_push_copy example 1 */
        bool try_reentrant_dyn_push_copy(progress_guarantee i_progress_guarantee, const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            auto tranasction = try_start_reentrant_dyn_push_copy(i_progress_guarantee, i_type, i_source);
            if (!tranasction)
                return false;
            tranasction.commit();
            return true;
        }

        /** Same to lf_heter_queue::try_dyn_push_move, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_reentrant_dyn_push_move example 1 */
        bool try_reentrant_dyn_push_move(progress_guarantee i_progress_guarantee, const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            auto tranasction = try_start_reentrant_dyn_push_move(i_progress_guarantee, i_type, i_source);
            if (!tranasction)
                return false;
            tranasction.commit();
            return true;
        }

        /** Same to lf_heter_queue::try_start_push, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            reentrant_put_transaction<typename std::decay<ELEMENT_TYPE>::type> try_start_reentrant_push(progress_guarantee i_progress_guarantee, ELEMENT_TYPE && i_source)
                noexcept(noexcept(std::declval<lf_heter_queue>().template try_start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(i_progress_guarantee, std::forward<ELEMENT_TYPE>(i_source))))
        {
            return try_start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(i_progress_guarantee, std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to lf_heter_queue::try_start_emplace, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            reentrant_put_transaction<ELEMENT_TYPE> try_start_reentrant_emplace(progress_guarantee i_progress_guarantee, CONSTRUCTION_PARAMS && ... i_construction_params)
                noexcept(noexcept(ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...)) &&
                    noexcept(runtime_type(runtime_type::template make<ELEMENT_TYPE>())) )
        {
            static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
                "ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");

            auto push_data = Base::template try_inplace_allocate<detail::NbQueue_Busy, true, detail::size_of<ELEMENT_TYPE>::value, alignof(ELEMENT_TYPE)>(i_progress_guarantee);
            if (push_data.m_user_storage == nullptr)
            {
                return reentrant_put_transaction<ELEMENT_TYPE>();
            }

            bool is_noexcept = std::is_nothrow_constructible<ELEMENT_TYPE, CONSTRUCTION_PARAMS...>::value &&
                noexcept(runtime_type(runtime_type::template make<ELEMENT_TYPE>()));

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;

            if (is_noexcept)
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(runtime_type::template make<ELEMENT_TYPE>());

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = new (push_data.m_user_storage) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
            }
            else
            {
                try
                {
                    auto const type_storage = Base::type_after_control(push_data.m_control_block);
                    DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                    type = new (type_storage) runtime_type(runtime_type::template make<ELEMENT_TYPE>());

                    DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                    element = new (push_data.m_user_storage) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
                }
                catch (...)
                {
                    if (type != nullptr)
                        type->RUNTIME_TYPE::~RUNTIME_TYPE();

                    Base::cancel_put_nodestroy_impl(push_data);
                    #ifdef _MSC_VER
                        #pragma warning(push)
                        #pragma warning(disable:4297) // function assumed not to throw an exception but does
                    #elif defined(__GNUG__) && !defined(__clang__)
                        #pragma GCC diagnostic push
                        #pragma GCC diagnostic ignored "-Wterminate"
                    #endif
                    throw;
                    #ifdef _MSC_VER
                        #pragma warning(pop)
                    #elif defined(__GNUG__) && !defined(__clang__)
                        #pragma GCC diagnostic pop
                    #endif
                }
            }

            return reentrant_put_transaction<ELEMENT_TYPE>(PrivateType(),
                this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Same to lf_heter_queue::try_start_dyn_push, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_reentrant_dyn_push example 1 */
        reentrant_put_transaction<> try_start_reentrant_dyn_push(progress_guarantee i_progress_guarantee, const runtime_type & i_type)
        {
            auto push_data = Base::try_inplace_allocate(i_progress_guarantee, detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());
            if (push_data.m_user_storage == nullptr)
            {
                return reentrant_put_transaction<>();
            }

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.default_construct(push_data.m_user_storage);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();

                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return reentrant_put_transaction<void>(PrivateType(), this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Same to lf_heter_queue::try_start_dyn_push_copy, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_reentrant_dyn_push_copy example 1 */
        reentrant_put_transaction<> try_start_reentrant_dyn_push_copy(progress_guarantee i_progress_guarantee, const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            auto push_data = Base::try_inplace_allocate(i_progress_guarantee, detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());
            if (push_data.m_user_storage == nullptr)
            {
                return reentrant_put_transaction<>();
            }

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.copy_construct(push_data.m_user_storage, i_source);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return reentrant_put_transaction<void>(PrivateType(), this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Same to lf_heter_queue::try_start_dyn_push_move, but allows reentrancy: during the construction of the element the queue is in a
            valid state.

            <b>Examples</b>
            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_reentrant_dyn_push_move example 1 */
        reentrant_put_transaction<> try_start_reentrant_dyn_push_move(progress_guarantee i_progress_guarantee, const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            auto push_data = Base::try_inplace_allocate(i_progress_guarantee, detail::NbQueue_Busy, true, i_type.size(), i_type.alignment());
            if (push_data.m_user_storage == nullptr)
            {
                return reentrant_put_transaction<>();
            }

            COMMON_TYPE * element = nullptr;
            runtime_type * type = nullptr;
            try
            {
                auto const type_storage = Base::type_after_control(push_data.m_control_block);
                DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
                type = new (type_storage) runtime_type(i_type);

                DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
                element = i_type.move_construct(push_data.m_user_storage, i_source);
            }
            catch (...)
            {
                if (type != nullptr)
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                Base::cancel_put_nodestroy_impl(push_data);
                throw;
            }

            return reentrant_put_transaction<void>(PrivateType(), this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Removes and destroy the first element of the queue, if the queue is not empty. Otherwise it has no effect.
            This is the reentrant version of try_pop.
            This function discards the element. Use a consume function if you want to access the element before it
            gets destroyed.

            @return whether an element was actually removed.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: nothing
        \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_reentrant_pop example 1 */
        bool try_reentrant_pop() noexcept
        {
            if (auto operation = try_start_reentrant_consume())
            {
                operation.commit();
                return true;
            }
            return false;
        }


        /** Tries to start a reentrant consume operation.
            This is the reentrant version of try_start_consume.

            @return a consume_operation which is empty if there are no elements to consume

            A non-empty consume must be committed for the consume to have effect.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_reentrant_consume example 1 */
        reentrant_consume_operation try_start_reentrant_consume() noexcept
        {
             return reentrant_consume_operation(PrivateType(), this);
        }

        /** Tries to start a consume operation using an existing consume_operation object.
            This is the reentrant version of try_start_consume.

            @param i_consume reference to a consume_operation to be used. If it is non-empty
                it gets canceled before trying to start the new consume.
            @return whether i_consume is non-empty after the call, that is whether the queue was
                not empty.

            A non-empty consume must be committed for the consume to have effect.

            This overload is similar to the one taking no arguments and returning a consume_operation.
            For an lf_heter_queue there is no performance difference between the two overloads. Anyway
            for lock-free concurrent queue this overload may be faster.

            \snippet lf_heterogeneous_queue_examples.cpp lf_heter_queue try_start_reentrant_consume_ example 1 */
        bool try_start_reentrant_consume(reentrant_consume_operation & i_consume) noexcept
        {
            return i_consume.start_consume_impl(PrivateType(), this);
        }
    };

} // namespace density

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
