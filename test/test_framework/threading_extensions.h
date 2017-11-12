
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <thread>
#include <memory>

namespace density_tests
{
    uint64_t get_num_of_processors();

    bool set_thread_affinity(uint64_t i_mask);

    bool set_thread_affinity(std::thread & i_thread, uint64_t i_mask);

    bool supend_thread(std::thread & i_thread);

    void resume_thread(std::thread & i_thread);

    bool set_thread_name(std::thread & i_thread, const char * i_name);
}
