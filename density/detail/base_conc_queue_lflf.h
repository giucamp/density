
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
				SynchronizationKind::LocklessMultiple, SynchronizationKind::LocklessMultiple> : public PAGE_ALLOCATOR
		{
			/** This is the alignment of ControlBlock's and the minimum alignment of the elements. Since the
				ControlBlock uses the least-significant bits as flags, this can't be less than 4. */
			static constexpr size_t default_alignment = size_max(alignof(void*), 4);

			/* Before each element there is a ControlBlock object */
			struct alignas(default_alignment) ControlBlock
			{
				std::atomic<uintptr_t> m_next; /** pointer to the next control block, plus two additional flags encoded in the least-significant bits.
										bit 0: exclusive access flag. The thread that succeeds in setting this flag has exclusive
										access on the content of the element.
										bit 1: dead element flag. The content of the element is not valid: it has been consumed,
										or the constructor threw an exception.
										The address of the next control block is given by:
											m_next.load() & (std::numeric_limits<uintptr_t>::max() - 3). */
				RUNTIME_TYPE m_type; /** type of the element */
			};
						
        public:

			/** Constructor. At least one page in always alive */
			base_concurrent_heterogeneous_queue()
            {
				auto const first_page = PAGE_ALLOCATOR::allocate_page();
				m_head.store(reinterpret_cast<uintptr_t>(first_page));
				m_tail_for_alloc.store(first_page);
				m_tail_for_consume.store(first_page);
            }

			base_concurrent_heterogeneous_queue(const base_concurrent_heterogeneous_queue&) = delete;
			base_concurrent_heterogeneous_queue & operator = (const base_concurrent_heterogeneous_queue&) = delete;

			struct PushData
			{
				ControlBlock * m_control;
				void * m_element;
			};

			struct ConsumeData
			{
				ControlBlock * m_control;
			};

			DENSITY_NO_INLINE void handle_end_of_page(void * i_tail)
			{
				/* The first thread that succeed in setting m_tail_for_alloc to last_byte is the one that allocates a new page.
					It's very important to set m_tail_for_alloc to the last byte of the page and not to the end of the page, because
					the latter is in another pages, and incoming producers would write out of the page.*/
				auto last_byte = reinterpret_cast<uintptr_t>(i_tail) | static_cast<uintptr_t>(PAGE_ALLOCATOR::page_alignment - 1);
				
				auto original_tail = i_tail;
				if (m_tail_for_alloc.compare_exchange_weak(original_tail, reinterpret_cast<void*>(last_byte)))
				{
					auto const new_page = PAGE_ALLOCATOR::allocate_page();
					auto control = static_cast<ControlBlock*>(i_tail);
					new (&control->m_type) RUNTIME_TYPE();
					control->m_next.store(reinterpret_cast<uintptr_t>(new_page) + 2);
					
					m_tail_for_alloc.store(new_page);
				}
				else
				{
					std::this_thread::yield();
				}
			}
			
			template <bool CAN_WAIT>
				PushData begin_push(size_t i_size, size_t i_alignment)
			{
				void * original_tail, * tail;
				ControlBlock * control;
				void * new_element;

				// this loop allocates the type and the element, updating m_tail_for_alloc and tail
				for(;;)
				{
					// read tail
					original_tail = tail = m_tail_for_alloc.load(std::memory_order_acquire);
					
					// linearly-allocate the control block and the element, updating tail
					control = static_cast<ControlBlock*>(allocate(&tail, sizeof(ControlBlock)));
					new_element = allocate(&tail, i_size, i_alignment > alignof(ControlBlock) ? i_alignment : alignof(ControlBlock));
										
					auto const end_of_page = ( reinterpret_cast<uintptr_t>(original_tail) | static_cast<uintptr_t>(PAGE_ALLOCATOR::page_alignment - 1) ) + 1;
					auto const limit = reinterpret_cast<void*>(end_of_page - sizeof(ControlBlock) );
					if (tail > limit)
					{
						/* There is no place to allocate another ControlBlock after the new element. 
							We must set this ControlBock as link for a new page. */
						handle_end_of_page(original_tail);
						continue;
					}

					// try to update m_tail_for_alloc
					if (m_tail_for_alloc.compare_exchange_weak(original_tail, tail, std::memory_order_release))
					{
						bool can_wait = CAN_WAIT;
						if (!can_wait)
						{
							return PushData{ nullptr };
						}
						break;
					}
				}

				/* Now we can initialize control->m_next, and set the exclusive-access flag in it (the +1).
				   Other producers can allocate space in the meanwhile (moving m_tail_for_alloc forward). 
				   Consumers are not allowed no read after m_tail_for_consume, which we still did not update,
				   therefore the current page can't be deallocated. */
				control->m_next.store(reinterpret_cast<uintptr_t>(tail) + 1, std::memory_order_relaxed);

				/** Now the 'slot' we have allocated is ready: it can be skipped (control->m_next) is valid,
					but we have exclusive access on it (the first bit of control->m_next is set).
					If other consumers have allocated space (i.e. modified m_tail_for_alloc), now we are going to synchronize
					with them: consumers will exit from the next loop in the exact order they succeeded in updating m_tail_for_alloc.
					Note: we have not yet constructed neither the type object, nor the element. The caller will do after begin_push exits.
					The next atomic operation needs at least the release semantic (consumers must see what 
					we have written in control->m_next). */
				while (!m_tail_for_consume.compare_exchange_weak(original_tail, tail, std::memory_order_acq_rel))
				{
					// this can happen only if slower consumer thread allocate space in m_tail_for_alloc before a faster consumer thread
					std::this_thread::yield();
				}				
				
				/* Done. Now the caller can construct the type and the element concurrently with consumers and other producers. 
				   The current page won't be deallocated until cancel_push or commit_push is called, because we have set the exclusive access
				   flag in control->m_next. */
				return PushData{control, new_element};
			}

			void cancel_push(ControlBlock * i_control_block) noexcept
			{
				DENSITY_ASSERT_INTERNAL((i_control_block->m_next.load() & 3) == 1);

				/* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the
				same time the exclusive access is removed (the first bit) */
				i_control_block->m_next.fetch_xor(3, std::memory_order::memory_order_relaxed);

				/** Consumers should see what we have done (while producers are not interested). Anyway, if they 
					don't, no inconsistency occurs: soon or later they will see the change in i_control_block->m_next,
					and they will understand that the element is ready to be consumed. So we have three options:
					1) do nothing: consumers will see that the element can be consumed with a delay.
					2) do an atomic operation to m_tail_for_consume with release semantic
					3) put a memory fence
					Let's do 2) by now. */
				m_tail_for_consume.fetch_add(0, std::memory_order::memory_order_release);
			}

			void commit_push(PushData i_push_data) noexcept
			{
				DENSITY_ASSERT_INTERNAL((i_push_data.m_control->m_next.load() & 3) == 1);

				/* decrementing the size we allow the consumers to process this element (this is an atomic operation)
					To do: this is a read-write operation. Make it a write-only op.
				*/
				DENSITY_TEST_RANDOM_WAIT();
				i_push_data.m_control->m_next.fetch_sub(1, std::memory_order::memory_order_relaxed);

				m_tail_for_consume.fetch_add(0, std::memory_order::memory_order_release);
			}

			ConsumeData begin_consume() noexcept
			{
				// Get exclusive access on m_head, setting it to zero
				uintptr_t head;
				do {
					head = m_head.fetch_and(0, std::memory_order_acquire);
					if (head == 0)
					{
						std::this_thread::yield();
					}
				} while (head == 0);

				// this is the value of head we are going to set when we have finished
				auto good_head = head;
				
				unsigned skip_count = 0;
				for(;;)
				{
					// check if we have reached the end of the queue
					if (reinterpret_cast<void*>(head) == m_tail_for_consume.load(std::memory_order_acquire))
					{
						m_head.store(good_head, std::memory_order_release);
						return ConsumeData{ nullptr };
					}

					auto const control = reinterpret_cast<ControlBlock*>(head);
					auto const dirt_next = control->m_next.fetch_or(1, std::memory_order_relaxed);
					if ((dirt_next & 1) == 0)
					{
						// exclusive access
						if ((dirt_next & 2) == 0)
						{
							// living object							
							m_head.store(good_head, std::memory_order_release);
							return ConsumeData{ control };
						}
						else
						{
							// dead element
							if (skip_count == 0)
							{
								DENSITY_ASSERT_INTERNAL( (dirt_next & 3) == 2 );

								#if DENSITY_DEBUG_INTERNAL
									control->m_next.store(37, std::memory_order_relaxed);
								#endif								
								if (control->m_type.empty())
								{
									PAGE_ALLOCATOR::deallocate_page(reinterpret_cast<void*>(head & ~static_cast<uintptr_t>(PAGE_ALLOCATOR::page_alignment - 1)));
								}
								good_head = head = dirt_next - 2;
								continue;
							}
							else
							{
								// unlock
								control->m_next.store(dirt_next, std::memory_order_relaxed);
							}
						}
					}

					// Move to the next element, which may be in another page
					head = dirt_next & ~static_cast<uintptr_t>(3);
					skip_count++;
				}
			}

			void commit_consume(ConsumeData i_consume_data) noexcept
			{
		        #if DENSITY_DEBUG
		            memset(&(i_consume_data.m_control->m_type), 0xB4, sizeof(i_consume_data.m_control->m_type));
	            #endif
					
				i_consume_data.m_control->m_next.fetch_xor(3, std::memory_order::memory_order_relaxed);

				m_head.fetch_add(0, std::memory_order_release);
			}

        private:

			static bool are_same_page(void * i_first, void * i_second)
			{
				return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) &
					~static_cast<uintptr_t>(PAGE_ALLOCATOR::page_alignment)) == 0;
			}

			static void * allocate(void * * io_ptr, size_t i_size)
			{
				auto const res = *io_ptr;
				*reinterpret_cast<unsigned char**>(io_ptr) += i_size;
				return res;
			}

			static void * allocate(void * * io_ptr, size_t i_size, size_t i_alignment)
			{
				*io_ptr = address_upper_align(*io_ptr, i_alignment);
				auto const res = *io_ptr;
				*reinterpret_cast<unsigned char**>(io_ptr) += i_size;
				return res;
			}

        private:

			alignas(concurrent_alignment) std::atomic<uintptr_t> m_head;

			alignas(concurrent_alignment) std::atomic<void*> m_tail_for_alloc;
			std::atomic<void*> m_tail_for_consume;
		};
	} // namespace detail

} // namespace density