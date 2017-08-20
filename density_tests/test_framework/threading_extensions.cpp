
#include "threading_extensions.h"
#ifdef _WIN32
	#include <Windows.h>
#endif

namespace density_tests
{
	#ifdef _WIN32

		uint64_t get_num_of_processors()
		{
			SYSTEM_INFO system_info;
			GetSystemInfo(&system_info);
			return system_info.dwNumberOfProcessors;
		}

		bool set_thread_affinity(HANDLE i_thread, uint64_t i_mask)
		{
			auto const new_affinity = static_cast<DWORD_PTR>(i_mask);
			return SetThreadAffinityMask(i_thread, new_affinity) != 0;
		}

		bool set_thread_affinity(uint64_t i_mask)
		{
			return set_thread_affinity(GetCurrentThread(), i_mask);
		}

		bool set_thread_affinity(std::thread & i_thread, uint64_t i_mask)
		{
			return set_thread_affinity(i_thread.native_handle(), i_mask);
		}

	#else

		uint64_t get_num_of_processors()
		{
			return 0;
		}

		bool set_thread_affinity(uint64_t)
		{
			return false;
		}

		bool set_thread_affinity(std::thread &, uint64_t)
		{
			return false;
		}

	#endif
}