
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
	enum concurrent_cardinality
	{
		concurrent_cardinality_single,
		concurrent_cardinality_multiple,
		concurrent_cardinality_multiple_high_contention
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

		template<typename COMMON_TYPE> struct NbQueueEndControl // used by nonblocking_heterogeneous_queue<T,...>
			: NbQueueControl<COMMON_TYPE>
		{
			void * m_prev_page;
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

		template < typename COMMON_TYPE, typename TYPE_ERASER_TYPE, typename ALLOCATOR_TYPE, concurrent_cardinality >
			class NonblockingQueueTail : protected ALLOCATOR_TYPE
		{
		protected:

			using ControlBlock = typename detail::NbQueueControl<COMMON_TYPE>;
			using EndControlBlock = typename detail::NbQueueEndControl<COMMON_TYPE>;

			struct ExternalBlock
			{
				COMMON_TYPE * m_element = nullptr;
				void * m_block = nullptr;
				size_t m_size = 0, m_alignment = 0;
			};
			
			/** Minimum alignment used for the storage of the elements. The storage of elements is always aligned according to the most-derived type. */
			constexpr static size_t min_alignment = detail::size_max(
				detail::size_log2(detail::NbQueue_AllFlags + 1),
				detail::size_max(alignof(EndControlBlock), alignof(TYPE_ERASER_TYPE)),
				sizeof(EndControlBlock), concurrent_alignment);

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
				pin_page(first_page);
				std::atomic_init(&m_tail, first_page);
			}

			static uintptr_t get_element_required_units(size_t i_size, size_t i_alignment) noexcept
			{
				size_t result = element_offset + i_size;

				// add the padding required to align the element after the runtime_type
				result += (i_alignment > alignof(TYPE_ERASER_TYPE) ? (i_alignment - alignof(TYPE_ERASER_TYPE)) : 0);

				return (result + (min_alignment - 1)) / min_alignment;
			}

			static uintptr_t get_raw_allocation_required_units(size_t i_size, size_t i_alignment) noexcept
			{
				size_t result = type_eraser_offset + i_size;

				// add the padding required to align the block
				result += (i_alignment > alignof(TYPE_ERASER_TYPE) ? (i_alignment - alignof(TYPE_ERASER_TYPE)) : 0);

				return (result + (min_alignment - 1)) / min_alignment;
			}

			static EndControlBlock * get_end_control_block(void * i_page)
			{
				DENSITY_ASSERT_INTERNAL(address_is_aligned(i_page, ALLOCATOR_TYPE::page_alignment));
				return static_cast<EndControlBlock *>(address_add(i_page, (page_unit_capacity - 1) * min_alignment));
			}

			ControlBlock * get_or_allocate_next_page(void * * i_pnt_tail)
			{
				// *i_pnt_tail must point to an end control block
				auto const end_block = static_cast<ControlBlock*>(*i_pnt_tail);
				DENSITY_ASSERT_INTERNAL( get_end_control_block(address_lower_align(end_block, ALLOCATOR_TYPE::page_alignment)) == end_block );
				
				ALLOCATOR_TYPE::pin_page(end_block);
				auto next_page = reinterpret_cast<uintptr_t>(nullptr);
				auto curr_tail = m_tail.load(std::memory_order_relaxed);
				if (curr_tail != end_block)
				{
					*i_pnt_tail = curr_tail;
				}
				else
				{
					next_page = raw_atomic_load(end_block->m_next);
					if (next_page == 0)
					{
						*i_pnt_tail = curr_tail;
					}
					else if(next_page == detail::NbQueue_InvalidNextPage)
					{
						/* Try to set the new page to the last block */
						auto const new_page = ALLOCATOR_TYPE::allocate_page_zeroed();

						DENSITY_ASSERT(new_page != nullptr && address_is_aligned(new_page, ALLOCATOR_TYPE::page_alignment));

						auto const new_page_end_block = get_end_control_block(new_page);

						new_page_end_block->m_next = detail::NbQueue_InvalidNextPage;
						new_page_end_block->m_prev_page = end_block;
						ALLOCATOR_TYPE::pin_page(new_page);

						if (!raw_atomic_compare_exchange_strong(end_block->m_next, next_page, 
							reinterpret_cast<uintptr_t>(new_page) + detail::NbQueue_Dead))
						{
							// to do: destroy ControlBlock
							new_page_end_block->m_next = 0;
							new_page_end_block->m_prev_page = nullptr;
							ALLOCATOR_TYPE::unpin_page(new_page);
							ALLOCATOR_TYPE::deallocate_page_zeroed(new_page);
						}
						else
						{
							next_page = reinterpret_cast<uintptr_t>(new_page);
						}
					}
				}

				unpin_page(end_block, false);
				auto const result = reinterpret_cast<ControlBlock*>(next_page & ~detail::NbQueue_AllFlags);
				DENSITY_ASSERT_INTERNAL((uintptr_t)result == 0 || (uintptr_t)result > 200);
				return result;
			}

			ControlBlock * inplace_allocate_impl(uintptr_t const i_units, intptr_t const i_flags)
			{
				DENSITY_ASSERT_INTERNAL(i_units <= page_unit_capacity && i_flags <= detail::NbQueue_AllFlags &&
					(i_flags & detail::NbQueue_Busy) != 0);

				ControlBlock * block = nullptr;
				bool padding = false;
				bool couldnt_get_next_page = false;
				do {
					auto tail = m_tail.load();
					void * new_tail;
					do {

						padding = false;
						couldnt_get_next_page = false;
						
						DENSITY_ASSERT_INTERNAL(address_is_aligned(tail, min_alignment));
						
						auto const curr_page = address_lower_align(tail, ALLOCATOR_TYPE::page_alignment);
						auto const page_end = get_end_control_block(curr_page);
						
						new_tail = address_add(tail, i_units * min_alignment);
						if (new_tail >= page_end)
						{
							if (tail != page_end)
							{
								// there is space between tail and page_end. We pad it with a dead control block
								new_tail = page_end;
								block = static_cast<ControlBlock*>(tail);
								padding = true;
							}
							else
							{
								// tail equals to page_end
								auto new_page = get_or_allocate_next_page(&tail);
								if (new_page == nullptr)
								{
									couldnt_get_next_page = true;
									DENSITY_ASSERT_INTERNAL(tail != nullptr);
									continue;
								}
								block = new_page;
								new_tail = address_add(new_page, i_units * min_alignment);
							}
						}
						else
						{
							block = static_cast<ControlBlock*>(tail);
						}						

					} while (couldnt_get_next_page || !m_tail.compare_exchange_weak(tail, new_tail));

					/* Assign m_next, and set the flags. This is very important for the consumers,
						because they that need this write happens before any other part of the
						allocated memory is modified. */
					auto const flags = padding ? detail::NbQueue_Dead : i_flags;
					raw_atomic_store(block->m_next, reinterpret_cast<uintptr_t>(new_tail) + flags);

				} while (padding);

				DENSITY_ASSERT_INTERNAL(block < get_end_control_block(address_lower_align(block, ALLOCATOR_TYPE::page_alignment)));

				return block;
			}

			static void commit_element_impl(ControlBlock * i_control) noexcept
			{
				// we expect to have NbQueue_Busy and not NbQueue_Dead
				DENSITY_ASSERT_INTERNAL(address_is_aligned(i_control, min_alignment));
				DENSITY_ASSERT_INTERNAL((raw_atomic_load(i_control->m_next, std::memory_order_relaxed) &
					(detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

				// remove the flag NbQueue_Busy
				raw_atomic_sub(i_control->m_next, detail::NbQueue_Busy, std::memory_order_seq_cst);
			}

			static void cancel_element_impl(ControlBlock * i_control) noexcept
			{
				// we expect to have NbQueue_Busy and not NbQueue_Dead
				DENSITY_ASSERT_INTERNAL(address_is_aligned(i_control, min_alignment));
				DENSITY_ASSERT_INTERNAL((raw_atomic_load(i_control->m_next, std::memory_order_relaxed) &
					(detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

				// remove NbQueue_Busy and add NbQueue_Dead
				auto const addend = static_cast<uintptr_t>(detail::NbQueue_Dead) - static_cast<uintptr_t>(detail::NbQueue_Busy);
				raw_atomic_add(i_control->m_next, addend, std::memory_order_seq_cst);
			}

			void * raw_allocate_impl(size_t i_size, size_t i_alignment)
			{
				if (i_size <= max_internal_size)
				{
					// the allocation fits in a page
					ControlBlock * const control = inplace_allocate_impl(get_raw_allocation_required_units(
						i_size, i_alignment, detail::NbQueue_Dead));
					return address_upper_align(address_add(control, type_eraser_offset), i_alignment);
				}
				else
				{
					// the allocation does not fit in a page, allocate inplace space for an ExternalBlock
					ControlBlock * const control = inplace_allocate_impl(get_raw_allocation_required_units(
						sizeof(ExternalBlock), alignof(ExternalBlock), detail::NbQueue_Dead | detail::NbQueue_External));
					void * block_alloc = address_add(control, type_eraser_offset);
					if (alignof(ExternalBlock) > alignof(TYPE_ERASER_TYPE))
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
				return static_cast<ControlBlock *>(m_tail.load());
			}

		protected:
			constexpr static uintptr_t type_eraser_offset = uint_upper_align(sizeof(ControlBlock), alignof(TYPE_ERASER_TYPE));
			constexpr static uintptr_t element_offset = type_eraser_offset + sizeof(TYPE_ERASER_TYPE);
			constexpr static uintptr_t page_unit_capacity = (ALLOCATOR_TYPE::page_size / min_alignment);
			constexpr static size_t max_internal_size = (page_unit_capacity / 2) * min_alignment;

		private: // data members
			alignas(concurrent_alignment) std::atomic<void *> m_tail;
		};

		template < typename COMMON_TYPE, typename TYPE_ERASER_TYPE, typename ALLOCATOR_TYPE, 
				concurrent_cardinality PROD_CARDINALITY, concurrent_cardinality CONSUMER_CARDINALITY >
			class NonblockingQueueHead : protected NonblockingQueueTail<COMMON_TYPE, TYPE_ERASER_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY>
		{
		private:

			using Base = NonblockingQueueTail<COMMON_TYPE, TYPE_ERASER_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY>;

		protected:

			NonblockingQueueHead()
			{
				std::atomic_init(&m_head, get_tail_for_consumers());
			}

			/** /internal
				Struct describing a consume operation in progress. If m_control is nullptr, the state is empty,
				and m_head is undefined. */
			struct ConsumeData
			{
				ControlBlock * m_control;
			};

			/** \internal
				Tries to start a consume operation. The returned ConsumeData is empty if the operation fails.
				Otherwise it contains a pointer to the head and a pointer to the element being consumed.
				All the pages in the inclusive range { m_head, m_control } get pinned. */
			ConsumeData start_consume_impl()
			{
				// pin the head
				auto control = pin_head();
				bool switched_page = false;
				for (;;)
				{
					auto const next_uint = raw_atomic_load(control->m_next, std::memory_order_seq_cst);
					if ( (next_uint & ~detail::NbQueue_InvalidNextPage) != 0)
					{
						if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == 0)
						{
							auto expected = next_uint;
							if (raw_atomic_compare_exchange_strong(control->m_next, expected, next_uint | detail::NbQueue_Busy,
								std::memory_order_seq_cst, std::memory_order_relaxed))
							{
								return ConsumeData{ control };
							}
						}

						auto const next = reinterpret_cast<ControlBlock*>(next_uint & ~detail::NbQueue_AllFlags);
						DENSITY_ASSERT_INTERNAL(next != nullptr);

						if (!same_page(control, next))
						{
							pin_page(next);
							unpin_page(control, !switched_page);
							switched_page = true;
						}
						control = next;
					}
					else
					{
						/////////////////////////////////
						// avoid scans
						//unpin_page(control, !switched_page);
						//return ConsumeData{ nullptr };
						////////////////////////////

						// to do: handle detail::NbQueue_InvalidNextPage
						
						/* We can't procrastinate the check on the tail anymore */
						auto const tail = get_tail_for_consumers();

						auto const first_nonzero = control == tail ? nullptr : reverse_scan_for_nonzeroed(control, tail);
						if (first_nonzero == nullptr)
						{
							// no element to consume
							return ConsumeData{ nullptr };
						}


						auto new_next = raw_atomic_load(control->m_next) & ~detail::NbQueue_AllFlags;
						if (new_next == 0)
						{
							control = first_nonzero;
						}
						else
						{
							control = reinterpret_cast<ControlBlock*>(new_next);
						}
						DENSITY_ASSERT_INTERNAL(ALLOCATOR_TYPE::get_pin_count(control) > 0);
					}
				}
			}

			/** /internal
				This function expects that the page containing i_start is pinned.
				After exiting, only the page containing the return value (if non-null) is pinned
			*/
			ControlBlock * reverse_scan_for_nonzeroed(ControlBlock * const i_first, ControlBlock * const i_tail) noexcept
			{
				DENSITY_ASSERT_INTERNAL(i_first != nullptr && i_tail != nullptr);
				DENSITY_ASSERT_INTERNAL(ALLOCATOR_TYPE::get_pin_count(i_first) > 0);

				ControlBlock * first_in_page = i_first;
				ControlBlock * end_control = get_end_control_block(address_lower_align(first_in_page, ALLOCATOR_TYPE::page_alignment));
				ControlBlock * curr = end_control;
				ControlBlock * last_nonzero = nullptr;
				for (;;)
				{
					if (same_page(first_in_page, i_tail))
					{
						first_in_page = i_tail;
					}

					while (curr != first_in_page)
					{
						curr = static_cast<ControlBlock *>(address_sub(curr, min_alignment));
						if (curr == i_first)
							break;

						auto uint_next = raw_atomic_load(curr->m_next, std::memory_order_seq_cst) & ~detail::NbQueue_Busy;
						if (uint_next != 0)
						{
							last_nonzero = curr;
						}
					}

					auto const uint_next = raw_atomic_load(end_control->m_next, std::memory_order_seq_cst);
					first_in_page = reinterpret_cast<ControlBlock *>(uint_next & ~detail::NbQueue_AllFlags);
					if (last_nonzero != nullptr || first_in_page == nullptr)
					{
						break;
					}

					end_control = get_end_control_block(first_in_page);
					unpin_page(curr, true);
					pin_page(end_control);

					curr = end_control;
				}

				return last_nonzero;
			}

			void commit_consume_impl(ConsumeData i_consume_data) noexcept
			{
				DENSITY_ASSERT_INTERNAL(ALLOCATOR_TYPE::get_pin_count(i_consume_data.m_control) > 0);

				// we expect to have NbQueue_Busy and not NbQueue_Dead...
				DENSITY_ASSERT_INTERNAL((raw_atomic_load(i_consume_data.m_control->m_next, std::memory_order_relaxed) &
					(detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

				// remove NbQueue_Busy and add NbQueue_Dead
				raw_atomic_add(i_consume_data.m_control->m_next, detail::NbQueue_Dead - detail::NbQueue_Busy, std::memory_order_seq_cst);

				auto head = m_head.load();
				void * additional_pinned_page = nullptr; 
				if (!same_page(head, i_consume_data.m_control))
				{
					pin_page(head);
					additional_pinned_page = i_consume_data.m_control;
				}
				for (;;)
				{
					auto control = static_cast<ControlBlock*>(address_lower_align(head, min_alignment));
					auto inplace_pins = address_diff(head, control);
					
					auto const next_uint = raw_atomic_load(control->m_next);
					auto const next = reinterpret_cast<void*>(control->m_next & ~detail::NbQueue_AllFlags);
					if ((next_uint & (detail::NbQueue_Busy | detail::NbQueue_Dead)) != detail::NbQueue_Dead)
					{
						// the element is not dead
						break;
					}

					DENSITY_ASSERT_INTERNAL(ALLOCATOR_TYPE::get_pin_count(head) > 0);
					if (same_page(control, next))
					{
						auto new_head = address_add(next, inplace_pins);
						auto const old_head = head;
						if (!m_head.compare_exchange_strong(head, new_head,
							std::memory_order_seq_cst, std::memory_order_relaxed))
						{
							// maybe another thread is advancing the head, give up
							head = old_head;
							break;
						}

						auto const address_of_next = const_cast<uintptr_t*>(&control->m_next);
						std::memset(control, 0, address_diff(address_of_next, control));
						raw_atomic_store(control->m_next, 0);
						std::memset(address_of_next, 0, address_diff(next, address_of_next + 1));

						head = new_head;
					}
					else
					{

						ALLOCATOR_TYPE::pin_page(head, inplace_pins);

						auto new_head = next;
						auto const old_head = head;
						if (!m_head.compare_exchange_strong(head, new_head,
							std::memory_order_seq_cst, std::memory_order_relaxed))
						{
							// another thread is advancing the head, give up
							head = old_head;
							auto const prev_pins = ALLOCATOR_TYPE::unpin_page(head, inplace_pins);
							DENSITY_ASSERT_INTERNAL(prev_pins - inplace_pins > 0);
							break;
						}

						if (same_page(new_head, additional_pinned_page))
						{
							additional_pinned_page = nullptr;
						}
						else
						{
							ALLOCATOR_TYPE::pin_page(new_head);
						}
						

						DENSITY_ASSERT_INTERNAL(address_is_aligned(next, ALLOCATOR_TYPE::page_alignment));
						DENSITY_ASSERT_INTERNAL(get_end_control_block(address_lower_align(control, ALLOCATOR_TYPE::page_alignment)) == control);

						unpin_page(head, false);
						unpin_page(head, false);
						head = new_head;
					}
				}
				unpin_page(head, true);
				if (additional_pinned_page != nullptr)
				{
					unpin_page(additional_pinned_page, true);
				}
			}

			void cancel_consume_impl(ConsumeData i_consume_data) noexcept
			{
				DENSITY_ASSERT_INTERNAL(ALLOCATOR_TYPE::get_pin_count(i_consume_data.m_control) > 0);

				auto const control = i_consume_data.m_control;

				// we expect to have NbQueue_Busy and not NbQueue_Dead...
				DENSITY_ASSERT_INTERNAL((raw_atomic_load(control->m_next, std::memory_order_relaxed) &
					(detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

				raw_atomic_sub(control->m_next, detail::NbQueue_Busy, std::memory_order_seq_cst);

				unpin_page(i_consume_data.m_control, true);
			}

			ControlBlock * pin_head() noexcept
			{
				/*auto head = m_head.load();
				void * new_head;
				bool overflow = false;
				do {
					new_head = address_add(head, 1);
					if (address_is_aligned(new_head, min_alignment))
					{
						overflow = true;
					}
				} while (!overflow && !m_head.compare_exchange_weak(head, new_head));

				if (!overflow)
				{
					return address_lower_align(new_head, min_alignment);
				}
				else*/
				
				{
					/////////////////////////
					auto head = m_head.load();
					/////////////////////////

					for (;;)
					{
						auto const pinned_page = head;
						ALLOCATOR_TYPE::pin_page(pinned_page);
						head = m_head.load();
						if (same_page(pinned_page, head))
						{
							return static_cast<ControlBlock*>(address_lower_align(head, min_alignment));
						}
						ALLOCATOR_TYPE::unpin_page(pinned_page);
					}
				}
			}

			void pin_page(void * i_page) noexcept
			{
				ALLOCATOR_TYPE::pin_page(i_page);
			}

			void unpin_page(void * i_page, bool i_check_head) noexcept
			{
				bool done = false;
				/*if (i_check_head)
				{
					auto head = m_head.load();
					do {
						if (!same_page(head, i_page))
						{
							break;
						}

						DENSITY_ASSERT_INTERNAL(!address_is_aligned(head, min_alignment));

						auto new_head = address_sub(head, 1);
						
						done = m_head.compare_exchange_weak(head, new_head);

					} while (!done);
				}*/

				if (!done)
				{
					auto const prev_pins = ALLOCATOR_TYPE::unpin_page(i_page);
					if (prev_pins == 1)
					{
						auto const page_start = address_lower_align(i_page, ALLOCATOR_TYPE::page_alignment);
						auto const end_of_page = address_add(page_start, ALLOCATOR_TYPE::page_size);
						auto const end_block = get_end_control_block(page_start);
						auto const address_of_next = const_cast<uintptr_t*>(&end_block->m_next);

						std::memset(end_block, 0, address_diff(address_of_next, end_block));
						raw_atomic_store(end_block->m_next, 0);
						std::memset(address_of_next, 0, address_diff(end_of_page, address_of_next + 1));

						ALLOCATOR_TYPE::deallocate_page_zeroed(i_page);
					}
				}
			}

		private: // data members
			alignas(concurrent_alignment)std::atomic<void *> m_head;
		};
	}

	namespace experimental
	{
		/** \brief Concurrent heterogeneous FIFO container-like class template. 
	
		*/
		template < typename COMMON_TYPE = void, typename TYPE_ERASER_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator,
				concurrent_cardinality PROD_CARDINALITY = concurrent_cardinality_multiple,
				concurrent_cardinality CONSUMER_CARDINALITY = concurrent_cardinality_multiple >
			class nonblocking_heterogeneous_queue : private detail::NonblockingQueueHead<COMMON_TYPE, TYPE_ERASER_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSUMER_CARDINALITY>
		{
		private:
			using Base = detail::NonblockingQueueHead<COMMON_TYPE, TYPE_ERASER_TYPE, ALLOCATOR_TYPE, PROD_CARDINALITY, CONSUMER_CARDINALITY>;
		
		public:

			using common_type = COMMON_TYPE;
			using runtime_type = TYPE_ERASER_TYPE;
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
				i_out_stream << ", sizeof(runtime_type): " << sizeof(runtime_type);
				
				auto const head = pin_head();
				DENSITY_ASSERT_INTERNAL(address_is_aligned(head, min_alignment));
				auto control = static_cast<ControlBlock*>(address_lower_align(head, ALLOCATOR_TYPE::page_alignment));
				for (;;)
				{
					auto const tail = get_tail_for_consumers();
					DENSITY_ASSERT_INTERNAL(address_is_aligned(tail, min_alignment));

					DENSITY_ASSERT_INTERNAL(address_is_aligned(control, min_alignment));
					auto next_uint = raw_atomic_load(control->m_next, std::memory_order_seq_cst);
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
							next_uint = raw_atomic_load(control->m_next, std::memory_order_seq_cst);
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
							pin_page(next);
							unpin_page(control, true);
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

				unpin_page(control, true);

				i_out_stream << "\npages: " << stats.pages;
				i_out_stream << "\ncontrols: " << stats.control_count;
				i_out_stream << "\nelements: " << stats.element_count;
				i_out_stream << "\nbusy elements: " << stats.busy_count;
				i_out_stream << "\nexternal elements: " << stats.external_count;
				i_out_stream << "\n\n";
			}

			/** Minimum alignment used for the storage of the elements. The storage of elements is always aligned according to the most-derived type. */
			constexpr static size_t min_alignment = Base::min_alignment;

			static_assert(std::is_same<COMMON_TYPE, typename TYPE_ERASER_TYPE::common_type>::value,
				"COMMON_TYPE and TYPE_ERASER_TYPE::common_type must be the same type (did you try to use a type like heter_cont<A,runtime_type<B>>?)");

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
				constexpr bool inplace_element = sizeof(ELEMENT_TYPE) <= max_internal_size;
				using inplace_type = typename std::conditional<inplace_element, ELEMENT_TYPE, ExternalBlock>::type;
				ControlBlock * const control = inplace_allocate_impl(
					get_element_required_units(sizeof(inplace_type), alignof(inplace_type)),
					detail::NbQueue_Busy | (inplace_element ? 0 : detail::NbQueue_External) );

				// compute the address of the element
				void * element_alloc = address_add(control, element_offset);
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
						auto type_eraser = address_add(control, type_eraser_offset);
						DENSITY_ASSERT_INTERNAL(type_eraser != nullptr);
						new(type_eraser) runtime_type(runtime_type::template make<ELEMENT_TYPE>());
					}
					catch (...)
					{
						//element->ELEMENT_TYPE::~ELEMENT_TYPE();
						detail::destroy_obj(element); /* workaround for internal compiler error with Vs2017 RC */
						throw;
					}
					return typed_put_transaction<ELEMENT_TYPE>(this, control, element);
				}
				catch (...)
				{
					cancel_element_impl(control);
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
					: m_control(i_source.m_control), m_element(i_source.m_element), m_queue(i_source.m_queue)
				{
					i_source.m_queue = nullptr;
				}

				/** Same to heterogeneous_queue::put_transaction::operator = (put_transaction &&). */
				put_transaction & operator = (put_transaction && i_source)
				{
					if (this != &i_source)
					{
						m_queue = i_source.m_queue;
						m_control = i_source.m_control;
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
					commit_element_impl(m_control);
					m_queue = nullptr;
				}

				/** Same to heterogeneous_queue::put_transaction::cancel. */
				void cancel() noexcept
				{
					DENSITY_ASSERT(!empty());
					cancel_element_impl(m_control);
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
				const TYPE_ERASER_TYPE & type() const noexcept
				{
					return *static_cast<TYPE_ERASER_TYPE*>(address_add(m_control, type_eraser_offset));
				}

				/** Same to heterogeneous_queue::put_transaction::~put_transaction. */
				~put_transaction()
				{
					if (m_queue != nullptr)
					{
						cancel_element_impl(m_control);
					}
				}

			/** /internal */
			put_transaction(nonblocking_heterogeneous_queue * i_queue, ControlBlock * i_control, COMMON_TYPE *  i_element) noexcept
				: m_control(i_control), m_element(i_element), m_queue(i_queue) { }

			private:
				ControlBlock * m_control;
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

			/** Same to heterogeneous_queue::/** Same to heterogeneous_queue::start_consume. */
			class consume_operation
			{
			public:

				/** Copy construction is not allowed */
				consume_operation(const consume_operation &) = delete;

				/** Copy assignment is not allowed */
				consume_operation & operator = (const consume_operation &) = delete;

				/** Same to heterogeneous_queue::consume_operation::consume_operation(put_transaction &&). */
				consume_operation(consume_operation && i_source) noexcept
					: m_consume_data(i_source.m_consume_data), m_queue(i_source.m_queue)
				{
					i_source.m_consume_data.m_control = nullptr;
				}

				/** Same to heterogeneous_queue::consume_operation::operator = (consume_operation &&). */
				consume_operation & operator = (consume_operation && i_source) noexcept
				{
					if (this != &i_source)
					{
						m_queue = i_source.m_queue;
						m_consume_data = i_source.m_consume_data;
						i_source.m_consume_data.m_control = nullptr;
					}
					return *this;
				}

				/** Destructor: cancel the operation (if any) */
				~consume_operation()
				{
					if (m_consume_data.m_control != nullptr)
					{
						m_queue->cancel_consume_impl(m_consume_data);
					}
				}

				/** Same to heterogeneous_queue::consume_operation::empty. */
				bool empty() const noexcept { return m_consume_data.m_control == nullptr; }

				/** Same to heterogeneous_queue::consume_operation::operator bool. */
				explicit operator bool() const noexcept
				{
					return m_consume_data.m_control != nullptr;
				}

				/** Same to heterogeneous_queue::consume_operation::commit. */
				void commit() noexcept
				{
					DENSITY_ASSERT(!empty());

					auto const & type = complete_type();
					auto const element = element_ptr();
					type.destroy(element);

					m_queue->commit_consume_impl(m_consume_data);
					m_consume_data.m_control = nullptr;
				}

				/** Same to heterogeneous_queue::consume_operation::commit_nodestroy. */
				void commit_nodestroy() noexcept
				{
					DENSITY_ASSERT(!empty());

					m_queue->commit_consume_impl(m_consume_data.m_control);
					m_consume_data.m_control = nullptr;
				}

				/** Same to heterogeneous_queue::consume_operation::cancel. */
				void cancel() noexcept
				{
					DENSITY_ASSERT(!empty());

					m_queue->cancel_consume_impl(m_consume_data);
					m_consume_data.m_control = nullptr;
				}

				/** Same to heterogeneous_queue::consume_operation::complete_type. */
				const TYPE_ERASER_TYPE & complete_type() const noexcept
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
				consume_operation(nonblocking_heterogeneous_queue * i_queue, ConsumeData i_consume_data) noexcept
					: m_queue(i_queue), m_consume_data(i_consume_data)
				{
				}

			private:

				static runtime_type * type_after_control(ControlBlock * i_control) noexcept
				{
					return static_cast<runtime_type*>(address_add(i_control, type_eraser_offset));
				}

				static void * get_unaligned_element(detail::NbQueueControl<void> * i_control)
				{
					auto result = address_add(i_control, element_offset);
					if (i_control->m_next & detail::NbQueue_External)
					{
						result = static_cast<ExternalBlock*>(result)->m_block;
					}
					return result;
				}

				static void * get_element(detail::NbQueueControl<void> * i_control)
				{
					auto result = address_add(i_control, element_offset);
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
				ConsumeData m_consume_data;
				nonblocking_heterogeneous_queue * m_queue;
			};

			/** Same to heterogeneous_queue::start_consume. */
			consume_operation start_consume() noexcept
			{
				return consume_operation(this, start_consume_impl());
			}
		};
	}

} // namespace density

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
