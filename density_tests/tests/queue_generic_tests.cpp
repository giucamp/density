
#include "queue_generic_tests.h"

namespace density_tests
{
	/** Runs the generic test on all the queues 
		@param i_flags misc options
		@param i_output output stream to use for the progression and the result
		@param i_random_seed seed to use for the PRG. If != 0, the test is deterministic. If == 0, PRG's
			are seeded with a non-deterministic source (like std::random_device).
		@param i_element_count number of elements to produce and consume in every test
	*/
	void all_queues_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
		uint32_t i_random_seed, size_t i_element_count)
	{
		using namespace density;

		// std::vector<size_t> const concurrent_thread_counts{ 1, 2 };
		// std::vector<size_t> const nonblocking_thread_counts{ 1, 7, 32 * 1024 };

		
		std::vector<size_t> const concurrent_thread_counts{ 1 };
		std::vector<size_t> const nonblocking_thread_counts{ 1 };

		EasyRandom rand = i_random_seed == 0 ? EasyRandom() : EasyRandom(i_random_seed);

		// heterogeneous_queue<void, ...>
		detail::single_queue_generic_test<heterogeneous_queue<>>(
			i_flags, i_output, rand, i_element_count, { 1 });
		detail::single_queue_generic_test<heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, { 1 });
		detail::single_queue_generic_test<heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, { 1 });
		detail::single_queue_generic_test<heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, { 1 });
		detail::single_queue_generic_test<heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, { 1 });

		// concurrent_heterogeneous_queue<void, ...>
		detail::single_queue_generic_test<concurrent_heterogeneous_queue<>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);
		detail::single_queue_generic_test<concurrent_heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);
		detail::single_queue_generic_test<concurrent_heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<>>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);
		detail::single_queue_generic_test<concurrent_heterogeneous_queue<void, runtime_type<>, UnmovableFastTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);
		detail::single_queue_generic_test<concurrent_heterogeneous_queue<void, TestRuntimeTime<>, DeepTestAllocator<256>>>(
			i_flags, i_output, rand, i_element_count, concurrent_thread_counts);

		// nonblocking_heterogeneous_queue
		detail::nb_queues_generic_tests<density::concurrent_cardinality_multiple, density::concurrent_cardinality_multiple, density::consistency_model_seq_cst>
			(i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		detail::nb_queues_generic_tests<density::concurrent_cardinality_multiple, density::concurrent_cardinality_single, density::consistency_model_seq_cst>
			(i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		detail::nb_queues_generic_tests<density::concurrent_cardinality_single, density::concurrent_cardinality_multiple, density::consistency_model_seq_cst>
			(i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		detail::nb_queues_generic_tests<density::concurrent_cardinality_single, density::concurrent_cardinality_single, density::consistency_model_seq_cst>
			(i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		detail::nb_queues_generic_tests<density::concurrent_cardinality_multiple, density::concurrent_cardinality_multiple, density::consistency_model_relaxed>
			(i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		detail::nb_queues_generic_tests<density::concurrent_cardinality_multiple, density::concurrent_cardinality_single, density::consistency_model_relaxed>
			(i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		detail::nb_queues_generic_tests<density::concurrent_cardinality_single, density::concurrent_cardinality_multiple, density::consistency_model_relaxed>
			(i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
		detail::nb_queues_generic_tests<density::concurrent_cardinality_single, density::concurrent_cardinality_single, density::consistency_model_relaxed>
			(i_flags, i_output, rand, i_element_count, nonblocking_thread_counts);
	}
}