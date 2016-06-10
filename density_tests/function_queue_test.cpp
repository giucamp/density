
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/dense_function_queue.h"
#include "../density/paged_function_queue.h"
#include <random>
#include <iostream>

namespace density
{
    void function_queue_test()
    {
		// code snippets included in the documentation

		{
			dense_function_queue<void()> queue_1;
			queue_1.push([]() { std::cout << "hello!" << std::endl; });
			queue_1.consume_front();

			dense_function_queue<int(double, double)> queue_2;
			queue_2.push([](double a, double b) { return static_cast<int>(a + b); });
			std::cout << queue_2.consume_front(40., 2.) << std::endl;
		}

		{
			paged_function_queue<void()> queue_1;
			queue_1.push([]() { std::cout << "hello!" << std::endl; });
			queue_1.consume_front();

			paged_function_queue<int(double, double)> queue_2;
			queue_2.push([](double a, double b) { return static_cast<int>(a + b); });
			std::cout << queue_2.consume_front(40., 2.) << std::endl;
		}

		// end of code snippets included in the documentation
    }
}
