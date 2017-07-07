
#include "test_objects.h"
#if DENSITY_TEST_INSTANCE_REGISTRY
	#include <typeinfo>
	#include <iostream>
#endif

namespace density_tests
{
	std::atomic<size_t> InstanceCounted::s_instance_counter{0};

	#if DENSITY_TEST_INSTANCE_PROGRESSIVE
		std::atomic<uint64_t> InstanceCounted::s_next_instance_progr{0};
	#endif

	void InstanceCounted::new_instance()
	{
		s_instance_counter.fetch_add(1, std::memory_order_relaxed);
		#if DENSITY_TEST_INSTANCE_PROGRESSIVE
			m_instance_progr = s_next_instance_progr.fetch_add(1, std::memory_order_relaxed);
		#endif

		#if DENSITY_TEST_INSTANCE_REGISTRY
			register_instance(this);
		#endif
	}

	#if DENSITY_TEST_INSTANCE_REGISTRY

		struct InstanceCounted::Registry
		{
			std::mutex m_mutex;
			std::vector<std::unordered_set<InstanceCounted*>> m_contexts{1};
		};

		InstanceCounted::Registry & InstanceCounted::get_registry()
		{
			static Registry s_registry;
			return s_registry;
		}

		void InstanceCounted::register_instance(InstanceCounted * i_instance)
		{
			auto & registry = get_registry();
			std::lock_guard<std::mutex> lock(registry.m_mutex);
			auto res = registry.m_contexts.back().insert(i_instance);
			if (!res.second)
			{
				std::cout << "already existing: ";
				i_instance->write_leak(std::cout);
				std::cout << "\n";
			}
			DENSITY_TEST_ASSERT(res.second);
		}

		void InstanceCounted::unregister_instance(InstanceCounted * i_instance)
		{
			auto & registry = get_registry();
			std::lock_guard<std::mutex> lock(registry.m_mutex);
			auto const erased_count = registry.m_contexts.back().erase(i_instance);
			DENSITY_TEST_ASSERT(erased_count == 1);
		}

		void InstanceCounted::write_leak(std::ostream & i_ostream) const
		{
			i_ostream << typeid(*this).name();

			#if DENSITY_TEST_INSTANCE_PROGRESSIVE
			
				i_ostream << " (" << m_instance_progr << ")";

			#endif
		}

	#endif

	InstanceCounted::ScopedLeakCheck::ScopedLeakCheck()
		: m_instances(s_instance_counter.load())
	{
		#if DENSITY_TEST_INSTANCE_REGISTRY
			auto & registry = get_registry();
			std::lock_guard<std::mutex> lock(registry.m_mutex);
			registry.m_contexts.emplace_back();
		#endif
	}

	InstanceCounted::ScopedLeakCheck::~ScopedLeakCheck()
	{
		DENSITY_TEST_ASSERT(m_instances == s_instance_counter.load());

		#if DENSITY_TEST_INSTANCE_REGISTRY
			auto & registry = get_registry();
			std::lock_guard<std::mutex> lock(registry.m_mutex);
		
			auto & leaks = registry.m_contexts.back();

			if (leaks.size() > 0)
			{
				for (auto leak : leaks)
				{
					leak->write_leak(std::cout);
					std::cout << "\n";
				}
				std::cout << leaks.size() << " leaks detected" << std::endl;
			}
		
			registry.m_contexts.pop_back();
		#endif
	}

} // namespace density_tests
