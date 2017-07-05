#pragma once
#include "../test_framework/density_test_common.h"
#include "../test_framework/test_objects.h"
#include "../test_framework/test_allocators.h"
#include "complex_polymorphism.h"
#include <density/heterogeneous_queue.h>
#include <type_traits>

namespace density_tests
{
	/** Basic tests heterogeneous_queue<void, ...> with a non-polymorphic base */
	template <typename QUEUE>
		void heterogeneous_queue_basic_void_tests()
	{
		static_assert(std::is_void<typename QUEUE::common_type>::value, "");

		{
			QUEUE queue;
			DENSITY_TEST_ASSERT(queue.empty());
			DENSITY_TEST_ASSERT(queue.begin() == queue.end());
			DENSITY_TEST_ASSERT(queue.cbegin() == queue.cend());
		}

		{
			QUEUE queue;
			queue.clear();

			queue.push(1);
			DENSITY_TEST_ASSERT(!queue.empty());
			DENSITY_TEST_ASSERT(queue.begin() != queue.end());
			DENSITY_TEST_ASSERT(queue.cbegin() != queue.cend());

			queue.clear();
			DENSITY_TEST_ASSERT(queue.empty());
			DENSITY_TEST_ASSERT(queue.begin() == queue.end());
			DENSITY_TEST_ASSERT(queue.cbegin() == queue.cend());
			queue.clear();
		}
	}


	/** Test heterogeneous_queue with a non-polymorphic base */
	void heterogeneous_queue_basic_nonpolymorphic_base_tests()
	{
		density::heterogeneous_queue<NonPolymorphicBase> queue;
		queue.push(NonPolymorphicBase());
		queue.emplace<SingleDerivedNonPoly>();

		for (const auto & val : queue)
		{
			val.second->check();
		}

		auto consume = queue.try_start_consume();
		consume.element<NonPolymorphicBase>().check();
		consume.commit();

		consume = queue.try_start_consume();
		consume.element<SingleDerivedNonPoly>().check();
		consume.commit();

		DENSITY_TEST_ASSERT(queue.empty());
	}

	/** Test heterogeneous_queue with a polymorphic base */
	void heterogeneous_queue_basic_polymorphic_base_tests()
	{
		density::heterogeneous_queue<PolymorphicBase> queue;

		queue.push(PolymorphicBase());
		queue.emplace<SingleDerived>();
		queue.emplace<Derived1>();
		queue.emplace<Derived2>();
		queue.emplace<MultipleDerived>();

		int elements = 0;
		for (const auto & val : queue)
		{
			elements++;
			val.second->check();
		}
		DENSITY_TEST_ASSERT(elements == 5);

		int consumed = 0;
		for (;;)
		{
			auto consume = queue.try_start_consume();
			if (!consume)
				break;
			consumed++;
			if (consume.complete_type().is<PolymorphicBase>())
			{
				DENSITY_TEST_ASSERT(consume.element_ptr()->class_id() == PolymorphicBase::s_class_id);
			}
			else if (consume.complete_type().is<SingleDerived>())
			{
				DENSITY_TEST_ASSERT(consume.element_ptr()->class_id() == SingleDerived::s_class_id);
			}
			else if (consume.complete_type().is<Derived1>())
			{
				DENSITY_TEST_ASSERT(consume.element_ptr()->class_id() == Derived1::s_class_id);
			}
			else if (consume.complete_type().is<Derived2>())
			{
				DENSITY_TEST_ASSERT(consume.element_ptr()->class_id() == Derived2::s_class_id);
			}
			else if (consume.complete_type().is<MultipleDerived>())
			{
				DENSITY_TEST_ASSERT(consume.element_ptr()->class_id() == MultipleDerived::s_class_id);
			}
			consume.commit();
		}

		DENSITY_TEST_ASSERT(consumed == 5);
		DENSITY_TEST_ASSERT(queue.empty());
	}

	/** Basic tests for heterogeneous_queue<...> */
	void heterogeneous_queue_basic_tests()
	{
		heterogeneous_queue_basic_nonpolymorphic_base_tests();

		heterogeneous_queue_basic_polymorphic_base_tests();

		using namespace density;
		
		heterogeneous_queue_basic_void_tests<heterogeneous_queue<>>();

		heterogeneous_queue_basic_void_tests<heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>();

		heterogeneous_queue_basic_void_tests<heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>();
	}
}