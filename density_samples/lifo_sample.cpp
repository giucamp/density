
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/lifo.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

namespace lifo_sample
{
    using namespace density;

    // This function prints a string on cout. Using the std::string_view introduced with C++17 this would be much easier!
    void print_reverse_words(const char * i_string, size_t i_length)
    {
        // find the end of the word
        auto end_of_word = static_cast<const char*>(std::memchr(i_string, ' ', i_length));
        if (end_of_word == nullptr)
            end_of_word = i_string + i_length;

        // use a variable length automatic array to manipulate the string
        lifo_array<char> word(i_string, end_of_word);
        std::reverse(word.begin(), word.end());

        std::cout.write(word.data(), word.size());
        std::cout << ' ';

        if (i_length > word.size())
        {
            // recursion is not really necessary, but this is just a sample
            print_reverse_words(end_of_word + 1, i_length - word.size() - 1);
        }
        else
        {
            std::cout << std::endl;
        }
    }

    void string_io()
    {
        using namespace std;
        vector<string> strings{ "string", "long string", "very long string", "much longer string!!!!!!" };
        uint32_t len = 0;

        // for each string, write a length-chars pair on a temporary file
        #ifndef _MSC_VER
            auto file = tmpfile();
        #else
            FILE * file = nullptr;
            tmpfile_s(&file);
        #endif
        for (const auto & str : strings)
        {
            len = static_cast<uint32_t>( str.length() + 1);
            fwrite(&len, sizeof(len), 1, file);
            fwrite(str.c_str(), 1, len, file);
        }

        // now we read what we have written
        rewind(file);
        lifo_buffer<> buff;
        while (fread(&len, sizeof(len), 1, file) == 1)
        {
            buff.resize(len);
            fread(buff.data(), len, 1, file);
            cout << static_cast<char*>(buff.data()) << endl;
        }

        fclose(file);
    }

    struct GraphNode
    {

    };

    void dijkstra_path_find(const GraphNode * i_nodes, size_t i_node_count, size_t i_initial_node_index)
    {
        lifo_array<float> distance(i_node_count, std::numeric_limits<float>::max());
        distance[i_initial_node_index] = 0.f;

        // ... not really implemented :)
        (void)i_nodes;
    }

    void run()
    {
        std::string sentence = "nI siht ecnetnes sdrow erew .desrever >rahc<yarra_ofil nac pleh ot xif !ti";
        print_reverse_words(sentence.data(), sentence.length());
        string_io();
    }
}
