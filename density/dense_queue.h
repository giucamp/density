
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <vector>
#include "density_common.h"
#include "dense_fixed_queue.h"
#include <list>

namespace density
{
	template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = RuntimeType<ELEMENT> >
		class DenseQueue final
	{
	private:
		using FixedQueue = DenseFixedQueue<ELEMENT, ALLOCATOR, RUNTIME_TYPE>;
		using ListAlloc = typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<FixedQueue *>;
		using ChunkList = std::list< FixedQueue, ALLOCATOR >;
	
	public:

		static const size_t s_default_chunck_size = 1024 * 256;

		using RuntimeType = RUNTIME_TYPE;
		using allocator_type = ALLOCATOR;
		using value_type = ELEMENT;
		using reference = ELEMENT &;
		using const_reference = const ELEMENT &;
		using pointer = typename std::allocator_traits<allocator_type>::pointer;
		using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
		using difference_type = ptrdiff_t;
		using size_type = size_t;
		class iterator;
		class const_iterator;

		DenseQueue(size_t i_chunk_size = s_default_chunck_size)
			: m_chunk_size(i_chunk_size)
		{
			m_tail = m_head = m_chunks.end();
		}

		class iterator final
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = ELEMENT;
			using reference = ELEMENT &;
			using const_reference = const ELEMENT &;
			using pointer = typename std::allocator_traits<allocator_type>::pointer;
			using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

			iterator() DENSITY_NOEXCEPT
			{
			}

			iterator(DenseQueue * i_queue,
				const typename ChunkList::iterator & i_chunk_iterator,
				const typename FixedQueue::iterator & i_queue_iterator) DENSITY_NOEXCEPT
					: m_queue_iterator(i_queue_iterator), m_chunk_iterator(i_chunk_iterator), m_queue(i_queue)
			{
			}

			iterator(DenseQueue * i_queue, const typename ChunkList::iterator & i_chunk_iterator ) DENSITY_NOEXCEPT
				: m_chunk_iterator(i_chunk_iterator), m_queue(i_queue)
			{
			}

			iterator & operator ++ () DENSITY_NOEXCEPT
			{
				if (!m_queue_iterator.is_end())
				{
					++m_queue_iterator;
				}
				else
				{
					slow_move_next();
				}
				return *this;
			}

			iterator operator++ (int) DENSITY_NOEXCEPT
			{
				const iterator copy(*this);
				++*this;
				return copy;
			}

			bool operator == (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				if (m_queue == i_other.m_queue && m_queue->m_chunks.empty())
				{
					return true;
				}
				return m_queue_iterator == i_other.m_queue_iterator;
			}

			bool operator != (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				return !operator == (i_other);
			}

			ELEMENT operator * () const DENSITY_NOEXCEPT
			{
				return *m_queue_iterator;
			}

		private:

			DENSITY_NO_INLINE void slow_move_next() DENSITY_NOEXCEPT
			{
				++m_chunk_iterator;
				if (m_chunk_iterator == m_queue->m_chunks.end())
				{
					m_chunk_iterator = m_queue->m_chunks.begin();
				}
				m_queue_iterator = m_chunk_iterator->begin();
			}

		private:
			typename FixedQueue::iterator m_queue_iterator;
			typename ChunkList::iterator m_chunk_iterator;
			DenseQueue * m_queue;
		}; // class iterator

		class const_iterator final
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = const ELEMENT;
			using reference = const ELEMENT &;
			using const_reference = const ELEMENT &;
			using pointer = typename std::allocator_traits<allocator_type>::pointer;
			using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

		private:
		}; // class const_iterator

		iterator begin() DENSITY_NOEXCEPT
		{
			if (!m_chunks.empty())
			{
				return iterator(this, m_head, m_head->begin());
			}
			else
			{
				return iterator(this, m_head);
			}
		}

		iterator end() DENSITY_NOEXCEPT
		{
			if (!m_chunks.empty())
			{
				return iterator(this, m_tail, m_tail->end());
			}
			else
			{
				return iterator(this, m_tail);
			}
		}

		template <typename ELEMENT_COMPLETE_TYPE>
			void push(ELEMENT_COMPLETE_TYPE && i_source) DENSITY_NOEXCEPT
		{
			if (m_chunks.empty())
			{
				m_tail = m_head = m_chunks.emplace(m_chunks.begin(), m_chunk_size);
			}

			while(!m_tail->try_push(std::forward<ELEMENT_COMPLETE_TYPE>(i_source)))
			{
				++m_tail;
				
				if (m_tail == m_chunks.end())
				{
					m_tail = m_chunks.begin();
				}
				if (m_tail == m_head)
				{
					m_tail = m_chunks.emplace(m_tail, m_chunk_size);
				}
			}
		}

		template <typename OPERATION>
			void consume(OPERATION && i_operation)
				//DENSITY_NOEXCEPT_V(DENSITY_NOEXCEPT_V((i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT>()))))
		{
			m_head->consume( std::forward<OPERATION>(i_operation) );
			if (m_head->empty())
			{
				++m_head;
			}
		}

		void pop() DENSITY_NOEXCEPT
		{
			consume([](const RUNTIME_TYPE &, const ELEMENT &) {});
		}

		bool empty() const DENSITY_NOEXCEPT
		{
			return m_head == m_tail && m_tail == m_chunks.end();
		}

	private:
		ChunkList m_chunks;
		typename ChunkList::iterator m_head, m_tail;
		size_t m_chunk_size;
	}; // class DenseQueue

} // namespace density

