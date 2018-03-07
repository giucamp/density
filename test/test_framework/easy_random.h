
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <random>
#include <functional>

namespace density_tests
{
    /** This class is an easy to use (and hopefully correct) wrapper around a prg (currently std::mt19937). */
    class EasyRandom
    {
      public:
        /** Initializes a non-deterministic EasyRandom (using std::random_device to seed it). */
        EasyRandom() : m_deterministic(false)
        {
            std::random_device        source;
            std::mt19937::result_type data[std::mt19937::state_size];
            std::generate(std::begin(data), std::end(data), std::ref(source));
            std::seed_seq seeds(std::begin(data), std::end(data));
            m_rand = std::mt19937(seeds);
        }

        /** Initializes a deterministic EasyRandom, using only an uint32_t as seed. */
        EasyRandom(uint32_t i_seed) : m_rand(i_seed), m_deterministic(true) {}

        /** Creates another instance of EasyRandom using this to seed it. This function may alter
            the state of this EasyRandom. */
        EasyRandom fork()
        {
            if (m_deterministic)
            {
                std::mt19937::result_type data[std::mt19937::state_size];
                std::generate(std::begin(data), std::end(data), std::ref(m_rand));
                std::seed_seq seeds(std::begin(data), std::end(data));
                return EasyRandom(std::mt19937(seeds));
            }
            else
            {
                return EasyRandom();
            }
        }

        /** Returns a random integer within an inclusive range */
        template <typename INT> INT get_int(INT i_min, INT i_max)
        {
            return std::uniform_int_distribution<INT>(i_min, i_max)(m_rand);
        }

        /** Returns a random integer within an inclusive range */
        template <typename INT> INT get_int(INT i_max)
        {
            return std::uniform_int_distribution<INT>(0, i_max)(m_rand);
        }

        /** Returns a random integer*/
        template <typename INT> INT get_int()
        {
            return std::uniform_int_distribution<INT>()(m_rand);
        }

        /** Returns a random boolean */
        int32_t get_bool() { return std::uniform_int_distribution<int32_t>(0, 1)(m_rand) == 0; }

        /** Returns a random boolean */
        int32_t get_bool(double i_true_probability)
        {
            return std::uniform_real_distribution<double>(0.0, 1.0)(m_rand) < i_true_probability;
        }

        std::mt19937 & underlying_rand() { return m_rand; }

      private:
        EasyRandom(const std::mt19937 & i_rand) : m_rand(i_rand) {}

      private:
        std::mt19937 m_rand;
        bool         m_deterministic = false;
    };

} // namespace density_tests
