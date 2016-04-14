
//          Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

namespace reflective
{
	/** A dense-list is a polymorphic sequence container optimized to be compact in both heap memory and inline storage.
		Elements is a DenseList are allocated respecting their alignment requirements.
		In a polymorphic container every element can have a different complete type, provided that this type is covariant to the type ELEMENT.
		All the elements of a DenseList are arranged in the same memory block of the heap. 
		Insertions\removals of a non-zero number of elements and clear() always reallocate the memory blocks and invalidate existing iterators.
		The inline storage of DenseList is the same of a pointer. An empty DenseList does not use heap memory.
		All the functions of DenseList gives at least the strong exception guarantee. */
	template <typename ALLOCATOR, typename ELEMENT_TYPE>
		class DenseList<void, ALLOCATOR, ELEMENT_TYPE> final : public details::DenseListBase<ALLOCATOR, ELEMENT_TYPE>
	{
	private:
		using BaseClass = DenseListBase<ALLOCATOR, ELEMENT_TYPE>;
		
	public:

		using allocator_type = ALLOCATOR;
		using difference_type = ptrdiff_t;
		using size_type = size_t;
		using value_type = void;
		using pointer = typename std::allocator_traits<allocator_type>::pointer;
		using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

		/** Alias for the template parameters */
		using ElementType = ELEMENT_TYPE;
		
		/** Creates a DenseList containing all the elements specified in the parameter list. 
			For each object of the parameter pack, an element is added to the list by copy-construction or move-construction.
				@param i_args elements to add to the list. 
				@return the new DenseList
			Example: 
				const auto list = DenseList<int>::make(1, 2, 3);
				const auto list1 = DenseList<BaseClass>::make(Derived1(), Derived2(), Derived1()); */
		template <typename... TYPES>
			inline static DenseList make(TYPES &&... i_args)
		{
			DenseList new_list;
			BaseClass::make_impl(new_list, std::forward<TYPES>(i_args)...);
			return std::move(new_list);
		}

		/** Creates a DenseList containing all the elements specified in the parameter list. The allocator of the new DenseList is copy-constructed from the provided one.
			For each object of the parameter pack, an element is added to the list by copy-construction or move-construction.
				@param i_args elements to add to the list. 
				@return the new DenseList 
			Example:
				MyAlloc<int> my_alloc;
				MyAlloc<BaseClass> my_alloc1;
				const auto list = DenseList<int>::make_with_alloc(my_alloc, 1, 2, 3);
				const auto list1 = DenseList<BaseClass>::make_with_alloc(my_alloc1, Derived1(), Derived2(), Derived1()); */
		template <typename... TYPES>
			inline static DenseList make_with_alloc(const ALLOCATOR & i_allocator, TYPES &&... i_args)
		{
			DenseList new_list;
			BaseClass::make_impl(new_list, std::forward<TYPES>(i_args)...);
			return std::move(new_list);
		}

		class iterator;
		class const_iterator;

		using IteratorBase = typename BaseClass::IteratorBase;

		class iterator final : private IteratorBase
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = void;
			using pointer = typename std::allocator_traits<allocator_type>::pointer;
			using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

			iterator(const IteratorBase & i_source) REFLECTIVE_NOEXCEPT
				: IteratorBase(i_source) {  }
			
			value_type * curr_element() const REFLECTIVE_NOEXCEPT
				{ return static_cast<value_type *>(IteratorBase::curr_element()); }

			iterator & operator ++ () REFLECTIVE_NOEXCEPT
			{
				IteratorBase::move_next();
				return *this;
			}

			iterator operator++ (int) REFLECTIVE_NOEXCEPT
			{
				iterator copy(*this);
				IteratorBase::move_next();
				return copy;
			}

			bool operator == (const iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return IteratorBase::m_curr_type == i_other.curr_type();
			}

			bool operator != (const iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return IteratorBase::m_curr_type != i_other.curr_type();
			}

			bool operator == (const const_iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return IteratorBase::m_curr_type == i_other.curr_type();
			}

			bool operator != (const const_iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return IteratorBase::m_curr_type != i_other.curr_type();
			}
			
			const ELEMENT_TYPE * curr_type() const REFLECTIVE_NOEXCEPT { return IteratorBase::m_curr_type; }

			friend class const_iterator;

		}; // class iterator

		class const_iterator final : private IteratorBase
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = const void;
			using pointer = typename std::allocator_traits<allocator_type>::pointer;
			using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

			const_iterator(const IteratorBase & i_source) REFLECTIVE_NOEXCEPT
				: IteratorBase(i_source) {  }

			const_iterator(const iterator & i_source) REFLECTIVE_NOEXCEPT
				: IteratorBase(i_source) {  }
									
			value_type * curr_element() const REFLECTIVE_NOEXCEPT
				{ return static_cast<value_type *>(IteratorBase::curr_element()); }

			const_iterator & operator ++ () REFLECTIVE_NOEXCEPT
			{
				IteratorBase::move_next();
				return *this;
			}

			const_iterator operator++ (int) REFLECTIVE_NOEXCEPT
			{
				iterator copy(*this);
				IteratorBase::move_next();
				return copy;
			}

			bool operator == (const iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_curr_type == i_other.curr_type();
			}

			bool operator != (const iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return m_curr_type != i_other.curr_type();
			}

			bool operator == (const const_iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return IteratorBase::m_curr_type == i_other.curr_type();
			}

			bool operator != (const const_iterator & i_other) const REFLECTIVE_NOEXCEPT
			{
				return IteratorBase::m_curr_type != i_other.curr_type();
			}

			const ELEMENT_TYPE * curr_type() const REFLECTIVE_NOEXCEPT { return IteratorBase::m_curr_type; }
		
			friend class DenseList;

		}; // class const_iterator

		iterator begin() REFLECTIVE_NOEXCEPT { return iterator(BaseClass::begin()); }
		iterator end() REFLECTIVE_NOEXCEPT { return iterator(BaseClass::end()); }

		const_iterator begin() const REFLECTIVE_NOEXCEPT { return const_iterator(BaseClass::begin()); }
		const_iterator end() const REFLECTIVE_NOEXCEPT{ return const_iterator(BaseClass::end()); }

		const_iterator cbegin() const REFLECTIVE_NOEXCEPT { return const_iterator(BaseClass::begin()); }
		const_iterator cend() const REFLECTIVE_NOEXCEPT { return const_iterator(BaseClass::end()); }

		struct CopyConstruct
		{
			const void * const m_source;

			CopyConstruct(const void * i_source)
				: m_source(i_source) { }

			void * operator () (typename BaseClass::ListBuilder & i_builder, const ElementType & i_element_type)
			{
				return i_builder.add_by_copy(i_element_type, m_source);
			}
		};

		struct MoveConstruct
		{
			void * const m_source;

			MoveConstruct(void * i_source)
				: m_source(i_source) { }

			void * operator () (typename BaseClass::ListBuilder & i_builder, const ElementType & i_element_type)
			{
				return i_builder.add_by_move(i_element_type, m_source);
			}
		};

		template <typename ELEMENT_COMPLETE_TYPE>
			void push_back(const ELEMENT_COMPLETE_TYPE & i_source)
		{
			BaseClass::insert_impl(BaseClass::m_types + BaseClass::size(),
				ElementType::template make<ELEMENT_COMPLETE_TYPE>(),
				CopyConstruct(&i_source) );
		}

		template <typename ELEMENT_COMPLETE_TYPE>
			void push_front(const ELEMENT_COMPLETE_TYPE & i_source)
		{
			BaseClass::insert_impl(BaseClass::m_types,
				ElementType::template make<ELEMENT_COMPLETE_TYPE>(),
				CopyConstruct(&i_source) );
		}

		template <typename ELEMENT_COMPLETE_TYPE>
			void push_back(ELEMENT_COMPLETE_TYPE && i_source)
		{
			BaseClass::insert_impl(BaseClass::m_types + BaseClass::size(),
				ElementType::template make<ELEMENT_COMPLETE_TYPE>(),
				MoveConstruct(&i_source) );
		}

		template <typename ELEMENT_COMPLETE_TYPE>
			void push_front(ELEMENT_COMPLETE_TYPE && i_source)
		{
			BaseClass::insert_impl(BaseClass::m_types,
				ElementType::template make<ELEMENT_COMPLETE_TYPE>(),
				MoveConstruct(&i_source) );
		}

		void pop_front()
		{
			auto const types = BaseClass::m_types;
			BaseClass::erase_impl(types, types + 1);
		}

		void pop_back()
		{
			auto const end_type = BaseClass::m_types + 
				BaseClass::get_size_not_empty();
			BaseClass::erase_impl(end_type - 1, end_type);
		}

		template <typename ELEMENT_COMPLETE_TYPE>
			iterator insert(const_iterator i_position, const ELEMENT_COMPLETE_TYPE & i_source)
		{
			return BaseClass::insert_impl(i_position.m_curr_type,
				ElementType::template make<ELEMENT_COMPLETE_TYPE>(),
				CopyConstruct(&i_source) );
		}

		template <typename ELEMENT_COMPLETE_TYPE>
			iterator insert(const_iterator i_position, size_t i_count, const ELEMENT_COMPLETE_TYPE & i_source)
		{
			if (i_count > 0)
			{
				return BaseClass::insert_n_impl(i_position.m_curr_type, i_count,
					ElementType::template make<ELEMENT_COMPLETE_TYPE>(),
					CopyConstruct(&i_source) );
			}
			else
			{
				// inserting 0 elements
				return iterator(i_position);
			}
		}

		/* Temporary disabled
		template <typename ELEMENT_COMPLETE_TYPE, typename = typename std::enable_if<  >::value, void >
			iterator insert(const_iterator i_position, ELEMENT_COMPLETE_TYPE && i_source)
		{
			return BaseClass::insert_impl(i_position.m_curr_type,
				ElementType::template make<ELEMENT_COMPLETE_TYPE>(),
				MoveConstruct(&i_source) );
		}*/

		iterator erase(const_iterator i_position)
		{
			return BaseClass::erase_impl(i_position.m_curr_type, i_position.m_curr_type + 1);
		}

		iterator erase(const_iterator i_from, const_iterator i_to)
		{
			auto from_type = i_from.curr_type();
			auto to_type = i_to.curr_type();
			if (from_type != to_type)
			{
				return BaseClass::erase_impl(from_type, to_type);
			}
			else
			{
				// removing 0 elements
				return iterator(i_from);
			}
		}

		void swap(DenseList & i_other) REFLECTIVE_NOEXCEPT
		{
			std::swap(BaseClass::m_types, i_other.m_types);
		}

					/////////////////////////

		/* to do, & WARNING!: this function is slicing-comparing. Fix or delete. */
		bool equal_to(const DenseList & i_source) const
		{
			if (BaseClass::size() != i_source.size())
			{
				return false;
			}
			else
			{
				const auto end_1 = cend();
				for (auto it_1 = cbegin(), it_2 = i_source.cbegin(); it_1 != end_1; ++it_1, ++it_2)
				{
					if (*it_1 != *it_2)
					{
						return false;
					}
				}
				return true;
			}
		}

		bool operator == (const DenseList & i_source) const { return equal_to(i_source); }
		bool operator != (const DenseList & i_source) const { return !equal_to(i_source); }

	}; // class DenseList;
	
} // namespace reflective
