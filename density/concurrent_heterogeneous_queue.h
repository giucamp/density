
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>
#include <random>

inline size_t get_rand(size_t i_max)
{
    static thread_local std::mt19937 rand{ std::random_device()() };
    return std::uniform_int_distribution<size_t>(0, i_max)(rand);
}

#define DENSITY_TEST_RANDOM_WAIT() //if(get_rand(7) == 3) { std::this_thread::sleep_for(std::chrono::nanoseconds(get_rand(65336) ) ); }

#define DENSITY_STATS(expr)

namespace density
{

    /* Defines the kind of algorithm used for a concurrent data structure.
        Note: by lock-free is intended that the implementation does not use mutexes for synchronization, and it is based on a lockfree
        algorithm. Anyway sometimes a lockfree method may need to allocate memory, which may be a locking operation.In density pages
        are allocated from a lockfree page freelist. Anyway, when the freelist is out of memory, a new memory region is requested to
        the operating system, which almost surely will lock a mutex of some kind.
        Refer to the documentation of the single methods to check the guarantees in case of concurrent access. */
    enum class SynchronizationKind
    {
        MutexBased, /**< The implementation is based on a mutex, so it is locking. This allows a thread to go in an efficient waiting state
                        (for an example a thread may wait for a command to be pushed on a queue). */
        LocklessMultiple, /**< The implementation is based on a lock-free algorithm. It is safe for multiple threads to concurrently do the
                            same operation (for example multiple threads can push an element to a queue). */
        LocklessSingle, /**< The implementation is based on a lock-free algorithm. If multiple threads do the same operation concurrently,
                            with no external synchronization, the program is incurring in a data race, therefore the behavior is undefined. */
    };

    namespace detail
    {
        constexpr size_t concurrent_alignment = 64;

        template < typename VOID_ALLOCATOR, typename RUNTIME_TYPE,
            SynchronizationKind PUSH_SYNC, SynchronizationKind CONSUME_SYNC >
                class base_concurrent_heterogeneous_queue;
    }
}

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif
#define DENSITY_INCLUDING_CONC_QUEUE_DETAIL
    #include <density/detail\base_conc_queue_lflf.h>
#undef DENSITY_INCLUDING_CONC_QUEUE_DETAIL
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace density
{
    namespace experimental
    {
        /**
            The queues always keep at least an allocated page. Therefore the constructor allocates a page. The reason is to allow
                producer to assume that the page in which the push is tried (the last one) doesn't get deallocated while the push
                is in progress.
                The consume algorithm uses hazard pointers to safely delete the pages. Pages are deleted immediately by consumers
                when no longer needed. Using the default (and recommended) allocator, deleted pages are added to a thread-local
                free-list. When the number of pages in this free-list exceeds a fixed number, a page is added in a global lock-free
                free-list. See void_allocator for details.
                See "Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects" of Maged M. Michael for details.
                There is no requirement on the type of the elements: they can be non-trivially movable, copyable and destructible.
                */
        template < typename ELEMENT = void, typename VOID_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT>,
            SynchronizationKind PUSH_SYNC = SynchronizationKind::LocklessMultiple,
            SynchronizationKind CONSUME_SYNC = SynchronizationKind::LocklessMultiple >
                class concurrent_heterogeneous_queue : private detail::base_concurrent_heterogeneous_queue<VOID_ALLOCATOR, RUNTIME_TYPE, PUSH_SYNC, CONSUME_SYNC>
        {
            using BaseClass = detail::base_concurrent_heterogeneous_queue<VOID_ALLOCATOR, RUNTIME_TYPE, PUSH_SYNC, CONSUME_SYNC>;

        public:

            static_assert(std::is_same<ELEMENT, void>::value, "Currently only fully-heterogeneous elements are supported");
                /* Reason: casting from a derived class is often a trivial operation (the value of the pointer does not change),
                    but sometimes it may require offsetting the pointer, or it may also require a memory indirection. Most containers
                    in this library store the result of this cast implicitly in the overhead data of the elements, but currently this container doesn't. */

            using allocator_type = VOID_ALLOCATOR;
            using runtime_type = RUNTIME_TYPE;
            using value_type = ELEMENT;
            using reference = typename std::add_lvalue_reference< ELEMENT >::type;
            using const_reference = typename std::add_lvalue_reference< const ELEMENT>::type;
            static constexpr SynchronizationKind push_sync = PUSH_SYNC;
            static constexpr SynchronizationKind consume_sync = CONSUME_SYNC;

            static_assert(allocator_type::page_size > sizeof(void*) * 8 && allocator_type::page_alignment == allocator_type::page_size,
                    "The size and alignment of the pages must be the same (and not too small)");

            template <typename ELEMENT_COMPLETE_TYPE>
                DENSITY_STRONG_INLINE void push(ELEMENT_COMPLETE_TYPE && i_source)
            {
                using ElementCompleteType = typename std::decay<ELEMENT_COMPLETE_TYPE>::type;

                emplace<ElementCompleteType>(std::forward<ELEMENT_COMPLETE_TYPE>(i_source));
            }

            template <typename COMPLETE_ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
                void emplace(CONSTRUCTION_PARAMS &&... i_args)
            {
                static_assert(std::is_convertible< COMPLETE_ELEMENT_TYPE*, ELEMENT*>::value,
                    "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT, or ELEMENT must be void");

                auto push_data = BaseClass:: template begin_push<true>(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE));

                try
                {
                    // construct the type
                    new(&push_data.m_control->m_type) RUNTIME_TYPE(RUNTIME_TYPE::template make<COMPLETE_ELEMENT_TYPE>());
                }
                catch (...)
                {
                    // this call release the exclusive access and set the dead flag
                    BaseClass::cancel_push(push_data.m_control);

                    // the exception is propagated to the caller, whatever it is
                    throw;
                }

                try
                {
                    // construct the element
                    new(push_data.m_element) COMPLETE_ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_args)...);
                }
                catch (...)
                {
                    // destroy the type (which is probably a no-operation)
                    push_data.m_control->m_type.RUNTIME_TYPE::~RUNTIME_TYPE();

                    // this call release the exclusive access and set the dead flag
                    BaseClass::cancel_push(push_data.m_control);

                    // the exception is propagated to the caller, whatever it is
                    throw;
                }

                BaseClass::commit_push(push_data);
            }

            template <typename CONSUMER_FUNC>
                bool try_consume(CONSUMER_FUNC && i_consumer_func)
            {
                auto consume_data = BaseClass::begin_consume();
                if (consume_data.m_control != nullptr)
                {
                    auto const type = &consume_data.m_control->m_type;
                    auto const element = consume_data.m_control + 1;
                    i_consumer_func(*type, element);
                    type->destroy(element);
                    type->RUNTIME_TYPE::~RUNTIME_TYPE();
                    BaseClass::commit_consume(consume_data);
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
                return *static_cast<VOID_ALLOCATOR*>(this);
            }

            /** Returns a reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            allocator_type & get_allocator_ref() noexcept
            {
                return *static_cast<VOID_ALLOCATOR*>(this);
            }

            /** Returns a const reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            const allocator_type & get_allocator_ref() const noexcept
            {
                return *static_cast<VOID_ALLOCATOR*>(this);
            }
        };

    } // namespace experimental

} // namespace density
