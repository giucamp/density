
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "performance_test.h"
#include <fstream>
#include <iostream>
#include <ios>
#include <algorithm>

namespace density_bench
{
    void PerformanceTestGroup::add_test(const char * i_source_file, int i_start_line,
        PerformanceTest::TestFunction i_function, int i_end_line)
    {
        using namespace std;

        // open the source file and read the lines from i_start_line to i_end_line
        ifstream stream(i_source_file);
        if (stream.fail())
        {
            auto const error_message = std::string("Can't open the source ") + i_source_file;
            std::cerr << error_message << std::endl;
            throw std::ios_base::failure(error_message);
        }
        int curr_line = 0;
        vector<string> lines;
        while (!stream.eof() && curr_line < i_end_line - 1)
        {
            string line;
            getline(stream, line);
            if (curr_line >= i_start_line)
            {
                lines.push_back(move(line));
            }
            curr_line++;
        }

        // find the longest white-char prefix common to all the lines
        size_t white_prefix_length = 0;
        bool match = lines.size() > 0;
        while (match)
        {
            bool first = true;
            char target_char = 0;
            for (const auto & line : lines)
            {
                if (match && white_prefix_length < line.length())
                {
                    const char curr_char = line[white_prefix_length];
                    if (!isspace(curr_char))
                    {
                        match = false;
                    }
                    else if (first)
                    {
                        target_char = curr_char;
                        first = false;
                    }
                    else
                    {
                        match = target_char == curr_char;
                    }
                }
            }
            if (match)
            {
                white_prefix_length++;
            }
        }

        // reconstruct the source code, with "#nl#" instead of newlines
        string source_code;
        for (auto & line : lines)
        {
            line.erase(line.begin(), line.begin() + std::min(white_prefix_length, line.length()));
            source_code += line + "#nl#";
        }

        add_test(PerformanceTest(source_code.c_str(), move(i_function)));
    }
} // namespace density_bench
