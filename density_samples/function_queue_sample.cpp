
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/dense_function_queue.h"
#include "../density/paged_function_queue.h"
#include <iostream>
#include <string>
#include <functional> // for std::bind

namespace function_queue_sample
{
	using namespace density;

	void run()
	{
		{
			auto print_func = [](const char * i_str) { std::cout << i_str; };

			dense_function_queue<void()> queue_1;
			queue_1.push(std::bind(print_func, "hello "));
			queue_1.push([print_func]() { print_func("world!"); });
			queue_1.push([]() { std::cout << std::endl; });
			while(!queue_1.empty())
				queue_1.consume_front();

			dense_function_queue<int(double, double)> queue_2;
			queue_2.push([](double a, double b) { return static_cast<int>(a + b); });
			std::cout << "a + b = " << queue_2.consume_front(40., 2.) << std::endl;
		}

		{
			auto print_func = [](const char * i_str) { std::cout << i_str; };

			paged_function_queue<void()> queue_1;
			queue_1.push(std::bind(print_func, "hello "));
			queue_1.push([print_func]() { print_func("world!"); });
			queue_1.push([]() { std::cout << std::endl; });
			queue_1.consume_front();
			while (!queue_1.empty())
				queue_1.consume_front();

			paged_function_queue<int(double, double)> queue_2;
			queue_2.push([](double a, double b) { return static_cast<int>(a + b); });
			std::cout << "a + b = " << queue_2.consume_front(40., 2.) << std::endl;
		}
	}
}