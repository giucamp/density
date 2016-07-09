
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

			IFunctionalityTest() = default;

			// disable copy
			IFunctionalityTest(const IFunctionalityTest &) = delete;
			IFunctionalityTest & operator = (const IFunctionalityTest &) = delete;

			virtual void execute(FunctionalityContext & i_context, void * i_target) = 0;

			virtual ~IFunctionalityTest() = default;
			
			class ITargetType
			{
			public:
				virtual void * create_instance() = 0;
				virtual void destroy_instance(void * i_instance) = 0;
				virtual ~ITargetType() = default;
			};

			struct TargetTypeAndKey
			{
				ITargetType * m_type;
				size_t m_type_key;
			};

			virtual TargetTypeAndKey get_target_type_and_key() const = 0;
		};

		class NoTargetFunctionalityTest : public IFunctionalityTest
		{
		public:

			using Function = void(*)(FunctionalityContext & i_context);

			NoTargetFunctionalityTest(Function i_function)
				: m_function(i_function)
			{
			}

			void execute(FunctionalityContext & i_context, void * /*i_target*/)
			{
				(*m_function)(i_context);
			}

			TargetTypeAndKey get_target_type_and_key() const override
			{
				return TargetTypeAndKey{ nullptr, 0 };
			}

		private:
			const Function m_function;
		};

		class FunctionalityTargetTypeRegistry
		{
		private:
			FunctionalityTargetTypeRegistry();
			~FunctionalityTargetTypeRegistry();

			FunctionalityTargetTypeRegistry(const FunctionalityTargetTypeRegistry &) = delete;
			FunctionalityTargetTypeRegistry & operator = (const FunctionalityTargetTypeRegistry &) = delete;

			template <typename TYPE> class TargetType : public IFunctionalityTest::ITargetType
			{
			public:
				void * create_instance() override { return new TYPE; }
				void destroy_instance(void * i_instance) { delete static_cast<TYPE*>(i_instance); }
			};

			size_t add_type(IFunctionalityTest::ITargetType * i_target_type);

		public:

			static FunctionalityTargetTypeRegistry & instance();

			template <typename TYPE>
				size_t add_type()
			{
				return add_type(new TargetType<TYPE>());
			}

			IFunctionalityTest::ITargetType & get_target_type(size_t i_type_key) const;

		private:
			struct Impl;
			std::unique_ptr<Impl> m_pimpl;
		};

		template <typename TARGET_TYPE>
			class TargetedFunctionalityTest : public IFunctionalityTest
		{
		public:

			using Function = void(*)(FunctionalityContext & i_context, TARGET_TYPE & i_target);

			void execute(FunctionalityContext & i_context, void * i_target) override
			{
				(*m_function)(i_context, *static_cast<TARGET_TYPE*>(i_target) );
			}

			TargetTypeAndKey get_target_type_and_key() const override
			{
				auto const key = type_key();
				return TargetTypeAndKey{ &FunctionalityTargetTypeRegistry::get_target_type(key), key };
			}

		private:
			static size_t type_key()
			{
				static size_t s_type_key = FunctionalityTargetTypeRegistry::instance().add_type<TARGET_TYPE>();
				return s_type_key;
			}

		private:
			Function m_function;
		};

	} // namespace detail
	
} // namespace testity
