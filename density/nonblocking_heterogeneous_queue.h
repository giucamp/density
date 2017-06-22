
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/raw_atomic.h>
#include <type_traits>
#include <limits>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

namespace density
{
	/** This enum describes the concurrency supported by a set of functions. */
	enum concurrent_cardinality
	{
		concurrent_cardinality_single, /**< Functions with this concurrent cardinality can be called by only one thread,
											or by multiple threads if externally synchronized with a mutex. */
		concurrent_cardinality_multiple, /**< Multiple threads can call the functions with this concurrent cardinality
											without external synchronization. */
		concurrent_cardinality_multiple_high_contention /**< Functionally equivalent to concurrent_cardinality_multiple, 
											but adds an hint to the implementation that there is high contention between
											threads on these functions. */
	};

	namespace detail
	{
		template<typename COMMON_TYPE> struct NbQueueControl // used by nonblocking_heterogeneous_queue<T,...>
		{
			volatile uintptr_t m_next; // raw atomic
			COMMON_TYPE * m_element;
		};

		template<> struct NbQueueControl<void> // used by nonblocking_heterogeneous_queue<void,...>
		{
			volatile uintptr_t m_next; // raw atomic
		};

		enum NbQueue_Flags : uintptr_t
		{
			NbQueue_Busy = 1, // set on NbQueueControl::m_next when a thread is producing or consuming an element
			NbQueue_Dead = 2,  /* set on NbQueueControl::m_next when an element is not consumable.
							   If NbQueue_Dead is set, then NbQueue_Busy is meaningless.
							   This flag is not revertible: once it is set, it can't be removed. */
			NbQueue_External = 4,  // set on NbQueueControl::m_next in case of external allocation
			NbQueue_InvalidNextPage = 8,  // initial value for the pointer to the next page
			NbQueue_AllFlags = NbQueue_Busy | NbQueue_Dead | NbQueue_External | NbQueue_InvalidNextPage
		};

		/* workaround for internal compiler error with Vs2017 RC */
		template <typename TYPE>
			inline void destroy_obj(TYPE * i_obj)
		{
			i_obj->TYPE::~TYPE();
		}

		/** /internal Class template that implements put operations */
		template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE, concurrent_cardinality >
			class NonblockingQueueTail : protected ALLOCATOR_TYPE
		{
		protected:

			using ControlBlock = typename detail::NbQueueControl<COMMON_TYPE>;
			
			/** Structure used for elements that must be allocated outside the pages */
			struct ExternalBlock
			{
				COMMON_TYPE * m_element = nullptr;
				void * m_block = nullptr;
				size_t m_size = 0, m_alignment = 0;
			};
			
			/** Minimum alignment used for the storage of the elements. The storage of elements is always aligned according to the most-derived type. */
			constexpr static size_t min_alignment = detail::size_max(
				detail::size_log2(detail::NbQueue_AllFlags + 1),
				detail::size_max(alignof(ControlBlock), alignof(RUNTIME_TYPE)),
				sizeof(ControlBlock), concurrent_alignment);

			/** Returns whether the input addresses belongs to the same page or they are both nullptr */
			static bool same_page(const void * i_first, const void * i_second) noexcept
			{
				auto const page_mask = ALLOCATOR_TYPE::page_alignment - 1;
				return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
			}

			NonblockingQueueTail()
			{
				auto const first_page = ALLOCATOR_TYPE::allocate_page_zeroed();
				auto const new_page_end_block = get_end_control_block(first_page);
				new_page_end_block->m_next = detail::NbQueue_InvalidNextPage;
				std::atomic_init(&m_tail, static_cast<ControlBlock*>(first_page) );
			}

			~NonblockingQueueTail()
			{
				auto const tail = m_tail.load();
				auto const end_block = get_end_control_block(tail);
				end_block->m_next = 0;
				ALLOCATOR_TYPE::deallocate_page_zeroed(tail);
			}

			static constexpr uintptr_t get_element_required_size(size_t i_size, size_t i_alignment) noexcept
			{
				size_t result = s_element_offset + i_size;

				// add the padding required to align the element after the runtime_type
				result += (i_alignment > alignof(RUNTIME_TYPE) ? (i_alignment - alignof(RUNTIME_TYPE)) : 0);

				return uint_upper_align(result, min_alignment);
			}

			static constexpr uintptr_t get_raw_allocation_required_size(size_t i_size, size_t i_alignment) noexcept
			{
				size_t result = s_type_eraser_offset + i_size;

				// add the padding required to align the block
				result += (i_alignment > alignof(RUNTIME_TYPE) ? (i_alignment - alignof(RUNTIME_TYPE)) : 0);

				return uint_upper_align(result, min_alignment);
			}

			static ControlBlock * get_end_control_block(void * i_address)
			{
				auto const page = address_lower_align(i_address, ALLOCATOR_TYPE::page_alignment);
				return static_cast<ControlBlock *>(address_add(page, s_end_control_offset));
			}

			struct Put
			{
				ControlBlock * m_control = nullptr;
				uintptr_t m_next_ptr = 0;
			};

			/** Appends a block of memory in the last page, after the tail, and then moves it. 
					This function pins pages only if it need to allocate a new page.
					This function pass-through any exception thrown by the page allocator.
				@param i_size size in bytes of the block to allocate. It must be greater than zero, and
					a multiple of min_alignment
				@param i_flags Combination of flags to set on the allocated block. The flag NbQueue_InvalidNextPage is not allowed.
				@return Instance of Put that holds a pointer to the allocated block, and the value of its field m_next
						when the function has returned. This information is returned just to avoid a redundant atomic load
						to the caller. */
			Put inplace_allocate_impl(uintptr_t const i_size, intptr_t const i_flags)
			{
				DENSITY_ASSERT_INTERNAL(i_size > 0 && i_size <= s_max_internal_size &&
					uint_is_aligned(i_size, min_alignment) &&
					i_flags <= detail::NbQueue_AllFlags && (i_flags & detail::NbQueue_InvalidNextPage) == 0);

				auto tail = m_tail.load(detail::mem_relaxed);
				for(;;)
				{					
					DENSITY_ASSERT_INTERNAL(tail != nullptr && address_is_aligned(tail, min_alignment));
						
					auto const new_tail = static_cast<ControlBlock*>(address_add(tail, i_size));
					auto const new_tail_offset = reinterpret_cast<uintptr_t>(new_tail) & (ALLOCATOR_TYPE::page_alignment - 1);

					if ( DENSITY_LIKELY( new_tail_offset <= s_end_control_offset) )
					{
						/* No page overflow occurs with the new tail we have computed. */
						if (m_tail.compare_exchange_weak(tail, new_tail, detail::mem_acquire, detail::mem_relaxed))
						{
							/* Assign m_next, and set the flags. This is very important for the consumers,
								because they that need this write happens before any other part of the
								allocated memory is modified. */
							auto const block = static_cast<ControlBlock*>(tail);
							auto const next_ptr = reinterpret_cast<uintptr_t>(new_tail) + i_flags;
							DENSITY_ASSERT_INTERNAL(raw_atomic_load(block->m_next, detail::mem_relaxed) == 0);
							raw_atomic_store(block->m_next, next_ptr, detail::mem_release);

							DENSITY_ASSERT_INTERNAL(block < get_end_control_block(tail));
							return { block, next_ptr };
						}
					}
					else
					{
						tail = page_overflow(tail);
					}
				}
			}

			/** Handles a page overflow of the tail. This function may allocate a new page.
				@param i_tail the value read from m_tail. Note that other threads may have updated m_tail 
					in then meanwhile.
				@return an update value of tail, that makes the curremt thread progress. */
			DENSITY_NO_INLINE ControlBlock * page_overflow(ControlBlock * const i_tail)
			{
				auto const page_end = get_end_control_block(i_tail);
				if (i_tail < page_end)
				{
					/* There is space between the (presumed) current tail and the end control block.
						We try to pad it with a dead element. */
					auto expected_tail = i_tail;
					if (m_tail.compare_exchange_weak(expected_tail, page_end, detail::mem_relaxed, detail::mem_relaxed))
					{
						// m_tail was successfully updated, now we can setup the padding element
						auto const block = static_cast<ControlBlock*>(i_tail);
						raw_atomic_store(block->m_next, reinterpret_cast<uintptr_t>(page_end) + detail::NbQueue_Dead, detail::mem_release);
						return page_end;
					}
					else
					{
						// we have in expected_tail an updated value of m_tail
						return expected_tail;
					}
				}
				else
				{
					// get or allocate a new page
					return get_or_allocate_next_page(i_tail);
				}
			}

			ControlBlock * get_or_allocate_next_page(ControlBlock * const i_end_control)
			{
				DENSITY_ASSERT_INTERNAL(i_end_control != nullptr &&
					address_is_aligned(i_end_control, min_alignment) &&
					i_end_control == get_end_control_block(i_end_control) );

				/* We are going to access the content of the end control, so we have to do a safe pin
					(that is, pin the presumed tail, and then check if the tail has changed in the meanwhile). */
				struct ScopedPin
				{
					ALLOCATOR_TYPE * const m_allocator;
					ControlBlock * const m_control;

					ScopedPin(ALLOCATOR_TYPE * i_allocator, ControlBlock * i_control)
						: m_allocator(i_allocator), m_control(i_control)
					{
						m_allocator->ALLOCATOR_TYPE::pin_page(m_control);
					}

					~ScopedPin()
					{
						m_allocator->ALLOCATOR_TYPE::unpin_page(m_control);
					}
				};
				ScopedPin const end_block(this, static_cast<ControlBlock*>(i_end_control));
				auto const updated_tail = m_tail.load(detail::mem_relaxed);
				if (updated_tail != end_block.m_control)
				{
					return updated_tail;
				}
				// now the end control block is pinned, we can safely access it

				// allocate and setup a new page
				auto new_page = reinterpret_cast<ControlBlock*>(ALLOCATOR_TYPE::allocate_page_zeroed());
				DENSITY_ASSERT(new_page != nullptr && address_is_aligned(new_page, ALLOCATOR_TYPE::page_alignment));
				auto const new_page_end_block = get_end_control_block(new_page);
				new_page_end_block->m_next = detail::NbQueue_InvalidNextPage;

				uintptr_t expected_next = detail::NbQueue_InvalidNextPage;
				if (!raw_atomic_compare_exchange_strong(end_block.m_control->m_next, expected_next,
					reinterpret_cast<uintptr_t>(new_page) + detail::NbQueue_Dead ) )
				{
					/* Some other thread has already linked a new page. We discard the page we 
						have just allocated. */
					new_page_end_block->m_next = 0;
					ALLOCATOR_TYPE::deallocate_page_zeroed(new_page);

					/* So end_block.m_control->m_next may now be the pointer to the next page 
						or 0 (if the page has been consumed in the meanwhile). */
					if (expected_next == 0)
					{
						return updated_tail;
					}

					new_page = reinterpret_cast<ControlBlock*>(expected_next & ~detail::NbQueue_AllFlags);
					DENSITY_ASSERT_INTERNAL(new_page != nullptr && address_is_aligned(new_page, ALLOCATOR_TYPE::page_alignment));
				}

				auto expected_tail = i_end_control;
				if (m_tail.compare_exchange_strong(expected_tail, new_page))
					return new_page;
				else
					return expected_tail;
			}

			static void commit_element_impl(const Put & i_put) noexcept
			{
				// we expect to have NbQueue_Busy and not NbQueue_Dead
				DENSITY_ASSERT_INTERNAL(address_is_aligned(i_put.m_control, min_alignment));
				DENSITY_ASSERT_INTERNAL(
					(i_put.m_next_ptr & ~detail::NbQueue_AllFlags) == (raw_atomic_load(i_put.m_control->m_next, detail::mem_relaxed) & ~detail::NbQueue_AllFlags) &&
					(i_put.m_next_ptr & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

				// remove the flag NbQueue_Busy
				raw_atomic_store(i_put.m_control->m_next, i_put.m_next_ptr - detail::NbQueue_Busy, detail::mem_seq_cst);
			}

			static void cancel_element_impl(const Put & i_put) noexcept
			{
				// we expect to have NbQueue_Busy and not NbQueue_Dead
				DENSITY_ASSERT_INTERNAL(address_is_aligned(i_put.m_control, min_alignment));
				DENSITY_ASSERT_INTERNAL(
					(i_put.m_next_ptr & ~detail::NbQueue_AllFlags) == (raw_atomic_load(i_put.m_control->m_next, detail::mem_relaxed) & ~detail::NbQueue_AllFlags) &&
					(i_put.m_next_ptr & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

				// remove NbQueue_Busy and add NbQueue_Dead
				auto const addend = static_cast<uintptr_t>(detail::NbQueue_Dead) - static_cast<uintptr_t>(detail::NbQueue_Busy);
				raw_atomic_store(i_put.m_control->m_next, i_put.m_next_ptr + addend, detail::mem_seq_cst);
			}

			void * raw_allocate_impl(size_t i_size, size_t i_alignment)
			{
				if (i_size <= s_max_internal_size)
				{
					// the allocation fits in a page
					auto const put = inplace_allocate_impl(get_raw_allocation_required_size(
						i_size, i_alignment, detail::NbQueue_Dead));
					return address_upper_align(address_add(put.m_control, s_type_eraser_offset), i_alignment);
				}
				else
				{
					// the allocation does not fit in a page, allocate inplace space for an ExternalBlock
					auto const put = inplace_allocate_impl(get_raw_allocation_required_size(
						sizeof(ExternalBlock), alignof(ExternalBlock), detail::NbQueue_Dead | detail::NbQueue_External));
					void * block_alloc = address_add(put.m_control, s_type_eraser_offset);
					if (alignof(ExternalBlock) > alignof(RUNTIME_TYPE))
					{
						block_alloc = address_upper_align(block_alloc, alignof(ExternalBlock));
					}

					// allocate the block externally
					DENSITY_ASSERT_INTERNAL(block_alloc != nullptr);
					auto external_block = new(block_alloc) ExternalBlock{};
					external_block->m_size = i_size;
					external_block->m_alignment = i_alignment;
					return external_block->m_block = ALLOCATOR_TYPE::allocate(i_size, i_alignment);
				}
			}

			ControlBlock * get_tail_for_consumers() noexcept
			{
				return m_tail.load();
			}

		protected:
			constexpr static uintptr_t s_type_eraser_offset = uint_upper_align(sizeof(ControlBlock), alignof(RUNTIME_TYPE));
			constexpr static uintptr_t s_element_offset = s_type_eraser_offset + sizeof(RUNTIME_TYPE);
			constexpr static uintptr_t s_page_unit_capacity = (ALLOCATOR_TYPE::page_size / min_alignment);
			constexpr static size_t s_max_internal_size = (s_page_unit_capacity / 2) * min_alignment;
			constexpr static uintptr_t s_end_control_offset = ALLOCATOR_TYPE::page_size - min_alignment;

		private: // data members
			alignas(concurrent_alignment) std::atomic<ControlBlock*> m_tail;
		};

		template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE, 
				concurrent_cardinality PROD_CARDINALITY, concurrent_cardinality CONSUMER_CARDINALITY >
			class NonblockingQueueHead : protected NonblockingQueueTail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY>
		{
		private:

			using Base = NonblockingQueueTail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY>;

		protected:

			NonblockingQueueHead()
			{
				std::atomic_init(&m_head, get_tail_for_consumers());
			}

			struct Consume
			{
				NonblockingQueueHead * m_queue = nullptr;
				ControlBlock * m_control = nullptr;				
				uintptr_t m_next_ptr = 0;

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
						m_being_consumed = i_source.m_being_consumed;
						i_source.m_control = nullptr;
						i_source.m_being_consumed = nullptr;
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

				/** Moves the Consume to the head */
				void start_from_head(NonblockingQueueHead * i_queue) noexcept
				{
					ControlBlock * head = i_queue->m_head.load();

					while (!same_page(m_control, head))
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

				/** \internal
					Tries to start a consume operation. The returned ConsumeData is empty if the operation fails.
					Otherwise it contains a pointer to the head and a pointer to the element being consumed.
					All the pages in the inclusive range { m_head, m_control } get pinned. */
				void start_consume_impl(NonblockingQueueHead * i_queue) noexcept
				{
					DENSITY_ASSERT_INTERNAL(m_next_ptr == 0);

					start_from_head(i_queue);

					for (;;)
					{
						auto const next_uint = raw_atomic_load(m_control->m_next, detail::mem_seq_cst);

						// check if next_uint is non-zero (excluding the bit NbQueue_InvalidNextPage)
						if ( (next_uint & ~detail::NbQueue_InvalidNextPage) != 0)
						{
							if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == 0)
							{
								auto expected = next_uint;
								if (raw_atomic_compare_exchange_strong(m_control->m_next, expected, next_uint | detail::NbQueue_Busy,
									detail::mem_seq_cst, detail::mem_relaxed))
								{
									m_next_ptr = next_uint | NbQueue_Dead;
									return;
								}
							}

							auto const next = reinterpret_cast<ControlBlock*>(next_uint & ~detail::NbQueue_AllFlags);
							DENSITY_ASSERT_INTERNAL(next != 0);

							if (!same_page(m_control, next))
							{
								DENSITY_ASSERT_INTERNAL(address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
								m_queue->ALLOCATOR_TYPE::pin_page(next);

								auto const updated_next_uint = raw_atomic_load(m_control->m_next, detail::mem_seq_cst);
								auto const updated_next = reinterpret_cast<ControlBlock*>(updated_next_uint & ~detail::NbQueue_AllFlags);
								if (updated_next == nullptr)
								{
									start_from_head(i_queue);
									m_queue->ALLOCATOR_TYPE::unpin_page(next);
									continue;
								}

								DENSITY_ASSERT_INTERNAL(next == updated_next);

								m_queue->ALLOCATOR_TYPE::unpin_page(m_control);
							}
							m_control = next;
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
					if (!same_page(m_control, i_tail))
					{
						curr = get_end_control_block(m_control);
					}

					ControlBlock * last_nonzero = nullptr;

					if (curr > m_control)
					{
						for (;;)
						{
							curr = static_cast<ControlBlock *>(address_sub(curr, min_alignment));
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
					raw_atomic_store(m_control->m_next, m_next_ptr, detail::mem_seq_cst);
					m_next_ptr = 0;

					start_from_head(m_queue);

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

						bool const is_same_page = same_page(m_control, next);
						DENSITY_ASSERT_INTERNAL(!is_same_page == address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
						auto const dbg_end_block = get_end_control_block(m_control);
						DENSITY_ASSERT_INTERNAL(is_same_page == (m_control != dbg_end_block));

						auto const address_of_next = const_cast<uintptr_t*>(&m_control->m_next);
						std::memset(m_control, 0, address_diff(address_of_next, m_control));

						if (DENSITY_LIKELY(is_same_page))
						{
							raw_atomic_store(m_control->m_next, 0);

							std::memset(address_of_next, 0, address_diff(next, address_of_next + 1));
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

					raw_atomic_store(m_control->m_next, m_next_ptr | detail::NbQueue_Busy, detail::mem_seq_cst);
					m_next_ptr = 0;
				}

			}; // Consume

			template <typename OUT_STREAM>
				void report(OUT_STREAM & i_out_stream)
			{
				struct Stats
				{
					size_t pages = 0;
					size_t element_count = 0;
					size_t external_count = 0;
					size_t busy_count = 0;
					size_t control_count = 0;
				} stats;

				i_out_stream << "nonblocking_heterogeneous_queue at: 0x" << static_cast<void*>(this);
				i_out_stream << "\n\tpage_alignment: " << ALLOCATOR_TYPE::page_alignment;
				i_out_stream << ", page_size: " << ALLOCATOR_TYPE::page_size;
				i_out_stream << ", min_alignment: " << min_alignment;
				i_out_stream << ", sizeof(ControlBlock): " << sizeof(ControlBlock);
				i_out_stream << ", sizeof(runtime_type): " << sizeof(RUNTIME_TYPE);
				
#if 0
				auto const head = ALLOCATOR_TYPE::pin_page(m_head.load());
				DENSITY_ASSERT_INTERNAL(address_is_aligned(head, min_alignment));
				auto control = static_cast<ControlBlock*>(address_lower_align(head, ALLOCATOR_TYPE::page_alignment));
				for (;;)
				{
					auto const tail = get_tail_for_consumers();
					DENSITY_ASSERT_INTERNAL(address_is_aligned(tail, min_alignment));

					DENSITY_ASSERT_INTERNAL(address_is_aligned(control, min_alignment));
					auto next_uint = raw_atomic_load(control->m_next, detail::mem_seq_cst);
					DENSITY_ASSERT_INTERNAL((next_uint & (min_alignment - 1)) <= detail::NbQueue_AllFlags);

					auto const page_start = address_lower_align(control, ALLOCATOR_TYPE::page_alignment);
					auto const end_control_block = get_end_control_block(page_start);
					DENSITY_ASSERT_INTERNAL(address_is_aligned(end_control_block, min_alignment));
					DENSITY_ASSERT_INTERNAL(control <= end_control_block);

					if (control == page_start)
					{
						i_out_stream << "\n\n ---- PAGE 0x" << page_start << " ----";
						stats.pages++;
					}

					bool const is_head = control == head;
					bool const is_tail = control == tail;

					if (next_uint == 0)
					{
						size_t zeroed_size = 0;
						auto start_of_zeroed_zone = control;
						do {

							#if DENSITY_DEBUG_INTERNAL
							for (size_t i = 0; i < min_alignment; i++)
							{
								DENSITY_ASSERT_INTERNAL(reinterpret_cast<unsigned char*>(control)[i] == 0);
							}
							#endif
							zeroed_size += min_alignment;
							control = static_cast<ControlBlock*>(address_add(control, min_alignment));
							next_uint = raw_atomic_load(control->m_next, detail::mem_seq_cst);
						} while (next_uint == 0 && control != end_control_block &&
							control != head && control != tail );

						i_out_stream << "\n0x" << static_cast<void*>(start_of_zeroed_zone);
						i_out_stream << ": " << zeroed_size << " zeroed bytes";
					}
					else
					{
						stats.control_count++;
						auto const next = reinterpret_cast<ControlBlock*>(next_uint & ~detail::NbQueue_AllFlags);

						i_out_stream << "\n0x" << static_cast<const void*>(control);

						DENSITY_ASSERT_INTERNAL((control == end_control_block) == !same_page(control, next));
						DENSITY_ASSERT_INTERNAL((control == end_control_block) == address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));

						if (control == end_control_block)
						{
							DENSITY_ASSERT_INTERNAL((next_uint & detail::NbQueue_AllFlags) == detail::NbQueue_Dead);
							i_out_stream << ": end control block, next:" << static_cast<const void*>(next);
						}
						else if (next_uint & detail::NbQueue_Dead)
						{
							i_out_stream << ": control block of " << address_diff(next, control) << " bytes";
						}
						else
						{
							i_out_stream << ": element of " << address_diff(next, control) << " bytes";
							stats.element_count++;
						}

						if (next_uint & detail::NbQueue_Busy)
						{
							i_out_stream << " +busy";
							stats.busy_count++;
						}

						if (next_uint & detail::NbQueue_External)
						{
							i_out_stream << " +external";
							stats.external_count++;
						}

						if (!same_page(next, control))
						{
							ALLOCATOR_TYPE::unpin_page(control, true);
							ALLOCATOR_TYPE::pin_page(next);
						}
						control = next;	
					}

					if (is_head && is_tail)
					{
						i_out_stream << " <- HEAD, TAIL";
					}
					else if (is_tail)
					{
						i_out_stream << " <- TAIL";
					}
					else if (is_head)
					{
						i_out_stream << " <- HEAD";
					}

					if (control == end_control_block)
					{
						break;
					}
				}

				do_unpin_page(3, control, true);

#endif

				i_out_stream << "\npages: " << stats.pages;
				i_out_stream << "\ncontrols: " << stats.control_count;
				i_out_stream << "\nelements: " << stats.element_count;
				i_out_stream << "\nbusy elements: " << stats.busy_count;
				i_out_stream << "\nexternal elements: " << stats.external_count;
				i_out_stream << "\n\n";
			}

		private: // data members
			alignas(concurrent_alignment) std::atomic<ControlBlock*> m_head;
		};
	}

	namespace experimental
	{
		/** \brief Concurrent heterogeneous FIFO container-like class template. 
	
		*/
		template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator,
				concurrent_cardinality PROD_CARDINALITY = concurrent_cardinality_multiple,
				concurrent_cardinality CONSUMER_CARDINALITY = concurrent_cardinality_multiple >
			class nonblocking_heterogeneous_queue : private detail::NonblockingQueueHead<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSUMER_CARDINALITY>
		{
		private:
			using Base = detail::NonblockingQueueHead<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSUMER_CARDINALITY>;
		
		public:

			using common_type = COMMON_TYPE;
			using runtime_type = RUNTIME_TYPE;
			using value_type = std::pair<const runtime_type &, common_type* const>;
			using allocator_type = ALLOCATOR_TYPE;
			using pointer = value_type *;
			using const_pointer = const value_type *;
			using reference = value_type;
			using const_reference = const value_type&;
			using size_type = std::size_t;

			template <typename OUT_STREAM>
				void report(OUT_STREAM & i_out_stream)
			{
				Base::report(i_out_stream);
			}

			/** Minimum alignment used for the storage of the elements. The storage of elements is always aligned according to the most-derived type. */
			constexpr static size_t min_alignment = Base::min_alignment;

			static_assert(std::is_same<COMMON_TYPE, typename RUNTIME_TYPE::common_type>::value,
				"COMMON_TYPE and RUNTIME_TYPE::common_type must be the same type (did you try to use a type like heter_cont<A,runtime_type<B>>?)");

			static_assert(std::is_same<COMMON_TYPE, typename std::decay<COMMON_TYPE>::type>::value,
				"COMMON_TYPE can't be cv-qualified, an array or a reference");

			static_assert(is_power_of_2(ALLOCATOR_TYPE::page_alignment) &&
				ALLOCATOR_TYPE::page_alignment >= ALLOCATOR_TYPE::page_size &&
				(ALLOCATOR_TYPE::page_alignment % min_alignment) == 0,
				"The alignment of the pages must be 1) a power of 2, 2) greater or equal to the size of the pages 3) a multiple of min_alignment");

			class put_transaction;
			template <typename TYPE> class typed_put_transaction;
			using reentrant_put_transaction = put_transaction;
			template <typename TYPE> using reentrant_typed_put_transaction = typed_put_transaction<TYPE>;

			/** Same to heterogeneous_queue::heterogeneous_queue() */
			nonblocking_heterogeneous_queue()
			{
			}

			// Destructor
			~nonblocking_heterogeneous_queue()
			{
				consume_operation consume_op;
				while (start_consume(consume_op))
				{
					consume_op.commit();
				}
			}

			/** Same to heterogeneous_queue::push.

				\n <b>Thread safeness</b>: thread safe. */
			template <typename ELEMENT_TYPE>
				void push(ELEMENT_TYPE && i_source)
			{
				emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
			}

			/** Same to heterogeneous_queue::emplace.

				\n <b>Thread safeness</b>: thread safe. */
			template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
				void emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
			{
				start_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
			}

			/** Same to heterogeneous_queue::start_emplace.

				\n <b>Thread safeness</b>: thread safe. */
			template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
				typed_put_transaction<ELEMENT_TYPE> start_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
			{
				// allocate on the queue space for an ELEMENT_TYPE (or an ExternalBlock in case the element is very big)
				constexpr bool inplace_element = sizeof(ELEMENT_TYPE) <= s_max_internal_size;
				using inplace_type = typename std::conditional<inplace_element, ELEMENT_TYPE, ExternalBlock>::type;
				auto const put = inplace_allocate_impl(
					get_element_required_size(sizeof(inplace_type), alignof(inplace_type)),
					detail::NbQueue_Busy | (inplace_element ? 0 : detail::NbQueue_External) );

				// compute the address of the element
				void * element_alloc = address_add(put.m_control, s_element_offset);
				constexpr bool needs_alignment = alignof(inplace_type) > alignof(runtime_type);
				if (needs_alignment)
				{
					element_alloc = address_upper_align(element_alloc, alignof(inplace_type));
				}

				try
				{
					if ( ! inplace_element )
					{
						// allocate the element externally
						DENSITY_ASSERT_INTERNAL(element_alloc != nullptr);
						auto external_block = new(element_alloc) ExternalBlock{};
						element_alloc = external_block->m_block = ALLOCATOR_TYPE::allocate(sizeof(ELEMENT_TYPE), alignof(ELEMENT_TYPE));
					}

					// construct the element
					DENSITY_ASSERT_INTERNAL(element_alloc != nullptr);
					COMMON_TYPE * const element = new(element_alloc) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);

					// construct the type eraser
					try
					{
						auto type_eraser = address_add(put.m_control, s_type_eraser_offset);
						DENSITY_ASSERT_INTERNAL(type_eraser != nullptr);
						new(type_eraser) runtime_type(runtime_type::template make<ELEMENT_TYPE>());
					}
					catch (...)
					{
						//element->ELEMENT_TYPE::~ELEMENT_TYPE();
						detail::destroy_obj(element); /* workaround for internal compiler error with Vs2017 RC */
						throw;
					}
					return typed_put_transaction<ELEMENT_TYPE>(this, put, element);
				}
				catch (...)
				{
					cancel_element_impl(put);
					throw;
				}				
			}

			/** Same to heterogeneous_queue::put_transaction. */
			class put_transaction
			{
			public:

				/** Copy construction is not allowed */
				put_transaction(const put_transaction &) = delete;

				/** Copy assignment is not allowed */
				put_transaction & operator = (const put_transaction &) = delete;

				/** Same to heterogeneous_queue::put_transaction::put_transaction(put_transaction &&). */
				put_transaction(put_transaction && i_source) noexcept
					: m_put(i_source.m_put), m_element(i_source.m_element), m_queue(i_source.m_queue)
				{
					i_source.m_queue = nullptr;
				}

				/** Same to heterogeneous_queue::put_transaction::operator = (put_transaction &&). */
				put_transaction & operator = (put_transaction && i_source)
				{
					if (this != &i_source)
					{
						m_queue = i_source.m_queue;
						m_put = i_source.m_put;
						m_element = i_source.m_element;
						i_source.m_queue = nullptr;
					}
					return *this;
				}

				/** Same to heterogeneous_queue::put_transaction::raw_allocate. */
				void * raw_allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t))
				{
					DENSITY_ASSERT(!empty());
					return m_queue->raw_allocate_impl(i_size, i_alignment);
				}

				/** Same to heterogeneous_queue::put_transaction::raw_allocate_copy. */
				template <typename INPUT_ITERATOR>
					typename std::iterator_traits<INPUT_ITERATOR>::value_type *
						raw_allocate_copy(INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end)
				{
					using DiffType = typename std::iterator_traits<INPUT_ITERATOR>::difference_type;
					using ValueType = typename std::iterator_traits<INPUT_ITERATOR>::value_type;
					static_assert(std::is_trivially_destructible<ValueType>::value,
						"put_transaction provides a raw memory inplace allocation that does not invoke destructors when deallocating");

					auto const count_s = std::distance(i_begin, i_end);
					auto const count = static_cast<size_t>(count_s);
					DENSITY_ASSERT(static_cast<DiffType>(count) == count_s);

					auto const elements = static_cast<ValueType*>(raw_allocate(sizeof(ValueType), alignof(ValueType)));
					//std::uninitialized_copy(i_begin, i_end, elements);
					for (auto curr = elements; i_begin != i_end; ++i_begin, ++curr)
						new(curr) ValueType(*i_begin);
					return elements;
				}

				/** Same to heterogeneous_queue::put_transaction::raw_allocate_copy. */
				template <typename INPUT_RANGE>
					auto raw_allocate_copy(const INPUT_RANGE & i_source_range)
						-> decltype(raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range)))
				{
					return raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range));
				}

				/** Same to heterogeneous_queue::put_transaction::commit. */
				void commit() noexcept
				{
					DENSITY_ASSERT(!empty());					
					commit_element_impl(m_put);
					m_queue = nullptr;
				}

				/** Same to heterogeneous_queue::put_transaction::cancel. */
				void cancel() noexcept
				{
					DENSITY_ASSERT(!empty());
					cancel_element_impl(m_put);
					m_queue = nullptr;
				}

				/** Same to heterogeneous_queue::put_transaction::empty. */
				bool empty() const noexcept { return m_queue == nullptr; }

				/** Same to heterogeneous_queue::put_transaction::element_ptr. */
				COMMON_TYPE * element_ptr() const noexcept
				{
					DENSITY_ASSERT(!empty());
					return m_element;
				}

				/** Same to heterogeneous_queue::put_transaction::type. */
				const RUNTIME_TYPE & type() const noexcept
				{
					return *static_cast<RUNTIME_TYPE*>(address_add(m_put.m_control, s_type_eraser_offset));
				}

				/** Same to heterogeneous_queue::put_transaction::~put_transaction. */
				~put_transaction()
				{
					if (m_queue != nullptr)
					{
						cancel_element_impl(m_put);
					}
				}

				/** /internal */
				put_transaction(nonblocking_heterogeneous_queue * i_queue, const Put & i_put, COMMON_TYPE *  i_element) noexcept
					: m_put(i_put), m_element(i_element), m_queue(i_queue) { }

			private:
				Put m_put;
				COMMON_TYPE * m_element;
				nonblocking_heterogeneous_queue * m_queue;
			};

			/** This class is used as return type from put functions with the element type known at compile time.
			
				typed_put_transaction derives from put_transaction adding just an element_ptr() that returns a
				pointer of correct the type. */
			template <typename TYPE>
				class typed_put_transaction : public put_transaction
			{
			public:

				using put_transaction::put_transaction;

				/** Returns a pointer to the object being added.
					\n <i>Note</i>: the object is constructed at the begin of the transaction, so this
						function always returns a pointer to a valid object.

					\pre The behavior is undefined if either:
						- this transaction is empty */
				TYPE * element_ptr() const noexcept
					{ return static_cast<TYPE *>(put_transaction::element_ptr()); }
			};

			/** Same to heterogeneous_queue::consume_operation */
			class consume_operation
			{
			public:

				consume_operation() noexcept = default;

				/** Copy construction is not allowed */
				consume_operation(const consume_operation &) = delete;

				/** Copy assignment is not allowed */
				consume_operation & operator = (const consume_operation &) = delete;

				/** Same to heterogeneous_queue::consume_operation::consume_operation(put_transaction &&). */
				consume_operation(consume_operation && i_source) noexcept
					: m_consume_data(std::move(i_source.m_consume_data))
				{
				}

				consume_operation & operator = (consume_operation && i_source) = delete;

				/** Destructor: cancel the operation (if any) */
				~consume_operation()
				{
					if (m_consume_data.m_next_ptr != 0)
					{
						m_consume_data.cancel_consume_impl();
					}
				}

				/** Same to heterogeneous_queue::consume_operation::empty. */
				bool empty() const noexcept { return m_consume_data.m_next_ptr == 0; }

				/** Same to heterogeneous_queue::consume_operation::operator bool. */
				explicit operator bool() const noexcept
				{
					return m_consume_data.m_next_ptr != 0;
				}

				/** Same to heterogeneous_queue::consume_operation::commit. */
				void commit() noexcept
				{
					DENSITY_ASSERT(!empty());

					auto const & type = complete_type();
					auto const element = element_ptr();
					type.destroy(element);

					m_consume_data.commit_consume_impl();
				}

				/** Same to heterogeneous_queue::consume_operation::commit_nodestroy. */
				void commit_nodestroy() noexcept
				{
					DENSITY_ASSERT(!empty());

					m_consume_data.commit_consume_impl();
				}

				/** Same to heterogeneous_queue::consume_operation::cancel. */
				void cancel() noexcept
				{
					DENSITY_ASSERT(!empty());

					m_consume_data.cancel_consume_impl();
				}

				/** Same to heterogeneous_queue::consume_operation::complete_type. */
				const RUNTIME_TYPE & complete_type() const noexcept
				{
					DENSITY_ASSERT(!empty());
					return *type_after_control(m_consume_data.m_control);
				}

				/** Same to heterogeneous_queue::consume_operation::unaligned_element_ptr. */
				void * unaligned_element_ptr() const noexcept
				{
					DENSITY_ASSERT(!empty());
					return get_unaligned_element(m_consume_data.m_control);
				}

				/** Same to heterogeneous_queue::consume_operation::element_ptr. */
				COMMON_TYPE * element_ptr() const noexcept
				{
					DENSITY_ASSERT(!empty());
					return get_element(m_consume_data.m_control);
				}

				/** Returns a reference to the element being consumed.

					\pre The behavior is undefined if this consume_operation is empty, that is it has been committed or used as source for a move operation.
					\pre The behavior is undefined if COMPLETE_ELEMENT_TYPE is not exactly the complete type of the element. */
				template <typename COMPLETE_ELEMENT_TYPE>
					COMPLETE_ELEMENT_TYPE & element() const noexcept
				{
					DENSITY_ASSERT(!empty() && complete_type() == runtime_type::make<COMPLETE_ELEMENT_TYPE>());
					return *static_cast<COMPLETE_ELEMENT_TYPE*>(get_element(m_consume_data.m_control));
				}

				/** /internal */
				consume_operation(nonblocking_heterogeneous_queue * i_queue) noexcept
				{
					m_consume_data.start_consume_impl(i_queue);
				}

				/** /internal */
				bool start_consume(nonblocking_heterogeneous_queue * i_queue) noexcept
				{
					if(m_consume_data.m_next_ptr != 0)
					{
						m_consume_data.cancel_consume_impl();
					}
					
					m_consume_data.start_consume_impl(i_queue);

					return m_consume_data.m_next_ptr != 0;
				}

			private:

				static runtime_type * type_after_control(ControlBlock * i_control) noexcept
				{
					return static_cast<runtime_type*>(address_add(i_control, s_type_eraser_offset));
				}

				static void * get_unaligned_element(detail::NbQueueControl<void> * i_control)
				{
					auto result = address_add(i_control, s_element_offset);
					if (i_control->m_next & detail::NbQueue_External)
					{
						result = static_cast<ExternalBlock*>(result)->m_block;
					}
					return result;
				}

				static void * get_element(detail::NbQueueControl<void> * i_control)
				{
					auto result = address_add(i_control, s_element_offset);
					if (i_control->m_next & detail::NbQueue_External)
					{
						result = static_cast<ExternalBlock*>(result)->m_block;
					}
					else
					{
						result = address_upper_align(result, type_after_control(i_control)->alignment());
					}
					return result;
				}

				template <typename TYPE>
					static void * get_unaligned_element(detail::NbQueueControl<TYPE> * i_control)
				{
					return i_control->m_element;
				}

				template <typename TYPE>
					static TYPE * get_element(detail::NbQueueControl<TYPE> * i_control)
				{
					return i_control->m_element;
				}

			private:
				Consume m_consume_data;
			};


			/** Same to heterogeneous_queue::start_consume. */
			consume_operation start_consume() noexcept
			{
				return consume_operation(this);
			}

			bool start_consume(consume_operation & io_consume) noexcept
			{
				return io_consume.start_consume(this);
			}
		};
	}

} // namespace density

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
