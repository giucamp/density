
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "..\density_common.h"
#include "..\runtime_type.h"
#include "void_allocator.h"
#include <atomic>

namespace density
{
    namespace detail
    {
		constexpr size_t concurrent_alignment = 64;

		/** Fixed-size disposable concurrent queue. 
			disposable_concurrent_queue is an concurrent lock-free multiple-consumers multiple-producers heterogeneous queue.
			This container is disposable, in the sense that it does not recycle the space in the buffer, like a ring buffer does. 
			Every push consumes some capacity. A consume has no effect on the capacity.
				- Both head and tail are monotonic: there is no wrapping at the end of the buffer 
				- The capacity is monotonic: if an element does not fit in the available space, it will never do
			This class is non-copyable and non-movable.
		*/
		template <typename INTERNAL_WORD, typename RUNTIME_TYPE, INTERNAL_WORD BUFFER_SIZE, INTERNAL_WORD INTERNAL_ALIGNMENT>
			class alignas(concurrent_alignment) disposable_concurrent_queue
		{
		public:
			
			static_assert(INTERNAL_ALIGNMENT >= 4, "The alignment must be at least 4");

			struct ControlBlock
			{
				std::atomic<INTERNAL_WORD> m_size; /** size of the element, plus two additional flags encoded in the least-significant bits.
												   The alignment of the size must be >= 4, so that the first two bits are available.
												   The bit 0 of m_size is 1 if a thread has exclusive access on the element. The thread that
												   succeeds in setting this bit from 0 to 1 has the exclusive access.
												   The bit 1 of m_size specify that this element is not valid: it has been consumed, or the
												   construction of the element threw an exception. */
				RUNTIME_TYPE m_type;
			};

			/** Default constructor, not thread-safe. */
			disposable_concurrent_queue()
				: m_head(0), m_tail(0)
			{
				// The push algorithm requires the size of the next control block to be zero
				static_cast<ControlBlock*>(&m_buffer[0])->m_size.store(0);
			}

			disposable_concurrent_queue(const disposable_concurrent_queue &) = delete;
			disposable_concurrent_queue & operator = (const disposable_concurrent_queue &) = delete;

			template <typename CONSTRUCTOR>
				bool push(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor, INTERNAL_WORD i_size)
			{
				INTERNAL_WORD tail;
				void * element;
				#if DENSITY_DEBUG
					INTERNAL_WORD dbg_original_tail;
				#endif
				
				/* The size of the control block we are going to allocate in guaranteed to be zero (see the constructor)
					Loop until we succeed in changing the size of the control block	from zero to i_size + 1.
					The +1 in the size means that we have exclusive access to the element (needed in order to construct it), 
					Consumer threads skip the element while we have exclusive access on it. */
				do {

					/* the tail is reloaded on every iteration, as a failure in the compare_exchange_weak means that
						another thread has succeed, so the tail is changed. */
					tail = m_tail.load();
					DENSITY_ASSERT_INTERNAL(is_uint_aligned(tail, INTERNAL_ALIGNMENT));
					#if DENSITY_DEBUG
						dbg_original_tail = tail;
					#endif

					/* linearly-allocate the control block and the element, updating tail */
					auto control = static_cast<ControlBlock*>(allocate(&tail, sizeof(ControlBlock)));
					element = static_cast<ControlBlock*>(allocate(&tail, i_size));

					/* linearly-allocate the next control block, setting future_tail to the updated tail */
					auto future_tail = tail;
					auto next_control = static_cast<ControlBlock*>(allocate(&future_tail, sizeof(ControlBlock)));

					/* If future_tail has overrun the buffer we fail. So maybe we are wasting some bytes (as the current element
						may still fit in the queue), but this allows a simper algorithm. */
					if (future_tail > BUFFER_SIZE)
					{
						return false; // the new element does not fit in the queue
					}

					/* try to commit, setting the size of the block. This is the first change visible to the other threads */
					INTERNAL_WORD expected = 0;
				} while ( ! control->m_size.compare_exchange_strong(expected, i_size + 1) );

				/* we initialize the size of the next block, to allow future pushes to be synchronized */
				next_control->m_size.store(0);

				/* now we can commit the tail. This allows the other pushes to skip the element we are going to construct */
				DENSITY_ASSERT_INTERNAL(m_tail.load() == dbg_original_tail);
				m_tail.store(tail);

				/* initialize the type of the new element. Hopefully RUNTIME_TYPE is just a pointer */
				#if DENSITY_HANDLE_EXCEPTIONS
					try
				#endif
				{
					new(&control->m_type) RUNTIME_TYPE(i_source_type);
				}
				#if DENSITY_HANDLE_EXCEPTIONS
					catch (...)
					{
						/* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the 
							same time the exclusive access is removed (the first bit) */
						DENSITY_ASSERT_INTERNAL(control->m_size.load() == i_size + 1);
						control->m_size.store(i_size + 2);
						throw;
					}
				#endif		

				#if DENSITY_HANDLE_EXCEPTIONS
					try
				#endif
				{
					/* constructs the new element */
					i_constructor(i_source_type, element);
				}
				#if DENSITY_HANDLE_EXCEPTIONS
					catch (...)
					{
						control->m_type->~RUNTIME_TYPE::RUNTIME_TYPE();

						/* The second bit of m_size is set to 1, meaning that the state of the element is invalid. At the
							same time the exclusive access is removed (the first bit) */
						DENSITY_ASSERT_INTERNAL(control->m_size.load() == i_size + 1);
						control->m_size.store(i_size + 2);
						throw;
					}
				#endif

				/* decrementing the size we allow the consumers to process this element (this is an atomic operation) */
				DENSITY_ASSERT_INTERNAL(control->m_size.load() == i_size + 1);
				--control->m_size;

				return true;
			}

			enum class ConsumeResult
			{
				Success, 
				NoConsumableElement,
				Empty,
			};

            template <typename OPERATION>
				ConsumeResult consume(OPERATION && i_operation)
                   noexcept(noexcept((i_operation(std::declval<RUNTIME_TYPE>(), std::declval<void*>()))))
            {
				auto head = m_head.load();
				auto const original_head = head;

				/* Try-and-repeat loop. On every iteration we skip an element. */
				ControlBlock * control;
				void * element;
				do {

					// check if we have reached the tail
					auto const tail = m_tail.load();
					DENSITY_ASSERT_INTERNAL(tail >= head);
					if (head >= tail)
					{
						// no consumable element is available
						return ConsumeResult::NoConsumableElement;
					}

					/* linearly-allocate the control block, updating head */
					control = static_cast<ControlBlock*>(allocate(&head, sizeof(ControlBlock)));

					/* atomically load the size of the block and set the first bit to 1. If the
						first bit was already set, we are going to repeat the loop. */
					auto const dirt_size = control->m_size.fetch_or(1);

					/* clean up the size and linearly-allocate the element*/
					auto const size = dirt_size & (std::numeric_limits<INTERNAL_WORD>::max() - 1);
					element = static_cast<ControlBlock*>(allocate(&head, size));

				} while ((dirt_size & 1) != 0);

				/* We have the exclusive access on this element, so we can consume it */
				i_operation(control->m_type, element);

				/* Now we try to update the head. If we fail, maybe there is an element being consumed */
				auto expected = original_head;
				if (m_head.compare_exchange_strong(expected, head))
				{
					jump_cunsumed_elements();
				}
				else
				{
					control->m_size.store(size + 2);
				}

				return ConsumeResult::Success;
            }

		private:

			void * allocate(INTERNAL_WORD * io_pos, INTERNAL_WORD i_size)
			{
				void * res = &m_buffer[*io_pos];
				*io_pos += i_size;
				
				return res;
			}

			void jump_cunsumed_elements()
			{
				auto head = m_head.load();
				auto const original_head = head;

				/* Try-and-repeat loop. On every iteration we skip an element. */
				ControlBlock * control;
				void * element;
				do {

					// check if we have reached the tail
					auto const tail = m_tail.load();
					DENSITY_ASSERT_INTERNAL(tail >= head);
					if (head >= tail)
					{
						// no consumable element is available
						return ConsumeResult::NoConsumableElement;
					}

					/* linearly-allocate the control block, updating head */
					control = static_cast<ControlBlock*>(allocate(&head, sizeof(ControlBlock)));

					/* atomically load the size of the block and set the first bit to 1. If the
					first bit was already set, we are going to repeat the loop. */
					auto const dirt_size = control->m_size.fetch_or(1);

					/* clean up the size and linearly-allocate the element*/
					auto const size = dirt_size & (std::numeric_limits<INTERNAL_WORD>::max() - 1);
					element = static_cast<ControlBlock*>(allocate(&head, size));

				} while ((dirt_size & 1) != 0);

				/* We have the exclusive access on this element, so we can consume it */
				i_operation(control->m_type, element);

				/* Now we try to update the head. If we fail, maybe there is an element being consumed */
				auto expected = original_head;
				if (m_head.compare_exchange_strong(expected, head))
				{
					jump_cunsumed_elements();
				}

				return ConsumeResult::Success;
			}

		private:
			alignas(DENSITY_CONCURRENT_DATA_ALIGNMENT) std::atomic<INTERNAL_WORD> m_head;
			alignas(DENSITY_CONCURRENT_DATA_ALIGNMENT) std::atomic<INTERNAL_WORD> m_tail;
			alignas(DENSITY_CONCURRENT_DATA_ALIGNMENT) unsigned char m_buffer[BUFFER_SIZE];
		};

    } // namespace detail

} // namespace density
