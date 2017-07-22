
#include "../test_framework/density_test_common.h"
#include "../test_framework/test_objects.h"
#include "../test_framework/test_allocators.h"
#include "../test_framework/progress.h"
#include "complex_polymorphism.h"
#include <density/heterogeneous_queue.h>
#include <type_traits>
#include <iterator>

namespace density_tests
{
	void heterogeneous_queue_lifetime_tests()
	{
		using namespace density;

		{
			void_allocator allocator;
			heterogeneous_queue<> queue(allocator); // copy construct allocator
			queue.push(1);
			queue.push(2);

			auto queue_copy(queue); // copy construct queue
			DENSITY_TEST_ASSERT(!queue.empty());
			DENSITY_TEST_ASSERT(!queue_copy.empty());
			DENSITY_TEST_ASSERT(std::distance(queue_copy.begin(), queue_copy.end()) == 2);

			auto other_queue(std::move(queue)); // move construct queue - the source becomes empty
			DENSITY_TEST_ASSERT(queue.empty() && !other_queue.empty());
			DENSITY_TEST_ASSERT(std::distance(other_queue.begin(), other_queue.end()) == 2);
			DENSITY_TEST_ASSERT(std::distance(queue.begin(), queue.end()) == 0);

			// test swaps
			swap(queue, other_queue);
			DENSITY_TEST_ASSERT(!queue.empty() && other_queue.empty());
			swap(queue, other_queue);
			DENSITY_TEST_ASSERT(queue.empty() && !other_queue.empty());
			auto cons = other_queue.try_start_consume();
			DENSITY_TEST_ASSERT(cons && cons.complete_type().is<int>() && cons.element<int>() == 1);
			cons.commit();
			cons = other_queue.try_start_consume();
			DENSITY_TEST_ASSERT(cons && cons.complete_type().is<int>() && cons.element<int>() == 2);
			cons.commit();
			DENSITY_TEST_ASSERT(other_queue.empty());

			// test allocator getters
			move_only_void_allocator movable_alloc(5);
			heterogeneous_queue<void, runtime_type<>, move_only_void_allocator> move_only_queue(std::move(movable_alloc));

			auto allocator_copy = other_queue.get_allocator();
			(void)allocator_copy;

			move_only_queue.push(1);
			move_only_queue.push(2);

			move_only_queue.get_allocator_ref().dummy_func();

			auto const & const_move_only_queue(move_only_queue);
			const_move_only_queue.get_allocator_ref().const_dummy_func();
		}

		//move_only_void_allocator
	}

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

	template<typename ELEMENT, typename QUEUE>
		void dynamic_pushes(QUEUE & i_queue)
	{
		auto const type = QUEUE::runtime_type::template make<ELEMENT>();
		
		i_queue.dyn_push(type);
		
		ELEMENT copy_source;
		i_queue.dyn_push_copy(type, &copy_source);

		ELEMENT move_source;
		i_queue.dyn_push_move(type, &move_source);
	}

	/** Test heterogeneous_queue with a non-polymorphic base */
	void heterogeneous_queue_basic_nonpolymorphic_base_tests()
	{
		using namespace density;
		using namespace type_features;
		using RunTimeType = runtime_type<NonPolymorphicBase, feature_list<
			default_construct, move_construct, copy_construct, destroy, size, alignment>>;
		heterogeneous_queue<NonPolymorphicBase, RunTimeType> queue;

		queue.push(NonPolymorphicBase());
		queue.emplace<SingleDerivedNonPoly>();

		dynamic_pushes<NonPolymorphicBase>(queue);
		dynamic_pushes<SingleDerivedNonPoly>(queue);

		for (const auto & val : queue)
		{
			val.second->check();
		}

		for (;;)
		{
			auto consume = queue.try_start_consume();
			if (!consume)
				break;

			if (consume.complete_type().is<NonPolymorphicBase>())
			{
				consume.element<NonPolymorphicBase>().check();
			}
			else
			{
				DENSITY_TEST_ASSERT(consume.complete_type().is<SingleDerivedNonPoly>());
				consume.element<SingleDerivedNonPoly>().check();
			}
			consume.commit();
		}

		DENSITY_TEST_ASSERT(queue.empty());
	}

	/** Test heterogeneous_queue with a polymorphic base */
	void heterogeneous_queue_basic_polymorphic_base_tests()
	{
		using namespace density;
		using namespace type_features;
		using RunTimeType = runtime_type<PolymorphicBase, feature_list<
			default_construct, move_construct, copy_construct, destroy, size, alignment>>;
		heterogeneous_queue<PolymorphicBase, RunTimeType> queue;

		queue.push(PolymorphicBase());
		queue.emplace<SingleDerived>();
		queue.emplace<Derived1>();
		queue.emplace<Derived2>();
		queue.emplace<MultipleDerived>();

		dynamic_pushes<PolymorphicBase>(queue);
		dynamic_pushes<SingleDerived>(queue);
		dynamic_pushes<Derived1>(queue);
		dynamic_pushes<Derived2>(queue);
		dynamic_pushes<MultipleDerived>(queue);

		int const put_count = 5 * 4;

		int elements = 0;
		for (const auto & val : queue)
		{
			elements++;
			val.second->check();
		}
		DENSITY_TEST_ASSERT(elements == put_count);

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

		DENSITY_TEST_ASSERT(consumed == put_count);
		DENSITY_TEST_ASSERT(queue.empty());
	}

	/** Basic tests for heterogeneous_queue<...> */
	void heterogeneous_queue_basic_tests(std::ostream & i_ostream)
	{
		PrintScopeDuration(i_ostream, "heterogeneous queue basic tests");

		heterogeneous_queue_lifetime_tests();

		heterogeneous_queue_basic_nonpolymorphic_base_tests();

		heterogeneous_queue_basic_polymorphic_base_tests();

		using namespace density;
		
		heterogeneous_queue_basic_void_tests<heterogeneous_queue<>>();

		heterogeneous_queue_basic_void_tests<heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>();

		heterogeneous_queue_basic_void_tests<heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>();
	}
}