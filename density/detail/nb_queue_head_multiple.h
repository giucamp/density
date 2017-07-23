
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

namespace density
{
	namespace detail
	{
		/** /internal Class template that implements put operations */
		template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE,
				concurrent_cardinality PROD_CARDINALITY >
			class NonblockingQueueHead< COMMON_TYPE, RUNTIME_TYPE, 
				ALLOCATOR_TYPE, concurrent_cardinality_multiple, PROD_CARDINALITY> 
					: protected NonblockingQueueTail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY>
		{
		private:

			using Base = NonblockingQueueTail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY>;

		protected:

			using ControlBlock = typename Base::ControlBlock;

			using Base::Base;

			NonblockingQueueHead()
			{
				std::atomic_init(&m_head, this->get_tail_for_consumers());
			}

			NonblockingQueueHead(ALLOCATOR_TYPE && i_allocator)
				: Base(std::move(i_allocator))
			{
				std::atomic_init(&m_head, this->get_tail_for_consumers());
			}

			NonblockingQueueHead(const ALLOCATOR_TYPE & i_allocator)
				: Base(i_allocator)
			{
				std::atomic_init(&m_head, this->get_tail_for_consumers());
			}

			NonblockingQueueHead(NonblockingQueueHead && i_source)
				: NonblockingQueueHead()
			{
				swap(i_source);
			}

			NonblockingQueueHead & operator = (NonblockingQueueHead && i_source)
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
				void move_to_head(NonblockingQueueHead * i_queue) noexcept
				{
					ControlBlock * head = i_queue->m_head.load();

					while (!Base::same_page(m_control, head))
					{
						DENSITY_ASSERT_INTERNAL(m_control != head);

						i_queue->ALLOCATOR_TYPE::pin_page(head);

						if (m_control != nullptr)
						{
							i_queue->ALLOCATOR_TYPE::unpin_page(m_control);
						}

						m_control = head;

						head = i_queue->m_head.load();
					}

					m_control = static_cast<ControlBlock*>(head);
					m_queue = i_queue;
				}

				bool is_queue_empty(const NonblockingQueueHead * i_queue) noexcept
				{
					DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);

					// we are not modifying the queue, but we have to pin/unpin anyway
					auto const queue = const_cast<NonblockingQueueHead *>(i_queue);

					move_to_head(queue);

					for (;;)
					{
						auto const next_uint = raw_atomic_load(m_control->m_next, detail::mem_seq_cst);

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

								auto const updated_next_uint = raw_atomic_load(m_control->m_next, detail::mem_seq_cst);
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
							DENSITY_ASSERT_INTERNAL(next_uint == 0 || next_uint == detail::NbQueue_InvalidNextPage);

							// to do: handle detail::NbQueue_InvalidNextPage

							/* We can't procrastinate the check on the tail anymore */
							auto const tail = m_queue->get_tail_for_consumers();

							auto const first_nonzero = m_control == tail ? nullptr : reverse_scan_for_nonzeroed(tail);
							if (first_nonzero == nullptr)
							{
								// no element to consume
								return true;
							}

							auto new_next = raw_atomic_load(m_control->m_next) & ~detail::NbQueue_AllFlags;
							if (new_next == 0)
							{
								m_control = first_nonzero;
							}
							else
							{
								m_control = reinterpret_cast<ControlBlock*>(new_next);
							}
							DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);
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

					move_to_head(i_queue);

					for (;;)
					{
						auto const next_uint = raw_atomic_load(m_control->m_next, detail::mem_seq_cst);

						// check if next_uint is non-zero (excluding the bit NbQueue_InvalidNextPage)
						if ((next_uint & ~detail::NbQueue_InvalidNextPage) != 0)
						{
							/* here we have observed a non-zero m_control->m_next. We handle
								detail::NbQueue_InvalidNextPage with the zero case.*/
							if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == 0)
							{
								/** This element is ready to be consumed, so we try to set the flag NbQueue_Busy on it */
								auto expected = next_uint;
								if (raw_atomic_compare_exchange_strong(m_control->m_next, expected, next_uint | detail::NbQueue_Busy,
									detail::mem_seq_cst, detail::mem_relaxed))
								{
									m_next_ptr = next_uint | NbQueue_Dead;
									return;
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

								auto const updated_next_uint = raw_atomic_load(m_control->m_next, detail::mem_seq_cst);
								auto const updated_next = reinterpret_cast<ControlBlock*>(updated_next_uint & ~detail::NbQueue_AllFlags);
								if (updated_next == nullptr)
								{
									move_to_head(i_queue);
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
							DENSITY_ASSERT_INTERNAL(next_uint == 0 || next_uint == detail::NbQueue_InvalidNextPage);

							// to do: handle detail::NbQueue_InvalidNextPage

							/* We can't procrastinate the check on the tail anymore */
							auto const tail = m_queue->get_tail_for_consumers();

							auto const first_nonzero = m_control == tail ? nullptr : reverse_scan_for_nonzeroed(tail);
							if (first_nonzero == nullptr)
							{
								// no element to consume
								return;
							}

							auto new_next = raw_atomic_load(m_control->m_next) & ~detail::NbQueue_AllFlags;
							if (new_next == 0)
							{
								m_control = first_nonzero;
							}
							else
							{
								m_control = reinterpret_cast<ControlBlock*>(new_next);
							}
							DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);
						}
					}
				}

				/** /internal
					No pin/unpin is performed */
				ControlBlock * reverse_scan_for_nonzeroed(ControlBlock * const i_tail) const noexcept
				{
					DENSITY_ASSERT_INTERNAL(m_control != nullptr && i_tail != nullptr);
					DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);

					/* the scan starts from the tail if it is in the same page of m_control, otherwise it
						starts from the end-block of the page. */
					auto curr = i_tail;
					if (!Base::same_page(m_control, i_tail))
					{
						curr = Base::get_end_control_block(m_control);
					}

					ControlBlock * last_nonzero = nullptr;

					if (curr > m_control)
					{
						for (;;)
						{
							curr = static_cast<ControlBlock *>(address_sub(curr, Base::min_alignment));
							if (curr == m_control)
								break;

							auto uint_next = raw_atomic_load(curr->m_next, detail::mem_seq_cst) & ~detail::NbQueue_Busy;
							if (uint_next != 0)
							{
								last_nonzero = curr;
							}
						}
					}

					return last_nonzero;
				}

				void commit_consume_impl() noexcept
				{
					DENSITY_ASSERT_INTERNAL(m_queue->ALLOCATOR_TYPE::get_pin_count(m_control) > 0);
					DENSITY_ASSERT_INTERNAL(m_next_ptr != 0);

					// we expect to have NbQueue_Busy and not NbQueue_Dead...
					DENSITY_ASSERT_INTERNAL((raw_atomic_load(m_control->m_next, detail::mem_relaxed) &
						(detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

					// remove NbQueue_Busy and add NbQueue_Dead
					DENSITY_ASSERT_INTERNAL((m_next_ptr & (detail::NbQueue_Dead | detail::NbQueue_Busy | detail::NbQueue_InvalidNextPage)) == detail::NbQueue_Dead);
					raw_atomic_store(m_control->m_next, m_next_ptr, detail::mem_seq_cst);
					m_next_ptr = 0;

					clean_dead_elements();
				}

				void clean_dead_elements() noexcept
				{
					move_to_head(m_queue);

					for (;;)
					{
						auto const next_uint = raw_atomic_load(m_control->m_next);
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

						bool const is_same_page = Base::same_page(m_control, next);
						DENSITY_ASSERT_INTERNAL(!is_same_page == address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
						auto const dbg_end_block = Base::get_end_control_block(m_control);
						DENSITY_ASSERT_INTERNAL(is_same_page == (m_control != dbg_end_block));

						auto const address_of_next = const_cast<uintptr_t*>(&m_control->m_next);
						std::memset(m_control, 0, address_diff(address_of_next, m_control));

						if (DENSITY_LIKELY(is_same_page))
						{
							raw_atomic_store(m_control->m_next, 0);

							std::memset(address_of_next + 1, 0, address_diff(next, address_of_next + 1));
							m_control = next;
						}
						else
						{
							m_queue->ALLOCATOR_TYPE::pin_page(next);

							#if DENSITY_DEBUG_INTERNAL
								auto const updated_next_uint = raw_atomic_load(m_control->m_next);
								auto const updated_next = reinterpret_cast<ControlBlock*>(updated_next_uint & ~detail::NbQueue_AllFlags);
							#endif

							raw_atomic_store(m_control->m_next, 0);
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
					DENSITY_ASSERT_INTERNAL((raw_atomic_load(m_control->m_next, detail::mem_relaxed) &
						(detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

					DENSITY_ASSERT_INTERNAL((m_next_ptr & (detail::NbQueue_Dead | detail::NbQueue_Busy | detail::NbQueue_InvalidNextPage)) == detail::NbQueue_Dead);
					raw_atomic_store(m_control->m_next, m_next_ptr - detail::NbQueue_Dead, detail::mem_seq_cst);
					m_next_ptr = 0;
				}

			}; // Consume

		private: // data members
			alignas(concurrent_alignment) std::atomic<ControlBlock*> m_head;
		};

	} // namespace detail

} // namespace density
