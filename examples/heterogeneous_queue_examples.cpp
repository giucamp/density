
#include <string>
#include <iostream>
#include <iterator>
#include <complex>
#include <string>
#include <assert.h>
#include <density/heterogeneous_queue.h>
#include <density/io_runtimetype_features.h>
#include "../density_tests/test_framework/progress.h"

// if assert expands to nothing, some local variable becomes unused
#if defined(_MSC_VER) && defined(NDEBUG)
	#pragma warning(push)
	#pragma warning(disable:4189) // local variable is initialized but not referenced
#endif

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

void heterogeneous_queue_samples_2()
{
	using namespace density;
{
	heterogeneous_queue<> queue;

	//! [heterogeneous_queue push example 1]
	queue.push(12);
	queue.push(std::string("Hello world!!"));
	//! [heterogeneous_queue push example 1]	

	//! [heterogeneous_queue emplace example 1]
	queue.emplace<int>();
	queue.emplace<std::string>(12, '-');
	//! [heterogeneous_queue emplace example 1]	

{
	//! [heterogeneous_queue start_push example 1]
	auto put = queue.start_push(12);
	put.element() += 2;
	put.commit(); // commits a 14
	//! [heterogeneous_queue start_push example 1]	
}
{
	//! [heterogeneous_queue start_emplace example 1]
	auto put = queue.start_emplace<std::string>(4, '*');
	put.element() += "****";
	put.commit(); // commits a "********"
	//! [heterogeneous_queue start_emplace example 1]	
}
}
{
	//! [heterogeneous_queue dyn_push example 1]
	using namespace type_features;
	using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
	heterogeneous_queue<void, MyRunTimeType> queue;

	auto const type = MyRunTimeType::make<int>();
	queue.dyn_push(type); // appends 0
	//! [heterogeneous_queue dyn_push example 1]
}
{
	//! [heterogeneous_queue dyn_push_copy example 1]
	using namespace type_features;
	using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
	heterogeneous_queue<void, MyRunTimeType> queue;

	std::string const source("Hello world!!");
	auto const type = MyRunTimeType::make<decltype(source)>();
	queue.dyn_push_copy(type, &source);
	//! [heterogeneous_queue dyn_push_copy example 1]
}
{
	//! [heterogeneous_queue dyn_push_move example 1]
	using namespace type_features;
	using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
	heterogeneous_queue<void, MyRunTimeType> queue;

	std::string source("Hello world!!");
	auto const type = MyRunTimeType::make<decltype(source)>();
	queue.dyn_push_move(type, &source);
	//! [heterogeneous_queue dyn_push_move example 1]
}

{
	//! [heterogeneous_queue start_dyn_push example 1]
	using namespace type_features;
	using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
	heterogeneous_queue<void, MyRunTimeType> queue;

	auto const type = MyRunTimeType::make<int>();
	auto put = queue.start_dyn_push(type);
	put.commit();
	//! [heterogeneous_queue start_dyn_push example 1]
}
{
	//! [heterogeneous_queue start_dyn_push_copy example 1]
	using namespace type_features;
	using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
	heterogeneous_queue<void, MyRunTimeType> queue;

	std::string const source("Hello world!!");
	auto const type = MyRunTimeType::make<decltype(source)>();
	auto put = queue.start_dyn_push_copy(type, &source);
	put.commit();
	//! [heterogeneous_queue start_dyn_push_copy example 1]
}
{
	//! [heterogeneous_queue start_dyn_push_move example 1]
	using namespace type_features;
	using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
	heterogeneous_queue<void, MyRunTimeType> queue;

	std::string source("Hello world!!");
	auto const type = MyRunTimeType::make<decltype(source)>();
	auto put = queue.start_dyn_push_move(type, &source);
	put.commit();
	//! [heterogeneous_queue start_dyn_push_move example 1]
}
{
	heterogeneous_queue<> queue;

	//! [heterogeneous_queue reentrant example 1]
	auto put_1 = queue.start_reentrant_push(1);
	auto put_2 = queue.start_reentrant_emplace<std::string>("Hello world!");
	double pi = 3.14;
	auto put_3 = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<double>(), &pi);
	assert(queue.empty());

	put_2.commit();

	auto consume2 = queue.try_start_consume();
	assert(!consume2.empty() && consume2.complete_type().is<std::string>());

	put_1.commit();

	auto consume1 = queue.try_start_consume();
	assert(!consume1.empty() && consume1.complete_type().is<int>());

	put_3.cancel();
	consume1.commit();
	consume2.commit();
	assert(queue.empty());

	//! [heterogeneous_queue reentrant example 1]
}

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
	//! [heterogeneous_queue example 2]
	auto consume = queue.try_start_consume();
	int my_int = consume.element<int>();
	consume.commit();

	consume = queue.try_start_consume();
	std::string my_string = consume.element<std::string>();
	consume.commit();
	//! [heterogeneous_queue example 3
	(void)my_int;
	(void)my_string;
}

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
	consume.commit();
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


	// this samples uses std::cout and std::cin
	// heterogeneous_queue_samples_1();

	heterogeneous_queue_samples_2();
}



} // namespace density_tests

#if defined(_MSC_VER) && defined(NDEBUG)		
	#pragma warning(pop)
#endif
