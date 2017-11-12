
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(_MSC_VER)
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include "test_settings.h"
#include <cstdio>
#include <stdexcept>
#include <inttypes.h>

std::shared_ptr<const TestSettings> parse_settings(int /*argc*/, char **argv)
{
    TestSettings results;

    int integer = 0;

    if(*argv != nullptr && *++argv != nullptr)
    {
        for (auto parameter = argv; *parameter != nullptr; parameter++)
        {
            if (sscanf(*parameter, "-rand_seed:%" SCNu32, &results.m_rand_seed) == 1)
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
            else if (sscanf(*parameter, "-queue_tests_cardinality:%zu", &results.m_queue_tests_cardinality) == 1)
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

#if defined(_MSC_VER)
    #undef _CRT_SECURE_NO_WARNINGS
#endif
