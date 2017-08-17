
#include "thread_affinity.h"
#include <Windows.h>

namespace density_tests
{
	void set_thread_affinity(HANDLE i_thread, uint64_t i_mask)
	{
		auto const new_affinity = static_cast<DWORD_PTR>(i_mask);
		if (SetThreadAffinityMask(i_thread, new_affinity) == 0)
		{
			throw std::exception();
		}
	}

	void set_thread_affinity(uint64_t i_mask)
	{
		set_thread_affinity(GetCurrentThread(), i_mask);
	}

	void set_thread_affinity(std::thread & i_thread, uint64_t i_mask)
	{
		set_thread_affinity(i_thread.native_handle(), i_mask);
	}
}