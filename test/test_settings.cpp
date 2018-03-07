
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "test_settings.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <stdexcept>

namespace
{
    bool read_string(const char * i_option, const char * i_prefix, std::string & o_value)
    {
        auto const prefix_len = strlen(i_prefix);
        auto const option_len = strlen(i_option);
        if (option_len > prefix_len && memcmp(i_prefix, i_option, prefix_len) == 0)
        {
            o_value.assign(i_option + prefix_len, option_len - prefix_len);
            return true;
        }
        return false;
    }

    bool append_string_list(
      const char * i_option, const char * i_prefix, std::vector<std::string> & o_values)
    {
        std::string list;
        if (read_string(i_option, i_prefix, list))
        {
            o_values.clear();
            while (!list.empty())
            {
                auto const separator_index = list.find_first_of(',');
                if (separator_index == std::string::npos)
                {
                    o_values.push_back(list);
                    list.clear();
                }
                else
                {
                    o_values.push_back(list.substr(0, separator_index));
                    list.erase(0, separator_index + 1);
                }
            }
            return true;
        }
        return false;
    }
} // namespace

std::shared_ptr<const TestSettings> parse_settings(int /*argc*/, char ** argv)
{
    TestSettings results;

    int integer = 0;

    if (*argv != nullptr && *++argv != nullptr)
    {
        for (auto parameter = argv; *parameter != nullptr; parameter++)
        {
            if (append_string_list(*parameter, "-only:", results.m_run_only))
            {
            }
            else if (append_string_list(*parameter, "-exclude:", results.m_exclude))
            {
            }
            else if (sscanf(*parameter, "-rand_seed:%" SCNu32, &results.m_rand_seed) == 1)
            {
            }
            else if (sscanf(*parameter, "-exceptions:%d", &integer) == 1)
            {
                results.m_exceptions = integer != 0;
            }
            else if (sscanf(*parameter, "-spare_one_cpu:%d", &integer) == 1)
            {
                results.m_spare_one_cpu = integer != 0;
            }
            else if (sscanf(*parameter, "-print_progress:%d", &integer) == 1)
            {
                results.m_print_progress = integer != 0;
            }
            else if (sscanf(*parameter, "-test_allocators:%d", &integer) == 1)
            {
                results.m_test_allocators = integer != 0;
            }
            else if (
              sscanf(
                *parameter, "-queue_tests_cardinality:%zu", &results.m_queue_tests_cardinality) ==
              1)
            {
            }
            else
            {
                throw std::logic_error(std::string("Unrecognized commandline: ") + *parameter);
            }
        }
    }
    return std::make_shared<TestSettings>(results);
}

bool TestSettings::should_run(const char * i_test_name) const
{
    if (!m_run_only.empty())
    {
        if (std::find(m_run_only.begin(), m_run_only.end(), i_test_name) == m_run_only.end())
        {
            return false;
        }
    }

    return std::find(m_exclude.begin(), m_exclude.end(), i_test_name) == m_exclude.end();
}

#if defined(_MSC_VER)
#undef _CRT_SECURE_NO_WARNINGS
#endif
