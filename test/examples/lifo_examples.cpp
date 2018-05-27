
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//

#include <algorithm>
#include <cstring>
#include <density/lifo.h>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
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

    //! [lifo example 1]
    void func(size_t i_size)
    {
        using namespace density;

        lifo_buffer buffer_1(i_size);
        assert(buffer_1.size() == i_size);

        lifo_buffer buffer_2; // now buffer_1 can't be resized until buffer_2 is destroyed
        assert(buffer_2.size() == 0);

        auto mem = buffer_2.resize(sizeof(int));
        assert(mem == buffer_2.data());
        *static_cast<int *>(mem) = 5;

        mem = buffer_2.resize(sizeof(int) * 20);
        assert(*static_cast<int *>(mem) == 5);

        lifo_array<int> other_numbers(7);
        // buffer_2.resize(20); <---- violation of the lifo constraint, other_numbers is more recent!
    }
    //! [lifo example 1]

    void lifo_array_example_2()
    {
        using namespace density;

        {
            lifo_buffer buff(100);
        }

        func(200);


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
            bool                    all_empty =
              std::all_of(strings.begin(), strings.end(), [](const std::string & i_str) {
                  return i_str.empty();
              });
            assert(all_empty);
            //! [lifo_array example 2]
            (void)sum;
            (void)other_sum;
            (void)all_empty;
        }

        {
            //! [lifo_array example 3]
            struct MyStruct
            {
                lifo_array<std::string> m_strings{6};
                lifo_array<std::string> m_other_strings{6};
            };

            // In C++ array elements and struct members have lifo-compliant lifetime
            lifo_array<MyStruct> structs{10};
            lifo_array<MyStruct> other_structs{10};
            //! [lifo_array example 3]
        }
        {
            //! [lifo_array example 4]
            struct MyStruct
            {
                lifo_array<std::string> m_strings{6};
                lifo_array<std::string> m_other_strings{6};
            };

            struct MyStruct1
            {
                lifo_array<MyStruct> m_structs{6};
                lifo_array<MyStruct> m_other_structs{6};
            };

            // In C++ array elements and struct members have lifo-compliant lifetime
            lifo_array<MyStruct> structs{10};

            // Still legal, but don't go too far
            lifo_array<std::unique_ptr<MyStruct1>> other_structs{10};
            std::generate(other_structs.begin(), other_structs.end(), []() {
                return std::unique_ptr<MyStruct1>(new MyStruct1);
            });
            //! [lifo_array example 4]
        }
        {
            //! [lifo_array constructor 2]
            std::vector<int> vect{1, 2, 3};
            lifo_array<int>  array(vect.cbegin(), vect.cend());
            auto             int_sum = std::accumulate(array.begin(), array.end(), 0);
            assert(int_sum == 6);
            //! [lifo_array constructor 2]
            (void)int_sum;
        }

        {
            //! [lifo_array constructor 3]
            lifo_array<std::string> strings(10, 4, '*');
            assert(strings.size() == 10);
            bool all_stars =
              std::all_of(strings.begin(), strings.end(), [](const std::string & i_str) {
                  return i_str == "****";
              });
            assert(all_stars);
            //! [lifo_array constructor 3]
            (void)all_stars;
        }

        auto lifo_allocator_example_1 = [] {
            for (int i = 0; i < 2; i++)
            {
                //! [lifo_allocator allocate_empty 1]
                lifo_allocator<> allocator;

                auto const block = allocator.allocate_empty();
                assert(address_is_aligned(block, decltype(allocator)::alignment));

                allocator.deallocate(block, 0);
                //! [lifo_allocator allocate_empty 1]
            }
        };

        auto lifo_allocator_example_2 = [] {
            for (int i = 0; i < 2; i++)
            {
                //! [lifo_allocator allocate_empty 2]
                lifo_allocator<> allocator;
                constexpr auto   alignment = decltype(allocator)::alignment;

                auto block = allocator.allocate_empty();
                assert(address_is_aligned(block, alignment));

                block = allocator.reallocate(block, 0, alignment * 2);
                assert(address_is_aligned(block, alignment));

                allocator.deallocate(block, alignment * 2);
                //! [lifo_allocator allocate_empty 2]
            }
        };

        // test on this thread (with a non-empty data stack) and on a separate thread (with an empty data stack)
        {
            lifo_array<int> arr(4, 4);
            lifo_allocator_example_1();
        }


        {
            lifo_array<int> arr(4, 4);
            lifo_allocator_example_2();
        }
        std::thread(lifo_allocator_example_2).join();
        std::thread(lifo_allocator_example_1).join();
    }

    //! [lifo_buffer example 1]
    // concatenate and print a null terminated array of strings
    void concat_and_print(const char ** i_strings)
    {
        using namespace density;

        lifo_buffer buff;
        while (*i_strings != nullptr)
        {
            auto const curr_len =
              buff.size() > 0 ? buff.size() - 1 : 0; // discard the previous null char, if any
            auto const additional_len = strlen(*i_strings);

            buff.resize(curr_len + additional_len + 1);
            memcpy(static_cast<char *>(buff.data()) + curr_len, *i_strings, additional_len + 1);

            i_strings++;
        }

        std::cout << static_cast<char *>(buff.data()) << std::endl;
    }
    //! [lifo_buffer example 1]

    void lifo_examples()
    {
        concat_and_print("Hello", " world!");

        lifo_array_example_2();

        const char * strings[] = {"Oh, ",
                                  "Hello ",
                                  "world: ",
                                  "this ",
                                  "is ",
                                  "an ",
                                  "array ",
                                  "of ",
                                  "strings!!",
                                  nullptr};
        concat_and_print(strings);
    }

} // namespace density_tests
