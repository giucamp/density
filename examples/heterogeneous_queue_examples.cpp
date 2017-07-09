
#include <string>
#include <iostream>
#include <iterator>
#include <complex>
#include <string>
#include <assert.h>
#include <density/heterogeneous_queue.h>
#include <density/io_runtimetype_features.h>
#include "../density_tests/test_framework/progress.h"

namespace density_tests
{

void heterogeneous_queue_samples_1()
{
	//! [heterogeneous_queue example 3]
	using namespace density;
	using namespace type_features;

	/* a runtime_type is internally like a pointer to a v-table, but it can 
		contain functions or data (like in the case of size and alignment). */
	using MyRunTimeType = runtime_type<void, feature_list<default_construct, copy_construct, destroy, size, alignment, ostream, istream, rtti>>;
		
	heterogeneous_queue<void, MyRunTimeType> queue;
	queue.push(4);
	queue.push(std::complex<double>(1., 4.));
	queue.emplace<std::string>("Hello!!");
	
	// This would not compile because std::thread does not have a << operator for streams
	// queue.emplace<std::thread>();

	// consume all the elements
	while (auto consume = queue.try_start_consume())
	{
		/* this is like: give me the function at the 6-th row in the v-table. The type ostream 
			is converted to an index at compile time. */
		auto const ostream_feature = consume.complete_type().get_feature<ostream>();
		
		ostream_feature(std::cout, consume.element_ptr()); // this invokes the feature
		std::cout << "\n";
		consume.commit();  // don't forget the commit, otherwise the element will remain in the queue
	}
	//! [heterogeneous_queue example 3]

	//! [heterogeneous_queue example 4]
	// this local function reads from std::cin an object of a given type and puts it in the queue
	auto ask_and_put = [&](const MyRunTimeType & i_type) {

		// for this we exploit the feature rtti that we have included in MyRunTimeType
		std::cout << "Enter a " << i_type.type_info().name() << std::endl;

		auto const istream_feature = i_type.get_feature<istream>();

		auto put = queue.start_dyn_push(i_type);
		istream_feature(std::cin, put.element_ptr());
		
		/* if an exception is thrown before the commit, the put is canceled without ever 
			having observable side effects. */
		put.commit();
	};
	
	ask_and_put(MyRunTimeType::make<int>());
	ask_and_put(MyRunTimeType::make<std::string>());
	//! [heterogeneous_queue example 4]
}

void heterogeneous_queue_samples(std::ostream & i_ostream)
{
	PrintScopeDuration(i_ostream, "heterogeneous queue samples");

	using namespace density;
		
	//! [heterogeneous_queue put example 1]
	heterogeneous_queue<> queue;
	queue.push(19); // the parameter can be an l-value or an r-value
	queue.emplace<std::string>(8, '*'); // pushes "********"
	//! [heterogeneous_queue put example 1]

{
	//! [heterogeneous_queue put example 2]
	struct MessageInABottle
	{
		const char * m_text = nullptr;
	};
	auto transaction = queue.start_emplace<MessageInABottle>();
	transaction.element().m_text = transaction.raw_allocate_copy("Hello world!");
	transaction.commit();
	//! [heterogeneous_queue put example 2]

	//! [heterogeneous_queue consume example 1]
	auto consume = queue.try_start_consume();
	if (consume.complete_type().is<std::string>())
	{
		std::cout << consume.element<std::string>() << std::endl;
	}
	else if (consume.complete_type().is<MessageInABottle>())
	{
		std::cout << consume.element<MessageInABottle>().m_text << std::endl;
	}
	//! [heterogeneous_queue consume example 1]

	//! [heterogeneous_queue iterators example 1]
	heterogeneous_queue<> queue_1, queue_2;
	queue_1.push(42);
	assert(queue_1.end() == queue_2.end() && queue_1.end() == heterogeneous_queue<>::iterator() );

	for (const auto & value : queue_1)
	{
		assert(value.first.is<int>());
		assert(*static_cast<int*>(value.second) == 42);
	}
	//! [heterogeneous_queue iterators example 1]
}

	//! [heterogeneous_queue example 2]
	auto consume = queue.try_start_consume();
	int my_int = consume.element<int>();
	consume.commit();

	consume = queue.try_start_consume();
	std::string my_string = consume.element<std::string>();
	consume.commit();
	//! [heterogeneous_queue example 3]
	
	(void)my_int;
	(void)my_string;

	// this samples uses std::cout and std::cin
	// heterogeneous_queue_samples_1();
}



} // namespace density_tests
