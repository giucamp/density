#pragma once
#include <typeinfo>
#include <string>
#include <chrono>
#include <density/void_allocator.h>

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
		eNone = 0,
		eTestExceptions = 1 << 1,
        eUseTestAllocators = 1 << 2,
		eReserveCoreToMainThread = 1 << 3,
	};

	constexpr QueueTesterFlags operator | (QueueTesterFlags i_first, QueueTesterFlags i_second) noexcept
	{
		return static_cast<QueueTesterFlags>(static_cast<int>(i_first) | static_cast<int>(i_second));
	}

	constexpr QueueTesterFlags operator & (QueueTesterFlags i_first, QueueTesterFlags i_second) noexcept
	{
		return static_cast<QueueTesterFlags>(static_cast<int>(i_first) & static_cast<int>(i_second));
	}

	constexpr bool operator && (QueueTesterFlags i_first, QueueTesterFlags i_second) noexcept
	{
		return (static_cast<int>(i_first) & static_cast<int>(i_second)) != 0;
	}

	namespace detail
	{
		void assert_failed(const char * i_source_file, const char * i_function, int i_line, const char * i_expr);
	}

	/** Move-only wrapper of void_allocator */
	struct move_only_void_allocator : density::void_allocator
	{
		move_only_void_allocator(int) {}

		move_only_void_allocator(move_only_void_allocator &&) = default;

		move_only_void_allocator & operator = (move_only_void_allocator &&) = default;

		move_only_void_allocator(const move_only_void_allocator&) = delete;

		move_only_void_allocator & operator = (const move_only_void_allocator&) = delete;

		void dummy_func()
		{

		}

		void const_dummy_func() const
		{

		}
	};
}