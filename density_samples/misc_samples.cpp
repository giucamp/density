
#include "../density/runtime_type.h"
#include "../density/array_any.h"
#include <string>
#include <iostream>

namespace misc_samples
{
	void run()
	{
		{
			//! [feature_list example 1]
	using namespace density;
	using namespace type_features;
	using MyFeatures = feature_list<default_construct, size, alignment>;
			//! [feature_list example 1]
			MyFeatures unused; (void)unused;
		}

		{
			//! [feature_concat example 1]
	using namespace density;
	using namespace type_features;
	using MyPartialFeatures = feature_list<default_construct, size>;
	using MyFeatures = feature_concat_t<MyPartialFeatures, alignment>;
	using MyFeatures1 = feature_concat_t<MyPartialFeatures, feature_list<alignment> >;
	static_assert(std::is_same<MyFeatures, MyFeatures1>::value, "");
			//! [feature_concat example 1]
		}

		{
			//! [type_features::invoke example 1]

	using namespace density;
	using namespace type_features;

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

		{
			//! [runtime_type example 1]

	using namespace density;
	using namespace type_features;

	using MyRTType = runtime_type<void, feature_list<default_construct, destroy, size> >;

	MyRTType type = MyRTType::make<std::string>();

	void * buff = malloc(type.size());
	
	type.default_construct(buff);

	// now buff points to a valid std::string
	*static_cast<std::string*>(buff) = "hello world!";

	type.destroy(buff);

	free(buff);

			//! [runtime_type example 1]
		}
	}
} // namespace misc_samples
	