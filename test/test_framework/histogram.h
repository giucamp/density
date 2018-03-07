
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "density_test_common.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace density_tests
{
    template <typename TYPE> TYPE clamp(TYPE i_value, TYPE i_min, TYPE i_max)
    {
        return std::min(std::max(i_value, i_min), i_max);
    }

    template <typename TYPE, bool ARITHMETIC = std::is_arithmetic<TYPE>::value> class histogram;

    /** This class template allows to construct an histogram and write it in textual format to an std::ostream.

        Example:
        \code
std::mt19937 rand(55);
auto dice = [&] { return std::uniform_int_distribution<int>(1, 6)(rand); };

histogram<int> hist("Throwing two dices 2000 times");
for(int i = 0; i < 2000; i++)
    hist << dice() + dice();

std::cout << hist;
        \endcode

        Output:
        \code
Throwing two dices 2000 times:         2|*                        |97.6
  [histogram]                          3|*******                  |174.6
                                       4|***************          |300.1
                                       6|************************ |418.7
                                       7|*************************|439.5
                                       9|****************         |313.1
                                      10|*******                  |177.2
                                      12|                         |79.2
        \endcode
    */
    template <typename TYPE> class histogram<TYPE, true>
    {
      public:
        /** Constructor that may assign the title.
            Content may be appended to the title later using the function title. */
        histogram(const char * i_title = "", size_t i_row_length = 25, size_t i_row_count = 8)
            : m_row_count(i_row_count), m_row_length(i_row_length)
        {
            m_title << i_title;
        }

        /** Returns a reference to the label output stream, that can be used to append strings or anything */
        std::ostream & title() { return m_title; }

        /** Puts a single value on the histogram */
        histogram & operator<<(const TYPE & i_value)
        {
            m_values.push_back(i_value);
            return *this;
        }

        /** Puts a vector of values on the histogram */
        histogram & operator<<(const std::vector<TYPE> & i_values)
        {
            m_values.insert(m_values.end(), i_values.begin(), i_values.end());
            return *this;
        }

        size_t row_count() const { return m_row_count; }

        size_t row_length() const { return m_row_length; }

        void set_row_count(size_t i_row_count)
        {
            DENSITY_TEST_ASSERT(i_row_count >= 1);
            m_row_count = i_row_count;
        }

        void set_row_length(size_t i_row_length)
        {
            DENSITY_TEST_ASSERT(i_row_length >= 1);
            m_row_length = i_row_length;
        }

        friend std::ostream & operator<<(std::ostream & i_ostream, const histogram & i_histogram)
        {
            i_histogram.write(i_ostream);
            return i_ostream;
        }

        void write(std::ostream & i_ostream) const
        {
            std::vector<double> rows(m_row_count);

            std::string histogram_str("  [histogram]");
            auto        title = m_title.str();
            if (title.length() > 0)
                title += ':';

            auto const strings_len = title.size();
            title.resize(strings_len, ' ');
            if (strings_len < histogram_str.length())
                histogram_str.clear();
            histogram_str.resize(strings_len, ' ');
            std::string const spaces(strings_len, ' ');

            if (m_values.size() == 0)
            {
                i_ostream << title << " no values\n";
            }
            else if (m_values.size() == 1)
            {
                i_ostream << title << " single value: " << m_values[0] << "\n";
            }
            else
            {
                auto const minmax_its = std::minmax_element(m_values.begin(), m_values.end());
                auto const min        = *minmax_its.first;
                auto const max        = *minmax_its.second;

                if (max - min < 0.0000001)
                {
                    i_ostream << title << " " << m_values.size() << " values ~= " << max << "\n";
                }
                else if (m_row_count == 1)
                {
                    i_ostream << title << " [" << min << ", " << max << "]\n";
                }
                else
                {
                    auto const value_to_row_index_factor =
                      static_cast<double>(m_row_count - 1) / (max - min);
                    for (const auto & value : m_values)
                    {
                        if (value == min)
                        {
                            rows.front() += 1.;
                        }
                        else if (value == max)
                        {
                            rows.back() += 1.;
                        }
                        else
                        {
                            auto const row_index_float = (value - min) * value_to_row_index_factor;
                            auto const row_index_floor = floor(row_index_float);
                            auto const row_index_fract =
                              clamp(row_index_float - row_index_floor, 0., 1.);
                            auto const index64 = clamp<int64_t>(
                              static_cast<int64_t>(row_index_floor + 0.2), 0, m_row_count - 1);
                            auto const index = static_cast<size_t>(index64);

                            if (index < m_row_count - 1)
                            {
                                rows[index] += 1. - row_index_fract;
                                rows[index + 1] += row_index_fract;
                            }
                            else
                            {
                                rows[index] *= 1.;
                            }
                        }
                    }

                    // write on the stream
                    std::string stars, padding;
                    auto const  minmax_rowlen_its = std::minmax_element(rows.begin(), rows.end());
                    auto const  min_row_len       = *minmax_rowlen_its.first;
                    auto const  max_row_len       = *minmax_rowlen_its.second;
                    auto const  row_to_value_factor =
                      static_cast<double>(max - min) / (m_row_count - 1);

                    for (size_t i = 0; i < rows.size(); i++)
                    {
                        if (i == 0)
                            i_ostream << title;
                        else if (i == 1)
                            i_ostream << histogram_str;
                        else
                            i_ostream << spaces;

                        TYPE row_value;
                        if (i == 0)
                            row_value = min;
                        else if (i == rows.size() - 1)
                            row_value = max;
                        else
                            row_value = static_cast<TYPE>(min + i * row_to_value_factor);
                        i_ostream << std::setw(10) << row_value << "|";

                        double row_length_float =
                          m_row_length * (rows[i] - min_row_len) / (max_row_len - min_row_len);
                        auto row_length = clamp<int64_t>(
                          static_cast<int64_t>(row_length_float + 0.5),
                          0,
                          static_cast<int64_t>(m_row_length));

                        stars.resize(static_cast<size_t>(row_length), '*');
                        i_ostream << stars;

                        padding.resize(m_row_length - static_cast<size_t>(row_length), ' ');
                        i_ostream << padding << "|" << rows[i] << '\n';
                    }
                }
            }
        }

      private:
        std::ostringstream m_title;
        std::vector<TYPE>  m_values;
        size_t             m_row_count  = 8;
        size_t             m_row_length = 10;
    };

    template <typename TYPE> class histogram<TYPE, false>
    {
      public:
        /** Constructor that may assign the title.
            Content may be appended to the title later using the function title. */
        histogram(const char * i_title = "", size_t i_row_length = 25) : m_row_length(i_row_length)
        {
            m_title << i_title;
        }


        /** Returns a reference to the label output stream, that can be used to append strings or anything */
        std::ostream & title() { return m_title; }

        /** Puts a single value on the histogram */
        histogram & operator<<(const TYPE & i_value)
        {
            m_values[i_value]++;
            return *this;
        }

        /** Puts a vector of values on the histogram */
        histogram & operator<<(const std::vector<TYPE> & i_values)
        {
            for (const auto & value : i_values)
            {
                m_values[value]++;
            }
            return *this;
        }

        size_t row_length() const { return m_row_length; }

        void set_row_count(size_t i_row_count)
        {
            DENSITY_TEST_ASSERT(i_row_count >= 1);
            m_row_count = i_row_count;
        }

        void set_row_length(size_t i_row_length)
        {
            DENSITY_TEST_ASSERT(i_row_length >= 1);
            m_row_length = i_row_length;
        }

        friend std::ostream & operator<<(std::ostream & i_ostream, const histogram & i_histogram)
        {
            i_histogram.write(i_ostream);
            return i_ostream;
        }

        void write(std::ostream & i_ostream) const
        {
            std::string histogram_str("  [histogram]");
            auto        title = m_title.str();
            if (title.length() > 0)
                title += ':';
            auto const strings_len = title.size();
            title.resize(strings_len, ' ');
            if (strings_len < histogram_str.length())
                histogram_str.clear();
            histogram_str.resize(strings_len, ' ');
            std::string const spaces(strings_len, ' ');

            using Row = std::pair<TYPE, size_t>;
            std::vector<Row> rows(m_values.begin(), m_values.end());
            std::sort(rows.begin(), rows.end(), [](const Row & i_first, const Row & i_second) {
                return i_first.first < i_second.first;
            });

            if (rows.size() == 0)
            {
                i_ostream << title << " no values\n";
            }
            else
            {
                auto const minmax_rowlen_its = std::minmax_element(
                  rows.begin(), rows.end(), [](const Row & i_first, const Row & i_second) {
                      return i_first.second < i_second.second;
                  });
                auto const min_count = minmax_rowlen_its.first->second;
                auto const max_count = minmax_rowlen_its.second->second;

                std::string stars, padding;

                double factor = 1.f;
                if (min_count != max_count)
                    factor = static_cast<double>(m_row_length - 1) / (max_count - min_count);
                for (size_t i = 0; i < rows.size(); i++)
                {
                    if (i == 0)
                        i_ostream << title;
                    else if (i == 1)
                        i_ostream << histogram_str;
                    else
                        i_ostream << spaces;

                    i_ostream << std::setw(10) << rows[i].first << "|";

                    double row_length_float = (rows[i].second - min_count) * factor;
                    auto   row_length       = clamp<int64_t>(
                      static_cast<int64_t>(row_length_float + 0.5),
                      0,
                      static_cast<int64_t>(m_row_length));

                    stars.resize(static_cast<size_t>(row_length), '*');
                    i_ostream << stars;

                    padding.resize(m_row_length - static_cast<size_t>(row_length), ' ');
                    i_ostream << padding << "|" << rows[i].second << '\n';
                }
            }
        }

      private:
        std::ostringstream               m_title;
        std::unordered_map<TYPE, size_t> m_values;
        size_t                           m_row_count  = 8;
        size_t                           m_row_length = 10;
    };

    template <typename TYPE>
    histogram<TYPE> make_histogram(const char * i_label, const std::vector<TYPE> & i_values)
    {
        histogram<TYPE> hist(i_label);
        hist << i_values;
        return hist;
    }

    template <typename STRUCT_TYPE, typename VALUE_TYPE>
    histogram<VALUE_TYPE> make_member_histogram(
      const char *                     i_label,
      const std::vector<STRUCT_TYPE> & i_structs,
      VALUE_TYPE(STRUCT_TYPE::*i_member))
    {
        histogram<VALUE_TYPE> hist(i_label);
        for (auto const & element : i_structs)
        {
            hist << element.*i_member;
        }
        return hist;
    }

} // namespace density_tests
