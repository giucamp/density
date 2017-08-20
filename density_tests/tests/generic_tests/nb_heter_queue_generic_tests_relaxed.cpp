
#include "queue_generic_tests.h"

namespace density_tests
{
	void concurr_heter_relaxed_queue_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
		EasyRandom & i_rand, size_t i_element_count)
	{
		using namespace density;

		std::vector<size_t> const nonblocking_thread_counts{ 100 };

		constexpr auto mult = density::concurrency_multiple;
		constexpr auto single = density::concurrency_single;
		constexpr auto relaxed = density::consistency_relaxed;

		detail::nb_queues_generic_tests<mult, mult, relaxed>
			(i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);

		detail::nb_queues_generic_tests<mult, single, relaxed>
			(i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);

		detail::nb_queues_generic_tests<single, mult, relaxed>
			(i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);

		detail::nb_queues_generic_tests<single, single, relaxed>
			(i_flags, i_output, i_rand, i_element_count, nonblocking_thread_counts);
	}
}