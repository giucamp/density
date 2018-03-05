
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_test_common.h"
#include <algorithm>
#include <limits>
#include <ostream>

namespace density_tests
{
    /** Computes incrementally the average, the mininum and the maximum of a set of samples. */
    class Statistics
    {
      public:
        double min() const noexcept { return m_min; }

        double max() const noexcept { return m_max; }

        double average() const noexcept { return m_average; }

        void sample(double i_value) noexcept
        {
            auto const new_count = m_count + 1.;
            m_min                = std::min(m_min, i_value);
            m_max                = std::max(m_max, i_value);
            m_average += (i_value - m_average) / new_count;
            m_count = new_count;
            m_sum += i_value;
        }

        void merge_with(const Statistics & i_statistics)
        {
            m_min                    = std::min(m_min, i_statistics.m_min);
            m_max                    = std::max(m_max, i_statistics.m_max);
            auto const   total_count = m_count + i_statistics.m_count;
            const double this_weight = m_count / total_count;
            m_average = m_average * this_weight + i_statistics.m_average * (1.0 - this_weight);
            m_count   = total_count;
            m_sum += i_statistics.m_sum;
        }

        void to_stream(std::ostream & i_stream) const
        {
            if (m_count == 0.)
                i_stream << "no samples";
            else if (m_min == m_max)
                i_stream << m_min;
            else
                i_stream << "[" << m_min << ", " << m_max << "](" << m_average << ")";
        }

        void to_stream_ex(std::ostream & i_stream) const
        {
            if (m_count == 0.)
                i_stream << "no samples";
            else if (m_min == m_max)
                i_stream << m_min;
            else
            {
                i_stream << "[" << m_min << ", " << m_max << "](avg: " << m_average
                         << ", sum: " << m_sum << ", count: " << m_count << ")";
            }
        }

        friend std::ostream & operator<<(std::ostream & i_stream, const Statistics & i_statistics)
        {
            i_statistics.to_stream(i_stream);
            return i_stream;
        }

      private:
        double m_min     = std::numeric_limits<double>::infinity();
        double m_max     = -std::numeric_limits<double>::infinity();
        double m_average = 0.;
        double m_count   = 0.;
        double m_sum     = 0.;
    };

} // namespace density_tests
