
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#include "..\density_common.h"
#include <functional>

namespace density
{
	namespace detail
	{
		/** Tries to evaluate i_func, and returns whether an Overflow has been thrown */
		template <typename EXCEPTION, typename FUNCTION> 
			bool throws(FUNCTION && i_func)
		{
			try
			{
				i_func();
			}
			catch (const Overflow &)
			{
				return true;
			}
			return false;
		}
		template <typename FUNCTION>
			bool throws_any(FUNCTION && i_func)
		{
			try
			{
				i_func();
			}
			catch (...)
			{
				return true;
			}
			return false;
		}

		bool is_valid_as_uint8(int32_t i_value)
		{
			return i_value >= 0 && i_value <= 255;
		}

		void mem_size_test()
		{
			// MemSize<int8_t>(); // <- must fail to compile

			assert( MemSize<size_t>().value() == 0 );
			
			// exhaustive test of MemSize<uint8_t>
			for (int32_t first = 0; first < 256; first++)
			{
				for (int32_t second = 0; second < 256; second++)
				{
					const MemSize<uint8_t> first_size(static_cast<uint8_t>(first)), second_size(static_cast<uint8_t>(second));
					MemSize<uint8_t> other_size;

					// test MemSize + MemSize
					const bool sum_throws = throws<Overflow>([=] { first_size + second_size; });
					assert(sum_throws == !is_valid_as_uint8(first + second));
					if (!sum_throws)
					{
						// test MemSize += MemSize
						other_size = first_size;
						other_size += second_size;
						assert(other_size == MemSize<uint8_t>(static_cast<uint8_t>(first + second)));
					}

					// test MemSize - MemSize
					const bool diff_throws = throws<Overflow>([=] { first_size - second_size; });
					assert(diff_throws == !is_valid_as_uint8(first - second));
					if (!diff_throws)
					{
						// test MemSize -= MemSize
						other_size = first_size;
						other_size -= second_size;
						assert(other_size == MemSize<uint8_t>(static_cast<uint8_t>(first - second)));
					}

					// test MemSize * uint
					const bool mul_throws = throws<Overflow>([=] { first_size * static_cast<uint8_t>(second); });
					assert(mul_throws == !is_valid_as_uint8(first * second));
					if (!mul_throws)
					{
						// test MemSize *= uint
						other_size = first_size;
						other_size *= static_cast<uint8_t>(second);
						assert(other_size == MemSize<uint8_t>(static_cast<uint8_t>(first * second)));
					}

					if (second > 0)
					{
						// test MemSize / uint
						const bool div_throws = throws<Overflow>([=] { first_size / static_cast<uint8_t>(second); });
						assert(div_throws == ((first % second) != 0) );
						if (!div_throws)
						{
							// test MemSize /= uint
							other_size = first_size;
							other_size /= static_cast<uint8_t>(second);
							assert(other_size == MemSize<uint8_t>(static_cast<uint8_t>(first / second)));
						}
					}
				}
			}

			MemSize<uint8_t> size;
		}
	}

	void pointer_arithmetic_test()
	{
		detail::mem_size_test();
	}
}
