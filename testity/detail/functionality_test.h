
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <random>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

namespace testity
{
    namespace detail
    {
		class ITargetType
		{
		public:
			virtual void * create_instance() const = 0;
			virtual void * clone_instance(const void * i_source_instance) const = 0;
			virtual void destroy_instance(void * i_instance) const noexcept = 0;
			virtual ~ITargetType() = default;
		};

		class TargetPtr
		{
		public:

			TargetPtr() noexcept 
				: m_type(nullptr), m_object(nullptr){}

			TargetPtr(const ITargetType * i_type, void * i_object) noexcept 
				: m_type(i_type), m_object(i_object) {}

			TargetPtr(const TargetPtr &) = delete;
			TargetPtr & operator = (const TargetPtr &) = delete;

			TargetPtr(TargetPtr && i_source) noexcept
				: m_type(i_source.m_type), m_object(i_source.m_object)
			{
				i_source.m_type = nullptr;
				i_source.m_object = nullptr;
			}

			TargetPtr & operator = (TargetPtr && i_source) noexcept
			{
				if (m_object != nullptr)
				{
					m_type->destroy_instance(m_object);
				}

				m_type = i_source.m_type;
				m_object = i_source.m_object;

				i_source.m_type = nullptr;
				i_source.m_object = nullptr;
			}

			~TargetPtr()
			{
				if (m_object != nullptr)
				{
					m_type->destroy_instance(m_object);
				}
			}

			const ITargetType * type() const noexcept { return m_type; }
			void * object() const noexcept { return m_object; }

		private:
			const ITargetType * m_type;
			void * m_object;
		};

        class IFunctionalityTest
        {
        public:

            IFunctionalityTest() = default;

            // disable copy
            IFunctionalityTest(const IFunctionalityTest &) = delete;
            IFunctionalityTest & operator = (const IFunctionalityTest &) = delete;

            virtual void execute(std::mt19937 & i_random, void * i_target) = 0;

            virtual ~IFunctionalityTest() = default;

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

            using Function = std::function< void(std::mt19937 & i_random) >;

            NoTargetFunctionalityTest(Function i_function)
                : m_function(i_function)
            {
            }

            void execute(std::mt19937 & i_random, void * /*i_target*/) override
            {
                m_function(i_random);
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

            template <typename TYPE> class TargetType : public ITargetType
            {
            public:
                void * create_instance() const override { return new TYPE; }
				void * clone_instance(const void * i_source_instance) const override
					{ return new TYPE(*static_cast<const TYPE*>(i_source_instance));  }
                void destroy_instance(void * i_instance) const noexcept override { delete static_cast<TYPE*>(i_instance); }
            };

            size_t add_type(ITargetType * i_target_type);

        public:

            static FunctionalityTargetTypeRegistry & instance();

            template <typename TYPE>
                size_t add_type()
            {
                return add_type(new TargetType<TYPE>());
            }

            ITargetType & get_target_type(size_t i_type_key) const;

        private:
            struct Impl;
            std::unique_ptr<Impl> m_pimpl;
        };

        template <typename TARGET_TYPE>
            class TargetedFunctionalityTest : public IFunctionalityTest
        {
        public:

            using Function = std::function< void(std::mt19937 & i_random, TARGET_TYPE & i_target) >;

            TargetedFunctionalityTest(Function i_function)
                : m_function(std::move(i_function))
            {
            }

            void execute(std::mt19937 & i_random, void * i_target) override
            {
                m_function(i_random, *static_cast<TARGET_TYPE*>(i_target) );
            }

            TargetTypeAndKey get_target_type_and_key() const override
            {
                auto const key = type_key();
                return TargetTypeAndKey{ &FunctionalityTargetTypeRegistry::instance().get_target_type(key), key };
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
