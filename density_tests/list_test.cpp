

//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/dense_list.h"
#include "../testity/testing_utils.h"
#include "container_test.h"
#include <algorithm>


namespace density
{
	namespace tests
	{

		/* TestDenseList<TYPE> - dense_list that uses TestAllocator and adds detail::FeatureHash to the automatic runtime type */
		template <typename TYPE>
			using TestDenseList = dense_list<TYPE, TestAllocator<TYPE>, runtime_type<TYPE,
				typename detail::FeatureConcat< typename detail::AutoGetFeatures<TYPE>::type, detail::FeatureHash >::type> >;

        template <typename COMPLETE_ELEMENT, typename BASE_ELEMENT, typename... CONSTRUCTION_PARAMS>
            void add_test_case_push_back_by_copy(
                ContainerTest<TestDenseList<BASE_ELEMENT>> & i_test,
                double i_probability,
                CONSTRUCTION_PARAMS && ... i_construction_parameters )
        {
            i_test.add_test_case("push_back_by_copy", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
                const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
                for (unsigned i = 0; i < times; i++)
                {
                    const COMPLETE_ELEMENT new_element(i_construction_parameters...);
                    i_test.dense_container().push_back(new_element);
                    i_test.shadow_container().push_back(new_element);
                }
            }, i_probability);
        }

		template <typename COMPLETE_ELEMENT, typename BASE_ELEMENT, typename... CONSTRUCTION_PARAMS>
            void add_test_case_push_back_by_move(
                ContainerTest<TestDenseList<BASE_ELEMENT>> & i_test,
                double i_probability,
                CONSTRUCTION_PARAMS && ... i_construction_parameters )
        {
            i_test.add_test_case("push_back_by_move", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
                const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
                for (unsigned i = 0; i < times; i++)
                {
                    i_test.dense_container().push_back(COMPLETE_ELEMENT(i_construction_parameters...));
                    i_test.shadow_container().push_back(COMPLETE_ELEMENT(i_construction_parameters...));
                }
            }, i_probability);
        }

        void list_test_impl(std::mt19937 & i_random, const char * i_container_name)
		{
			NoLeakScope no_leak_scope;

			{
				ContainerTest<TestDenseList<void>> test(i_container_name);
				add_test_case_push_back_by_copy<TestObjectBase>(test, 1., i_random);				
				const auto rand_size_t = std::uniform_int_distribution<size_t>()(i_random);
				add_test_case_push_back_by_move<TestObjectBase>(test, 1., rand_size_t);
				add_test_case_copy_and_assign(test, .1);
				test.run(i_random);
			}

			{
				ContainerTest<TestDenseList<TestObjectBase>> test(i_container_name);
				add_test_case_push_back_by_copy<TestObjectBase>(test, 1., i_random);
				const auto rand_size_t = std::uniform_int_distribution<size_t>()(i_random);
				add_test_case_push_back_by_move<TestObjectBase>(test, 1., rand_size_t);
				add_test_case_copy_and_assign(test, .1);
				test.run(i_random);
			}
		}

	} // namespace tests

	void list_test()
	{
		using namespace tests;

		std::mt19937 random;
		list_test_impl(random, "dense_list");

		run_exception_stress_test([] {
			std::mt19937 random;
			list_test_impl(random, "dense_list");
		});
	}

} // namespace density
