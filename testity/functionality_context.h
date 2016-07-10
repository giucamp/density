#pragma once
#include <string>
#include <random>

namespace testity
{
	class FunctionalityContext
    {
    public:

        FunctionalityContext(std::mt19937 && i_random)
            : m_random(std::move(i_random))
        {

        }

        template <typename INT_TYPE>
            INT_TYPE random_int(INT_TYPE i_exclusive_upper)
        {
            return std::uniform_int_distribution<INT_TYPE>(0, i_exclusive_upper - 1)(m_random);
        }

        template <typename INT_TYPE>
            INT_TYPE random_int(INT_TYPE i_inclusive_lower, INT_TYPE i_exclusive_upper)
        {
            return std::uniform_int_distribution<INT_TYPE>(i_inclusive_lower, i_exclusive_upper - 1)(m_random);
        }

        template <typename CHAR>
            CHAR random_char()
        {
            return CHAR('A') + static_cast<CHAR>(random_int<int>('Z' - 'A'));
        }

        template <typename CHAR, typename CHAR_TRAITS=std::char_traits<CHAR> >
            std::basic_string<CHAR, CHAR_TRAITS> random_string(size_t i_exclusive_length_upper)
        {
            const size_t len = random_int<size_t>(i_exclusive_length_upper);
            std::basic_string<CHAR, CHAR_TRAITS> result;
            result.reserve(len);
            for (size_t i = 0; i < len; i++)
            {
                result += random_char<CHAR>();
            }
            return result;
        }

        std::mt19937 & random_generator() { return m_random; }

    private:
        std::mt19937 m_random;
    };
}