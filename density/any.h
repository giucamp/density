
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/void_allocator.h>
#include <density/runtime_type.h>

namespace density
{
	/** Class with value semantics that can contain an object of any type (similar to std::any)
	*/
    template <typename COMMON_TYPE = void, typename VOID_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>>
        class any : VOID_ALLOCATOR
    {
    public:

        using allocator_type = VOID_ALLOCATOR;
        using runtime_type = RUNTIME_TYPE;

		template <typename TYPE, typename ... CONSTRUCTION_ARGS>
            static any make(CONSTRUCTION_ARGS && ...i_args)
        {
			allocator_type allocator;
			auto const memory_block = allocator.allocate(sizeof(TYPE), alignof(TYPE));
			try
			{
				COMMON_TYPE * const object = new(memory_block) TYPE(std::forward<CONSTRUCTION_ARGS>(i_args)...);
				return any(
					object,
					RUNTIME_TYPE::template make<TYPE>(),
					std::move(allocator));
			}
			catch (...)
			{
				allocator.deallocate(memory_block, sizeof(TYPE), alignof(TYPE));
				throw;
			}
        }

		/**
			Throws: anything the default constructor of the allocator throws.
		*/
        any()
            : m_object(nullptr)
        {
        }

		/** Copy constructor */
        any(const any & i_source)
            : VOID_ALLOCATOR(static_cast<const VOID_ALLOCATOR&>(i_source)), m_type(i_source.m_type)
        {
			auto const size = i_source.m_type.size();
			auto const alignment = i_source.m_type.alignment();
			auto const memory_block = allocator_type::allocate(size, alignment);
			try
			{
				m_object = m_type.copy_construct(memory_block, i_source.m_object);
			}
			catch (...)
			{
				allocator_type::deallocate(memory_block, size, alignment);
				throw;
			}
        }

		any & operator = (const any & i_source)
		{
			// move to a local variable the old allocator, and copy-assign the new one
			VOID_ALLOCATOR old_allocator(std::move(internal_get_allocator_ref()));
			internal_get_allocator_ref() = static_cast<const VOID_ALLOCATOR&>(i_source);			

			// allocate the new block
			auto const new_size = i_source.m_type.size();
			auto const new_alignment = i_source.m_type.alignment();
			auto const new_memory_block = internal_get_allocator_ref().allocate(new_size, new_alignment);
			try
			{
				auto new_type(i_source.m_type); /* make a copy of the type. This may throw (actually it is unlikely),
													so we do it before doing any change to this object. We do it before
													constructing the object, otherwise the catch block has capture the
													local variable new_object. */
				auto const new_object = i_source.m_type.copy_construct(new_memory_block, i_source.m_object);

				// from now on, nothing can throw
				static_assert(noexcept(m_type = std::move(new_type)), "The move assignment of RUNTIME_TYPE must be noexcept");

				if (m_object != nullptr)
				{
					auto const old_memory_block = m_type.destroy(m_object);
					auto const old_size = m_type.size();
					auto const old_alignment = m_type.alignment();
					old_allocator.deallocate(old_memory_block, old_size, old_alignment);
				}

				m_object = new_object;	
				m_type = std::move(new_type);
			}
			catch (...)
			{	
				internal_get_allocator_ref().deallocate(new_memory_block, i_source.m_type.size(), i_source.m_type.alignment());
				internal_get_allocator_ref() = std::move(old_allocator);				
				throw;
			}
			return *this;
		}

        any(any && i_source) noexcept
            : VOID_ALLOCATOR(std::move(static_cast<VOID_ALLOCATOR&&>(i_source))),
			  m_object(i_source.m_object), m_type(std::move(i_source.m_type))
        {
			i_source.m_object = nullptr;
        }

		any & operator = (any && i_source) noexcept			
		{
			allocator_type::operator = (std::move(static_cast<VOID_ALLOCATOR&&>(i_source)));
			m_object = i_source.m_object;
			m_type = std::move(i_source.m_type);
			i_source.m_object = nullptr;
			return *this;
		}

		~any()
        {
            if (m_object != nullptr)
            {
				auto const old_memory_block = m_type.destroy(m_object);
				auto const old_size = m_type.size();
				auto const old_alignment = m_type.alignment();
				internal_get_allocator_ref().deallocate(old_memory_block, old_size, old_alignment);
            }
        }

		bool empty() const noexcept
		{
			return m_object == nullptr;
		}

		COMMON_TYPE * object() const noexcept
		{
			return m_object;
		}

		const RUNTIME_TYPE & type() const noexcept
		{
			return m_type;
		}

		allocator_type get_allocator()
		{
			return *this;
		}

		const allocator_type & get_allocator_ref() const
		{
			return *this;
		}

    private:

		any(COMMON_TYPE * i_object, RUNTIME_TYPE && i_type, allocator_type && i_allocator) noexcept
			: allocator_type(std::move(i_allocator)), m_object(i_object), m_type(std::move(i_type))
		{
		}

		allocator_type & internal_get_allocator_ref() noexcept
		{
			return *this;
		}

    private:
		COMMON_TYPE * m_object;
		RUNTIME_TYPE m_type;        
    };
}
