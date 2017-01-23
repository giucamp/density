
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/small_function_queue.h>
#include <density/function_queue.h>
#include <density/lifo.h>
#include <string>
#include <functional> // for std::bind
#include <iostream>
#include <utility>

namespace heter_queue_samples
{
    void run()
    {

	{
		//! [heter_queue push example 1]

	using namespace density;
	heterogeneous_queue<> queue;

	queue.push(std::string("abc")); // move-construct

	std::string s("def");
	queue.push(s); // copy-construct

		//! [heter_queue push example 1]
	}
	{
		//! [heter_queue emplace example 1]

	using namespace density;
	heterogeneous_queue<> queue;

	// insert an int
	queue.emplace<int>();

	// check the type and the value
	auto it = queue.cbegin();
	assert(it.complete_type() == runtime_type<>::make<int>() );
	assert(*static_cast<const int*>(it.element()) == 0);

	queue.emplace<std::string>("abc"); // move-construct

	std::string s("def");
	queue.emplace<std::string>(s); // copy-construct
	
		//! [heter_queue emplace example 1]
	}
	{
		//! [heter_queue push_by_copy example 1]

	using namespace density;
	heterogeneous_queue<> queue;

	std::string s("abc");
	auto const source_ptr = static_cast<const void*>(&s);
	auto const type = runtime_type<>::make<std::string>();
	queue.push_by_copy(type, source_ptr); // move-construct


		//! [heter_queue push_by_copy example 1]
	}
	{
		//! [heter_queue push_by_move example 1]

	using namespace density;
	heterogeneous_queue<> queue;

	std::string s("abc");
	auto const source_ptr = static_cast<const void*>(&s);
	auto const type = runtime_type<>::make<std::string>();
	queue.push_by_copy(type, source_ptr); // move-construct


		//! [heter_queue push_by_move example 1]
	}
	{
		//! [heter_queue begin_push example 1]

	using namespace density;
	heterogeneous_queue<> queue;

	queue.begin_push(std::string("abc")).commit(); // move-construct

	std::string s("def");
	queue.begin_push(s).commit(); // copy-construct

		//! [heter_queue begin_push example 1]
	}
    } // void run()

} // namespace heter_queue_samples
