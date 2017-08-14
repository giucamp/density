
#include "queue_generic_tests.h"

namespace density_tests
{
	void concurr_heter_queue_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
		EasyRandom & i_rand, size_t i_element_count)
	{
		using namespace density;

		std::vector<size_t> const concurrent_thread_counts{ 1, 2 };

		detail::single_queue_generic_test<conc_heter_queue<>>(
			i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);

		detail::single_queue_generic_test<conc_heter_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>(
			i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);

		detail::single_queue_generic_test<conc_heter_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>(
			i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);

		detail::single_queue_generic_test<conc_heter_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>>>(
			i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);

		detail::single_queue_generic_test<conc_heter_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>>>(
			i_flags, i_output, i_rand, i_element_count, concurrent_thread_counts);
	}
}