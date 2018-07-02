
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//

#include "density_test_common.h"
#include "easy_random.h"
#include <assert.h>
#include <iostream>
#include <memory>
#include <thread>

namespace density_tests
{
    namespace detail
    {
        void assert_failed3(const char * i_text)
        {
            std::cerr << i_text;
            std::cerr.flush();

#ifdef _MSC_VER
            __debugbreak();
#elif defined(__GNUC__)
            __builtin_trap();
#else
            assert(false);
#endif
        }
    } // namespace detail

} // namespace density_tests
