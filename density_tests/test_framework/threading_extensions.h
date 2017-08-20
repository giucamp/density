#pragma once

#include <thread>
#include <memory>

namespace density_tests
{
	uint64_t get_num_of_processors();

	bool set_thread_affinity(uint64_t i_mask);

	bool set_thread_affinity(std::thread & i_thread, uint64_t i_mask);
}
