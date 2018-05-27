
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//

#include "line_updater_stream_adapter.h"

namespace density_tests
{
    LineUpdaterStreamAdapter::LineUpdaterStreamAdapter(std::ostream & i_dest_stream)
        : m_dest_stream(i_dest_stream)
    {
    }

    LineUpdaterStreamAdapter::~LineUpdaterStreamAdapter()
    {
        if (m_line.str().length())
        {
            end_line_impl();
        }
        m_dest_stream << std::endl;
    }

    void LineUpdaterStreamAdapter::end_line_impl()
    {
        auto const line = m_line.str();

        // print spaces
        m_dest_stream << "\r";
        m_spaces.resize(m_prev_line_len, ' ');
        m_dest_stream << m_spaces;
        m_dest_stream << "\r";

        // print the line
        m_dest_stream << line;
        m_dest_stream.flush();

        // reset the string stream
        m_line.str("");
        m_line.clear();

        m_prev_line_len = line.length();
    }


} // namespace density_tests

#if 0

#include <iostream>
#include <thread>

void test()
{
    density_tests::LineUpdaterStreamAdapter line(std::cout);
    for (int i = 100000; i > 0; i /= 8)
    {
        line << "progress: " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

#endif
