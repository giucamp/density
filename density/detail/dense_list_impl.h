
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <memory>
#include "../runtime_type.h"

namespace density
{
    namespace detail
    {
        template < typename ALLOCATOR, typename RUNTIME_TYPE >
            class DenseListImpl : private ALLOCATOR
        {
		private:

			#if DENSITY_DEBUG_INTERNAL
				void check_invariants() const
				{				
					if (m_control_blocks != nullptr)
					{
						Header * const header = reinterpret_cast<Header*>(m_control_blocks) - 1;
						DENSITY_ASSERT_INTERNAL(header->m_count > 0);
					}
				}
			#endif

        public:

			struct ControlBlock : RUNTIME_TYPE
			{
				ControlBlock(const RUNTIME_TYPE & i_type, void * i_element) DENSITY_NOEXCEPT
					: RUNTIME_TYPE(i_type), m_element(i_element) { }

				void * const m_element;
			};

            size_t size() const DENSITY_NOEXCEPT
            {
				#if DENSITY_DEBUG_INTERNAL
					check_invariants();
				#endif		

                if (m_control_blocks != nullptr)
                {
                    Header * const header = reinterpret_cast<Header*>(m_control_blocks) - 1;
                    return header->m_count;
                }
                else
                {
                    return 0;
                }
            }

            bool empty() const DENSITY_NOEXCEPT
            {
				#if DENSITY_DEBUG_INTERNAL
					check_invariants();
				#endif		
                return m_control_blocks == nullptr;
            }

            void clear() DENSITY_NOEXCEPT
            {	
				#if DENSITY_DEBUG_INTERNAL
					check_invariants();
				#endif	
                destroy_impl();
                m_control_blocks = nullptr;
            }

            DenseListImpl() DENSITY_NOEXCEPT
                : m_control_blocks(nullptr)
                    { }

            DenseListImpl(DenseListImpl && i_source) DENSITY_NOEXCEPT
            {
				#if DENSITY_DEBUG_INTERNAL
					i_source.check_invariants();
				#endif	
                move_impl(std::move(i_source));
            }

            DenseListImpl & operator = (DenseListImpl && i_source) DENSITY_NOEXCEPT
            {
				DENSITY_ASSERT(this != &i_source); // self assignment not supported

				#if DENSITY_DEBUG_INTERNAL
					this->check_invariants();
					i_source.check_invariants();
				#endif	
                
                destroy_impl();
                move_impl(std::move(i_source));
                return *this;
            }

            DenseListImpl(const DenseListImpl & i_source)
            {
				#if DENSITY_DEBUG_INTERNAL
					i_source.check_invariants();
				#endif	
                copy_impl(i_source);
            }

            DenseListImpl & operator = (const DenseListImpl & i_source)
            {
                DENSITY_ASSERT(this != &i_source); // self assignment not supported

				#if DENSITY_DEBUG_INTERNAL
					i_source.check_invariants();
				#endif	

				// use a copy to provide the strong exception guarantee
				auto copy(*this);
				destroy_impl();                
				move_impl(std::move(copy));
				return *this;
            }

            ~DenseListImpl() DENSITY_NOEXCEPT
            {
				#if DENSITY_DEBUG_INTERNAL
					check_invariants();
				#endif	

                destroy_impl();
            }

            struct IteratorBaseImpl
            {
                IteratorBaseImpl() DENSITY_NOEXCEPT { }

                IteratorBaseImpl(const ControlBlock * i_curr_control_block) DENSITY_NOEXCEPT
                    : m_curr_control_block(i_curr_control_block)
						{ }

                void move_next() DENSITY_NOEXCEPT
                {
					m_curr_control_block++;
                }

                void * element() const DENSITY_NOEXCEPT
                {
                    return m_curr_control_block->m_element;
                }

				const RUNTIME_TYPE & complete_type() const DENSITY_NOEXCEPT
				{
					return *m_curr_control_block;
				}

				const ControlBlock * control() const DENSITY_NOEXCEPT
				{
					return m_curr_control_block;
				}

                bool operator == (const IteratorBaseImpl & i_other) const DENSITY_NOEXCEPT
                {
                    return m_curr_control_block == i_other.m_curr_control_block;
                }

                bool operator != (const IteratorBaseImpl & i_other) const DENSITY_NOEXCEPT
                {
                    return m_curr_control_block != i_other.m_curr_control_block;
                }

                void operator ++ () DENSITY_NOEXCEPT
                {
					m_curr_control_block++;
                }

			private:
                const ControlBlock * m_curr_control_block;

            }; // class IteratorBaseImpl

            IteratorBaseImpl begin() const DENSITY_NOEXCEPT { return IteratorBaseImpl(m_control_blocks); }
            IteratorBaseImpl end() const DENSITY_NOEXCEPT { return IteratorBaseImpl(m_control_blocks + size()); }
			
            size_t get_size_not_empty() const DENSITY_NOEXCEPT
            {
				return (reinterpret_cast<Header*>(m_control_blocks) - 1)->m_count;
            }

            struct Header
            {
                size_t m_count;
            };

            struct ListBuilder
            {
                ListBuilder() DENSITY_NOEXCEPT
                    : m_control_blocks(nullptr)
                {
                }

                ListBuilder(const ListBuilder&) = delete;
                ListBuilder & operator = (const ListBuilder&) = delete;

                void init(ALLOCATOR & i_allocator, size_t i_count, size_t i_buffer_size, size_t i_buffer_alignment)
                {
                    void * const memory_block = aligned_alloc(i_allocator, i_buffer_size + sizeof(Header), i_buffer_alignment, sizeof(Header));
                    Header * header = static_cast<Header*>(memory_block);
                    header->m_count = i_count;
                    m_end_of_control_blocks = m_control_blocks = reinterpret_cast<ControlBlock*>(header + 1);
                    m_end_of_elements = m_elements = m_control_blocks + i_count;

					#if DENSITY_DEBUG_INTERNAL
						m_dbg_end_of_buffer = address_add(m_control_blocks, i_buffer_size);
					#endif
                }

                /* Adds a (type-info, element) pair to the list. The new element is copy-constructed.
					Note: ELEMENT is not the complete type of the element, as the
					list allows polymorphic types. The use of the RUNTIME_TYPE avoid slicing or partial constructions. */
                void * add_by_copy(const RUNTIME_TYPE & i_element_info, const void * i_source)
                {
                    // copy-construct the element first (this may throw)
                    void * complete_new_element = address_upper_align(m_end_of_elements, i_element_info.alignment());
					#if DENSITY_DEBUG_INTERNAL
						dbg_check_range(complete_new_element, address_add(complete_new_element, i_element_info.size()));
					#endif
                    const auto element_base = i_element_info.copy_construct(complete_new_element, i_source);
                    // from now on, for the whole function, we cant except
                    m_end_of_elements = address_add(complete_new_element, i_element_info.size());

                    // construct the typeinfo - if this would throw, the element just constructed would not be destroyed. A static_assert guarantees the noexcept-ness.
					#if DENSITY_DEBUG_INTERNAL
						dbg_check_range(m_end_of_control_blocks, m_end_of_control_blocks + 1);
					#endif
                    DENSITY_ASSERT_NOEXCEPT(new(m_end_of_control_blocks++) ControlBlock(i_element_info, element_base));
                    new(m_end_of_control_blocks++) ControlBlock(i_element_info, element_base);
                    return element_base;
                }

                /* Adds a (element-info, element) pair to the list. The new element is move-constructed.
					Note: ELEMENT is not the complete type of the element, as the
					list allows polymorphic types. The use of the RUNTIME_TYPE avoid slicing or partial constructions. */
                void * add_by_move(const RUNTIME_TYPE & i_element_info, void * i_source)
                {
                    // copy-construct the element first (this may throw)
                    void * complete_new_element = address_upper_align(m_end_of_elements, i_element_info.alignment());
					#if DENSITY_DEBUG_INTERNAL
						dbg_check_range(complete_new_element, address_add(complete_new_element, i_element_info.size()));
					#endif
                    const auto element_base = i_element_info.move_construct_nothrow(complete_new_element, i_source);
                    // from now on, for the whole function, we cant except
                    m_end_of_elements = address_add(complete_new_element, i_element_info.size());

                    // construct the typeinfo - if this would throw, the element just constructed would not be destroyed. A static_assert guarantees the noexcept-ness.
					#if DENSITY_DEBUG_INTERNAL
						dbg_check_range(m_end_of_control_blocks, m_end_of_control_blocks + 1);
					#endif
                    DENSITY_ASSERT_NOEXCEPT(new(m_end_of_control_blocks++) ControlBlock(i_element_info, element_base));
                    new(m_end_of_control_blocks++) ControlBlock(i_element_info, element_base);
                    return element_base;
                }

                void add_only_control_block(const RUNTIME_TYPE & i_element_info, void * i_element) DENSITY_NOEXCEPT
                {
                    DENSITY_ASSERT_NOEXCEPT(new(m_end_of_control_blocks++) ControlBlock(i_element_info, i_element));
					#if DENSITY_DEBUG_INTERNAL
						dbg_check_range(m_end_of_control_blocks, m_end_of_control_blocks + 1);
					#endif
                    new(m_end_of_control_blocks++) ControlBlock(i_element_info, i_element);
                }

				ControlBlock * end_of_control_blocks()
                {
                    return m_end_of_control_blocks;
                }

				ControlBlock * control_blocks()
                {
                    return m_control_blocks;
                }

                void rollback(ALLOCATOR & i_allocator, size_t i_buffer_size, size_t i_buffer_alignment) DENSITY_NOEXCEPT
                {
                    if (m_control_blocks != nullptr)
                    {
                        void * element = m_elements;
                        for (ControlBlock * element_info = m_control_blocks; element_info < m_end_of_control_blocks; element_info++)
                        {
                            element = address_upper_align(element, element_info->alignment());
                            element_info->destroy(element);
                            element = address_add(element, element_info->size());
                            element_info->~ControlBlock();
                        }
                        aligned_free(i_allocator, reinterpret_cast<Header*>(m_control_blocks) - 1, i_buffer_size, i_buffer_alignment);
                    }
                }

				#if DENSITY_DEBUG_INTERNAL
					void dbg_check_range(const void * i_start, const void * i_end) DENSITY_NOEXCEPT
					{
						DENSITY_ASSERT_INTERNAL(i_start >= m_control_blocks && i_end <= m_dbg_end_of_buffer);
					}
				#endif

                ControlBlock * m_control_blocks;
                void * m_elements;
				ControlBlock * m_end_of_control_blocks;
                void * m_end_of_elements;
				#if DENSITY_DEBUG_INTERNAL
					void * m_dbg_end_of_buffer;
				#endif
            };

			ControlBlock * & edit_control_blocks() DENSITY_NOEXCEPT { return m_control_blocks; }

			ControlBlock * get_control_blocks() DENSITY_NOEXCEPT { return m_control_blocks; }
			const ControlBlock * get_control_blocks() const DENSITY_NOEXCEPT { return m_control_blocks; }

            void destroy_impl() DENSITY_NOEXCEPT
            {
                if (m_control_blocks != nullptr)
                {
                    size_t dense_alignment = std::alignment_of<ControlBlock>::value;
                    const auto end_it = end();
                    size_t dense_size = get_size_not_empty() * sizeof(ControlBlock) + sizeof(Header);
                    for (auto it = begin(); it != end_it; ++it)
                    {
                        auto control_block = it.control();
						dense_size += control_block->size();
						dense_alignment = detail::size_max(dense_alignment, control_block->alignment());
						control_block->destroy(it.element());
						control_block->ControlBlock::~ControlBlock();
                    }

                    Header * const header = reinterpret_cast<Header*>(m_control_blocks) - 1;
                    aligned_free(*static_cast<ALLOCATOR*>(this), header, dense_size, dense_alignment);
                }
            }

            void copy_impl(const DenseListImpl & i_source)
            {
                if (i_source.m_control_blocks != nullptr)
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
                            builder.add_by_copy(it.complete_type(), it.element());
                        }

                        m_control_blocks = builder.control_blocks();
                    }
                    catch (...)
                    {
                        builder.rollback(*static_cast<ALLOCATOR*>(this), buffer_size, buffer_alignment);
                        throw;
                    }
                }
                else
                {
                    m_control_blocks = nullptr;
                }
            }

            void move_impl(DenseListImpl && i_source) DENSITY_NOEXCEPT
            {
                m_control_blocks = i_source.m_control_blocks;
                i_source.m_control_blocks = nullptr;
            }

            template <typename ELEMENT, typename... TYPES>
				static void make_impl(DenseListImpl & o_dest_list, TYPES &&... i_args)
            {
                DENSITY_ASSERT(o_dest_list.m_control_blocks == nullptr); // precondition

                size_t const buffer_size = RecursiveSize<RecursiveHelper<ELEMENT, TYPES...>::s_element_count * sizeof(ControlBlock), TYPES...>::s_buffer_size;
                size_t const buffer_alignment = detail::size_max(RecursiveHelper<ELEMENT, TYPES...>::s_element_alignment, std::alignment_of<ControlBlock>::value);
                size_t const element_count = RecursiveHelper<ELEMENT, TYPES...>::s_element_count;

				bool not_empty = element_count != 0; // this avoids 'warning C4127: conditional expression is constant' on msvc
                if (not_empty)
                {
                    ListBuilder builder;
                    try
                    {
                        builder.init(static_cast<ALLOCATOR&>(o_dest_list), element_count, buffer_size, buffer_alignment);

                        RecursiveHelper<ELEMENT, TYPES...>::construct(builder, std::forward<TYPES>(i_args)...);

                        o_dest_list.m_control_blocks = builder.control_blocks();
                    }
                    catch (...)
                    {
                        builder.rollback(static_cast<ALLOCATOR&>(o_dest_list), buffer_size, buffer_alignment);
                        throw;
                    }
                }

				#if DENSITY_DEBUG_INTERNAL
					size_t dbg_buffer_size = 0, dbg_buffer_alignment = 0;
					o_dest_list.compute_buffer_size_and_alignment(&dbg_buffer_size, &dbg_buffer_alignment);
					DENSITY_ASSERT_INTERNAL(dbg_buffer_size == buffer_size);
					DENSITY_ASSERT_INTERNAL(dbg_buffer_alignment == buffer_alignment);
				#endif
            }

            void compute_buffer_size_and_alignment(size_t * o_buffer_size, size_t * o_buffer_alignment) const DENSITY_NOEXCEPT
            {
                size_t buffer_size = size() * sizeof(ControlBlock);
                size_t buffer_alignment = std::alignment_of<ControlBlock>::value;
                auto const end_it = end();
                for (auto it = begin(); it != end_it; ++it)
                {
                    const size_t curr_size = it.complete_type().size();
                    const size_t curr_alignment = it.complete_type().alignment();
					DENSITY_ASSERT(curr_size > 0 && is_power_of_2(curr_alignment));
                    buffer_size = (buffer_size + (curr_alignment - 1)) & ~(curr_alignment - 1);
                    buffer_size += curr_size;

                    buffer_alignment = detail::size_max(buffer_alignment, curr_alignment);
                }

                *o_buffer_size = buffer_size;
                *o_buffer_alignment = buffer_alignment;
            }

            void compute_buffer_size_and_alignment_for_insert(size_t * o_buffer_size, size_t * o_buffer_alignment,
                const ControlBlock * i_insert_at, size_t i_new_element_count, const RUNTIME_TYPE & i_new_type) const DENSITY_NOEXCEPT
            {
                DENSITY_ASSERT(i_new_type.size() > 0 && is_power_of_2(i_new_type.alignment())); // the size must be non-zero, the alignment must be a non-zero power of 2

                size_t buffer_size = (size() + i_new_element_count) * sizeof(ControlBlock);
                size_t buffer_alignment = detail::size_max(std::alignment_of<ControlBlock>::value, i_new_type.alignment());
                auto const end_it = end();
                for (auto it = begin(); ; ++it)
                {
                    if (it.control() == i_insert_at && i_new_element_count > 0)
                    {
                        const auto alignment_mask = i_new_type.alignment() - 1;
                        buffer_size = (buffer_size + alignment_mask) & ~alignment_mask;
                        buffer_size += i_new_type.size() * i_new_element_count;
                    }

                    if (it == end_it)
                    {
                        break;
                    }

                    const size_t curr_size = it.complete_type().size();
                    const size_t curr_alignment = it.complete_type().alignment();
                    DENSITY_ASSERT(curr_size > 0 && is_power_of_2(curr_alignment)); // the size must be non-zero, the alignment must be a non-zero power of 2
                    buffer_size = (buffer_size + (curr_alignment - 1)) & ~(curr_alignment - 1);
                    buffer_size += curr_size;
                    buffer_alignment = detail::size_max(buffer_alignment, curr_alignment);
                }

                *o_buffer_size = buffer_size;
                *o_buffer_alignment = buffer_alignment;
            }
			
            template <typename CONSTRUCTOR>
				IteratorBaseImpl insert_n_impl(const ControlBlock * i_position, size_t i_count_to_insert,
					const RUNTIME_TYPE & i_source_type, CONSTRUCTOR && i_constructor)
            {
                DENSITY_ASSERT(i_count_to_insert > 0);

                const ControlBlock * return_control_box = nullptr;
                
                size_t buffer_size = 0, buffer_alignment = 0;
                compute_buffer_size_and_alignment_for_insert(&buffer_size, &buffer_alignment, i_position, i_count_to_insert, i_source_type);

                ListBuilder builder;
				IteratorBaseImpl it = begin();
				try
                {
                    builder.init(*static_cast<ALLOCATOR*>(this), size() + i_count_to_insert, buffer_size, buffer_alignment);

                    size_t count_to_insert = i_count_to_insert;
                    auto const end_it = end();
                    for (;;)
                    {
                        if (it.control() == i_position && count_to_insert > 0)
                        {
                            auto const end_of_control_blocks = builder.end_of_control_blocks();
                            i_constructor(builder, i_source_type);
                            if (count_to_insert == i_count_to_insert)
                            {
								return_control_box = end_of_control_blocks;
                            }
                            count_to_insert--;
                        }
                        else
                        {
                            if (it == end_it)
                            {
                                break;
                            }
                            builder.add_by_move(it.complete_type(), it.element());
                            it.move_next();
                        }
                    }

                    destroy_impl();

                    m_control_blocks = builder.control_blocks();

					return IteratorBaseImpl(return_control_box);
                }
                catch (...)
                {
					/* we iterate this list exactly like we did in the loop interrupted by the exception,
						but we stop at 'it', the iterator we were using */
					size_t count_to_insert = i_count_to_insert;
					IteratorBaseImpl this_it = begin();

					/* in the same loop we iterate over tmp, that is the list that we was creating. The 
						elements that were moved from this list to tmp have to be moved back to this list. The elements
						that was just constructed have to be destroyed. */
					DenseListImpl tmp;
					tmp.m_control_blocks = builder.control_blocks();
					if (tmp.m_control_blocks != nullptr) // if the allocation fails builder.control_blocks() is null
					{
						auto tmp_it = tmp.begin();
						auto tmp_end = builder.end_of_control_blocks();

						for (; tmp_it != tmp_end; tmp_it.move_next())
						{
							if (this_it.control() == i_position && count_to_insert > 0)
							{
								tmp_it.complete_type().destroy(tmp_it.element());
								count_to_insert--;
							}
							else
							{
								tmp_it.complete_type().move_construct_nothrow(this_it.element(), tmp_it.element());
								this_it.move_next();
							}
						}

						Header * const header = reinterpret_cast<Header*>(tmp.m_control_blocks) - 1;
						aligned_free(*static_cast<ALLOCATOR*>(this), header, buffer_size, buffer_alignment);
						tmp.m_control_blocks = nullptr;
					}
                    throw;
                }               
            }

            IteratorBaseImpl erase_impl(const ControlBlock * i_from, const ControlBlock * i_to)
            {
                // test preconditions
                const auto prev_size = get_size_not_empty();
                DENSITY_ASSERT(m_control_blocks != nullptr && i_from < i_to &&
                    i_from >= m_control_blocks && i_from <= m_control_blocks + prev_size &&
                    i_to >= m_control_blocks && i_to <= m_control_blocks + prev_size);

                const size_t size_to_remove = i_to - i_from;

                DENSITY_ASSERT(size_to_remove <= prev_size);
                if (size_to_remove == prev_size)
                {
                    // erasing all the elements
                    DENSITY_ASSERT(i_from == m_control_blocks && i_to == m_control_blocks + prev_size);
                    clear();
                    return begin();
                }
                else
                {
                    size_t buffer_size = 0, buffer_alignment = 0;
                    compute_buffer_size_and_alignment_for_erase(&buffer_size, &buffer_alignment, i_from, i_to);

                    ControlBlock * return_control_block = nullptr;

                    ListBuilder builder;
                    builder.init(*static_cast<ALLOCATOR*>(this), prev_size - size_to_remove, buffer_size, buffer_alignment);

                    const auto end_it = end();
                    bool is_in_range = false;
                    bool first_in_range = false;
                    for (auto it = begin(); ; ++it)
                    {
                        if (it.control() == i_from)
                        {
                            is_in_range = true;
                            first_in_range = true;
                        }
                        if (it.control() == i_to)
                        {
                            is_in_range = false;
                        }

                        if (it == end_it)
                        {
                            DENSITY_ASSERT(!is_in_range);
                            break;
                        }

                        if (!is_in_range)
                        {
                            auto const new_element_info = builder.end_of_control_blocks();
							builder.add_by_move(it.complete_type(), it.element());

                            if (first_in_range)
                            {
								return_control_block = new_element_info;
                                first_in_range = false;
                            }
                        }
                    }

                    if (return_control_block == nullptr) // if no elements were copied after the erased range
                    {
                        DENSITY_ASSERT(i_to == m_control_blocks + prev_size);
						return_control_block = builder.end_of_control_blocks();
                    }

                    destroy_impl();

                    m_control_blocks = builder.control_blocks();
                    return IteratorBaseImpl(return_control_block);
                }
            }

            void compute_buffer_size_and_alignment_for_erase(size_t * o_buffer_size, size_t * o_buffer_alignment,
                const ControlBlock * i_remove_from, const ControlBlock * i_remove_to) const DENSITY_NOEXCEPT
            {
                DENSITY_ASSERT(i_remove_to >= i_remove_from);
                const size_t size_to_remove = i_remove_to - i_remove_from;
                DENSITY_ASSERT(size() >= size_to_remove);
                size_t buffer_size = (size() - size_to_remove) * sizeof(ControlBlock);
                size_t buffer_alignment = std::alignment_of<ControlBlock>::value;

                bool in_range = false;
                auto const end_it = end();
                for (auto it = begin(); it != end_it; ++it)
                {
                    if (it.control() == i_remove_from)
                    {
                        in_range = true;
                    }
                    if (it.control() == i_remove_to)
                    {
                        in_range = false;
                    }

                    if (!in_range)
                    {
                        const size_t curr_size = it.complete_type().size();
                        const size_t curr_alignment = it.complete_type().alignment();
                        DENSITY_ASSERT(curr_size > 0 && is_power_of_2(curr_alignment)); // the size must be non-zero, the alignment must be a non-zero power of 2
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

            template <typename ELEMENT, typename... TYPES>
				struct RecursiveHelper
            {
                static const size_t s_element_count = 0;
                static const size_t s_element_alignment = 1;
                static void construct(ListBuilder &, TYPES &&...) { }
            };
            template <typename ELEMENT, typename FIRST_TYPE, typename... OTHER_TYPES>
				struct RecursiveHelper<ELEMENT, FIRST_TYPE, OTHER_TYPES...>
            {
                static_assert(std::alignment_of<FIRST_TYPE>::value > 0 && (std::alignment_of<FIRST_TYPE>::value & (std::alignment_of<FIRST_TYPE>::value - 1)) == 0,
                    "the alignment must be a non-zero integer power of 2");

                static const size_t s_element_count = 1 + RecursiveHelper<ELEMENT, OTHER_TYPES...>::s_element_count;
                static const size_t s_element_alignment = std::alignment_of<FIRST_TYPE>::value > RecursiveHelper<ELEMENT, OTHER_TYPES...>::s_element_alignment ?
                    std::alignment_of<FIRST_TYPE>::value : RecursiveHelper<ELEMENT, OTHER_TYPES...>::s_element_alignment;

                inline static void construct(ListBuilder & i_builder, FIRST_TYPE && i_source, OTHER_TYPES && ... i_other_arguments)
                    // DENSITY_NOEXCEPT_IF( new (nullptr) FIRST_TYPE(std::forward<FIRST_TYPE>(std::declval<FIRST_TYPE>())) )
                {
                    void * new_element_complete = address_upper_align(i_builder.m_end_of_elements, std::alignment_of<FIRST_TYPE>::value);
					ELEMENT * new_element = new (new_element_complete) FIRST_TYPE(std::forward<FIRST_TYPE>(i_source));
                    i_builder.m_end_of_elements = address_add(new_element, sizeof(FIRST_TYPE));
					#if DENSITY_DEBUG_INTERNAL
						i_builder.dbg_check_range(new_element, i_builder.m_end_of_elements);
					#endif

                    i_builder.add_only_control_block(RUNTIME_TYPE::template make<FIRST_TYPE>(), new_element);

                    RecursiveHelper<ELEMENT, OTHER_TYPES...>::construct(i_builder, std::forward<OTHER_TYPES>(i_other_arguments)...);
                }
            };

			ControlBlock * m_control_blocks;
        };

    } // namespace detail

} // namespace density

