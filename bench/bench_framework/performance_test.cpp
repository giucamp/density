
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
    std::string PerformanceTestGroup::s_source_dir;

    void PerformanceTestGroup::set_source_dir(const char * i_dir)
    {
        s_source_dir = i_dir;
        while (!s_source_dir.empty() && (s_source_dir.back() == '\\' || s_source_dir.back() == '/'))
        {
            s_source_dir.resize(s_source_dir.size() - 1);
        }
        if(!s_source_dir.empty())
            s_source_dir += '/';
    }

    void PerformanceTestGroup::add_test(const char * i_source_file, int i_start_line,
        PerformanceTest::TestFunction i_function, int i_end_line)
    {
        auto const source_file = s_source_dir + i_source_file;

        // open the source file and read the lines from i_start_line to i_end_line
        std::ifstream stream(source_file.c_str());
        if (stream.fail())
        {
            auto const error_message = std::string("Can't open the source ") + source_file;
            std::cerr << error_message << std::endl;
            throw std::ios_base::failure(error_message);
        }
        int curr_line = 0;
        std::vector<std::string> lines;
        while (!stream.eof() && curr_line < i_end_line - 1)
        {
            std::string line;
            std::getline(stream, line);
            if (curr_line >= i_start_line)
            {
                lines.push_back(std::move(line));
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
        std::string source_code;
        for (auto & line : lines)
        {
            line.erase(line.begin(), line.begin() + std::min(white_prefix_length, line.length()));
            source_code += line + "#nl#";
        }

        add_test(PerformanceTest(source_code.c_str(), std::move(i_function)));
    }
} // namespace density_bench
