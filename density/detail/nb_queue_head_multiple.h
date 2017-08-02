
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

namespace density
{
	namespace detail
	{
		/** /internal Class template that implements consume operations */
		template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE,
				concurrent_cardinality PROD_CARDINALITY, consistency_model CONSISTENCY_MODEL >
			class NonblockingQueueHead< COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, 
				PROD_CARDINALITY, concurrent_cardinality_multiple, CONSISTENCY_MODEL> 
					: protected NonblockingQueueTail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSISTENCY_MODEL>
		{
		private:

			using Base = NonblockingQueueTail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSISTENCY_MODEL>;

		protected:

			using ControlBlock = typename Base::ControlBlock;

			static ControlBlock s_initial_block;

			NonblockingQueueHead() noexcept
				: m_head(nullptr)
			{
			}

			NonblockingQueueHead(ALLOCATOR_TYPE && i_allocator) noexcept
				: Base(std::move(i_allocator)),
				  m_head(nullptr)
			{
			}

			NonblockingQueueHead(const ALLOCATOR_TYPE & i_allocator)
				: Base(i_allocator),
				  m_head(nullptr)
			{
			}

			NonblockingQueueHead(NonblockingQueueHead && i_source) noexcept
				: NonblockingQueueHead()
			{
				swap(i_source);
			}

			NonblockingQueueHead & operator = (NonblockingQueueHead && i_source) noexcept
			{
				swap(i_source);
				return *this;
			}

			void swap(NonblockingQueueHead & i_other) noexcept
			{
				Base::swap(i_other);

				// swap the head
				auto const tmp = i_other.m_head.load();
				i_other.m_head.store(m_head.load());
				m_head.store(tmp);
			}

			struct Consume
			{
				NonblockingQueueHead * m_queue = nullptr; /**< Owning queue if the Consume is not empty, undefined otherwise. */
				ControlBlock * m_control = nullptr; /**< Currently pinned control block. Independent from the empty-ness of the Consume */
				uintptr_t m_next_ptr = 0; /**< m_next member of the ControlBox of the element being consumed. The Consume is empty if and only if m_next_ptr == 0 */

				Consume() noexcept = default;

				Consume(const Consume &) = delete;
				Consume & operator = (const Consume &) = delete;

				Consume(Consume && i_source) noexcept
					: m_queue(i_source.m_queue), m_control(i_source.m_control), m_next_ptr(i_source.m_next_ptr)
				{
					i_source.m_control = nullptr;
					i_source.m_next_ptr = 0;
				}

				Consume & operator = (Consume && i_source) noexcept
				{
					if (this != &i_source)
					{
						if (m_control != nullptr)
						{
							m_queue->ALLOCATOR_TYPE::unpin_page(m_control);
						}
						m_queue = i_source.m_queue;
						m_control = i_source.m_control;
						m_next_ptr = i_source.m_next_ptr;
						i_source.m_control = nullptr;
						i_source.m_next_ptr = 0;
					}
					return *this;
				}

				~Consume()
				{
					if (m_control != nullptr)
					{
						m_queue->ALLOCATOR_TYPE::unpin_page(m_control);
					}
				}

				void swap(Consume & i_other) noexcept
				{
					using std::swap;
					swap(m_queue, i_other.m_queue);
					swap(m_control, i_other.m_control);
					swap(m_next_ptr, i_other.m_next_ptr);
				}

				/** Moves the Consume to the head, pinning it. The previously pinned page is unpinned.*/
				bool move_to_head(NonblockingQueueHead * i_queue) noexcept
				{
					DENSITY_ASSERT_INTERNAL(address_is_aligned(m_control, Base::s_alloc_granularity));

					ControlBlock * head = i_queue->m_head.load();
					DENSITY_ASSERT_INTERNAL(address_is_aligned(head, Base::s_alloc_granularity));
					
					if (head == nullptr)
					{
						auto const initial_page = i_queue->Base::get_initial_page();

						/* If this CAS succeeds, we have to update our local variable head. Otherwise 
							after the call we have the value of m_head stored by another concurrent consumer. */
						if (i_queue->m_head.compare_exchange_strong(head, initial_page))
							head = initial_page;

						// in any case there is no more s_invalid_control_block
						if (head == nullptr)
						{
							return false;
						}
					}

					while (DENSITY_UNLIKELY( !Base::same_page(m_control, head) || m_control == nullptr ) )
					{
						DENSITY_ASSERT_INTERNAL(m_control != head);

						i_queue->ALLOCATOR_TYPE::pin_page(head);

						if (m_control != nullptr)
						{
							i_queue->ALLOCATOR_TYPE::unpin_page(m_control);
						}

						m_control = head;

						head = i_queue->m_head.load();
						DENSITY_ASSERT_INTERNAL(address_is_aligned(head, Base::s_alloc_granularity));
					}

					m_control = static_cast<ControlBlock*>(head);
					m_queue = i_queue;
					return true;
				}

				bool is_queue_empty(const NonblockingQueueHead * i_queue) noexcept
				{
					DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);

					// we are not modifying the queue, but we have to pin/unpin anyway
					auto const queue = const_cast<NonblockingQueueHead *>(i_queue);

					if (!move_to_head(queue))
					{
						return true;
					}

					for (;;)
					{
						auto const next_uint = raw_atomic_load(&m_control->m_next, detail::mem_seq_cst);

						// check if next_uint is non-zero (excluding the bit NbQueue_InvalidNextPage)
						if ((next_uint & ~detail::NbQueue_InvalidNextPage) != 0)
						{
							/* here we have observed a non-zero m_control->m_next. We handle
								detail::NbQueue_InvalidNextPage with the zero case.*/
							if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == 0)
							{
								/** This element is ready to be consumed, so we try to set the flag NbQueue_Busy on it */
								return false;
							}

							// advance m_control to the next element
							auto const next = reinterpret_cast<ControlBlock*>(next_uint & ~detail::NbQueue_AllFlags);
							DENSITY_ASSERT_INTERNAL(next != 0);
							if (DENSITY_LIKELY(Base::same_page(m_control, next)))
							{
								// no page switch
								DENSITY_ASSERT_INTERNAL(m_control != Base::get_end_control_block(m_control));
								m_control = next;
							}
							else
							{
								/* page switch: we have to do a safe pinning of the next page */
								DENSITY_ASSERT_INTERNAL(m_control == Base::get_end_control_block(m_control));

								DENSITY_ASSERT_INTERNAL(address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
								m_queue->ALLOCATOR_TYPE::pin_page(next);

								auto const updated_next_uint = raw_atomic_load(&m_control->m_next, detail::mem_seq_cst);
								auto const updated_next = reinterpret_cast<ControlBlock*>(updated_next_uint & ~detail::NbQueue_AllFlags);
								if (updated_next == nullptr)
								{
									move_to_head(queue);
									m_queue->ALLOCATOR_TYPE::unpin_page(next);
									continue;
								}

								DENSITY_ASSERT_INTERNAL(next == updated_next);

								m_queue->ALLOCATOR_TYPE::unpin_page(m_control);
								m_control = next;
							}
						}
						else
						{
							return true;
						}
					}
				}

				/** \internal
					Tries to start a consume operation. The Consume must be initially empty.
					If there are no consumable elements, the Consume remains empty (m_next_ptr == 0).
					Otherwise m_next_ptr is the value to set on the ControlBox to commit the consume
					(it has the NbQueue_Dead flag). */
				void start_consume_impl(NonblockingQueueHead * i_queue) noexcept
				{
					DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);
					DENSITY_ASSERT_INTERNAL(address_is_aligned(m_control, Base::s_alloc_granularity));

					if (!move_to_head(i_queue))
						return;

					DENSITY_ASSERT_INTERNAL(m_control != nullptr && address_is_aligned(m_control, Base::s_alloc_granularity));

					for (;;)
					{
						// We do an initial relaxed read. We will do a memory acquire in the CAS
						auto const next_uint = raw_atomic_load(&m_control->m_next, detail::mem_relaxed);
						if ((next_uint & ~detail::NbQueue_InvalidNextPage) == 0)
						{
							break;
						}

						/** Check if this element is ready to be consumed */
						if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == 0)
						{
							/* We try to set the flag NbQueue_Busy on it */
							auto expected = next_uint;
							if (raw_atomic_compare_exchange_strong(&m_control->m_next, &expected, next_uint | detail::NbQueue_Busy,
								detail::mem_acquire, detail::mem_relaxed))
							{
								m_next_ptr = next_uint | NbQueue_Dead;
								break;
							}
						}

						// advance m_control to the next element
						auto const next = reinterpret_cast<ControlBlock*>(next_uint & ~detail::NbQueue_AllFlags);
						DENSITY_ASSERT_INTERNAL(next != 0);
						if (DENSITY_LIKELY(Base::same_page(m_control, next)))
						{
							// no page switch
							DENSITY_ASSERT_INTERNAL(m_control != Base::get_end_control_block(m_control));
							m_control = next;
						}
						else
						{
							/* page switch: we have to do a safe pinning of the next page */
							DENSITY_ASSERT_INTERNAL(m_control == Base::get_end_control_block(m_control));
							DENSITY_ASSERT_INTERNAL(address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
							m_queue->ALLOCATOR_TYPE::pin_page(next);

							auto const updated_next_uint = raw_atomic_load(&m_control->m_next, detail::mem_seq_cst);
							auto const updated_next = reinterpret_cast<ControlBlock*>(updated_next_uint & ~detail::NbQueue_AllFlags);
							if (updated_next == nullptr)
							{
								move_to_head(i_queue);
								m_queue->ALLOCATOR_TYPE::unpin_page(next);
							}
							else
							{
								DENSITY_ASSERT_INTERNAL(next == updated_next);

								m_queue->ALLOCATOR_TYPE::unpin_page(m_control);
								m_control = next;
							}
						}					
					}
				}

				void commit_consume_impl() noexcept
				{
					DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);
					DENSITY_ASSERT_INTERNAL(m_next_ptr != 0);

					// we expect to have NbQueue_Busy and not NbQueue_Dead...
					DENSITY_ASSERT_INTERNAL((raw_atomic_load(&m_control->m_next, detail::mem_relaxed) &
						(detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

					// remove NbQueue_Busy and add NbQueue_Dead
					DENSITY_ASSERT_INTERNAL((m_next_ptr & (detail::NbQueue_Dead | detail::NbQueue_Busy | detail::NbQueue_InvalidNextPage)) == detail::NbQueue_Dead);
					raw_atomic_store(&m_control->m_next, m_next_ptr, detail::mem_seq_cst);
					m_next_ptr = 0;

					clean_dead_elements();
				}

				void clean_dead_elements() noexcept
				{
					if (!move_to_head(m_queue))
						return;

					for (;;)
					{
						auto const next_uint = raw_atomic_load(&m_control->m_next);
						auto const next = reinterpret_cast<ControlBlock*>(next_uint & ~detail::NbQueue_AllFlags);
						if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) != detail::NbQueue_Dead)
						{
							// the element is not dead
							break;
						}

						auto expected = m_control;
						if (!m_queue->m_head.compare_exchange_strong(expected, next,
							detail::mem_seq_cst, detail::mem_relaxed))
						{
							// another thread is advancing the head, give up
							break;
						}

						if (next_uint & detail::NbQueue_External)
						{
							auto const external_block = static_cast<typename Base::ExternalBlock*>(
								address_add(m_control, Base::s_element_min_offset) );
							m_queue->ALLOCATOR_TYPE::deallocate(external_block->m_block, external_block->m_size, external_block->m_alignment);
						}

						bool const is_same_page = Base::same_page(m_control, next);
						DENSITY_ASSERT_INTERNAL(!is_same_page == address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
						auto const dbg_end_block = Base::get_end_control_block(m_control);
						DENSITY_ASSERT_INTERNAL(is_same_page == (m_control != dbg_end_block));

						auto const address_of_next = const_cast<uintptr_t*>(&m_control->m_next);
						std::memset(m_control, 0, address_diff(address_of_next, m_control));

						if (DENSITY_LIKELY(is_same_page))
						{
							raw_atomic_store(&m_control->m_next, 0);

							std::memset(address_of_next + 1, 0, address_diff(next, address_of_next + 1));
							m_control = next;
						}
						else
						{
							m_queue->ALLOCATOR_TYPE::pin_page(next);

							#if DENSITY_DEBUG_INTERNAL
								auto const updated_next_uint = raw_atomic_load(&m_control->m_next);
								auto const updated_next = reinterpret_cast<ControlBlock*>(updated_next_uint & ~detail::NbQueue_AllFlags);
							#endif

							raw_atomic_store(&m_control->m_next, 0);
							m_queue->ALLOCATOR_TYPE::deallocate_page_zeroed(m_control);

							DENSITY_ASSERT_INTERNAL_NO_ASSUME(updated_next == next);

							m_queue->ALLOCATOR_TYPE::unpin_page(m_control);
							m_control = next;
						}
					}
				}

				void cancel_consume_impl() noexcept
				{
					DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);
					DENSITY_ASSERT_INTERNAL(m_next_ptr != 0);

					// we expect to have NbQueue_Busy and not NbQueue_Dead...
					DENSITY_ASSERT_INTERNAL((raw_atomic_load(&m_control->m_next, detail::mem_relaxed) &
						(detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

					DENSITY_ASSERT_INTERNAL((m_next_ptr & (detail::NbQueue_Dead | detail::NbQueue_Busy | detail::NbQueue_InvalidNextPage)) == detail::NbQueue_Dead);
					raw_atomic_store(&m_control->m_next, m_next_ptr - detail::NbQueue_Dead, detail::mem_seq_cst);
					m_next_ptr = 0;
				}

			}; // Consume

		private: // data members
			alignas(concurrent_alignment) std::atomic<ControlBlock*> m_head;
		};


		template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE, concurrent_cardinality PROD_CARDINALITY, consistency_model CONSISTENCY_MODEL >
			NbQueueControl<COMMON_TYPE> NonblockingQueueHead< COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE,
				PROD_CARDINALITY, concurrent_cardinality_multiple, CONSISTENCY_MODEL>::s_initial_block;

	} // namespace detail

} // namespace density
