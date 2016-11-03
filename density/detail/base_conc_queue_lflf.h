
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
			/* Before each element there is a ControlBlock object */
			struct ControlBlock
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

			/** This is the alignment of ControlBlock's and the minimum alignment of the elements. Since the
				ControlBlock uses the least-significant bits as flags, this can't be less than 4. */
			static constexpr size_t default_alignment = size_max( alignof(ControlBlock), 4 );
			
			/** Constructor. At least one page in always alive */
			base_concurrent_heterogeneous_queue()
            {
				auto const first_page = new_page();
				m_head.store(first_page);
				m_tail.store(first_page);
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

			struct ConsumeData
			{
				ControlBlock * m_control;
				void * m_element;
				ControlBlock * m_next_control;
			};

			template <bool CAN_WAIT>
				PushData begin_push(size_t i_size, size_t i_alignment) noexcept
			{
				DENSITY_ASSERT_INTERNAL(i_size > 0 && is_uint_aligned(i_size, default_alignment));

				void * tail, * future_tail;
				ControlBlock * control, * next_control;
				void * end_of_page;
				void * new_element;
				#if DENSITY_DEBUG_INTERNAL
					void * dbg_original_tail;
				#endif

				for (;;)
				{
					/* The m_next member variable of the control block we are going to allocate is guaranteed to be zero (see the constructor)
						We loop until we succeed in changing the size of the control block from zero to i_size + 1.
						The +1 in the size means that we have exclusive access to the element (needed in order to construct it),
						Consumer threads can skip the element while we have exclusive access on it. */
					bool exclusive_access;
					do {

						/* the tail is reloaded on every iteration, as a failure in the compare_exchange_strong means that
							another thread has succeed, so the tail is changed. */
						DENSITY_TEST_RANDOM_WAIT();
						tail = m_tail.load();
						end_of_page = last_byte_of_page(tail);
						#if DENSITY_DEBUG_INTERNAL
							dbg_original_tail = tail;
						#endif

						/* linearly-allocate the control block and the element, updating tail */					
						control = static_cast<ControlBlock*>(allocate(&tail, sizeof(ControlBlock)));
						new_element = allocate(&tail, i_size, i_alignment > alignof(ControlBlock) ? i_alignment : alignof(ControlBlock));

						/* linearly-allocate the next control block, setting future_tail to the updated tail */
						future_tail = tail;
						next_control = static_cast<ControlBlock*>(allocate(&future_tail, sizeof(ControlBlock)));

						/* try to commit, setting the size of the block. This is the first change visible to the other threads.
							This works because the first block after tail has always the size set to zero. */
						size_t expected = 0;
						DENSITY_TEST_RANDOM_WAIT();
						exclusive_access = control->m_next.compare_exchange_weak(expected, reinterpret_cast<uintptr_t>(next_control) + 1);

						bool can_wait = CAN_WAIT; // use a local variable to avoid the warning about the conditional expression being constant
						if (!can_wait)
						{
							if (!exclusive_access)
							{
								return PushData{nullptr};
							}
						}

					} while ( !exclusive_access );

					/* If future_tail has overrun the page, we fail. So maybe we are wasting some bytes (as the current element
						may still fit in the queue), but this allows a simpler algorithm. */
					if (future_tail > end_of_page)
					{
						DENSITY_ASSERT_INTERNAL( reinterpret_cast<unsigned char*>(control) <= end_of_page);
						new(&(control->m_type)) RUNTIME_TYPE();						
						tail = static_cast<unsigned char*>(new_page());
						control->m_next.store(reinterpret_cast<uintptr_t>(tail));
						m_tail.store(tail);
						continue;
					}
					break;
				}

				/* after gaining exclusive access to the element after tail, we initialize the next control block to zero, to allow
					future concurrent pushes to play the compare_exchange_strong game (the race to obtain exclusive access). */
				DENSITY_TEST_RANDOM_WAIT();
				next_control->m_next.store(0);

				/* now we can commit the tail. This allows the other pushes to skip the element we are going to construct.
					So the duration of the contention between concurrent pushes is really minimal (two atomic stores). */
				DENSITY_ASSERT_INTERNAL(m_tail.load() == dbg_original_tail);
				DENSITY_TEST_RANDOM_WAIT();
				m_tail.store(tail);

				#if DENSITY_DEBUG_INTERNAL
					//m_dbg_size = i_size;
				#endif
											
				return PushData{control, new_element};
			}

			void cancel_push(ControlBlock * i_control_block) noexcept
			{
				/* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the
					same time the exclusive access is removed (the first bit) */
				i_control_block->m_next.fetch_and(~static_cast<uintptr_t>(3));
				i_control_block->m_next.fetch_or(2);
			}

			void commit_push(PushData i_push_data) noexcept
			{
				/* decrementing the size we allow the consumers to process this element (this is an atomic operation) */
				DENSITY_TEST_RANDOM_WAIT();
				--(i_push_data.m_control->m_next);
			}

			template <bool CAN_WAIT>
				ConsumeData begin_consume() noexcept
			{
				DENSITY_TEST_RANDOM_WAIT();
				auto head = m_head.load();

				/* Try-and-repeat loop. On every iteration we skip an element. We stop when we
					get exclusive access on a valid element. If we reach m_tail, we exit. */
				uintptr_t dirt_next;
				size_t tries = 0;
				ControlBlock * control;
				void * element;
				for(;;)
				{
					auto const tail = m_tail.load();
					void * limit;
					if (are_same_page(head, tail))
					{
						limit = tail;
					}
					else
					{
						limit = last_byte_of_page(head);
					}

					// check if we have reached the tail
					DENSITY_TEST_RANDOM_WAIT();
					
					if (head >= tail)
					{
						DENSITY_ASSERT_INTERNAL(tail == head);

						// no consumable element is available
						return ConsumeData{ nullptr };
					}

					/* linearly-allocate the control block, updating head */
					control = static_cast<ControlBlock*>(allocate(&head, sizeof(ControlBlock)));
					element = head;

					/* atomically load the size of the block and set the first bit to 1. If the
						first bit was already set, we are going to repeat the loop. Otherwise we
						have the exclusive access on the content of the element. */
					DENSITY_TEST_RANDOM_WAIT();
					dirt_next = control->m_next.fetch_or(1);

					/*
						cases for (dirt_next & 3):

							0 -> we have got exclusive access to a living element - exit the loop
							1 -> the element is living, but we don't have access - skip and continue
							2 -> we have got exclusive access to a dead element - if this is the first iteration, remove it
							3 -> dead element, and we don't have access to it - skip and continue
					*/

					auto next = reinterpret_cast<ControlBlock*>(dirt_next & (std::numeric_limits<uintptr_t>::max() - 3));

					if ((dirt_next & 3) == 0)
					{
						/* We have obtained exclusive access on a valid element. Exit the loop. */
						if (control->m_type.empty())
						{
							++control->m_next; // clear the bit 0 (exclusive access), and set the bit 1 (dead element)
							head = reinterpret_cast<unsigned char*>(next);
							continue;
						}
						return ConsumeData{control, element, next};
					}

					if ((dirt_next & 1) != 0)
					{
						/* someone else has the exclusive access on the element, continue with the next one. */
						tries++;
					}
					else
					{
						/* We have the exclusive access to a dead element, and this is the first iteration, we can
							advance the head. Only the thread with exclusive access can do this, so we can do it safely */
						DENSITY_TEST_RANDOM_WAIT();
						if (tries == 0)
						{					
							head = reinterpret_cast<unsigned char *>(next);
							m_head.store(head);
						}
						else
						{
							DENSITY_ASSERT_INTERNAL((control->m_next.load() & 1) == 1);
							#if DENSITY_DEBUG_INTERNAL
								const auto prev =
							#endif
							--control->m_next;
							DENSITY_ASSERT_INTERNAL((prev & 1) == 0);

							tries++;

							bool can_wait = CAN_WAIT; // use a local variable to avoid the warning about the conditional expression being constant
							if (!can_wait)
							{
								return ConsumeData{nullptr};
							}
						}
					}
				}
			}

			void commit_consume(ConsumeData i_data) noexcept
			{
		        #if DENSITY_DEBUG
		            memset(&i_data.m_control->m_type, 0xB4, sizeof(i_data.m_control->m_type));
	            #endif

				/* drop the exclusive access, and mark the element as dead */
				DENSITY_TEST_RANDOM_WAIT();
				i_data.m_control->m_next.store(reinterpret_cast<uintptr_t>(i_data.m_next_control) + 2);
			}

        private:

			static bool are_same_page(void * i_first, void * i_second)
			{
				return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) &
					~static_cast<uintptr_t>(PAGE_ALLOCATOR::page_alignment)) == 0;
			}

			static void * last_byte_of_page(void * i_address)
			{
				return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(i_address) | static_cast<uintptr_t>(PAGE_ALLOCATOR::page_alignment - 1));
			}

            DENSITY_NO_INLINE void * new_page()
            {
                auto const result = allocate_page();
				auto const first_control = static_cast<ControlBlock*>(result);
				first_control->m_next.store(0);
				return result;
            }

            void delete_page(void * i_page) noexcept
            {
                deallocate_page(i_page);
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
			alignas(concurrent_alignment) std::atomic<void*> m_tail;
		};
	} // namespace detail

} // namespace density