
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/dense_list.h"
#include "../testity/test_tree.h"
#include <functional>
#include <vector>
#include <memory>

namespace density
{
	namespace tests
	{
		using namespace testity;

		PerformanceTestGroup make_list_benchmarks()
		{
			PerformanceTestGroup group("iterate dense_list", "density version: " + std::to_string(DENSITY_VERSION));
			
			using namespace std;

			struct Widget { float a; };
			struct TextWidget : Widget { char s[256]; };
			struct ImageWidget : Widget { double e, f, g; };

			static auto ptr_vector = []() {			
				vector<unique_ptr<Widget>> res;
				for (size_t i = 0; i < 3000; i++)
				{
					switch (i % 3)
					{
						case 0: res.push_back(make_unique<Widget>()); break;
						case 1: res.push_back(make_unique<TextWidget>()); break;
						case 2: res.push_back(make_unique<ImageWidget>()); break;
						default: break;
					}
				}
				return res;
			}();

			static auto den_list = []() {
				dense_list<Widget> list;
				for (size_t i = 0; i < 3000; i++)
				{
					switch (i % 3)
					{
						case 0: list.push_back(Widget()); break;
						case 1: list.push_back(TextWidget()); break;
						case 2: list.push_back(ImageWidget()); break;
						default: break;
					}
				}
				return list;
			}();
			
			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				size_t index = 0;
				for (const auto & wid : ptr_vector)
				{
					wid->a = 0.f;
					if (index > i_cardinality) break;
				}
			}, __LINE__);

			group.add_test(__FILE__, __LINE__, [](size_t i_cardinality) {
				size_t index = 0;
				for (auto & wid : den_list)
				{
					wid.a = 0.f;
					if (index > i_cardinality) break;
				}
			}, __LINE__);

			return group;
		}

	} // namespace tests

} // namespace density