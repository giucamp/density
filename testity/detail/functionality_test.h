
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <random>
#include <string>
#include <unordered_map>
#include <memory>
#include "..\functionality_context.h"

namespace testity
{
	namespace detail
	{
		class IFunctionalityTest
		{
		public:

			virtual void execute(FunctionalityContext & i_context) = 0;

			virtual ~IFunctionalityTest() = default;

		private:
		};

		class SimpleFunctionalityTest : public IFunctionalityTest
		{
		public:

			using Function = void(*)(FunctionalityContext & i_context);

			SimpleFunctionalityTest(Function i_function)
				: m_function(i_function)
			{
			}

			void execute(FunctionalityContext & i_context)
			{
				(*m_function)(i_context);
			}

		private:
			Function m_function;
		};

		class FunctionalityTargetTypeRegistry
		{
		public:

			static FunctionalityTargetTypeRegistry & instance();

			class ITargetType
			{
			public:
				virtual void * create_instance() = 0;
				virtual void destroy_instance(void * i_instance) = 0;
				virtual ~ITargetType() = default;
			};

			template <typename TYPE>
				class TargetType : public ITargetType
			{
			public:
				void * create_instance() override { return new TYPE; }
				void destroy_instance(void * i_instance) { delete static_cast<TYPE*>(i_instance); }
			};

			size_t add_type(ITargetType * i_target_type);

			template <typename TYPE>
				size_t add_type()
			{
				return add_type(new TargetType<TYPE>());
			}

		private:
			FunctionalityTargetTypeRegistry() = default;
			~FunctionalityTargetTypeRegistry() = default;

			FunctionalityTargetTypeRegistry(const FunctionalityTargetTypeRegistry &) = delete;
			FunctionalityTargetTypeRegistry & operator = (const FunctionalityTargetTypeRegistry &) = delete;

		private:
			size_t m_next_type_key;
			std::unordered_map<size_t, std::unique_ptr<ITargetType> > m_registry;
		};

		template <typename TARGET_TYPE>
			class TargetedFunctionalityTest
		{
		public:

			static size_t type_key()
			{
				static size_t s_type_key = FunctionalityTargetTypeRegistry::instance().add_type<TARGET_TYPE>();
				return s_type_key;
			}			
		};

	} // namespace detail
	
} // namespace testity
