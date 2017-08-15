
#include "../test_framework/density_test_common.h"
#include "../test_framework/test_objects.h"
#include "../test_framework/test_allocators.h"
#include "../test_framework/progress.h"
#include "complex_polymorphism.h"
#include <density/lf_heter_queue.h>
#include <type_traits>
#include <iterator>

namespace density_tests
{
	template < density::concurrency_cardinality PROD_CARDINALITY,
		density::concurrency_cardinality CONSUMER_CARDINALITY,
		density::consistency_model CONSISTENCY_MODEL>
		struct NbQueueBasicTests
	{
		template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = density::runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = density::void_allocator>
			using lf_heter_queue = density::lf_heter_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE>;

		static void nonblocking_heterogeneous_queue_lifetime_tests()
		{
			using density::void_allocator;
			using density::runtime_type;

			{
				void_allocator allocator;
				lf_heter_queue<> queue(allocator); // copy construct allocator
				queue.push(1);
				queue.push(2);

				auto other_queue(std::move(queue)); // move construct queue - the source becomes empty
				DENSITY_TEST_ASSERT(queue.empty() && !other_queue.empty());
			
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
				lf_heter_queue<void, runtime_type<>, move_only_void_allocator> move_only_queue(std::move(movable_alloc));

				auto allocator_copy = other_queue.get_allocator();
				(void)allocator_copy;

				move_only_queue.push(1);
				move_only_queue.push(2);

				move_only_queue.get_allocator_ref().dummy_func();

				auto const & const_move_only_queue(move_only_queue);
				const_move_only_queue.get_allocator_ref().const_dummy_func();
			}
		}

		/** Basic tests lf_heter_queue<void, ...> with a non-polymorphic base */
		template <typename QUEUE>
			static void nonblocking_heterogeneous_queue_basic_void_tests()
		{
			static_assert(std::is_void<typename QUEUE::common_type>::value, "");

			{
				QUEUE queue;
				DENSITY_TEST_ASSERT(queue.empty());
			}

			{
				QUEUE queue;
				queue.clear();

				queue.push(1);
				DENSITY_TEST_ASSERT(!queue.empty());

				queue.clear();
				DENSITY_TEST_ASSERT(queue.empty());
				queue.clear();
			}
		}

		template<typename ELEMENT, typename QUEUE>
			static void dynamic_pushes(QUEUE & i_queue)
		{
			auto const type = QUEUE::runtime_type::template make<ELEMENT>();
		
			i_queue.dyn_push(type);
		
			ELEMENT copy_source;
			i_queue.dyn_push_copy(type, &copy_source);

			ELEMENT move_source;
			i_queue.dyn_push_move(type, &move_source);
		}

		/** Test lf_heter_queue with a non-polymorphic base */
		static void nonblocking_heterogeneous_queue_basic_nonpolymorphic_base_tests()
		{
			using namespace density::type_features;
			using density::runtime_type;
			using density::void_allocator;

			using RunTimeType = runtime_type<NonPolymorphicBase, feature_list<
				default_construct, move_construct, copy_construct, destroy, size, alignment>>;
			lf_heter_queue<NonPolymorphicBase, RunTimeType> queue;

			queue.push(NonPolymorphicBase());
			queue.emplace<SingleDerivedNonPoly>();

			dynamic_pushes<NonPolymorphicBase>(queue);
			dynamic_pushes<SingleDerivedNonPoly>(queue);

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

		/** Test lf_heter_queue with a polymorphic base */
		static void nonblocking_heterogeneous_queue_basic_polymorphic_base_tests()
		{
			using namespace density::type_features;
			using density::runtime_type;
			using density::void_allocator;

			using RunTimeType = runtime_type<PolymorphicBase, feature_list<
				default_construct, move_construct, copy_construct, destroy, size, alignment>>;
			lf_heter_queue<PolymorphicBase, RunTimeType> queue;

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

		static void tests(std::ostream & /*i_ostream*/)
		{			
			using density::runtime_type;

			nonblocking_heterogeneous_queue_lifetime_tests();

			nonblocking_heterogeneous_queue_basic_nonpolymorphic_base_tests();

			nonblocking_heterogeneous_queue_basic_polymorphic_base_tests();
		
			nonblocking_heterogeneous_queue_basic_void_tests<lf_heter_queue<>>();

			nonblocking_heterogeneous_queue_basic_void_tests<lf_heter_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>();

			nonblocking_heterogeneous_queue_basic_void_tests<lf_heter_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>();
		}

	}; // NbQueueBasicTests



	/** Basic tests for lf_heter_queue<...> */
	void nonblocking_heterogeneous_queue_basic_tests(std::ostream & i_ostream)
	{
		PrintScopeDuration(i_ostream, "heterogeneous queue basic tests");

		constexpr auto mult = density::concurrency_multiple;
		constexpr auto single = density::concurrency_single;
		constexpr auto seq_cst = density::consistency_sequential;
		constexpr auto relaxed = density::consistency_relaxed;

		NbQueueBasicTests<		mult,		mult,			seq_cst		>::tests(i_ostream);
		NbQueueBasicTests<		single,		mult,			seq_cst		>::tests(i_ostream);
		NbQueueBasicTests<		mult,		single,			seq_cst		>::tests(i_ostream);
		NbQueueBasicTests<		single,		single,			seq_cst		>::tests(i_ostream);
		
		NbQueueBasicTests<		mult,		mult,			relaxed		>::tests(i_ostream);
		NbQueueBasicTests<		single,		mult,			relaxed		>::tests(i_ostream);
		NbQueueBasicTests<		mult,		single,			relaxed		>::tests(i_ostream);
		NbQueueBasicTests<		single,		single,			relaxed		>::tests(i_ostream);
	}
}