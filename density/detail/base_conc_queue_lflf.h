
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
			
        public:

			static constexpr size_t default_alignment = queue_header::default_alignment;
			
			base_concurrent_heterogeneous_queue()
            {
                
            }

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

			template <typename COMPLETE_ELEMENT_TYPE>
				void push(COMPLETE_ELEMENT_TYPE && i_source)
			{
				using ElementType = typename std::decay<COMPLETE_ELEMENT_TYPE>::type;
				emplace<ElementType>(std::forward<COMPLETE_ELEMENT_TYPE>(i_source));
			}

			template <typename COMPLETE_ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
				void emplace(CONSTRUCTION_PARAMS && i_args)
			{
				using ElementType = typename std::decay<COMPLETE_ELEMENT_TYPE>::type;

				for (;;)
				{
					PushData push_data = template begin_push<true>(sizeof(ElementType), alignof(ElementType));
					if (push_data.m_control != nullptr)
					{
						try
						{
							new(&push_data.m_control.m_type) RUNTIME_TYPE(RUNTIME_TYPE::make<ElementType>());
						}
						catch (...)
						{
							/* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the
								same time the exclusive access is removed (the first bit) */
							DENSITY_ASSERT_INTERNAL(control->m_size.load() == sizeof(ElementType) + 1);
							push_data.m_control->m_size.store(sizeof(ElementType) + 2);
							throw; // the exception is propagated to the caller, whatever it is
						}

						try
						{
							new(push_data.m_new_element) ElementType(std::forward<CONSTRUCTION_PARAMS>(i_args)...);
						}
						catch (...)
						{
							push_data.m_control->m_type->RUNTIME_TYPE::~RUNTIME_TYPE();

							/* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the
								same time the exclusive access is removed (the first bit) */
							DENSITY_ASSERT_INTERNAL(control->m_size.load() == i_size + 1);
							push_data.m_control->m_size.store(i_size + 2);
							throw; // the exception is propagated to the caller, whatever it is
						}
						push_data.commit_push();
						break;
					}
				}
			}

		private:

			/** Before each element there is a ControlBlock object */
			struct ControlBlock
			{
				std::atomic<size_t> m_size; /** size of the element, plus two additional flags encoded in the least-significant bits.
													   bit 0: exclusive access flag. The thread that succeeds in setting this flag has exclusive
													   access on the content of the element.
													   bit 1: dead element flag. The content of the element is not valid: it has been consumed,
													   or the constructor threw an exception.
													   The size of the element (excluding the control block) is given by:
													   m_size.load() & (std::numeric_limits<m_size>::max - 3). */
				RUNTIME_TYPE m_type; /** type of the element */
			};

			struct PushData
			{
				ControlBlock * m_control;
				void * m_element;
				#if DENSITY_DEBUG_INTERNAL
					INTERNAL_WORD m_dbg_size;
				#endif
			};

			struct ConsumeData
			{
				ControlBlock * m_control;
				void * m_element;
				size_t m_size;
			};

			template <bool CAN_WAIT>
				PushData begin_push(size_t i_size, size_t i_alignment) noexcept
			{
				DENSITY_ASSERT_INTERNAL(i_size > 0 && is_uint_aligned(i_size, default_alignment));

				unsigned char * tail;
				ControlBlock * control, * next_control;
				void * new_element;
				#if DENSITY_DEBUG_INTERNAL
					unsigned char* dbg_original_tail;
				#endif

				/* The size of the control block we are going to allocate is guaranteed to be zero (see the constructor)
					We loop until we succeed in changing the size of the control block from zero to i_size + 1.
					The +1 in the size means that we have exclusive access to the element (needed in order to construct it),
					Consumer threads can skip the element while we have exclusive access on it. */
				bool exclusive_access;
				do {

					/* the tail is reloaded on every iteration, as a failure in the compare_exchange_strong means that
						another thread has succeed, so the tail is changed. */
					DENSITY_TEST_RANDOM_WAIT();
					tail = m_tail.load();
					auto const end_of_page = address_upper_align(tail, PAGE_ALLOCATOR::page_alignment);
					#if DENSITY_DEBUG_INTERNAL
						dbg_original_tail = tail;
					#endif

					/* linearly-allocate the control block and the element, updating tail */					
					control = static_cast<ControlBlock*>(allocate(&tail, sizeof(ControlBlock)));
					new_element = allocate(&tail, i_size, i_alignment > alignof(ControlBlock) ? i_alignment : alignof(ControlBlock));

					/* linearly-allocate the next control block, setting future_tail to the updated tail */
					auto future_tail = tail;
					next_control = static_cast<ControlBlock*>(allocate(&future_tail, sizeof(ControlBlock)));

					/* If future_tail has overrun the page, we fail. So maybe we are wasting some bytes (as the current element
						may still fit in the queue), but this allows a simpler algorithm. */
					if (future_tail > end_of_page)
					{
						return PushData{nullptr};
					}

					/* try to commit, setting the size of the block. This is the first change visible to the other threads.
						This works because the first block after tail has always the size set to zero. */
					size_t expected = 0;
					DENSITY_TEST_RANDOM_WAIT();
					exclusive_access = m_control->m_size.compare_exchange_weak(expected, i_size + 1);

					bool can_wait = CAN_WAIT; // use a local variable to avoid the warning about the conditional expression being constant
					if (!can_wait)
					{
						if (!exclusive_access)
						{
							return PushData{nullptr};
						}
					}

				} while ( !exclusive_access );

				/* after gaining exclusive access to the element after tail, we initialize the next control block to zero, to allow
					future concurrent pushes to play the compare_exchange_strong game (the race to obtain exclusive access). */
				DENSITY_TEST_RANDOM_WAIT();
				next_control->m_size.store(0);

				/* now we can commit the tail. This allows the other pushes to skip the element we are going to construct.
					So the duration of the contention between concurrent pushes is really minimal (two atomic stores). */
				DENSITY_ASSERT_INTERNAL(m_tail.load() == dbg_original_tail);
				DENSITY_TEST_RANDOM_WAIT();
				m_tail.store(tail);

				#if DENSITY_DEBUG_INTERNAL
					m_dbg_size = i_size;
				#endif
											
				return PushData{control, new_element};
			}

			void commit_push(PushData i_push_data) noexcept
			{
				/* decrementing the size we allow the consumers to process this element (this is an atomic operation) */
				DENSITY_TEST_RANDOM_WAIT();
				DENSITY_ASSERT_INTERNAL(i_push_data.m_control->m_size.load() == i_push_data.m_dbg_size + 1);
				--(i_push_data.m_control->m_size);
			}

			template <bool CAN_WAIT>
				ConsumeData begin_consume() noexcept
			{
				DENSITY_TEST_RANDOM_WAIT();
				auto head = m_head.load();
				#if DENSITY_DEBUG_INTERNAL
					const INTERNAL_WORD dbg_original_head = head;
				#endif

				/* Try-and-repeat loop. On every iteration we skip an element. We stop when we
					get exclusive access on a valid element. If we reach m_tail, we exit. */
				size_t dirt_size;
				size_t tries = 0;
				ControlBlock * control;
				void * new_element;
				for(;;)
				{
					// check if we have reached the tail
					DENSITY_TEST_RANDOM_WAIT();
					auto const tail = m_tail.load();
					DENSITY_ASSERT_INTERNAL(tail >= head);
					if (head >= tail)
					{
						DENSITY_ASSERT_INTERNAL(tail == head);

						// no consumable element is available
						return false;
					}

					/* linearly-allocate the control block, updating head */
					control = static_cast<ControlBlock*>(allocate(&head, sizeof(ControlBlock)));

					/* atomically load the size of the block and set the first bit to 1. If the
						first bit was already set, we are going to repeat the loop. Otherwise we
						have the exclusive access on the content of the element. */
					DENSITY_TEST_RANDOM_WAIT();
					dirt_size = control->m_size.fetch_or(1);
					DENSITY_ASSERT_INTERNAL(dirt_size < PAGE_SIZE + 3);

					/* clean up the size and linearly-allocate the element */
					m_size = dirt_size & (std::numeric_limits<INTERNAL_WORD>::max() - 3);
					new_element = allocate(&head, m_size);

					/*
						cases for (dirt_size & 3):

							0 -> we have got exclusive access to a living element - exit the loop
							1 -> the element is living, but we don't have access - skip and continue
							2 -> we have got exclusive access to a dead element - if this is the first iteration, remove it
							3 -> dead element, and we don't have access to it - skip and continue
					*/

					if ((dirt_size & 3) == 0)
					{
						/* We have obtained exclusive access on a valid element. Exit the loop. */
						break;
					}

					if ((dirt_size & 1) != 0)
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
							m_head.store(head);
						}
						else
						{
							DENSITY_ASSERT_INTERNAL((m_control->m_size.load() & 1) == 1);
							#if DENSITY_DEBUG
								const auto prev =
							#endif
							--control->m_size;
							DENSITY_ASSERT_INTERNAL((prev & 1) == 0);

							tries++;

							bool can_wait = CAN_WAIT; // use a local variable to avoid the warning about the conditional expression being constant
							if (!can_wait)
							{
								return false;
							}
						}
					}
				}

				return true;
			}

			void commit() noexcept
			{
		        #if DENSITY_DEBUG
		            memset(&m_control->m_type, 0xB4, sizeof(m_control->m_type));
	            #endif

				/* drop the exclusive access, and mark the element as dead */
				DENSITY_TEST_RANDOM_WAIT();
				m_control->m_size.store(m_size + 2);
			}

        private:

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

			static void * allocate(unsigned char * * io_ptr, size_t i_size)
			{
				auto res = *io_ptr;
				*io_ptr += i_size;
				return *io_ptr;
			}

			static void * allocate(unsigned char * * io_ptr, size_t i_size, size_t i_alignment)
			{
				*io_ptr = static_cast<unsigned char *>(address_upper_align(*io_ptr, i_alignment));
				auto res = *io_ptr;				
				*io_ptr += i_size;
				return *io_ptr;
			}

        private:
            std::atomic<unsigned char*> m_head, m_tail;
		};
	} // namespace detail

} // namespace density