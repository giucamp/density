
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
				auto const first_page = allocate_page();
				m_head.store(first_page);
				m_tail_for_alloc.store(first_page);
				m_tail_for_consume.store(first_page);
            }

			base_concurrent_heterogeneous_queue(const base_concurrent_heterogeneous_queue&) = delete;
			base_concurrent_heterogeneous_queue & operator = (const base_concurrent_heterogeneous_queue&) = delete;

			template <typename COMPLETE_ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
				void emplace(CONSTRUCTION_PARAMS &&... i_args)
			{
				auto push_data = begin_push<true>(sizeof(COMPLETE_ELEMENT_TYPE), alignof(COMPLETE_ELEMENT_TYPE));
				
				try
				{
					// construct the type
					new(&push_data.m_control->m_type) RUNTIME_TYPE(RUNTIME_TYPE::template make<COMPLETE_ELEMENT_TYPE>());
				}
				catch (...)
				{
					// this call release the exclusive access and set the dead flag 
					cancel_push(push_data.m_control);

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
					// destroy the type (which probably is a no-operation)
					push_data.m_control->m_type.RUNTIME_TYPE::~RUNTIME_TYPE();

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
				auto consume_data = begin_consume<true>();
				if (consume_data.m_control != nullptr)
				{
					auto const type = &consume_data.m_control->m_type;
					auto const element = consume_data.m_element;
					i_consumer_func(*type, element);
					type->destroy(element);
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
					commit_consume(consume_data);
					return true;
				}
				else
				{
					return false;
				}
			}

		private:
			
			struct PushData
			{
				ControlBlock * m_control;
				void * m_element;
			};
			
			template <bool CAN_WAIT>
				PushData begin_push(size_t i_size, size_t i_alignment)
			{
				void * original_tail, * tail;

				// allocate the type and the element, updating m_tail_for_alloc and tail
				bool has_allocated_space;
				do {

					// read tail
					original_tail = tail = m_tail_for_alloc.load();
					
					// linearly-allocate the control block and the element, updating tail
					control = static_cast<ControlBlock*>(allocate(&tail, sizeof(ControlBlock)));
					new_element = allocate(&tail, i_size, i_alignment > alignof(ControlBlock) ? i_alignment : alignof(ControlBlock));
					
					auto const last_byte = reinterpret_cast<uintptr_t>(original_tail) | static_cast<uintptr_t>(PAGE_ALLOCATOR::page_alignment - 1);
					auto limit = last_byte - sizeof(ControlBlock);
					if (tail > end)
					{
						auto const new_page = allocate_page();

					}

					// try to update m_tail_for_alloc
					has_allocated_space = m_tail_for_alloc.compare_exchange_weak(original_tail, tail);

				} while (!has_allocated_space);

				/* Now we can initialize control->m_next, and set the exclusive-access flag in it (the +1)
				   Other producers can allocate space in the meanwhile (moving m_tail_for_alloc forward). 
				   Consumers are not allowed no read after m_tail_for_consume, which we still did not update */
				control->m_next.store(reinterpret_cast<uintptr_t>(tail) + 1);

				/** Now the 'slot' we have allocated is ready: it can be skipped (control->m_next) is valid,
					but we have exclusive access on it (the first bit of control->m_next is set).
					If other consumers have allocated space (i.e. modified m_tail_for_alloc), now we are going to synchronize
					with them: consumers will exit from the next loop in the exact order they succeeded in updating m_tail_for_alloc.
					Note: we have not yet constructed neither the type object, nor the element. The caller will do after begin_push exits.
					The next atomic operation needs at least the release semantic (consumers must see what 
					we have written in control->m_next). */
				while (!m_tail_for_consume.compare_exchange_weak(original_tail, tail))
				{
					// this can happen only if slower consumer thread allocate space in m_tail_for_alloc before a faster consumer thread
					std::this_thread::yeld();
				}
				
				// Done. Now the caller can construct the type and the element concurrently with consumers and other producers.
				return PushData{control, new_element};
			}

			void cancel_push(ControlBlock * i_control_block) noexcept
			{
				DENSITY_ASSERT_INTERNAL((i_control_block->m_next.load() & 3) == 1);

				/* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the
				same time the exclusive access is removed (the first bit) */
				i_control_block->m_next.fetch_xor(3, std::memory_order::memory_order_release);

				/** Consumers should see what we have done (while producers are not interested). Anyway, if they 
					don't, no inconsistency occurs: soon or later they will see the change in i_control_block->m_next,
					and they will understand that the element is ready to be consumed. So we have three options:
					1) do nothing: consumers will see that the element can be consumed with a delay.
					2) do an atomic operation to m_tail_for_consume with release semantic
					3) put a memory fence
					Let's do 2) by now. */
				//m_tail_for_consume.fetch_add(0, std::memory_order::memory_order_release);
			}

			void commit_push(PushData i_push_data) noexcept
			{
				DENSITY_ASSERT_INTERNAL((i_control_block->m_next.load() & 3) == 1);

				/* decrementing the size we allow the consumers to process this element (this is an atomic operation)
					To do: this is a read-write operation. Make it a write-only op.
				*/
				DENSITY_TEST_RANDOM_WAIT();
				i_push_data.m_control->m_next.fetch_sub(1, std::memory_order::memory_order_release);

				//m_tail_for_consume.fetch_add(0, std::memory_order::memory_order_release);
			}

			template <bool CAN_WAIT>
				ControlBock * begin_consume() noexcept
			{
				void * head;
				ControlBlock * control;

				auto auto limit = m_tail.load();
				
				head = m_head.load();
				
				unsigned skip_counter = 0;
				for (;;)
				{
					if (head == limit)
					{
						//
						return ConsumeData{ nullptr };
					}
					control = static_cast<ControlBlock *>(head);
					
					auto const next_dirt = control->m_next.fetch_or(1, std::memory_order_acquire);
					if ((next_dirt & 1) == 0)
					{
						// we have exclusive access
						if ((next_dirt & 2) == 0)
						{
							// this element is ready to be consumed
							return control;
						}
						else
						{
							DENSITY_ASSERT_INTERNAL( (next_dirt & 3) == 2 );

							/* this element is dead: either it was already consumed, or it threw an exception while
								being constructed */
							if (skip_counter == 0)
							{
								head = reinterpret_cast<void*>(next_dirt - 2);
								m_head.store(head);
								continue;
							}
						}
					}

					/* A producer thread is constructing this element, or a consumer thread is consuming it */
					head = reinterpret_cast<void*>(next_dirt & ~static_cast<uintptr_t>(3));
					skip_counter++;
				}
			}

			void commit_consume(ControlBlock * i_control) noexcept
			{
		        #if DENSITY_DEBUG
		            memset(&(i_control->m_type), 0xB4, sizeof(i_control->m_type));
	            #endif

				/* drop the exclusive access, and mark the element as dead */
				DENSITY_TEST_RANDOM_WAIT();
				i_control->m_next.store(reinterpret_cast<uintptr_t>(i_data.m_next_control) + 2);
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

			alignas(concurrent_alignment) std::atomic<void*> m_head;

			alignas(concurrent_alignment) std::atomic<void*> m_tail_for_alloc;
			std::atomic<void*> m_tail_for_consume;
		};
	} // namespace detail

} // namespace density