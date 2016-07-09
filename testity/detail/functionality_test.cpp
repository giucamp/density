
#include "functionality_test.h"

namespace testity
{
	namespace detail
	{
		FunctionalityTargetTypeRegistry & FunctionalityTargetTypeRegistry::instance()
		{
			static FunctionalityTargetTypeRegistry s_instance;
			return s_instance;
		}

		size_t FunctionalityTargetTypeRegistry::add_type(ITargetType * i_target_type)
		{
			auto res = m_registry.insert(std::make_pair(m_next_type_key++, i_target_type));
			if (!res.second)
			{
				throw std::exception("duplicate in FunctionalityTargetTypeRegistry");
			}
			return res.first->first;
		}
	}
}