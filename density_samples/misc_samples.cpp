
#include "../density/runtime_type.h"
#include "../density/array_any.h"
#include <string>
#include <iostream>

namespace misc_samples
{
	void run()
	{
		using namespace density;
		using namespace type_features;

		{
			//! [feature_list example 1]
	using MyFeatures = feature_list<default_construct, size, alignment>;
			//! [feature_list example 1]
			MyFeatures unused; (void)unused;
		}

		{
			//! [feature_concat example 1]
	using MyPartialFeatures = feature_list<default_construct, size>;
	using MyFeatures = feature_concat<MyPartialFeatures, alignment>::type;
	using MyFeatures1 = feature_concat<MyPartialFeatures, feature_list<alignment> >::type;
	static_assert(std::is_same<MyFeatures, MyFeatures1>::value, "");
			//! [feature_concat example 1]
		}

		{
			//! [type_features::invoke example 1]
	// This feature allows to call a function object passing a std::string
	using MyInvoke = invoke<void(std::string)>;

	// since we are going to declare a type-erased container, we must include the default features (size, alignment, ...)
	using MyFeatures = feature_concat_t<default_type_features_t<void>, MyInvoke >;
			
	// alias for a specific array_any. Until now we have just declared types
	using ArrayOfInvokables = array_any<void, std::allocator<void>, runtime_type<void, MyFeatures> >;
			
	// instances an array with a lambda expression as only element
	auto my_array = ArrayOfInvokables::make([](std::string s) { std::cout << s << std::endl; });
			
	// invoke my_array.begin()
	my_array.begin().complete_type().get_feature<MyInvoke>()(my_array.begin().element(), "hello!");
			//! [type_features::invoke example 1]
		}
	}
} // namespace misc_samples
	