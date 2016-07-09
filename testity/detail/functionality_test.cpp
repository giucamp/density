
#include "functionality_test.h"
#include <mutex>

namespace testity
{
	namespace detail
	{
		struct FunctionalityTargetTypeRegistry::Impl
		{
			std::mutex m_mutex;
			size_t m_next_type_key;
			std::unordered_map<size_t, std::unique_ptr<IFunctionalityTest::ITargetType> > m_registry;
		};

		FunctionalityTargetTypeRegistry::FunctionalityTargetTypeRegistry()
			: m_pimpl(new Impl)
		{
		}

		FunctionalityTargetTypeRegistry::~FunctionalityTargetTypeRegistry() = default;

		FunctionalityTargetTypeRegistry & FunctionalityTargetTypeRegistry::instance()
		{
			static FunctionalityTargetTypeRegistry s_instance;
			return s_instance;
		}

		size_t FunctionalityTargetTypeRegistry::add_type(IFunctionalityTest::ITargetType * i_target_type)
		{
			std::lock_guard<std::mutex> lock(m_pimpl->m_mutex);
			auto res = m_pimpl->m_registry.insert(std::make_pair(m_pimpl->m_next_type_key++, i_target_type));
			if (!res.second)
			{
				throw std::exception("duplicate in FunctionalityTargetTypeRegistry");
			}
			return res.first->first;
		}

		IFunctionalityTest::ITargetType & FunctionalityTargetTypeRegistry::get_target_type(size_t i_type_key) const
		{
			std::lock_guard<std::mutex> lock(m_pimpl->m_mutex);

			auto const it = m_pimpl->m_registry.find(i_type_key);
			if (it == m_pimpl->m_registry.end())
			{
				throw std::exception("type not found in FunctionalityTargetTypeRegistry");
			}
			return *(it->second);
		}
	}
}