
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/heterogeneous_array.h"
#include "../testity/testity_common.h"
#include "../testity/test_tree.h"
#include "container_test.h"
#include "test_allocators.h"
#include <algorithm>


namespace density_tests
{
	using namespace density;
	using namespace testity;

	template <typename TYPE>
		using HeterogeneousArray = heterogeneous_array<TYPE, TestVoidAllocator, runtime_type<TYPE,
			typename type_features::feature_concat< typename type_features::default_type_features_t<TYPE>, type_features::feature_list<type_features::hash, type_features::equals> >::type> >;

	template <typename TYPE>
		struct HeterogeneousArrayTest
	{		
		HeterogeneousArray<TYPE> m_array;
		ShadowContainer<HeterogeneousArray<TYPE>> m_shadow;

		void compare() const
		{
			m_shadow.compare_all(m_array);
		}
	};


	template <typename TYPE>
		void add_typed_heterogeneous_array_cases(TestTree & i_dest)
	{
		using TestTarget = HeterogeneousArrayTest<TYPE>;
		using TestFunc = std::function< void(std::mt19937 & i_random, TestTarget & i_target)>;

		i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
			HeterogeneousArray<TYPE> tmp_array;
			TESTITY_ASSERT(tmp_array.size() == 0);
			TESTITY_ASSERT(tmp_array.empty());
			TESTITY_ASSERT(tmp_array.begin() == tmp_array.end());
			tmp_array = i_target.m_array;
			TESTITY_ASSERT(tmp_array == i_target.m_array);

			HeterogeneousArray<TYPE> tmp_array_1(tmp_array);
			TESTITY_ASSERT(tmp_array_1 == i_target.m_array);

			HeterogeneousArray<TYPE> tmp_array_2(std::move(tmp_array));
			TESTITY_ASSERT(tmp_array_2 == i_target.m_array);
			TESTITY_ASSERT(tmp_array.size() == 0);

			tmp_array = std::move(tmp_array_2);
			TESTITY_ASSERT(tmp_array == i_target.m_array);
			TESTITY_ASSERT(tmp_array_2.size() == 0);

			tmp_array.clear();
			TESTITY_ASSERT(tmp_array.size() == 0);
		}));

		i_dest.add_case(TestFunc([](std::mt19937 & /*i_random*/, TestTarget & i_target) {
			i_target.m_array.push_back(1);
			i_target.m_shadow.push_back(1);
			i_target.compare();
		}));
	}

	void add_heterogeneous_array_cases(TestTree & i_dest)
	{
		add_typed_heterogeneous_array_cases<void>(i_dest["void"]);

	}

    /* TestHeterogeneousArray<TYPE> - heterogeneous_array that uses TestAllocator and adds type_features::hash to the automatic runtime type */
    template <typename TYPE>
        using TestHeterogeneousArray = heterogeneous_array<TYPE, TestVoidAllocator, runtime_type<TYPE,
            typename type_features::feature_concat< typename type_features::default_type_features_t<TYPE>, type_features::hash >::type> >;

#if 0

    template <typename COMPLETE_ELEMENT, typename BASE_ELEMENT, typename... CONSTRUCTION_PARAMS>
        void add_test_case_add_by_copy(
            ContainerTest<TestHeterogeneousArray<BASE_ELEMENT>> & i_test,
            double i_probability,
            CONSTRUCTION_PARAMS && ... i_construction_parameters )
    {
        const double probability = i_probability / 4.;

        i_test.add_test_case("push_back_by_copy", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
            const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
            for (unsigned i = 0; i < times; i++)
            {
                const COMPLETE_ELEMENT new_element(i_construction_parameters...);
                i_test.dense_container().push_back(new_element);
                i_test.shadow_container().push_back(new_element);
            }
        }, probability);

        i_test.add_test_case("push_front_by_copy", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
            const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
            for (unsigned i = 0; i < times; i++)
            {
                const COMPLETE_ELEMENT new_element(i_construction_parameters...);
                i_test.dense_container().push_front(new_element);
                i_test.shadow_container().push_front(new_element);
            }
        }, probability);

        i_test.add_test_case("insert_by_copy", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
            const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
            for (unsigned i = 0; i < times; i++)
            {
                const COMPLETE_ELEMENT new_element(i_construction_parameters...);
                const auto at_index = std::uniform_int_distribution<size_t>(0, i_test.shadow_container().size())(i_random);
                i_test.dense_container().insert(std::next(i_test.dense_container().begin(), at_index), new_element);
                i_test.shadow_container().insert_at(at_index, new_element);
            }
        }, probability);

        i_test.add_test_case("insert_n_by_copy", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
            const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
            for (unsigned i = 0; i < times; i++)
            {
                const COMPLETE_ELEMENT new_element(i_construction_parameters...);
                const auto at_index = std::uniform_int_distribution<size_t>(0, i_test.shadow_container().size())(i_random);
                const auto count = std::uniform_int_distribution<size_t>(0, 3)(i_random);
                i_test.dense_container().insert(std::next(i_test.dense_container().begin(), at_index), count, new_element);
                i_test.shadow_container().insert_at(at_index, new_element, count);
            }
        }, probability);
    }

    template <typename COMPLETE_ELEMENT, typename BASE_ELEMENT, typename... CONSTRUCTION_PARAMS>
        void add_test_case_add_by_move(
            ContainerTest<TestHeterogeneousArray<BASE_ELEMENT>> & i_test,
            double i_probability,
            CONSTRUCTION_PARAMS && ... i_construction_parameters )
    {
        const double probability = i_probability / 3.;

        i_test.add_test_case("push_back_by_move", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
            const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
            for (unsigned i = 0; i < times; i++)
            {
                // if the constructor of shadow_element throws, dense_container is left unchanged
                COMPLETE_ELEMENT shadow_element(i_construction_parameters...);
                i_test.dense_container().push_back(COMPLETE_ELEMENT(i_construction_parameters...));
                i_test.shadow_container().push_back(shadow_element);
            }
        }, probability);

        i_test.add_test_case("push_front_by_move", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
            const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
            for (unsigned i = 0; i < times; i++)
            {
                // if the constructor of shadow_element throws, dense_container is left unchanged
                COMPLETE_ELEMENT shadow_element(i_construction_parameters...);
                i_test.dense_container().push_front(COMPLETE_ELEMENT(i_construction_parameters...));
                i_test.shadow_container().push_front(shadow_element);
            }
        }, probability);

        i_test.add_test_case("insert_by_move", [&i_test, &i_construction_parameters...](std::mt19937 & i_random) {
            const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
            for (unsigned i = 0; i < times; i++)
            {
                // if the constructor of shadow_element throws, dense_container is left unchanged
                COMPLETE_ELEMENT shadow_element(i_construction_parameters...);
                const auto at_index = std::uniform_int_distribution<size_t>(0, i_test.shadow_container().size())(i_random);
                i_test.dense_container().insert(std::next(i_test.dense_container().begin(), at_index), COMPLETE_ELEMENT(i_construction_parameters...));
                i_test.shadow_container().insert_at(at_index, shadow_element);
            }
        }, probability);
    }

    template <typename BASE_ELEMENT>
        void add_test_case_erase( ContainerTest<TestHeterogeneousArray<BASE_ELEMENT>> & i_test, double i_probability )
    {
        const double probability = i_probability / 2.;

        i_test.add_test_case("erase", [&i_test](std::mt19937 & i_random) {
            const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
            for (unsigned i = 0; i < times; i++)
            {
                if (!i_test.dense_container().empty())
                {
                    const auto at_index = std::uniform_int_distribution<size_t>(0, i_test.shadow_container().size() - 1)(i_random);
                    i_test.dense_container().erase(std::next(i_test.dense_container().begin(), at_index));
                    i_test.shadow_container().erase_at(at_index);
                }
            }
        }, probability);

        i_test.add_test_case("erase_n", [&i_test](std::mt19937 & i_random) {
            const auto times = std::uniform_int_distribution<unsigned>(0, 3)(i_random);
            for (unsigned i = 0; i < times; i++)
            {
                if (!i_test.dense_container().empty())
                {
                    const auto at_index = std::uniform_int_distribution<size_t>(0, i_test.shadow_container().size())(i_random);
                    const auto count = std::uniform_int_distribution<size_t>(at_index, i_test.shadow_container().size())(i_random) - at_index;
                    i_test.dense_container().erase(std::next(i_test.dense_container().begin(), at_index),
                        std::next(i_test.dense_container().begin(), (at_index + count)));
                    i_test.shadow_container().erase_at(at_index, count);
                }
            }
        }, probability);
    }

    void list_test_impl(std::mt19937 & i_random, const char * i_container_name)
    {
		{
            ContainerTest<TestHeterogeneousArray<void>> test(i_container_name);
            add_test_case_add_by_copy<TestObjectBase>(test, 1., i_random);
            const auto rand_size_t = std::uniform_int_distribution<size_t>()(i_random); // this is a number used to initialize the instances of TestObjectBase
            add_test_case_add_by_move<TestObjectBase>(test, 1., rand_size_t);
            add_test_case_copy_and_assign(test, .1);
            add_test_case_erase(test, .1);
            test.run(i_random);
        }

        {
            ContainerTest<TestHeterogeneousArray<TestObjectBase>> test(i_container_name);
            add_test_case_add_by_copy<TestObjectBase>(test, 1., i_random);
            const auto rand_size_t = std::uniform_int_distribution<size_t>()(i_random); // this is a number used to initialize the instances of TestObjectBase
            add_test_case_add_by_move<TestObjectBase>(test, 1., rand_size_t);
            add_test_case_copy_and_assign(test, .1);
            add_test_case_erase(test, .1);
            test.run(i_random);
        }
    }

} // namespace tests

void list_test()
{
    // code snippets included in the documentation

    {
        using namespace density;
        using namespace std;
        auto list = heterogeneous_array<>::make(3 + 5, string("abc"), 42.f);
        list.push_front(wstring(L"ABC"));
        for (auto it = list.begin(); it != list.end(); it++)
        {
            cout << it.complete_type().type_info().name() << endl;
        }
    }

    {
        using namespace density;
        using namespace std;

        struct Widget { virtual void draw() { /* ... */ } };
        struct TextWidget : Widget { virtual void draw() override { /* ... */ } };
        struct ImageWidget : Widget { virtual void draw() override { /* ... */ } };

        auto widgets = heterogeneous_array<Widget>::make(TextWidget(), ImageWidget(), TextWidget());
        for (auto & widget : widgets)
        {
            widget.draw();
        }

        widgets.push_back(TextWidget());
    }

    // end of code snippets included in the documentation

    using namespace tests;

    std::mt19937 random;
    list_test_impl(random, "heterogeneous_array");

    run_exception_stress_test([] {
        std::mt19937 random;
        list_test_impl(random, "heterogeneous_array");
    });

#endif
	
} // namespace density_tests
