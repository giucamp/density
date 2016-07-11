
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/pointer_artithmetic.h"
#include <functional>

namespace density
{
    namespace detail
    {
        /** Tries to evaluate i_func, and returns whether an EXCEPTION has been thrown */
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
            // BasicMemSize<int8_t>(); // <- must fail to compile

            DENSITY_ASSERT( MemSize().value() == 0 );

            // exhaustive test of BasicMemSize<uint8_t>
            for (int32_t first = 0; first < 256; first++)
            {
                for (int32_t second = 0; second < 256; second++)
                {
                    const BasicMemSize<uint8_t> first_size(static_cast<uint8_t>(first)), second_size(static_cast<uint8_t>(second));
                    BasicMemSize<uint8_t> other_size;

                    // test BasicMemSize + BasicMemSize
                    const bool sum_throws = throws<Overflow>([=] { first_size + second_size; });
                    DENSITY_ASSERT(sum_throws == !is_valid_as_uint8(first + second));
                    if (!sum_throws)
                    {
                        // test BasicMemSize += BasicMemSize
                        other_size = first_size;
                        other_size += second_size;
                        DENSITY_ASSERT(other_size == BasicMemSize<uint8_t>(static_cast<uint8_t>(first + second)));
                    }

                    // test BasicMemSize - BasicMemSize
                    const bool diff_throws = throws<Overflow>([=] { first_size - second_size; });
                    DENSITY_ASSERT(diff_throws == !is_valid_as_uint8(first - second));
                    if (!diff_throws)
                    {
                        // test BasicMemSize -= BasicMemSize
                        other_size = first_size;
                        other_size -= second_size;
                        DENSITY_ASSERT(other_size == BasicMemSize<uint8_t>(static_cast<uint8_t>(first - second)));
                    }

                    // test BasicMemSize * uint
                    const bool mul_throws = throws<Overflow>([=] { first_size * static_cast<uint8_t>(second); });
                    DENSITY_ASSERT(mul_throws == !is_valid_as_uint8(first * second));
                    if (!mul_throws)
                    {
                        // test BasicMemSize *= uint
                        other_size = first_size;
                        other_size *= static_cast<uint8_t>(second);
                        DENSITY_ASSERT(other_size == BasicMemSize<uint8_t>(static_cast<uint8_t>(first * second)));
                    }

                    if (second > 0)
                    {
                        // test BasicMemSize / uint
                        const bool div_throws = throws<Overflow>([=] { first_size / static_cast<uint8_t>(second); });
                        DENSITY_ASSERT(div_throws == ((first % second) != 0) );
                        if (!div_throws)
                        {
                            // test BasicMemSize /= uint
                            other_size = first_size;
                            other_size /= static_cast<uint8_t>(second);
                            DENSITY_ASSERT(other_size == BasicMemSize<uint8_t>(static_cast<uint8_t>(first / second)));
                        }
                    }
                }
            }

            BasicMemSize<uint8_t> size;
        }
    }

    void pointer_arithmetic_test()
    {
        detail::mem_size_test();
    }
}
