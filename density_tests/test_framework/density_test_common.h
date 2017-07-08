#pragma once
#include <typeinfo>
#include <string>

#define DENSITY_TEST_ASSERT(expr)		if(!(expr)) detail::assert_failed(__FILE__, __func__, __LINE__, #expr); else (void)0

namespace density_tests
{
	template <typename TYPE>
		std::string truncated_type_name(size_t i_max_size = 80)
	{
		std::string name = typeid(TYPE).name();
		if (name.size() > i_max_size)
			name.resize(i_max_size);
		return name;
	}

	enum class QueueTesterFlags
	{
		eNone,
		eTestExceptions,
	};

	constexpr QueueTesterFlags operator | (QueueTesterFlags i_first, QueueTesterFlags i_second) noexcept
	{
		return static_cast<QueueTesterFlags>(static_cast<int>(i_first) | static_cast<int>(i_second));
	}

	constexpr QueueTesterFlags operator & (QueueTesterFlags i_first, QueueTesterFlags i_second) noexcept
	{
		return static_cast<QueueTesterFlags>(static_cast<int>(i_first) & static_cast<int>(i_second));
	}

	namespace detail
	{
		void assert_failed(const char * i_source_file, const char * i_function, int i_line, const char * i_expr);
	}
}