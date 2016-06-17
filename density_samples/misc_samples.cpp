
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
	using MyFeatures = FeatureList<DefaultConstruct, Size, Alignment>;
				//! [FeatureList example 1]
				MyFeatures unused; (void)unused;
			}

			{
				//! [FeatureConcat example 1]
	using MyPartialFeatures = FeatureList<DefaultConstruct, Size>;
	using MyFeatures = FeatureConcat<MyPartialFeatures, Alignment>::type;
	using MyFeatures1 = FeatureConcat<MyPartialFeatures, FeatureList<Alignment> >::type;
	static_assert(std::is_same<MyFeatures, MyFeatures1>::value, "???");
				//! [FeatureConcat example 1]
			}
		}

	} // namespace detail

} // namespace density
