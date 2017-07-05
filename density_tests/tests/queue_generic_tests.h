#pragma once
#include "../test_framework/density_test_common.h"
#include <cstdint>
#include <ostream>

namespace density_tests
{
	// see queue_generic_tests.cpp for the doc
	void all_queues_generic_tests(QueueTesterFlags i_flags,
		std::ostream & i_output,
		uint32_t i_random_seed, 
		size_t i_element_count);

} // namespace density_tests
