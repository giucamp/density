
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>

namespace density
{
	inline void raw_atomic_store(volatile uintptr_t & o_atomic, uintptr_t i_value, std::memory_order = std::memory_order_seq_cst) noexcept
	{
		_InterlockedExchange64((volatile __int64*)&o_atomic, (__int64)i_value);
	}

	/* This function may read from i_atomic even if it is concurrently modified by another thread
		without causing undefined behaviour. Anyway in this case the read value is undefined. */
	inline uintptr_t raw_atomic_load(const volatile uintptr_t & i_atomic, std::memory_order = std::memory_order_seq_cst) noexcept
	{
		return (uintptr_t)_InterlockedExchangeAdd64((volatile __int64*)&i_atomic, 0);
	}

	inline bool raw_atomic_compare_exchange_strong(volatile uintptr_t & io_atomic,
		uintptr_t & io_expected, uintptr_t i_desired, 
		std::memory_order i_success = std::memory_order_seq_cst,
		std::memory_order i_failure = std::memory_order_seq_cst) noexcept
	{
		DENSITY_ASSERT(address_is_aligned(const_cast<uintptr_t*>(&io_atomic), 8));
		(void)i_success;
		(void)i_failure;
		__int64 const prev_val = _InterlockedCompareExchange64((volatile __int64*)&io_atomic, (__int64)i_desired, (__int64)io_expected);
		if (prev_val == (__int64)io_expected)
		{
			return true;
		}
		else
		{
			io_expected = (uintptr_t)prev_val;
			return false;
		}
	}

	inline bool raw_atomic_compare_exchange_weak(volatile uintptr_t & io_atomic,
		uintptr_t & io_expected, uintptr_t i_desired,
		std::memory_order i_success = std::memory_order_seq_cst,
		std::memory_order i_failure = std::memory_order_seq_cst) noexcept
	{
		return raw_atomic_compare_exchange_weak(io_atomic, io_expected, i_desired, i_success, i_failure);
	}

} // namespace density
