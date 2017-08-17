#pragma once

#include <thread>
#include <memory>

namespace density_tests
{
	void set_thread_affinity(uint64_t i_mask);

	void set_thread_affinity(std::thread & i_thread, uint64_t i_mask);
}
