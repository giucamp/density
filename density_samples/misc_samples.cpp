
#include "../density/runtime_type.h"

namespace density
{
	namespace detail
	{

		void feature_list_samples()
		{
			using namespace type_features;

			{
				//! [FeatureList example 1]
	using MyFeatures = FeatureList<FeatureDefaultConstruct, FeatureSize, FeatureAlignment>;
				//! [FeatureList example 1]
				MyFeatures unused; (void)unused;
			}

			{
				//! [FeatureConcat example 1]
	using MyPartialFeatures = FeatureList<FeatureDefaultConstruct, FeatureSize>;
	using MyFeatures = FeatureConcat<MyPartialFeatures, FeatureAlignment>::type;
	using MyFeatures1 = FeatureConcat<MyPartialFeatures, FeatureList<FeatureAlignment> >::type;
	static_assert(std::is_same<MyFeatures, MyFeatures1>::value, "???");
				//! [FeatureConcat example 1]
			}
		}

	} // namespace detail

} // namespace density
