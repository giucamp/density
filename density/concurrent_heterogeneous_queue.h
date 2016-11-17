
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>

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
		template <typename SCOPE_EXIT_ACTION>
			class ScopeExit
		{
		public:

			ScopeExit(SCOPE_EXIT_ACTION && i_scope_exit_action)
				: m_scope_exit_action(std::move(i_scope_exit_action))
			{
				static_assert(noexcept(m_scope_exit_action()), "The scope exit action must be noexcept");
			}

			~ScopeExit()
			{
				m_scope_exit_action();
			}

			ScopeExit(const ScopeExit &) = delete;
			ScopeExit & operator = (const ScopeExit &) = delete;

			ScopeExit(ScopeExit &&) noexcept = default;
			ScopeExit & operator = (ScopeExit &&) noexcept = default;

		private:
			SCOPE_EXIT_ACTION m_scope_exit_action;
		};

		template <typename SCOPE_EXIT_ACTION>
			inline ScopeExit<SCOPE_EXIT_ACTION> at_scope_exit(SCOPE_EXIT_ACTION && i_scope_exit_action)
		{
			return ScopeExit<SCOPE_EXIT_ACTION>(std::move(i_scope_exit_action));
		}
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
        template < typename COMMON_TYPE = void, typename ALLOCATOR_TYPE = void_allocator, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>,
            SynchronizationKind PRODUCE_SYNC = SynchronizationKind::LocklessMultiple,
            SynchronizationKind CONSUME_SYNC = SynchronizationKind::LocklessMultiple >
                class concurrent_heterogeneous_queue : private ALLOCATOR_TYPE
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
            static constexpr SynchronizationKind produce_sync = PRODUCE_SYNC;
            static constexpr SynchronizationKind consume_sync = CONSUME_SYNC;

            concurrent_heterogeneous_queue()
            {
                auto const first_page = allocator_type::allocate_page();
                m_tail.initialize(this, first_page);
                m_head.initialize(this, first_page, m_tail.get_tail_for_consumers());
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
                static_assert(decltype(m_tail)::element_fits_in_a_page(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE)),
                    "currently ELEMENT_TYPE must fit in a page");

                auto push_data = m_tail.template begin_push<true>(i_runtime_type.size(), i_runtime_type.alignment());

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
                    m_tail.cancel_push(push_data.m_control);

                    // the exception is propagated to the caller, whatever it is
                    throw;
                }

                m_tail.commit_push(push_data);
            }


            template <typename COMPLETE_ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
                void emplace(CONSTRUCTION_PARAMS &&... i_args)
            {
                static_assert(std::is_convertible< COMPLETE_ELEMENT_TYPE*, COMMON_TYPE*>::value,
                    "ELEMENT_TYPE must be covariant to (i.e. must derive from) COMMON_TYPE, or COMMON_TYPE must be void");

                static_assert(decltype(m_tail)::element_fits_in_a_page(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE)),
                    "currently ELEMENT_TYPE must fit in a page");

                auto push_data = m_tail.template begin_push<true>(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE));

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
                    m_tail.cancel_push(push_data.m_control);

                    // the exception is propagated to the caller, whatever it is
                    throw;
                }

                m_tail.commit_push(push_data);
            }

            template <typename CONSUMER_FUNC>
                bool try_consume(CONSUMER_FUNC && i_consumer_func)
            {
                auto consume_data = m_head.begin_consume();
                if (consume_data.m_control != nullptr)
                {
					auto scope_exit = detail::at_scope_exit([this, consume_data] () noexcept {
						consume_data.type_ptr()->destroy(consume_data.element_ptr());
						consume_data.type_ptr()->RUNTIME_TYPE::~RUNTIME_TYPE();
						m_head.commit_consume(consume_data);
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
                auto consume_data = m_head.begin_consume();
                if (consume_data.m_control != nullptr)
                {
					auto scope_exit = detail::at_scope_exit([this, consume_data]() noexcept {
						consume_data.type_ptr()->RUNTIME_TYPE::~RUNTIME_TYPE();
						m_head.commit_consume(consume_data);
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
            detail::conc_queue::Head<void_allocator, runtime_type, consume_sync > m_head;
            detail::conc_queue::Tail<void_allocator, runtime_type, produce_sync > m_tail;
        };

    } // namespace experimental

} // namespace density
