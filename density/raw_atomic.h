
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <cstdint>
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
	#include <intrin.h>
#endif

namespace density
{
	template <typename TYPE>
		TYPE raw_atomic_load(TYPE const volatile * i_atomic, std::memory_order i_memory_order) = delete;

	template <typename TYPE>
		void raw_atomic_store(TYPE volatile * i_atomic, TYPE i_value, std::memory_order i_memory_order) = delete;

	template <typename TYPE>
		bool raw_atomic_compare_exchange_strong(TYPE volatile * i_atomic,
			TYPE * i_expected, TYPE i_desired, std::memory_order i_success, std::memory_order i_failure) = delete;

	template <typename TYPE>
		bool raw_atomic_compare_exchange_weak(TYPE volatile * i_atomic,
			TYPE * i_expected, TYPE i_desired, std::memory_order i_success, std::memory_order i_failure) = delete;

	#if defined(_MSC_VER) && defined(_M_X64)


		inline uint32_t raw_atomic_load(uint32_t const volatile * i_atomic, std::memory_order i_memory_order)
		{
			DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));

			switch (i_memory_order)
			{
				case std::memory_order_relaxed:
					return *i_atomic;

				default:
					DENSITY_ASSERT(false); // invalid memory order
				case std::memory_order_consume:
				case std::memory_order_acquire:
				case std::memory_order_seq_cst:
				{
					auto const value = *i_atomic;
					_ReadWriteBarrier();
					return value;
				}
			}		
		}

		inline uint64_t raw_atomic_load(uint64_t const volatile * i_atomic, std::memory_order i_memory_order)
		{
			DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));

			switch (i_memory_order)
			{
				case std::memory_order_relaxed:
					return *i_atomic;

				default:
					DENSITY_ASSERT(false); // invalid memory orders
				case std::memory_order_consume:
				case std::memory_order_acquire:
				case std::memory_order_seq_cst:
				{
					auto const value = *i_atomic;
					_ReadWriteBarrier();
					return value;
				}
			}		
		}

		inline void raw_atomic_store(uint32_t volatile * i_atomic, uint32_t i_value, std::memory_order i_memory_order)
		{
			DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));

			switch (i_memory_order)
			{
				case std::memory_order_relaxed:			
					*i_atomic = i_value;
					break;

				case std::memory_order_release:
					_ReadWriteBarrier();
					*i_atomic = i_value;
					break;

				case std::memory_order_seq_cst:
					_InterlockedExchange(reinterpret_cast<long volatile*>(i_atomic), static_cast<long>(i_value));
					break;

				default:
					DENSITY_ASSERT(false); // invalid memory order
			}		
		}

		inline void raw_atomic_store(uint64_t volatile * i_atomic, uint64_t i_value, std::memory_order i_memory_order)
		{
			DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));

			switch (i_memory_order)
			{
				case std::memory_order_relaxed:			
					*i_atomic = i_value;
					break;

				case std::memory_order_release:
					_ReadWriteBarrier();
					*i_atomic = i_value;
					break;

				case std::memory_order_seq_cst:
					_InterlockedExchange64(reinterpret_cast<long long volatile*>(i_atomic), static_cast<long long>(i_value));
					break;

				default:
					DENSITY_ASSERT(false); // invalid memory order
			}		
		}

		inline bool raw_atomic_compare_exchange_strong(uint32_t volatile * i_atomic,
			uint32_t * i_expected, uint32_t i_desired, std::memory_order i_success, std::memory_order i_failure)
		{
			DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));
			(void)i_success;
			(void)i_failure;
			long const prev_val = _InterlockedCompareExchange(reinterpret_cast<volatile long*>(i_atomic), (long)i_desired, *(long*)i_expected);
			if (prev_val == *(long*)i_expected)
			{
				return true;
			}
			else
			{
				*i_expected = (long)prev_val;
				return false;
			}
		}

		inline bool raw_atomic_compare_exchange_strong(uint64_t volatile * i_atomic,
			uint64_t * i_expected, uint64_t i_desired, std::memory_order i_success, std::memory_order i_failure)
		{
			DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));
			(void)i_success;
			(void)i_failure;
			long long const prev_val = _InterlockedCompareExchange64(reinterpret_cast<volatile long long*>(i_atomic), (long long)i_desired, *(long long*)i_expected);
			if (prev_val == *(long long*)i_expected)
			{
				return true;
			}
			else
			{
				*i_expected = (long long)prev_val;
				return false;
			}
		}

		inline bool raw_atomic_compare_exchange_weak(uint32_t volatile * i_atomic,
			uint32_t * i_expected, uint32_t i_desired, std::memory_order i_success, std::memory_order i_failure)
		{
			return raw_atomic_compare_exchange_strong(i_atomic, i_expected, i_desired, i_success, i_failure);
		}

		inline bool raw_atomic_compare_exchange_weak(uint64_t volatile * i_atomic,
			uint64_t * i_expected, uint64_t i_desired, std::memory_order i_success, std::memory_order i_failure)
		{
			return raw_atomic_compare_exchange_strong(i_atomic, i_expected, i_desired, i_success, i_failure);
		}

	#endif

	inline uint32_t raw_atomic_load(uint32_t const volatile * i_atomic)
	{
		return raw_atomic_load(i_atomic, std::memory_order_seq_cst);
	}

	inline void raw_atomic_store(uint32_t volatile * i_atomic, uint32_t i_value)
	{
		raw_atomic_store(i_atomic, i_value, std::memory_order_seq_cst);
	}

	inline bool raw_atomic_compare_exchange_strong(uint32_t volatile * i_atomic, uint32_t * i_expected, uint32_t i_desired)
	{
		return raw_atomic_compare_exchange_strong(i_atomic, i_expected, i_desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
	}

	inline bool raw_atomic_compare_exchange_weak(uint32_t volatile * i_atomic, uint32_t * i_expected, uint32_t i_desired)
	{
		return raw_atomic_compare_exchange_weak(i_atomic, i_expected, i_desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
	}

	inline uint64_t raw_atomic_load(uint64_t const volatile * i_atomic)
	{
		return raw_atomic_load(i_atomic, std::memory_order_seq_cst);
	}

	inline void raw_atomic_store(uint64_t volatile * i_atomic, uint64_t i_value)
	{
		raw_atomic_store(i_atomic, i_value, std::memory_order_seq_cst);
	}

	inline bool raw_atomic_compare_exchange_strong(uint64_t volatile * i_atomic, uint64_t * i_expected, uint64_t i_desired)
	{
		return raw_atomic_compare_exchange_strong(i_atomic, i_expected, i_desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
	}

	inline bool raw_atomic_compare_exchange_weak(uint64_t volatile * i_atomic, uint64_t * i_expected, uint64_t i_desired)
	{
		return raw_atomic_compare_exchange_weak(i_atomic, i_expected, i_desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
	}

} // namespace density
