
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "..\density_common.h"
#include "..\runtime_type.h"
#include <atomic>

namespace density
{
    namespace detail
    {
		constexpr size_t concurrent_alignment = 64;

		/** Fixed-size disposable concurrent queue. 
			disposable_concurrent_queue is an concurrent lock-free multiple-consumers multiple-producers heterogeneous queue.
			This container is disposable, in the sense that it does not recycle the space in the buffer, like a ring buffer does. 
			Every push consumes some capacity. A consume has no effect on the capacity. When the capacity is over.
		*/
		template <typename INTERNAL_WORD, typename RUNTIME_TYPE, size_t BUFFER_SIZE>
			class alignas(concurrent_alignment) disposable_concurrent_queue
		{
		public:
			
			static const size_t granularity = sizeof(INTERNAL_WORD);

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
				DENSITY_ASSERT_INTERNAL( is_uint_aligned(i_size, granularity) );

				INTERNAL_WORD tail;
				void * element;
				#if DENSITY_DEBUG
					INTERNAL_WORD original_tail;
				#endif
				
				/* The size of the control block we are going to allocate in guaranteed to be zero (see the constructor)
					Loop until we succeed in changing the size of the control block	from zero to i_size + 1.
					The +1 in the size signals to consumer threads that the block is not ready to be consumed. */
				do {

					/* the tail is reloaded on every iteration, as a failure in the compare_exchange_weak means that
						another thread has succeed, so the tail is changed. */
					tail = m_tail.load();
					#if DENSITY_DEBUG
						original_tail = tail;
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

					// try to commit, setting the size of the block. This is the first change visible to the other threads
					INTERNAL_WORD expected = 0;
				} while (control->m_size.compare_exchange_weak(expected, i_size + 1) != 0);

				/* we initialize the size of the next block, to allow future pushes to be synchronized */
				next_control->m_size.store(0);

				/* now we can commit the tail. This allows the next pushes */
				DENSITY_ASSERT_INTERNAL(m_tail.load() == original_tail);
				m_tail.store(tail);

				#if DENSITY_HANDLE_EXCEPTIONS
					try
				#endif
				{
					/* initialize the type of the new element. Hopefully RUNTIME_TYPE is just a pointer */
					new(&control->m_type) RUNTIME_TYPE(i_source_type);

					/* constructs the new element */
					i_constructor(i_source_type, element);
				}
				#if DENSITY_HANDLE_EXCEPTIONS
					catch (...)
					{
						DENSITY_ASSERT_INTERNAL(control->m_size.load() == i_size + 1);
						control->m_size.store(1);
						throw;
					}
				#endif

				/* decrementing the size we allow the consumes to process this element (this is an atomic operation) */
				DENSITY_ASSERT_INTERNAL(control->m_size.load() == i_size + 1);
				--control->m_size;

				return true;
			}

            template <typename OPERATION>
                bool consume(OPERATION && i_operation)
                   noexcept(noexcept((i_operation(std::declval<RUNTIME_TYPE>(), std::declval<void*>()))))
            {
				auto head = m_head.load();
				auto const original_head = head;

				for (;;)
				{
					auto const tail = m_tail.load();
					DENSITY_ASSERT_INTERNAL(tail >= head);
					if (head >= tail)
					{
						// no consumable element is available
						return false;
					}

					/* linearly-allocate the control block, updating head */
					auto control = static_cast<ControlBlock*>(allocate(&head, sizeof(ControlBlock)));

					auto const size = control->m_size.fetch_or(1);
					if ((size & 1) == 0)
					{
						element = static_cast<ControlBlock*>(allocate(&head, size));

						i_operation(control->m_type, element);

						auto expected = original_head;
						m_head.compare_exchange_weak(expected, head);
					}
					else
					{
						element = static_cast<ControlBlock*>(allocate(&head, size - 1));
					}
				}
            }

		private:

			struct alignas(granularity) ControlBlock
			{
				std::atomic<INTERNAL_WORD> m_size;
				RUNTIME_TYPE m_type;
			};

			void * allocate(INTERNAL_WORD * io_pos, INTERNAL_WORD i_size)
			{
				void * res = &m_buffer[*io_pos];
				*io_pos += i_size;
				return res;
			}

		private:
			std::atomic<INTERNAL_WORD> m_head;
			std::atomic<INTERNAL_WORD> m_tail;
			unsigned char m_buffer[BUFFER_SIZE];
		};

    } // namespace detail

} // namespace density
