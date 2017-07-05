#pragma once

#define DENSITY_TEST_ASSERT(expr)		if(!(expr)) __debugbreak(); else (void)0


namespace density_tests
{
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
}