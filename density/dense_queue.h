
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "detail\queue_impl.h"
#include <memory>

namespace density
{
	/** Class template that implements an heterogeneous fifo sequence with dynamic size.  */
	template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>,
			typename RUNTIME_TYPE = RuntimeType<ELEMENT>, size_t DEFAULT_ELEMENT_ALIGNMENT = 0 >
		class DenseQueue final : private std::allocator_traits<ALLOCATOR>::template rebind_alloc<char>
	{	
	public:

		using RuntimeType = RUNTIME_TYPE;
		using value_type = ELEMENT;
		using reference = ELEMENT &;
		using const_reference = const ELEMENT &;
		using difference_type = ptrdiff_t;
		using size_type = size_t;
		class iterator;
		class const_iterator;

		static const size_t s_initial_mem_reserve = 1024;

		DenseQueue()
		{
			alloc(s_initial_mem_reserve, m_impl.element_max_alignment());
		}

		DenseQueue(DenseQueue && i_source) DENSITY_NOEXCEPT
			: Allocator(std::move(static_cast<Allocator&>(i_source))),
			  m_impl(std::move(i_source.m_impl))
		{
		}

		DenseQueue & operator = (DenseQueue && i_source)
		{
			assert(this != &i_source);
			destroy_impl();
			static_cast<Allocator&>(*this) = std::move( static_cast<Allocator&>(i_source) );
			m_impl = std::move(i_source.m_impl);
			return *this;
		}

		DenseQueue(const DenseQueue & i_source)
		{
			copy_from_impl(i_source);
		}

		DenseQueue & operator = (const DenseQueue & i_source)
		{
			assert(this != &i_source);
			destroy_impl();
			static_cast<Allocator&>(*this) = static_cast<const Allocator&>(i_source);
			copy_from_impl(i_source);
			return *this;
		}

		~DenseQueue()
		{
			destroy_impl();
		}

					// insertion \ removal
		
		template <typename ELEMENT_COMPLETE_TYPE>
			void push(ELEMENT_COMPLETE_TYPE && i_source)
		{
			push_impl(std::forward<ELEMENT_COMPLETE_TYPE>(i_source),
				typename std::is_rvalue_reference<ELEMENT_COMPLETE_TYPE&&>::type());
		}

		template <typename ELEMENT_COMPLETE_TYPE, typename... PARAMETERS>
			void emplace(PARAMETERS && ... i_parameters)
				DENSITY_NOEXCEPT_V((std::is_nothrow_constructible<ELEMENT_COMPLETE_TYPE, PARAMETERS...>::value))
		{
			return insert_back_impl(RuntimeType::template make<ELEMENT_COMPLETE_TYPE>(),
				[&i_parameters...](const RuntimeType &, void * i_dest) {
				new(i_dest) ELEMENT_COMPLETE_TYPE(std::forward<PARAMETERS>(i_parameters)...);
			});
		}

		bool copy_push(const RUNTIME_TYPE & i_type, const ELEMENT * i_source)
		{
			return insert_back_impl(i_type,
				typename detail::QueueImpl<RUNTIME_TYPE>::CopyConstruct(i_source));
		}

		bool move_push(const RUNTIME_TYPE & i_type, ELEMENT * i_source) DENSITY_NOEXCEPT
		{
			return insert_back_impl(i_type,
				typename detail::QueueImpl<RUNTIME_TYPE>::MoveConstruct(i_source));
		}

		template <typename OPERATION>
			void consume(OPERATION && i_operation)
				DENSITY_NOEXCEPT_V(DENSITY_NOEXCEPT_V((i_operation(std::declval<const RUNTIME_TYPE>(), std::declval<ELEMENT>()))))
		{
			m_impl.consume([&i_operation](const RUNTIME_TYPE & i_type, void * i_element) {
				i_operation(i_type, *static_cast<ELEMENT*>(i_element));
			});
		}

		void pop() DENSITY_NOEXCEPT
		{
			m_impl.consume([](const RUNTIME_TYPE &, void *) {});
		}

		void mem_reserve(size_t i_mem_size)
		{
			if (i_mem_size > m_impl.mem_capacity())
			{
				mem_realloc_impl(i_mem_size);
			}
		}

					// iterators

		class iterator final
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = ELEMENT;
			using pointer = ELEMENT *;
			using reference = ELEMENT &;
			using const_reference = const ELEMENT &;

			iterator() DENSITY_NOEXCEPT {}

			iterator(const typename detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>::IteratorImpl & i_source) DENSITY_NOEXCEPT
				: m_impl(i_source) {  }

			value_type & operator * () const DENSITY_NOEXCEPT { return *static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * curr_element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }

			iterator & operator ++ () DENSITY_NOEXCEPT
			{
				m_impl.move_next();
				return *this;
			}

			iterator operator++ (int) DENSITY_NOEXCEPT
			{
				const iterator copy(*this);
				m_impl.move_next();
				return copy;
			}

			bool operator == (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type == i_other.curr_type();
			}

			bool operator != (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type != i_other.curr_type();
			}

			bool operator == (const const_iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type == i_other.curr_type();
			}

			bool operator != (const const_iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type != i_other.curr_type();
			}

			const RUNTIME_TYPE * curr_type() const DENSITY_NOEXCEPT { return m_impl.m_curr_type; }

			bool is_end() const DENSITY_NOEXCEPT
			{
				return m_impl.is_end();
			}

		private:
			typename detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>::IteratorImpl m_impl;

		}; // class iterator

		class const_iterator final
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = const ELEMENT;
			using pointer = const ELEMENT *;
			using reference = const ELEMENT &;
			using const_reference = const ELEMENT &;
			
			const_iterator() DENSITY_NOEXCEPT {}

			const_iterator(const typename detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>::IteratorImpl & i_source) DENSITY_NOEXCEPT
				: m_impl(i_source) {  }

			const_iterator(const iterator & i_source) DENSITY_NOEXCEPT
				: m_impl(i_source.m_impl) {  }

			value_type & operator * () const DENSITY_NOEXCEPT { return *static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * operator -> () const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }
			value_type * curr_element() const DENSITY_NOEXCEPT { return static_cast<value_type *>(m_impl.m_curr_element); }

			const_iterator & operator ++ () DENSITY_NOEXCEPT
			{
				m_impl.move_next();
				return *this;
			}

			const_iterator operator++ (int) DENSITY_NOEXCEPT
			{
				const const_iterator copy(*this);
				m_impl.move_next();
				return copy;
			}

			bool operator == (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_curr_type == i_other.curr_type();
			}

			bool operator != (const iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_curr_type != i_other.curr_type();
			}

			bool operator == (const const_iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type == i_other.curr_type();
			}

			bool operator != (const const_iterator & i_other) const DENSITY_NOEXCEPT
			{
				return m_impl.m_curr_type != i_other.curr_type();
			}

			const RUNTIME_TYPE * curr_type() const DENSITY_NOEXCEPT { return m_impl.m_curr_type; }

			bool is_end() const DENSITY_NOEXCEPT
			{
				return m_impl.is_end();
			}

		private:
			typename detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>::IteratorImpl m_impl;
		}; // class const_iterator

		iterator begin() DENSITY_NOEXCEPT { return iterator(m_impl.begin()); }
		iterator end() DENSITY_NOEXCEPT { return iterator(m_impl.end()); }

		const_iterator begin() const DENSITY_NOEXCEPT { return const_iterator(m_impl.begin()); }
		const_iterator end() const DENSITY_NOEXCEPT { return const_iterator(m_impl.end()); }

		const_iterator cbegin() const DENSITY_NOEXCEPT { return const_iterator(m_impl.begin()); }
		const_iterator cend() const DENSITY_NOEXCEPT { return const_iterator(m_impl.end()); }

		bool empty() const DENSITY_NOEXCEPT { return m_impl.empty(); }

		void clear() DENSITY_NOEXCEPT 
		{ 
			m_impl.delete_all(); 
		}
		
		const ELEMENT & front() DENSITY_NOEXCEPT
		{
			assert(!empty());
			const auto it = m_impl.begin();
			return *static_cast<value_type *>(it.m_curr_element);
		}

		MemSize<size_t> mem_capacity() const DENSITY_NOEXCEPT
		{
			return m_impl.mem_capacity();
		}

		MemSize<size_t> mem_size() const DENSITY_NOEXCEPT
		{
			return m_impl.mem_size();
		}

		MemSize<size_t> mem_free() const DENSITY_NOEXCEPT
		{
			return m_impl.mem_capacity() - m_impl.mem_size();
		}

	private:

		using Impl = typename detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>;
		using Allocator = typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char>;

		typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<char> & allocator() DENSITY_NOEXCEPT
		{
			return *this;
		}

		void alloc(size_t i_size, size_t i_alignment)
		{
			m_impl = Impl(AllocatorUtils::aligned_allocate(allocator(), i_size, i_alignment, 0), i_size);
		}

		void free() DENSITY_NOEXCEPT
		{
			AllocatorUtils::aligned_deallocate(allocator(), m_impl.buffer(), m_impl.mem_capacity().value());
			m_impl = Impl();
		}

		void destroy_impl()
		{
			m_impl.delete_all();
			free();
		}

		void move_from_impl(DenseQueue & i_source)
		{
			m_impl = std::move(i_source.m_impl);
		}

		void copy_from_impl(const DenseQueue & i_source)
		{
			alloc(i_source.m_impl.mem_capacity().value(), i_source.m_impl.element_max_alignment());
			m_impl.copy_elements_from(i_source.m_impl);
		}

		void mem_realloc_impl(size_t i_mem_size)
		{
			assert(i_mem_size > m_impl.mem_capacity().value());

			Impl new_impl(AllocatorUtils::aligned_allocate(allocator(), i_mem_size, m_impl.element_max_alignment(), 0), i_mem_size);
			new_impl.move_elements_from(m_impl);

			AllocatorUtils::aligned_deallocate(allocator(), m_impl.buffer(), m_impl.mem_capacity().value());
			m_impl = std::move(new_impl);
		}

		template <typename CONSTRUCTOR>
			void insert_back_impl(const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
		{
			while(!m_impl.push(i_source_type, std::forward<CONSTRUCTOR>(i_constructor)))
			{
				mem_realloc_impl( detail::size_max(
					m_impl.mem_capacity().value() * 2, 
					i_source_type.size() * 16 + i_source_type.alignment() ) );
			}
		}

		// overload used if i_source is an rvalue
		template <typename ELEMENT_COMPLETE_TYPE>
			void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::true_type)
		{
			insert_back_impl(RuntimeType::template make<typename detail::RemoveRefsAndConst<ELEMENT_COMPLETE_TYPE>::type>(),
				typename detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>::MoveConstruct(&i_source));
		}

		// overload used if i_source is an lvalue
		template <typename ELEMENT_COMPLETE_TYPE>
			void push_impl(ELEMENT_COMPLETE_TYPE && i_source, std::false_type)
		{
			insert_back_impl(RuntimeType::template make<typename detail::RemoveRefsAndConst<ELEMENT_COMPLETE_TYPE>::type>(),
				typename detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT>::CopyConstruct(&i_source));
		}

	private:
		detail::QueueImpl<RUNTIME_TYPE, DEFAULT_ELEMENT_ALIGNMENT> m_impl;

	}; // class DenseQueue
}
