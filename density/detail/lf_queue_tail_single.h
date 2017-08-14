
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


namespace density
{
	namespace detail
	{
		/** \internal Class template that implements put operations */
		template < typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE, consistency_model CONSISTENCY_MODEL>
			class LFQueue_Tail<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE, concurrent_cardinality_single, CONSISTENCY_MODEL>
				: protected ALLOCATOR_TYPE
		{
		public:

			using ControlBlock = typename detail::NbQueueControl<COMMON_TYPE>;

			/** Minimum alignment used for the storage of the elements.
				The storage of elements is always aligned according to the most-derived type. */
			constexpr static size_t min_alignment = alignof(void*); /* there are no particular requirements on 
				the choice of this value: it just should be a very common alignment. */

			/** Head and tail pointers are alway multiple of this constant. To avoid the need of 
				upper-aligning the addresses of the control-block and the runtime type, we raise it to the
				maximum alignment between ControlBlock and RUNTIME_TYPE (which are unlikely to be overaligned). 
				The ControlBlock is always at offset 0 in the layout of a value or raw block. */
			constexpr static uintptr_t s_alloc_granularity = size_max( size_max( concurrent_alignment,
				alignof(ControlBlock), alignof(RUNTIME_TYPE), alignof(ExternalBlock) ), 
				min_alignment, detail::size_log2(detail::NbQueue_AllFlags + 1) );

			/** Offset of the runtime_type in the layout of a value */
			constexpr static uintptr_t s_type_offset = uint_upper_align(sizeof(ControlBlock), alignof(RUNTIME_TYPE));

			/** Minimum offset of the element in the layout of a value (The actual offset is dependent on
				the alignment of the element). */
			constexpr static uintptr_t s_element_min_offset = uint_upper_align(s_type_offset + sizeof(RUNTIME_TYPE), min_alignment);

			/** Minimum offset of a row block. (The actual offset is dependent on the alignment of the block). */
			constexpr static uintptr_t s_rawblock_min_offset = uint_upper_align(sizeof(ControlBlock), size_max(min_alignment, alignof(ExternalBlock)));
			
			/** Offset from the beginning of the page of the end-control-block. */
			constexpr static uintptr_t s_end_control_offset = uint_lower_align(ALLOCATOR_TYPE::page_size - sizeof(ControlBlock), s_alloc_granularity);

			/** Maximum size for an element or raw block to be allocated in a page. */
			constexpr static size_t s_max_size_inpage = s_end_control_offset - s_element_min_offset;

			/** Value used to initialize the head and the tail. 
				This value is designed to always cause a page overflow in the fast path.
				This mechanism allows the default constructor to be small, fast, and noexcept. */
			constexpr static uintptr_t s_invalid_control_block = s_end_control_offset;

			/** Whether the head should zero the content of pages before deallocating. */
			constexpr static bool s_deallocate_zeroed_pages = false;

			/** Type-safe (at least for the caller) version of s_invalid_control_block */
			static ControlBlock * invalid_control_block() noexcept
			{
				return reinterpret_cast<ControlBlock*>(s_invalid_control_block);
			}

			// some static checks
			static_assert(ALLOCATOR_TYPE::page_size > sizeof(ControlBlock) && 
				s_end_control_offset > 0 && s_end_control_offset > s_element_min_offset, "pages are too small");
			static_assert(is_power_of_2(s_alloc_granularity), "isn't concurrent_alignment a power of 2?");

			/** Returns whether the input addresses belong to the same page or they are both nullptr */
			static bool same_page(const void * i_first, const void * i_second) noexcept
			{
				auto const page_mask = ALLOCATOR_TYPE::page_alignment - 1;
				return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
			}

			LFQueue_Tail() noexcept
				: m_tail(invalid_control_block()),
				  m_initial_page(nullptr)
			{
			}

			LFQueue_Tail(ALLOCATOR_TYPE && i_allocator) noexcept
				: ALLOCATOR_TYPE(std::move(i_allocator)),
				  m_tail(invalid_control_block()),
				  m_initial_page(nullptr)
			{
			}

			LFQueue_Tail(const ALLOCATOR_TYPE & i_allocator)
				: ALLOCATOR_TYPE(i_allocator),
				  m_tail(invalid_control_block()),
				  m_initial_page(nullptr)
			{
			}

			LFQueue_Tail(LFQueue_Tail && i_source) noexcept
				: LFQueue_Tail()
			{
				swap(i_source);
			}

			LFQueue_Tail & operator = (LFQueue_Tail && i_source) noexcept
			{
				LFQueue_Tail::swap(i_source);
				return *this;
			}

			void swap(LFQueue_Tail & i_other) noexcept
			{
				// swap the allocator
				using std::swap;
				swap(static_cast<ALLOCATOR_TYPE&>(*this), static_cast<ALLOCATOR_TYPE&>(i_other));

				// swap m_tail
				swap(m_tail, i_other.m_tail);

				// swap m_initial_page
				auto const tmp1 = i_other.m_initial_page.load();
				i_other.m_initial_page.store(m_initial_page.load());
				m_initial_page.store(tmp1);
			}

			~LFQueue_Tail()
			{
				if (m_tail != invalid_control_block())
				{
					ALLOCATOR_TYPE::deallocate_page(m_tail);
				}
			}

			/** Given an address, returns the end block of the page containing it. */
			static ControlBlock * get_end_control_block(void * i_address) noexcept
			{
				auto const page = address_lower_align(i_address, ALLOCATOR_TYPE::page_alignment);
				return static_cast<ControlBlock *>(address_add(page, s_end_control_offset));
			}

			/** This struct contains the result of an allocation. */
			struct Block
			{
				ControlBlock * m_control_block;
				uintptr_t m_next_ptr = 0;
				void * m_user_storage;
			};

			/** Allocates a block of memory.
				The block may be allocated in the pages or in a legacy memory block, depending on the size and the alignment.
				@param i_control_bits flags to add to the control block. Only NbQueue_Busy, NbQueue_Dead and NbQueue_External are supported
				@param i_include_type true if this is an element value, false if it's a raw allocation
				@param i_size it must be > 0 and a multiple of the alignment
				@param i_alignment is must be > 0 and a power of two */
			Block inplace_allocate(uintptr_t i_control_bits, bool i_include_type, size_t i_size, size_t i_alignment)
			{
				DENSITY_ASSERT_INTERNAL((i_control_bits & ~(detail::NbQueue_Busy | detail::NbQueue_Dead | detail::NbQueue_External)) == 0);
				DENSITY_ASSERT_INTERNAL(is_power_of_2(i_alignment) && (i_size % i_alignment) == 0);
				
				if (i_alignment < min_alignment)
				{
					i_alignment = min_alignment;
					i_size = uint_upper_align(i_size, min_alignment);
				}

				auto tail = m_tail;
				for (;;)
				{
					DENSITY_TEST_ARTIFICIAL_DELAY;
					DENSITY_ASSERT_INTERNAL(tail != nullptr && address_is_aligned(tail, s_alloc_granularity));

					// allocate space for the control block (and possibly the runtime type)
					void * address = address_add(tail, i_include_type ? s_element_min_offset : s_rawblock_min_offset);

					// allocate space for the element
					address = address_upper_align(address, i_alignment);
					void * const user_storage = address;
					address = address_add(address, i_size);
					address = address_upper_align(address, s_alloc_granularity);
					auto const new_tail = static_cast<ControlBlock*>(address);

					// check for page overflow
					auto const new_tail_offset = address_diff(new_tail, address_lower_align(tail, ALLOCATOR_TYPE::page_alignment));
					if (DENSITY_LIKELY(new_tail_offset <= s_end_control_offset))
					{
						/* note: while control_block->m_next is zero, no consumers may ever read this
							variable. Maybe this does not need to be atomic. */
						raw_atomic_store(&new_tail->m_next, 0, detail::mem_relaxed);

						auto const control_block = tail;
						auto const next_ptr = reinterpret_cast<uintptr_t>(new_tail) + i_control_bits;
						DENSITY_ASSERT_INTERNAL(raw_atomic_load(&control_block->m_next, detail::mem_relaxed) == 0);
						raw_atomic_store(&control_block->m_next, next_ptr, detail::mem_release);

						DENSITY_ASSERT_INTERNAL(control_block < get_end_control_block(tail));
						m_tail = new_tail;
						return { control_block, next_ptr, user_storage };
					}
					else if (i_size + (i_alignment - min_alignment) <= s_max_size_inpage) // if this allocation may fit in a page
					{
						tail = page_overflow(tail);
					}
					else
					{
						// this allocation would never fit in a page, allocate an external block
						return external_allocate(i_control_bits, i_size, i_alignment);
					}
				}
			}

			/** Overload of inplace_allocate that can be used when all parameters are compile time constants */
			template <uintptr_t CONTROL_BITS, bool INCLUDE_TYPE, size_t SIZE, size_t ALIGNMENT>
				Block inplace_allocate()
			{
				static_assert((CONTROL_BITS & ~(detail::NbQueue_Busy | detail::NbQueue_Dead | detail::NbQueue_External)) == 0, "");
				static_assert(is_power_of_2(ALIGNMENT) && (SIZE % ALIGNMENT) == 0, "");

				constexpr auto alignment = detail::size_max(ALIGNMENT, min_alignment);
				constexpr auto size = uint_upper_align(SIZE, alignment);
				constexpr auto can_fit_in_a_page = size + (alignment - min_alignment) <= s_max_size_inpage;
				constexpr auto over_aligned = alignment > min_alignment;

				auto tail = m_tail;
				for (;;)
				{
					DENSITY_TEST_ARTIFICIAL_DELAY;

					DENSITY_ASSERT_INTERNAL(tail != nullptr && address_is_aligned(tail, s_alloc_granularity));

					// allocate space for the control block (and possibly the runtime type)
					void * address = address_add(tail, INCLUDE_TYPE ? s_element_min_offset : s_rawblock_min_offset);

					// allocate space for the element
					if (over_aligned)
					{
						address = address_upper_align(address, alignment);
					}
					void * const user_storage = address;
					address = address_add(address, size);
					address = address_upper_align(address, s_alloc_granularity);
					auto const new_tail = static_cast<ControlBlock*>(address);

					// check for page overflow
					auto const new_tail_offset = address_diff(new_tail, address_lower_align(tail, ALLOCATOR_TYPE::page_alignment));
					if (DENSITY_LIKELY(new_tail_offset <= s_end_control_offset))
					{
						/* note: while control_block->m_next is zero, no consumers may ever read this
							variable. Maybe this does not need to be atomic. */
						raw_atomic_store(&new_tail->m_next, 0, detail::mem_relaxed);

						/* Assign m_next, and set the flags. This is very important for the consumers,
							because they that need this write happens before any other part of the
							allocated memory is modified. */
						auto const control_block = tail;
						auto const next_ptr = reinterpret_cast<uintptr_t>(new_tail) + CONTROL_BITS;
						DENSITY_ASSERT_INTERNAL(raw_atomic_load(&control_block->m_next, detail::mem_relaxed) == 0);
						raw_atomic_store(&control_block->m_next, next_ptr, detail::mem_release);

						DENSITY_ASSERT_INTERNAL(control_block < get_end_control_block(tail));
						m_tail = new_tail;
						return { control_block, next_ptr, user_storage };
					}
					else if (can_fit_in_a_page) // if this allocation may fit in a page
					{
						tail = page_overflow(tail);
					}
					else
					{
						// this allocation would never fit in a page, allocate an external block
						return external_allocate(CONTROL_BITS, SIZE, ALIGNMENT);
					}
				}
			}

			/** Used by inplace_allocate when the block can't be allocated in a page- */
			Block external_allocate(uintptr_t i_control_bits, size_t i_size, size_t i_alignment)
			{
				auto const external_block = ALLOCATOR_TYPE::allocate(i_size, i_alignment);
				try
				{
					/* external blocks always allocate space for the type, because it would be complicated 
						for the consumers to handle both cases*/
					auto const inplace_put = inplace_allocate(i_control_bits | detail::NbQueue_External, true, sizeof(ExternalBlock), alignof(ExternalBlock));
					new(inplace_put.m_user_storage) ExternalBlock{external_block, i_size, i_alignment};
					return Block{ inplace_put.m_control_block, inplace_put.m_next_ptr, external_block };
				}
				catch (...)
				{
					/* if inplace_allocate fails, that means that we were able to allocate the external block,
						but we were not able to put the struct ExternalBlock in the page (because a new page was
						necessary, but we could not allocate it). */
					ALLOCATOR_TYPE::deallocate(external_block, i_size, i_alignment);
					throw;
				}
			}

			/** Handles a page overflow of the tail. This function may allocate a new page.
				@param i_tail the value read from m_tail. Note that other threads may have updated m_tail
					in then meanwhile.
				@return an update value of tail, that makes the current thread progress. */
			DENSITY_NO_INLINE ControlBlock * page_overflow(ControlBlock * const i_tail)
			{
				auto const page_end = get_end_control_block(i_tail);
				if (i_tail < page_end)
				{
					/* There is space between the (presumed) current tail and the end control block.
						We try to pad it with a dead element. */

					DENSITY_ASSERT_INTERNAL(m_tail == i_tail);
					m_tail = i_tail;
					
					auto const block = static_cast<ControlBlock*>(i_tail);
					raw_atomic_store(&block->m_next, reinterpret_cast<uintptr_t>(page_end) + detail::NbQueue_Dead, detail::mem_release);
					return page_end;
				}
				else
				{
					// get or allocate a new page
					DENSITY_ASSERT_INTERNAL(i_tail == page_end);
					return get_or_allocate_next_page(i_tail);
				}
			}


			/** Tries to allocate a new page. In any case returns an update value of m_tail.
				@param i_tail the value read from m_tail. Note that other threads may have updated m_tail
					in then meanwhile.
				@return an update value of tail, that makes the current thread progress. */
			ControlBlock * get_or_allocate_next_page(ControlBlock * const i_end_control)
			{
				DENSITY_ASSERT_INTERNAL(i_end_control != nullptr &&
					address_is_aligned(i_end_control, s_alloc_granularity) &&
					i_end_control == get_end_control_block(i_end_control));

				if (i_end_control != invalid_control_block())
				{
					DENSITY_TEST_ARTIFICIAL_DELAY;

					// allocate and setup a new page
					auto new_page = create_page();

					i_end_control->m_next = reinterpret_cast<uintptr_t>(new_page) + detail::NbQueue_Dead;

					m_tail = new_page;

					return m_tail;
				}
				else
				{
					return create_initial_page();
				}
			}

			DENSITY_NO_INLINE ControlBlock * create_initial_page()
			{
				// m_initial_page = initial_page = create_page()
				auto const initial_page = create_page();
				DENSITY_ASSERT_INTERNAL(m_initial_page.load() == nullptr);
				m_initial_page.store(initial_page);

				// m_tail = initial_page;
				DENSITY_TEST_ARTIFICIAL_DELAY;
				DENSITY_ASSERT_INTERNAL(m_tail == invalid_control_block());
				m_tail = initial_page;

				return m_tail;
			}

			ControlBlock * create_page()
			{
				DENSITY_TEST_ARTIFICIAL_DELAY;

				auto const new_page = static_cast<ControlBlock *>(ALLOCATOR_TYPE::allocate_page());
				auto const new_page_end_block = get_end_control_block(new_page);
				new_page_end_block->m_next = detail::NbQueue_InvalidNextPage;
				
				raw_atomic_store(&new_page->m_next, 0, mem_release);

				return new_page;
			}

			static void commit_put_impl(const Block & i_put) noexcept
			{
				DENSITY_TEST_ARTIFICIAL_DELAY;

				// we expect to have NbQueue_Busy and not NbQueue_Dead
				DENSITY_ASSERT_INTERNAL(address_is_aligned(i_put.m_control_block, s_alloc_granularity));
				DENSITY_ASSERT_INTERNAL(
					(i_put.m_next_ptr & ~detail::NbQueue_AllFlags) == (raw_atomic_load(&i_put.m_control_block->m_next, detail::mem_relaxed) & ~detail::NbQueue_AllFlags) &&
					(i_put.m_next_ptr & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

				// remove the flag NbQueue_Busy
				raw_atomic_store(&i_put.m_control_block->m_next, i_put.m_next_ptr - detail::NbQueue_Busy, detail::mem_seq_cst);
			}

			static void cancel_put_impl(const Block & i_put) noexcept
			{
				// destroy the element and the type
				auto type_ptr = type_after_control(i_put.m_control_block);
				type_ptr->destroy(static_cast<COMMON_TYPE*>(i_put.m_user_storage));
				type_ptr->RUNTIME_TYPE::~RUNTIME_TYPE();

				cancel_put_nodestroy_impl(i_put);
			}

			static void cancel_put_nodestroy_impl(const Block & i_put) noexcept
			{
				DENSITY_TEST_ARTIFICIAL_DELAY;

				// we expect to have NbQueue_Busy and not NbQueue_Dead
				DENSITY_ASSERT_INTERNAL(address_is_aligned(i_put.m_control_block, s_alloc_granularity));
				DENSITY_ASSERT_INTERNAL(
					(i_put.m_next_ptr & ~detail::NbQueue_AllFlags) == (raw_atomic_load(&i_put.m_control_block->m_next, detail::mem_relaxed) & ~detail::NbQueue_AllFlags) &&
					(i_put.m_next_ptr & (detail::NbQueue_Busy | detail::NbQueue_Dead)) == detail::NbQueue_Busy);

				// remove NbQueue_Busy and add NbQueue_Dead
				auto const addend = static_cast<uintptr_t>(detail::NbQueue_Dead) - static_cast<uintptr_t>(detail::NbQueue_Busy);
				raw_atomic_store(&i_put.m_control_block->m_next, i_put.m_next_ptr + addend, detail::mem_seq_cst);
			}

			ControlBlock * get_initial_page() const noexcept
			{
				return m_initial_page.load();
			}

			static RUNTIME_TYPE * type_after_control(ControlBlock * i_control) noexcept
			{
				return static_cast<RUNTIME_TYPE*>(address_add(i_control, s_type_offset));
			}

			static void * get_unaligned_element(detail::NbQueueControl<void> * i_control)
			{
				auto result = address_add(i_control, s_element_min_offset);
				if (i_control->m_next & detail::NbQueue_External)
				{
					/* i_control and s_element_min_offset are aligned to alignof(ExternalBlock), so 
						we don't need to align further */
					result = static_cast<ExternalBlock*>(result)->m_block;
				}
				return result;
			}

			static void * get_element(detail::NbQueueControl<void> * i_control)
			{
				auto result = address_add(i_control, s_element_min_offset);
				if (i_control->m_next & detail::NbQueue_External)
				{
					/* i_control and s_element_min_offset are aligned to alignof(ExternalBlock), so 
						we don't need to align further */
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

		private: // data members
			alignas(concurrent_alignment) ControlBlock * m_tail;
			std::atomic<ControlBlock*> m_initial_page;
		};

	} // namespace detail

} // namespace density
