
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/lifo.h>
#include <iostream>
#include <cstring>
#include <string>
#include <algorithm>
#include <numeric>
#include <vector>

namespace density_tests
{
    //! [lifo_array example 1]
    void concat_and_print(const char * i_str_1, const char * i_str_2)
    {
        using namespace density;

        auto const len_1 = strlen(i_str_1);
        auto const len_2 = strlen(i_str_2);
        
        lifo_array<char> string(len_1 + len_2 + 1);
        memcpy(string.data(), i_str_1, len_1);
        memcpy(string.data() + len_1, i_str_2, len_2);
        string[len_1 + len_2] = 0;

        std::cout << string.data() << std::endl;
    }
    //! [lifo_array example 1]

    
    void lifo_array_example_2()
    {
        using namespace density;

        {
    //! [lifo_array example 2]
    // uninitialized array of doubles
    lifo_array<double> numbers(7);

    // initialize the array
    for (auto & num : numbers)
        num = 1.;

    // compute the sum
    auto const sum = std::accumulate(numbers.begin(), numbers.end(), 0.);
    assert(sum == 7.);

    // initialized array
    lifo_array<double> other_numbers(7, 1.);
    auto const other_sum = std::accumulate(other_numbers.begin(), other_numbers.end(), 0.);
    assert(other_sum == 7.);

    // array of class objects - they are initialized by the default constructor
    lifo_array<std::string> strings(10);
    bool all_empty = std::all_of(strings.begin(), strings.end(), [](const std::string & i_str) {
        return i_str.empty();
    });
    assert(all_empty);
    //! [lifo_array example 2]
    (void)sum;
    (void)other_sum;
    (void)all_empty;
        }

        {
    //! [lifo_array constructor 2]
    std::vector<int> vect{ 1, 2, 3 };
    lifo_array<int> array(vect.cbegin(), vect.cend());
    auto int_sum = std::accumulate(array.begin(), array.end(), 0);
    assert(int_sum == 6);
    //! [lifo_array constructor 2]
    (void)int_sum;
        }

        {
    //! [lifo_array constructor 3]
    lifo_array<std::string> strings(10, 4, '*');
    assert(strings.size() == 10);
    bool all_stars = std::all_of(strings.begin(), strings.end(), [](const std::string & i_str) {
        return i_str == "****"; 
    });
    assert(all_stars);
    //! [lifo_array constructor 3]
    (void)all_stars;
        }
    }    

    //! [lifo_buffer example 1]
    // concatenate and print a null terminated array of strings
    void concat_and_print(const char * * i_strings)
    {
        using namespace density;
        
        lifo_buffer buff;
        while (*i_strings != nullptr)
        {
            auto const curr_len = buff.size() > 0 ? buff.size() - 1 : 0; // discard the previous null char, if any
            auto const additional_len = strlen(*i_strings);

            buff.resize(curr_len + additional_len + 1);
            memcpy(static_cast<char*>(buff.data()) + curr_len, *i_strings, additional_len + 1);

            i_strings++;
        }

        std::cout << static_cast<char*>(buff.data())  << std::endl;
    }
    //! [lifo_buffer example 1]

    void lifo_examples()
    {
        concat_and_print("Hello", " world!");

        lifo_array_example_2();

        const char * strings[] = {"Oh, ", "Hello ", "world: ", "this ", "is ", "an ", "array ", "of ", "strings!!", nullptr};
        concat_and_print(strings);
    }

} // namespace density_tests
