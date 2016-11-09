#pragma once

//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

namespace density
{
	namespace detail
	{
		template <typename RUNTIME_TYPE = runtime_type<ELEMENT>>
			struct Any_TargetType
		{
			RUNTIME_TYPE m_type;
			size_t m_size, m_alignment;

			Any_TargetType()
			{

			}
		};
	}

	template <typename COMMON_TYPE = void, typename UNTYPED_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT>>
		class any : VOID_ALLOCATOR
	{
	public:

		using allocator_type = VOID_ALLOCATOR;
		using runtime_type = RUNTIME_TYPE;

		any()
			: m_object(nullptr)
		{			
		}

		template <typename TYPE, typename ... CONSTRUCTION_ARGS>
			static any make(CONSTRUCTION_ARGS && ..i_args)
				: m_inner_type(i_inner_type)
		{
			any res;
			res.m_size = sizeof(TYPE);
			res.m_alignment = alignof(TYPE);
			auto const block = allocate(res.m_size, res.m_alignment);			
			res.m_object = new(res.m_object) TYPE(std::forward<CONSTRUCTION_ARGS>(i_args)...);
		}

		any(const any & i_source)
			: m_inner_type(i_source.m_inner_type),
				m_size(i_source.m_size), m_alignment(i_source.m_alignment)
		{
			auto const memory_bock = void_allocator().allocate(res.m_size, res.m_alignment);
			try
			{
				m_object = m_inner_type.copy_construct(memory_bock, i_source.m_object);
			}
			catch (...)
			{
				throw;
			}
		}

		any(any && i_source)
			: m_inner_type(i_source.m_inner_type),
			m_size(i_source.m_size), m_alignment(i_source.m_alignment)
		{
			m_object = m_inner_type.copy_construct(
				void_allocator().allocate(res.m_size, res.m_alignment), i_source.m_object);
		}

		~any()
		{
			if (m_object != nullptr)
			{
				m_inner_type.destroy(m_object);
				void_allocator().deallocate(m_object, m_size, m_alignment);
			}
		}

	private:

		any()
		{
		}

		void copy_from(const any & i_source)
		{
		}

	private:
		Any_TargetType m_type;
		COMMON_TYPE * m_object;		
	};
}
