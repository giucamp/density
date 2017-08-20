
#include "queue_generic_tests.h"

namespace density_tests
{
	void heter_queue_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
		EasyRandom & i_rand, size_t i_element_count);

	void concurr_heter_queue_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
		EasyRandom & i_rand, size_t i_element_count);

	void lf_heter_relaxed_queue_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
		EasyRandom & i_rand, size_t i_element_count);

	void lf_heter_seq_cst_queue_generic_tests(QueueTesterFlags i_flags, std::ostream & i_output,
		EasyRandom & i_rand, size_t i_element_count);

	
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
		EasyRandom rand = i_random_seed == 0 ? EasyRandom() : EasyRandom(i_random_seed);

		heter_queue_generic_tests(i_flags, i_output, rand, i_element_count);

		concurr_heter_queue_generic_tests(i_flags, i_output, rand, i_element_count);

		lf_heter_relaxed_queue_generic_tests(i_flags, i_output, rand, i_element_count);

		lf_heter_seq_cst_queue_generic_tests(i_flags, i_output, rand, i_element_count);
	}
}