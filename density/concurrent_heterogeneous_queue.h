
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_common.h"
#include "density_common.h"
#include "runtime_type.h"
#include "void_allocator.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <list>

#define DENSITY_TEST_RANDOM_WAIT()

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

        template <typename INTERNAL_WORD, typename RUNTIME_TYPE, INTERNAL_WORD PAGE_SIZE,
            SynchronizationKind PUSH_SYNC, SynchronizationKind CONSUME_SYNC>
                class ou_conc_queue_header;

		template < typename PAGE_ALLOCATOR, typename RUNTIME_TYPE,
			SynchronizationKind PUSH_SYNC, SynchronizationKind CONSUME_SYNC >
				class base_concurrent_heterogeneous_queue;

		/** Due to private inheritance, c++ name lookup rules makes the public members of PAGE_ALLOCATOR not accessible 
			by concurrent_heterogeneous_queue. This template workarounds for problem. */
		template <typename PAGE_ALLOCATOR> struct PageAllocatorTraits
		{
			static constexpr size_t page_size = PAGE_ALLOCATOR::page_size;
			static constexpr size_t page_alignment = PAGE_ALLOCATOR::page_alignment;
		};
    }
}

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif
#define DENSITY_INCLUDING_CONC_QUEUE_DETAIL
	#include "detail\hazard_pointers.h"
    #include "detail\ou_conc_queue_header_lflf.h"
	#include "detail\base_conc_queue_lflf.h"
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
		template < typename ELEMENT = void, typename PAGE_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT>,
			SynchronizationKind PUSH_SYNC = SynchronizationKind::LocklessMultiple,
			SynchronizationKind CONSUME_SYNC = SynchronizationKind::LocklessMultiple >
				class concurrent_heterogeneous_queue : public detail::base_concurrent_heterogeneous_queue<PAGE_ALLOCATOR, RUNTIME_TYPE, PUSH_SYNC, CONSUME_SYNC>
        {
			using BaseClass = detail::base_concurrent_heterogeneous_queue<PAGE_ALLOCATOR, RUNTIME_TYPE, PUSH_SYNC, CONSUME_SYNC>;

        public:

            static_assert(std::is_same<ELEMENT, void>::value, "Currently only fully-heterogeneous elements are supported");
                /* Reason: casting from a derived class is often a trivial operation (the value of the pointer does not change),
                    but sometimes it may require offsetting the pointer, or it may also require a memory indirection. Most containers
                    in this library store the result of this cast implicitly in the overhead data of the elements, but currently this container doesn't. */

            using allocator_type = PAGE_ALLOCATOR;
            using runtime_type = RUNTIME_TYPE;
            using value_type = ELEMENT;
            using reference = typename std::add_lvalue_reference< ELEMENT >::type;
            using const_reference = typename std::add_lvalue_reference< const ELEMENT>::type;
			static constexpr SynchronizationKind push_sync = PUSH_SYNC;
			static constexpr SynchronizationKind consume_sync = CONSUME_SYNC;

			static_assert(detail::PageAllocatorTraits<allocator_type>::page_alignment >= 2, "The implementation requires that guaranteed alignment of the pages is at least 2");
				/* Reason: the push algorithm uses the first bit of m_last (which is a pointer to a page) as exclusive-access flag. */

            template <typename ELEMENT_COMPLETE_TYPE>
				DENSITY_STRONG_INLINE void push(ELEMENT_COMPLETE_TYPE && i_source)
            {
                static_assert(std::is_convertible< typename std::decay<ELEMENT_COMPLETE_TYPE>::type*, ELEMENT*>::value,
                    "ELEMENT_COMPLETE_TYPE must be covariant to (i.e. must derive from) ELEMENT, or ELEMENT must be void");
                push_impl(std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
                    typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
            }

            template <typename CONSTRUCTOR>
				DENSITY_STRONG_INLINE void push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
            {
				BaseClass::push(i_source_type, std::forward<CONSTRUCTOR>(i_constructor), i_source_type.size());
            }

            template <typename CONSTRUCTOR>
				void push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor, size_t i_size)
			{
				DENSITY_ASSERT(i_source_type.size() == i_size); // inconsistent element size?

				BaseClass::push(i_source_type, std::forward<CONSTRUCTOR>(i_constructor), i_size);
			}

            template <typename OPERATION>
                bool try_consume(OPERATION && i_operation)
            {
				return BaseClass::try_consume(std::forward<OPERATION>(i_operation));
            }

			using view = typename BaseClass::view;

            /** Returns a copy of the allocator instance owned by the queue.
                \n\b Throws: anything that the copy-constructor of the allocator throws
                \n\b Complexity: constant */
            allocator_type get_allocator() const
            {
				return BaseClass::get_allocator_ref();
            }

            /** Returns a reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            allocator_type & get_allocator_ref() noexcept
            {
				return BaseClass::get_allocator_ref();
            }

            /** Returns a const reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            const allocator_type & get_allocator_ref() const noexcept
            {
				return BaseClass::get_allocator_ref();
            }

        private:

            // overload used if i_source is an r-value
            template <typename ELEMENT_COMPLETE_TYPE>
                void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::true_type)
            {
                using ElementCompleteType = typename std::decay<ELEMENT_COMPLETE_TYPE>::type;
                BaseClass::push(runtime_type::template make<ElementCompleteType>(),
                    [&i_source](const runtime_type &, void * i_dest) {
                    auto const result = new(i_dest) ElementCompleteType(std::move(i_source));
                    return static_cast<ELEMENT*>(result);
                }, sizeof(ELEMENT_COMPLETE_TYPE));
            }

            // overload used if i_source is an l-value
            template <typename ELEMENT_COMPLETE_TYPE>
                void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::false_type)
            {
                using ElementCompleteType = typename std::decay<ELEMENT_COMPLETE_TYPE>::type;
				BaseClass::push(runtime_type::template make<ElementCompleteType>(),
                    [&i_source](const runtime_type &, void * i_dest) {
                    auto const result = new(i_dest) ElementCompleteType(i_source);
                    return static_cast<ELEMENT*>(result);
                }, sizeof(ELEMENT_COMPLETE_TYPE));
            }
        };

    } // namespace experimental

} // namespace density
