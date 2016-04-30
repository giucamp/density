
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <memory>
#include "runtime_type.h"

#ifdef NDEBUG
	#define DENSITY_DENSE_LIST_DEBUG		0
#else
	#define DENSITY_DENSE_LIST_DEBUG		1
#endif

namespace density
{
	namespace detail
	{
		template < typename ALLOCATOR, typename RUNTIME_TYPE >
			class DenseListImpl : private ALLOCATOR
		{
		public:

			size_t size() const DENSITY_NOEXCEPT
			{
				if (m_types != nullptr)
				{
					Header * const header = reinterpret_cast<Header*>(m_types) - 1;
					return header->m_count;
				}
				else
				{
					return 0;
				}
			}

			bool empty() const DENSITY_NOEXCEPT
			{
				return m_types == nullptr;
			}

			void clear() DENSITY_NOEXCEPT
			{
				destroy_impl();
				m_types = nullptr;
			}

		
			DenseListImpl() DENSITY_NOEXCEPT
				: m_types(nullptr)
					{ }

			DenseListImpl(DenseListImpl && i_source) DENSITY_NOEXCEPT
			{
				move_impl(std::move(i_source));
			}

			DenseListImpl & operator = (DenseListImpl && i_source) DENSITY_NOEXCEPT
			{
				assert(this != &i_source); // self assignment not supported
				destroy_impl();
				move_impl(std::move(i_source));
				return *this;
			}

			DenseListImpl(const DenseListImpl & i_source)
			{
				copy_impl(i_source);
			}

			DenseListImpl & operator = (const DenseListImpl & i_source)
			{
				assert(this != &i_source); // self assignment not supported
				destroy_impl();
				copy_impl(i_source);
				return *this;
			}

			~DenseListImpl() DENSITY_NOEXCEPT
			{
				destroy_impl();
			}

			struct IteratorBaseImpl
			{
				IteratorBaseImpl() DENSITY_NOEXCEPT
					: m_unaligned_curr_element(nullptr), m_curr_type(nullptr) { }

				IteratorBaseImpl(void * i_curr_element, const RUNTIME_TYPE * i_curr_type) DENSITY_NOEXCEPT
					: m_unaligned_curr_element(i_curr_element), m_curr_type(i_curr_type)
				{
				}

				void move_next() DENSITY_NOEXCEPT
				{
					auto const prev_element_ptr = curr_element();
					auto const curr_element_size = m_curr_type->size();
					m_curr_type++;
					m_unaligned_curr_element = address_add(prev_element_ptr, curr_element_size);
				}

				void * curr_element() const DENSITY_NOEXCEPT
				{
					auto const curr_element_alignment = m_curr_type->alignment();
					return address_upper_align(m_unaligned_curr_element, curr_element_alignment);
				}

				bool operator == (const IteratorBaseImpl & i_other) const DENSITY_NOEXCEPT
				{
					return m_curr_type == i_other.m_curr_type;
				}

				bool operator != (const IteratorBaseImpl & i_other) const DENSITY_NOEXCEPT
				{
					return m_curr_type != i_other.m_curr_type;
				}

				void operator ++ () DENSITY_NOEXCEPT
				{
					move_next();
				}

				void * m_unaligned_curr_element;
				const RUNTIME_TYPE * m_curr_type;

			}; // class IteratorBaseImpl

			IteratorBaseImpl begin() const DENSITY_NOEXCEPT { return IteratorBaseImpl(get_elements(), m_types); }
			IteratorBaseImpl end() const DENSITY_NOEXCEPT { return IteratorBaseImpl(nullptr, m_types + size()); }

			void * get_elements() const // this function gives a non-const element from a const container, but this avoids duplicating the function
			{
				return m_types + size();
			}

			size_t get_size_not_empty() const DENSITY_NOEXCEPT
			{
				if (m_types != nullptr)
				{
					return (reinterpret_cast<Header*>(m_types) - 1)->m_count;
				}
				else
				{
					return 0;
				}
			}

			struct Header
			{
				size_t m_count;
			};

			struct ListBuilder
			{
				ListBuilder() DENSITY_NOEXCEPT
					: m_element_infos(nullptr)
				{
				}

				ListBuilder(const ListBuilder&) = delete;
				ListBuilder & operator = (const ListBuilder&) = delete;

				void init(ALLOCATOR & i_allocator, size_t i_count, size_t i_buffer_size, size_t i_buffer_alignment)
				{
					void * const memory_block = aligned_alloc(i_allocator, i_buffer_size + sizeof(Header), i_buffer_alignment, sizeof(Header));
					Header * header = static_cast<Header*>(memory_block);
					header->m_count = i_count;
					m_end_of_types = m_element_infos = reinterpret_cast<RUNTIME_TYPE*>(header + 1);
					m_end_of_elements = m_elements = m_element_infos + i_count;

#if DENSITY_DENSE_LIST_DEBUG
					m_dbg_end_of_buffer = address_add(m_element_infos, i_buffer_size);
#endif
				}

				/* Adds a (type-info, element) pair to the list. The new element is copy-constructed.
				Note: ELEMENT is not the comlete type of the element, as the
				list allows polymorphic types. The use of the RUNTIME_TYPE avoid slicing or partial constructions. */
				void * add_by_copy(const RUNTIME_TYPE & i_element_info, const void * i_source)
					// DENSITY_NOEXCEPT_V(std::declval<RUNTIME_TYPE>().copy_construct(nullptr, std::declval<ELEMENT>() ))
				{
					// copy-construct the element first (this may throw)
					void * new_element = address_upper_align(m_end_of_elements, i_element_info.alignment());
#if DENSITY_DENSE_LIST_DEBUG
					dbg_check_range(new_element, address_add(new_element, i_element_info.size()));
#endif
					i_element_info.copy_construct(new_element, i_source);
					// from now on, for the whole function, we cant except
					m_end_of_elements = address_add(new_element, i_element_info.size());

					// construct the typeinfo - if this would throw, the element just constructed would not be destroyed. A static_assert guarantees the noexcept-ness.
#if DENSITY_DENSE_LIST_DEBUG
					dbg_check_range(m_end_of_types, m_end_of_types + 1);
#endif
					DENSITY_ASSERT_NOEXCEPT(new(m_end_of_types++) RUNTIME_TYPE(i_element_info));
					new(m_end_of_types++) RUNTIME_TYPE(i_element_info);
					return new_element;
				}

				/* Adds a (element-info, element) pair to the list. The new element is move-constructed.
				Note: ELEMENT is not the complete type of the element, as the
				list allows polymorphic types. The use of the RUNTIME_TYPE avoid slicing or partial constructions. */
				void * add_by_move(const RUNTIME_TYPE & i_element_info, void * i_source)
				{
					// move-construct the element first (this may throw)
					void * new_element = address_upper_align(m_end_of_elements, i_element_info.alignment());
#if DENSITY_DENSE_LIST_DEBUG
					dbg_check_range(new_element, address_add(new_element, i_element_info.size()));
#endif
					i_element_info.move_construct_nothrow(new_element, i_source);
					// from now on, for the whole function, we cant except
					m_end_of_elements = address_add(new_element, i_element_info.size());

					// construct the typeinfo - if this would throw, the element just constructed would not be destroyed. A static_assert guarantees the noexcept-ness.
#if DENSITY_DENSE_LIST_DEBUG
					dbg_check_range(m_end_of_types, m_end_of_types + 1);
#endif
					DENSITY_ASSERT_NOEXCEPT(new(m_end_of_types++) RUNTIME_TYPE(i_element_info));
					new(m_end_of_types++) RUNTIME_TYPE(i_element_info);
					return new_element;
				}

				void add_only_element_info(const RUNTIME_TYPE & i_element_info) DENSITY_NOEXCEPT
				{
					DENSITY_ASSERT_NOEXCEPT(new(m_end_of_types++) RUNTIME_TYPE(i_element_info));
#if DENSITY_DENSE_LIST_DEBUG
					dbg_check_range(m_end_of_types, m_end_of_types + 1);
#endif
					new(m_end_of_types++) RUNTIME_TYPE(i_element_info);
				}

				RUNTIME_TYPE * end_of_types()
				{
					return m_end_of_types;
				}

				RUNTIME_TYPE * commit()
				{
					return m_element_infos;
				}

				void rollback(ALLOCATOR & i_allocator, size_t i_buffer_size, size_t i_buffer_alignment) DENSITY_NOEXCEPT
				{
					if (m_element_infos != nullptr)
					{
						void * element = m_elements;
						for (RUNTIME_TYPE * element_info = m_element_infos; element_info < m_end_of_types; element_info++)
						{
							element = address_upper_align(element, element_info->alignment());
							element_info->destroy(element);
							element = address_add(element, element_info->size());
							element_info->~RUNTIME_TYPE();
						}
						aligned_free(i_allocator, reinterpret_cast<Header*>(m_element_infos) - 1, i_buffer_size, i_buffer_alignment);
					}
				}

#if DENSITY_DENSE_LIST_DEBUG
				void dbg_check_range(const void * i_start, const void * i_end) DENSITY_NOEXCEPT
				{
					assert(i_start >= m_element_infos && i_end <= m_dbg_end_of_buffer);
				}
#endif

				RUNTIME_TYPE * m_element_infos;
				void * m_elements;
				RUNTIME_TYPE * m_end_of_types;
				void * m_end_of_elements;
#if DENSITY_DENSE_LIST_DEBUG
				void * m_dbg_end_of_buffer;
#endif
			};

			void destroy_impl() DENSITY_NOEXCEPT
			{
				if (m_types != nullptr)
				{
					size_t dense_alignment = std::alignment_of<RUNTIME_TYPE>::value;
					const auto end_it = end();
					const size_t dense_size = get_size_not_empty() * sizeof(RUNTIME_TYPE);
					for (auto it = begin(); it != end_it; ++it)
					{
						auto curr_type = it.m_curr_type;
						dense_alignment = detail::size_max(dense_alignment, curr_type->alignment());
						curr_type->destroy(it.curr_element());
						curr_type->RUNTIME_TYPE::~RUNTIME_TYPE();
					}

					Header * const header = reinterpret_cast<Header*>(m_types) - 1;
					aligned_free(*static_cast<ALLOCATOR*>(this), header, dense_size, dense_alignment);
				}
			}

			void copy_impl(const DenseListImpl & i_source)
			{
				if (i_source.m_types != nullptr)
				{
					ListBuilder builder;
					size_t buffer_size = 0, buffer_alignment = 0;
					i_source.compute_buffer_size_and_alignment(&buffer_size, &buffer_alignment);
					try
					{
						const auto source_size = i_source.get_size_not_empty();
						builder.init(*static_cast<ALLOCATOR*>(this), source_size, buffer_size, buffer_alignment);
						auto const end_it = i_source.end();
						for (auto it = i_source.begin(); it != end_it; ++it)
						{
							builder.add_by_copy(*it.m_curr_type, it.curr_element());
						}

						m_types = builder.commit();
					}
					catch (...)
					{
						builder.rollback(*static_cast<ALLOCATOR*>(this), buffer_size, buffer_alignment);
						throw;
					}
				}
				else
				{
					m_types = nullptr;
				}
			}

			void move_impl(DenseListImpl && i_source) DENSITY_NOEXCEPT
			{
				m_types = i_source.m_types;
				i_source.m_types = nullptr;
			}

			template <typename... TYPES>
			static void make_impl(DenseListImpl & o_dest_list, TYPES &&... i_args)
			{
				assert(o_dest_list.m_types == nullptr); // precondition

				size_t const buffer_size = RecursiveSize<RecursiveHelper<TYPES...>::s_element_count * sizeof(RUNTIME_TYPE), TYPES...>::s_buffer_size;
				size_t const buffer_alignment = detail::size_max(RecursiveHelper<TYPES...>::s_element_alignment, std::alignment_of<RUNTIME_TYPE>::value);
				size_t const element_count = RecursiveHelper<TYPES...>::s_element_count;

				bool is_empty = element_count == 0;
				if (!is_empty) // this bool avoids 'warning C4127: conditional expression is constant' in visual studio
				{
					ListBuilder builder;
					try
					{
						builder.init(static_cast<ALLOCATOR&>(o_dest_list), element_count, buffer_size, buffer_alignment);

						RecursiveHelper<TYPES...>::construct(builder, std::forward<TYPES>(i_args)...);

						o_dest_list.m_types = builder.commit();
					}
					catch (...)
					{
						builder.rollback(static_cast<ALLOCATOR&>(o_dest_list), buffer_size, buffer_alignment);
						throw;
					}
				}

#ifndef NDEBUG
				size_t dbg_buffer_size = 0, dbg_buffer_alignment = 0;
				o_dest_list.compute_buffer_size_and_alignment(&dbg_buffer_size, &dbg_buffer_alignment);
				assert(dbg_buffer_size == buffer_size);
				assert(dbg_buffer_alignment == buffer_alignment);
#endif
			}

			void compute_buffer_size_and_alignment(size_t * o_buffer_size, size_t * o_buffer_alignment) const DENSITY_NOEXCEPT
			{
				size_t buffer_size = size() * sizeof(RUNTIME_TYPE);
				size_t buffer_alignment = std::alignment_of<RUNTIME_TYPE>::value;
				auto const end_it = end();
				for (auto it = begin(); it != end_it; ++it)
				{
					const size_t curr_size = it.m_curr_type->size();
					const size_t curr_alignment = it.m_curr_type->alignment();
					assert(curr_size > 0 && is_power_of_2(curr_alignment));
					buffer_size = (buffer_size + (curr_alignment - 1)) & ~(curr_alignment - 1);
					buffer_size += curr_size;

					buffer_alignment = detail::size_max(buffer_alignment, curr_alignment);
				}

				*o_buffer_size = buffer_size;
				*o_buffer_alignment = buffer_alignment;
			}

			void compute_buffer_size_and_alignment_for_insert(size_t * o_buffer_size, size_t * o_buffer_alignment,
				const RUNTIME_TYPE * i_insert_at, size_t i_new_element_count, const RUNTIME_TYPE & i_new_type) const DENSITY_NOEXCEPT
			{
				assert(i_new_type.size() > 0 && is_power_of_2(i_new_type.alignment())); // the size must be non-zero, the alignment must be a non-zero power of 2

				size_t buffer_size = (size() + i_new_element_count) * sizeof(RUNTIME_TYPE);
				size_t buffer_alignment = detail::size_max(std::alignment_of<RUNTIME_TYPE>::value, i_new_type.alignment());
				auto const end_it = end();
				for (auto it = begin(); ; ++it)
				{
					if (it.m_curr_type == i_insert_at && i_new_element_count > 0)
					{
						const auto alignment_mask = i_new_type.alignment() - 1;
						buffer_size = (buffer_size + alignment_mask) & ~alignment_mask;
						buffer_size += i_new_type.size() * i_new_element_count;
					}

					if (it == end_it)
					{
						break;
					}

					const size_t curr_size = it.m_curr_type->size();
					const size_t curr_alignment = it.m_curr_type->alignment();
					assert(curr_size > 0 && is_power_of_2(curr_alignment)); // the size must be non-zero, the alignment must be a non-zero power of 2
					buffer_size = (buffer_size + (curr_alignment - 1)) & ~(curr_alignment - 1);
					buffer_size += curr_size;
					buffer_alignment = detail::size_max(buffer_alignment, curr_alignment);
				}

				*o_buffer_size = buffer_size;
				*o_buffer_alignment = buffer_alignment;
			}

			template <typename CONSTRUCTOR>
			IteratorBaseImpl insert_impl(const RUNTIME_TYPE * i_position,
				const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
			{
				return insert_n_impl(i_position, 1, i_source_type, std::forward<CONSTRUCTOR>(i_constructor));
			}

			template <typename CONSTRUCTOR>
			IteratorBaseImpl insert_n_impl(const RUNTIME_TYPE * i_position, size_t i_count_to_insert,
				const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
			{
				assert(i_count_to_insert > 0);

				const RUNTIME_TYPE * return_element_info = nullptr;
				void * return_element = nullptr;

				size_t buffer_size = 0, buffer_alignment = 0;
				compute_buffer_size_and_alignment_for_insert(&buffer_size, &buffer_alignment, i_position, i_count_to_insert, i_source_type);

				ListBuilder builder;
				try
				{
					builder.init(*static_cast<ALLOCATOR*>(this), size() + i_count_to_insert, buffer_size, buffer_alignment);

					size_t count_to_insert = i_count_to_insert;
					auto const end_it = end();
					for (auto it = begin();;)
					{
						if (it.m_curr_type == i_position && count_to_insert > 0)
						{
							auto const end_of_types = builder.end_of_types();
							void * const new_element = i_constructor(builder, i_source_type);
							if (count_to_insert == i_count_to_insert)
							{
								return_element_info = end_of_types;
								return_element = new_element;
							}
							count_to_insert--;
						}
						else
						{
							if (it == end_it)
							{
								break;
							}
							builder.add_by_move(*it.m_curr_type, it.curr_element());
							it.move_next();
						}
					}

					destroy_impl();

					m_types = builder.commit();
				}
				catch (...)
				{
					builder.rollback(*static_cast<ALLOCATOR*>(this), buffer_size, buffer_alignment);
					throw;
				}

				return IteratorBaseImpl(return_element, return_element_info);
			}

			IteratorBaseImpl erase_impl(const RUNTIME_TYPE * i_from, const RUNTIME_TYPE * i_to)
			{
				// test preconditions
				const auto prev_size = get_size_not_empty();
				assert(m_types != nullptr && i_from < i_to &&
					i_from >= m_types && i_from <= m_types + prev_size &&
					i_to >= m_types && i_to <= m_types + prev_size);

				const size_t size_to_remove = i_to - i_from;

				assert(size_to_remove <= prev_size);
				if (size_to_remove == prev_size)
				{
					// erasing all the elements
					assert(i_from == m_types && i_to == m_types + prev_size);
					clear();
					return begin();
				}
				else
				{
					size_t buffer_size = 0, buffer_alignment = 0;
					compute_buffer_size_and_alignment_for_erase(&buffer_size, &buffer_alignment, i_from, i_to);

					const RUNTIME_TYPE * return_element_info = nullptr;
					void * return_element = nullptr;

					ListBuilder builder;
					try
					{
						builder.init(*static_cast<ALLOCATOR*>(this), prev_size - size_to_remove, buffer_size, buffer_alignment);

						const auto end_it = end();
						bool is_in_range = false;
						bool first_in_range = false;
						for (auto it = begin(); ; ++it)
						{
							if (it.m_curr_type == i_from)
							{
								is_in_range = true;
								first_in_range = true;
							}
							if (it.m_curr_type == i_to)
							{
								is_in_range = false;
							}

							if (it == end_it)
							{
								assert(!is_in_range);
								break;
							}

							if (!is_in_range)
							{
								auto const new_element_info = builder.end_of_types();
								auto const new_element = builder.add_by_move(*it.m_curr_type, it.curr_element());

								if (first_in_range)
								{
									return_element_info = new_element_info;
									return_element = new_element;
									first_in_range = false;
								}
							}
						}

						if (return_element_info == nullptr) // if no elements were copied after the erased range
						{
							assert(i_to == m_types + prev_size);
							return_element_info = builder.end_of_types();
						}

						destroy_impl();

						m_types = builder.commit();
					}
					catch (...)
					{
						builder.rollback(*static_cast<ALLOCATOR*>(this), buffer_size, buffer_alignment);
						throw;
					}
					return IteratorBaseImpl(return_element, return_element_info);
				}
			}

			void compute_buffer_size_and_alignment_for_erase(size_t * o_buffer_size, size_t * o_buffer_alignment,
				const RUNTIME_TYPE * i_remove_from, const RUNTIME_TYPE * i_remove_to) const DENSITY_NOEXCEPT
			{
				assert(i_remove_to >= i_remove_from);
				const size_t size_to_remove = i_remove_to - i_remove_from;
				assert(size() >= size_to_remove);
				size_t buffer_size = (size() - size_to_remove) * sizeof(RUNTIME_TYPE);
				size_t buffer_alignment = std::alignment_of<RUNTIME_TYPE>::value;

				bool in_range = false;
				auto const end_it = end();
				for (auto it = begin(); it != end_it; ++it)
				{
					if (it.m_curr_type == i_remove_from)
					{
						in_range = true;
					}
					if (it.m_curr_type == i_remove_to)
					{
						in_range = false;
					}

					if (!in_range)
					{
						const size_t curr_size = it.m_curr_type->size();
						const size_t curr_alignment = it.m_curr_type->alignment();
						assert(curr_size > 0 && is_power_of_2(curr_alignment)); // the size must be non-zero, the alignment must be a non-zero power of 2
						buffer_size = (buffer_size + (curr_alignment - 1)) & ~(curr_alignment - 1);
						buffer_size += curr_size;
						buffer_alignment = detail::size_max(buffer_alignment, curr_alignment);
					}
				}

				*o_buffer_size = buffer_size;
				*o_buffer_alignment = buffer_alignment;
			}

			template <size_t PREV_ELEMENTS_SIZE, typename...> struct RecursiveSize
			{
				static const size_t s_buffer_size = PREV_ELEMENTS_SIZE;
			};
			template <size_t PREV_ELEMENTS_SIZE, typename FIRST_TYPE, typename... OTHER_TYPES>
			struct RecursiveSize<PREV_ELEMENTS_SIZE, FIRST_TYPE, OTHER_TYPES...>
			{
				static const size_t s_aligned_prev_size = (PREV_ELEMENTS_SIZE + (std::alignment_of<FIRST_TYPE>::value - 1)) & ~(std::alignment_of<FIRST_TYPE>::value - 1);
				static const size_t s_buffer_size = RecursiveSize<s_aligned_prev_size + sizeof(FIRST_TYPE), OTHER_TYPES...>::s_buffer_size;
			};

			template <typename... TYPES>
			struct RecursiveHelper
			{
				static const size_t s_element_count = 0;
				static const size_t s_element_alignment = 1;
				static void construct(ListBuilder &, TYPES &&...) { }
			};
			template <typename FIRST_TYPE, typename... OTHER_TYPES>
			struct RecursiveHelper<FIRST_TYPE, OTHER_TYPES...>
			{
				static_assert(std::alignment_of<FIRST_TYPE>::value > 0 && (std::alignment_of<FIRST_TYPE>::value & (std::alignment_of<FIRST_TYPE>::value - 1)) == 0,
					"the alignment must be a non-zero integer power of 2");

				static const size_t s_element_count = 1 + RecursiveHelper<OTHER_TYPES...>::s_element_count;
				static const size_t s_element_alignment = std::alignment_of<FIRST_TYPE>::value > RecursiveHelper<OTHER_TYPES...>::s_element_alignment ?
					std::alignment_of<FIRST_TYPE>::value : RecursiveHelper<OTHER_TYPES...>::s_element_alignment;

				inline static void construct(ListBuilder & i_builder, FIRST_TYPE && i_source, OTHER_TYPES && ... i_other_arguments)
					// DENSITY_NOEXCEPT_V( new (nullptr) FIRST_TYPE(std::forward<FIRST_TYPE>(std::declval<FIRST_TYPE>())) )
				{
					void * new_element = address_upper_align(i_builder.m_end_of_elements, std::alignment_of<FIRST_TYPE>::value);
					new (new_element) FIRST_TYPE(std::forward<FIRST_TYPE>(i_source));
					i_builder.m_end_of_elements = address_add(new_element, sizeof(FIRST_TYPE));
#if DENSITY_DENSE_LIST_DEBUG
					i_builder.dbg_check_range(new_element, i_builder.m_end_of_elements);
#endif

					i_builder.add_only_element_info(RUNTIME_TYPE::template make<FIRST_TYPE>());

					RecursiveHelper<OTHER_TYPES...>::construct(i_builder, std::forward<OTHER_TYPES>(i_other_arguments)...);
				}
			};

			RUNTIME_TYPE * m_types;
		};

	} // namespace detail

} // namespace density

#undef DENSITY_DENSE_LIST_DEBUG

#include "detail\dense_list_typed.h"
#include "detail\dense_list_void.h"

