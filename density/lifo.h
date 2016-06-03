
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_common.h"
#include "void_allocator.h"
#include <vector>
#include <memory>

namespace density
{
	template <typename UNDERLYING_VOID_ALLOCATOR = void_allocator >
		class lifo_allocator
	{
	public:

		static const size_t s_granularity = alignof(std::max_align_t);

		lifo_allocator() = delete;		
		
		static void * allocate(size_t i_size)
		{
			DENSITY_ASSERT(i_size % s_granularity == 0);

			ThreadData & thread_data = get_thread_data();
			void * result = thread_data.m_last_page->allocate(i_size);
			if (result == nullptr)
			{
				result = alloc_new_page(i_size);
			}
			return result;
		}
		
		static void deallocate(void * i_block)
		{
			ThreadData & thread_data = get_thread_data();
			auto const last_page = thread_data.m_last_page;
			last_page->free(i_block);
			if (last_page->is_empty())
			{
				pop_page();
			}
		}

		static UNDERLYING_VOID_ALLOCATOR & get_underlying_allocator()
		{
			return get_thread_data();
		}

	private:

		static const size_t s_min_page_size = 2048;

		DENSITY_NO_INLINE static void * alloc_new_page(size_t i_size)
		{
			const size_t page_size = detail::size_max(
				i_size + alignof(PageHeader),
				detail::size_max(s_min_page_size, UNDERLYING_VOID_ALLOCATOR::s_min_block_size));
			
			ThreadData & thread_data = get_thread_data();

			PageHeader * const new_page = static_cast<PageHeader*>( get_underlying_allocator().allocate(page_size, s_granularity) );
			thread_data.m_last_page = new(new_page) PageHeader(thread_data.m_last_page, 
				new_page + 1, address_add(new_page, page_size));

			void * const new_block = new_page->allocate(i_size);
			DENSITY_ASSERT_INTERNAL(new_block != nullptr);
			return new_block;
		}

		static void pop_page()
		{
			ThreadData & thread_data = get_thread_data();

			auto const last_page = thread_data.m_last_page;
			auto const prev_page = last_page->prev_page();
			last_page->PageHeader::~PageHeader();
			get_underlying_allocator().deallocate(last_page, last_page->capacity() + sizeof(PageHeader), s_granularity);
			thread_data.m_last_page = prev_page;
		}

		class PageHeader
		{
		public:

			PageHeader(PageHeader * const i_prev_page, void * i_start_address, void * i_end_address)
				: m_end_address(i_end_address), m_curr_address(i_start_address),
				  m_prev_page(i_prev_page), m_start_address(i_start_address)
			{
			}

			void * allocate(size_t i_size)
			{
				const auto start_of_block = m_curr_address;
				const auto end_of_block = address_add(start_of_block, i_size);
				// check for arithmetic overflow or insufficient space
				if (end_of_block >= start_of_block && end_of_block <= m_end_address)
				{					
					m_curr_address = end_of_block;
					return start_of_block;
				}
				else
				{
					return nullptr;
				}
			}

			void free(void * i_block)
			{
				m_curr_address = i_block;
			}

			bool owns(void * i_address) const
			{
				return i_address >= m_start_address && i_address < m_end_address;
			}

			bool is_empty() const
			{
				return m_curr_address == m_start_address;
			}

			PageHeader * prev_page() const		{ return m_prev_page; }

			size_t capacity() const				{ return address_diff(m_end_address, m_start_address); }
			
		private:
			void * const m_end_address;
			void * m_curr_address;
			PageHeader * const m_prev_page;
			void * const m_start_address;			
		};

		static PageHeader * get_empty_page()
		{
			static PageHeader s_empty(nullptr, nullptr, nullptr);
			return &s_empty;
		}
		
		struct ThreadData : UNDERLYING_VOID_ALLOCATOR
		{
			PageHeader * m_last_page = get_empty_page();
		};

		static ThreadData & get_thread_data()
		{
			return t_thread_data;
		}

		static thread_local ThreadData t_thread_data;
	};

	template <typename UNDERLYING_VOID_ALLOCATOR>
		thread_local typename lifo_allocator<UNDERLYING_VOID_ALLOCATOR>::ThreadData lifo_allocator<UNDERLYING_VOID_ALLOCATOR>::t_thread_data;

	template <typename LIFO_ALLOCATOR = lifo_allocator<> >
		class lifo_buffer
	{
	public:

		lifo_buffer(size_t i_size) 
			: m_buffer(LIFO_ALLOCATOR::allocate(i_size)), m_size(i_size) { }

		lifo_buffer(const lifo_buffer &) = delete;
		lifo_buffer & operator = (const lifo_buffer &) = delete;

		~lifo_buffer()
		{
			LIFO_ALLOCATOR::deallocate(m_buffer);
		}

		void resize(size_t i_new_size)
		{
			m_buffer = LIFO_ALLOCATOR::reallocate(m_buffer, i_new_size);
			m_size = i_new_size;
		}

		void * data() const
		{
			return m_buffer;
		}

		size_t size() const
		{
			return m_size;
		}

	private:
		void * m_buffer;
		size_t m_size;
	};

	template <typename TYPE>
		class lifo_array
	{
	public:

		lifo_array(size_t i_size)
		{

		}

	private:
		TYPE * m_elements;
		size_t m_size;
	};

} // namespace density