#pragma once
#include "detail\disposable_concurrent_queue_header.h"

namespace density
{
	namespace experimental
	{
		enum class ConcurrentQueueKind
		{
			SingleConsumerSingleProducer,
			MultipleConsumerSingleProducer,
			SingleConsumerMultipleProducer,
			MultipleConsumerMultipleProducer,
		};

		template < typename ELEMENT = void, typename VOID_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT>,
				ConcurrentQueueKind KIND = ConcurrentQueueAlgorithm::MultipleConsumerMultipleProducer >
			class concurrent_heterogeneous_queue : private VOID_ALLOCATOR
		{
		public:

			static_assert(std::is_same< typename std::decay<ELEMENT>::type, void >::value ? std::is_same<ELEMENT, void>::value : true,
				"If ELEMENT decays to void, it must be void (i.e. use plain 'void', not cv or ref qualified voids, like 'void&' or 'const void' )");

			using allocator_type = VOID_ALLOCATOR;
			using runtime_type = RUNTIME_TYPE;
			using value_type = ELEMENT;
			using reference = typename std::add_lvalue_reference< ELEMENT >::type;
			using const_reference = typename std::add_lvalue_reference< const ELEMENT>::type;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			class iterator;
			class const_iterator;

			concurrent_heterogeneous_queue()
			{

			}


			template <typename CONSTRUCTOR>
			void push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor, size_t i_size)
			{
				auto tail = m_tail.load();
				auto const result = tail->push(i_source_type, std::move(i_constructor), i_size);
				if (!result)
				{
					new_page = get_allocator_ref().allocate_page();
					tail->m_next.set_if_equal(nullptr, new_page);
					m_tail = new_page;
				}
			}

			template <typename OPERATION>
			bool manual_consume(OPERATION && i_operation)
			{
				for (;;)
				{
					auto res = m_head->consume(std::move(i_operation));
					if (res == ConsumeResult::Success)
					{
						return true;
					}
				}
			}

			/** Returns a reference to the allocator instance owned by the queue.
				\n\b Throws: nothing
				\n\b Complexity: constant */
			allocator_type & get_allocator_ref() noexcept
			{
				return *this;
			}

		private:
			using queue = detail::disposable_concurrent_queue_header<uint64_t, RUNTIME_TYPE, 4096, 8>;
			using queue::ConsumeResult;
			std::atomic<queue*> m_head, m_tail;
		};

	} // namespace experimental

} // namespace density
