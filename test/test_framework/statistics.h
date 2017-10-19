
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_test_common.h"
#include <limits>
#include <algorithm>
#include <ostream>

namespace density_tests
{
    class Statistics
    {
    public:

        double min() const noexcept
        {
            return m_min;
        }

        double max() const noexcept
        {
            return m_max;
        }

        double average() const noexcept
        {
            return m_average;
        }

        void sample(double i_value) noexcept
        {
            m_min = std::min(m_min, i_value);
            m_max = std::max(m_max, i_value);
            m_average += (i_value - m_average) / m_count_plus_one;
            m_count_plus_one += 1.;
        }

        void to_stream(std::ostream & i_stream) const
        {
            if (m_count_plus_one == 1.)
                i_stream << "no samples";
            else
                i_stream << "[" << m_min << ", " << m_max << "](" << m_average << ")";
        }

        friend std::ostream & operator << (std::ostream & i_stream, const Statistics & i_statistics)
        {
            i_statistics.to_stream(i_stream);
            return i_stream;
        }

    private:
        double m_min = std::numeric_limits<double>::infinity();
        double m_max = -std::numeric_limits<double>::infinity();
        double m_average = 0.;
        double m_count_plus_one = 1.;
    };

} // namespace density_test
