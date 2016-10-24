
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#ifndef DENSITY_INCLUDING_CONC_QUEUE_DETAIL
	#error "THIS IS A PRIVATE HEADER OF DENSITY. DO NOT INCLUDE IT DIRECTLY"
#endif // ! DENSITY_INCLUDING_CONC_QUEUE_DETAIL

namespace density
{
	namespace detail
	{
		template < typename PAGE_ALLOCATOR, typename RUNTIME_TYPE>
			class base_concurrent_heterogeneous_queue<PAGE_ALLOCATOR, RUNTIME_TYPE, 
				SynchronizationKind::LocklessMultiple, SynchronizationKind::LocklessMultiple> : private PAGE_ALLOCATOR
		{
			using internal_word = uint64_t;

			static_assert(PAGE_ALLOCATOR::page_size == static_cast<internal_word>(PAGE_ALLOCATOR::page_size), 
				"internal_word can't represent the page size");

            using queue_header = detail::ou_conc_queue_header<uint64_t, RUNTIME_TYPE, 
				static_cast<internal_word>(PAGE_ALLOCATOR::page_size),
				SynchronizationKind::LocklessMultiple, SynchronizationKind::LocklessMultiple>;

        public:

			static constexpr size_t default_alignment = queue_header::default_alignment;
			
			base_concurrent_heterogeneous_queue()
            {
                auto first_queue = new_page();
                m_first.store(first_queue);
                m_last.store(reinterpret_cast<uintptr_t>(first_queue));
                m_can_delete_page.clear();
                m_first_consumer = nullptr;
            }

			~base_concurrent_heterogeneous_queue()
			{
				/** detach all the consumers */
				{
					std::lock_guard<std::mutex> lock(m_hazard_pointers_mutex);
					for (auto curr = m_first_consumer; curr != nullptr; curr = curr->m_next)
					{
						DENSITY_ASSERT_INTERNAL(curr->m_target_queue == this);
						curr->detach();
					}
				}
			}

            template <typename CONSTRUCTOR>
                void push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor, size_t i_size)
            {
                for (;;)
                {
                    /* Take the last page, and try to push in it. We can assume that the page will dot be deleted in the meanwhile,
                        because consumer threads can delete a page only if it is not the last one. */
                    auto last = reinterpret_cast<queue_header*>(m_last.load() & ~static_cast<uintptr_t>(1));
                    if (last->try_push<true>(i_source_type, std::forward<CONSTRUCTOR>(i_constructor), i_size))
                    {
                        /* Here the push is successful, so we exit the loop */
                        break;
                    }

                    /* Now we try to get exclusive access on m_last. If we fail to do that, just continue the loop. */
                    DENSITY_TEST_RANDOM_WAIT();
                    auto prev_last = m_last.fetch_or(1);
                    if ((prev_last & 1) == 0)
                    {
                        /* Ok, we have set the exclusive flag from 0 to 1. First allocate the page. */
                        DENSITY_TEST_RANDOM_WAIT();
                        auto const queue = new_page();

                        /** Now we can link the prev-last with the new page. This is a non-atomic write. */
                        DENSITY_TEST_RANDOM_WAIT();
                        DENSITY_ASSERT_INTERNAL(reinterpret_cast<queue_header*>(prev_last)->m_next == nullptr);
                        reinterpret_cast<queue_header*>(prev_last)->m_next = queue;

                        /** Finally we update m_last, and continue the loop */
                        DENSITY_TEST_RANDOM_WAIT();
                        m_last.store(reinterpret_cast<uintptr_t>(queue));
                        DENSITY_STATS(++g_stats.allocated_pages);
                    }
                }
            }

            template <typename OPERATION>
                bool try_consume(OPERATION && i_operation)
            {
				return get_impicit_consumer().try_consume(std::forward<OPERATION>(i_operation));
            }

            class consumer
            {
            public:

				explicit consumer(base_concurrent_heterogeneous_queue & i_target_queue)
					: m_hazard_page(nullptr), m_next(nullptr), m_target_queue(nullptr)
				{
					attach(i_target_queue);
				}

                ~consumer()
                {
                    if (m_target_queue != nullptr)
                    {
                        detach();
                    }
                }

                consumer(const consumer &) = delete;
                consumer & operator = (const consumer &) = delete;
			
                template <typename CONSUMER_FUNC>
                    bool try_consume(CONSUMER_FUNC && i_consumer_func)
                {
                    DENSITY_ASSERT(m_target_queue != nullptr); /** this consumer is not attached to a queue. This results in undefined behavior!! */

                    bool result;
                    for (;;)
                    {
                        /** loads the first page of the queue, and sets it as hazard page (no consumer thread will delete it). If
                            the first page changes in the meanwhile, we have to repeat the operation. */
                        queue_header * first;
                        do {
                            first = m_target_queue->m_first.load();
                            m_hazard_page.store(first);
                            DENSITY_TEST_RANDOM_WAIT();
                        } while (first != m_target_queue->m_first.load());
                        DENSITY_TEST_RANDOM_WAIT();

                        /* try to consume an element. On success, exit the loop */
                        result = first->try_consume<true>(std::forward<CONSUMER_FUNC>(i_consumer_func));
                        if (result)
                        {
                            break;
                        }

                        DENSITY_TEST_RANDOM_WAIT();
                        auto const last = reinterpret_cast<queue_header*>(m_target_queue->m_last.load() & ~static_cast<uintptr_t>(1));
                        DENSITY_TEST_RANDOM_WAIT();
                        if (first == last || !first->is_empty())
                        {
                            /** The consume has failed, and we can't delete this page because it is not empty, or because it
                                is the only page in the queue. */
                            break;
                        }

                        /** Try to delete the page */
                        DENSITY_TEST_RANDOM_WAIT();
                        try_to_delete_first();

                    }

                    m_hazard_page.store(nullptr);
                    return result;
                }

					void attach(base_concurrent_heterogeneous_queue & i_target_queue)
					{
						{
							std::lock_guard<std::mutex> lock(i_target_queue.m_hazard_pointers_mutex); // note: the lock may throw
							m_next = i_target_queue.m_first_consumer;
							i_target_queue.m_first_consumer = this;
						}
						m_target_queue = &i_target_queue;
					}

					void detach()
					{
						DENSITY_ASSERT_INTERNAL(m_target_queue != nullptr && m_hazard_page.load() == nullptr);

						/* linear search for this in the linked list of consumers associated to this queue. */
						consumer * prev = nullptr;
						for (consumer * curr = m_target_queue->m_first_consumer; curr != nullptr; curr = curr->m_next)
						{
							if (curr == this)
								break;
							prev = curr;
						}

						if (prev != nullptr)
						{
							// not the first consumer
							DENSITY_ASSERT_INTERNAL(this != m_target_queue->m_first_consumer);
							DENSITY_ASSERT_INTERNAL(prev->m_next == this);
							prev->m_next = m_next;
						}
						else
						{
							// it's the first consumer
							DENSITY_ASSERT_INTERNAL(this == m_target_queue->m_first_consumer);
							m_target_queue->m_first_consumer = m_next;
						}

						m_target_queue = nullptr;
						m_next = nullptr;
					}

            private:

                DENSITY_NO_INLINE void try_to_delete_first()
                {
                    m_hazard_page.store(nullptr);

                    if (!m_target_queue->m_can_delete_page.test_and_set())
                    {
                        auto const first = m_target_queue->m_first.load();
                        m_hazard_page.store(first);

                        auto const last = reinterpret_cast<queue_header*>(m_target_queue->m_last.load() & ~static_cast<uintptr_t>(1));

                        if (first != last && first->is_empty())
                        {
                            /* This first page is not the last (so it can't have further elements pushed) */
                            auto const next = first->m_next;
                            DENSITY_ASSERT_INTERNAL(next != nullptr);
                            DENSITY_TEST_RANDOM_WAIT();
                            m_target_queue->m_first.store(next);
                            m_target_queue->m_can_delete_page.clear();

                            m_hazard_page.store(nullptr);

                            while (!m_target_queue->is_safe_to_free(first))
                            {
                                DENSITY_STATS(++g_stats.delete_page_waits);
                            }

                            m_target_queue->delete_page(first);
                        }
                        else
                        {
                            m_hazard_page.store(nullptr);
                            m_target_queue->m_can_delete_page.clear();
                        }
                    }
                }

                friend class base_concurrent_heterogeneous_queue;

            private:
                std::atomic<queue_header*> m_hazard_page; /** pointer to the page currently used by this
                                                                   consumer. This pointer prevents a page to be destroyed by another consumer. */
                consumer * m_next;
                base_concurrent_heterogeneous_queue * m_target_queue;
            };

            /** Returns a reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
			PAGE_ALLOCATOR & get_allocator_ref() noexcept
            {
                return *static_cast<PAGE_ALLOCATOR*>(this);
            }

            /** Returns a const reference to the allocator instance owned by the queue.
                \n\b Throws: nothing
                \n\b Complexity: constant */
            const PAGE_ALLOCATOR & get_allocator_ref() const noexcept
            {
                return *static_cast<const PAGE_ALLOCATOR*>(this);
            }

        private:
			
            // overload used if i_source is an r-value
            template <typename ELEMENT_COMPLETE_TYPE>
                void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::true_type)
            {
                using ElementCompleteType = typename std::decay<ELEMENT_COMPLETE_TYPE>::type;
                push(runtime_type::template make<ElementCompleteType>(),
                    [&i_source](const runtime_type &, void * i_dest) {
                    auto const result = new(i_dest) ElementCompleteType(std::move(i_source));
                    return static_cast<ELEMENT*>(result);
                });
            }

            // overload used if i_source is an l-value
            template <typename ELEMENT_COMPLETE_TYPE>
                void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::false_type)
            {
                using ElementCompleteType = typename std::decay<ELEMENT_COMPLETE_TYPE>::type;
                push(runtime_type::template make<ElementCompleteType>(),
                    [&i_source](const runtime_type &, void * i_dest) {
                    auto const result = new(i_dest) ElementCompleteType(i_source);
                    return static_cast<ELEMENT*>(result);
                });
            }

            queue_header * new_page()
            {
                return new(get_allocator_ref().allocate_page()) queue_header();
            }

            void delete_page(queue_header * i_page) noexcept
            {
                DENSITY_ASSERT_INTERNAL(i_page->is_empty());

                // the destructor of a page header is trivial, so just deallocate it
                get_allocator_ref().deallocate_page(i_page);
            }

            bool is_safe_to_free(queue_header* i_page)
            {
                std::lock_guard<std::mutex> lock(m_hazard_pointers_mutex);

                for (auto curr = m_first_consumer; curr != nullptr; curr = curr->m_next)
                {
                    if (curr->m_hazard_page == i_page)
                    {
                        return false;
                    }
                }

                return true;
            }

			/** Returns a consumer attached to this queue and which is being used for a consume still in progress */
			consumer & get_impicit_consumer()
			{
				static thread_local std::list<consumer> s_implicit_consumers;

				auto const end = s_implicit_consumers.end();
				consumer * notarget_consumer = nullptr;
				for (auto it = s_implicit_consumers.begin(); it != end; it++)
				{
					if (it->m_target_queue == this && it->m_hazard_page.load() == nullptr)
					{
						return *it;
					}
					if (it->m_target_queue == nullptr)
					{
						notarget_consumer = &*it;
					}
				}
				if (notarget_consumer != nullptr)
				{
					notarget_consumer->attach(*this);
					return *notarget_consumer;
				}
				s_implicit_consumers.emplace_back(*this);
				return s_implicit_consumers.back();
			}

        private:
            std::atomic<queue_header*> m_first; /**< Pointer to the first page of the queue. Consumer will try to consume from this page first. */
            std::atomic<uintptr_t> m_last; /**< Pointer to the first page of the queue, plus the exclusive-access flag (the least significant bit).
                                                A thread gets exclusive-access on this pointer if it succeeds in setting this bit to 1.
                                                The address of the page is obtained by masking away the first bit. Producer threads try to push
                                                new elements in this page first. */

            std::atomic_flag m_can_delete_page; /** Atomic flag used to synchronize consumer threads that attempt to delete the first page. */

            std::mutex m_hazard_pointers_mutex; /** This mutex protects the linked list of consumer, whose head is m_first_consumer. */
            consumer * m_first_consumer; /* Head of the linked list of consumers. Each consumer has an hazard pointer, that is the page
                                            that the consumer is using. A consumer can delete a page only when it is not the only page in the queue
                                            (that is, no producer is possibly using it), and no other consumer has this page in its hazard pointer. */
		};

	} // namespace detail

} // namespace density