
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "threading_extensions.h"
#ifdef _WIN32
    #include <Windows.h>
#elif __linux__
    #include <limits>
    #include <unistd.h>
    #include <sched.h>
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

        bool supend_thread(std::thread & i_thread)
        {
            return SuspendThread(i_thread.native_handle()) != (DWORD)-1;
        }

        void resume_thread(std::thread & i_thread)
        {
            ResumeThread(i_thread.native_handle());
        }

        // taken from https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
        const DWORD MS_VC_EXCEPTION = 0x406D1388;  
        #pragma pack(push,8)  
        typedef struct tagTHREADNAME_INFO  
        {  
            DWORD dwType; // Must be 0x1000.  
            LPCSTR szName; // Pointer to name (in user addr space).  
            DWORD dwThreadID; // Thread ID (-1=caller thread).  
            DWORD dwFlags; // Reserved for future use, must be zero.  
         } THREADNAME_INFO;  
        #pragma pack(pop)  

        bool set_thread_name(std::thread & i_thread, const char * i_name)
        {
            auto const id = GetThreadId(i_thread.native_handle());

            THREADNAME_INFO info;  
            info.dwType = 0x1000;  
            info.szName = i_name;  
            info.dwThreadID = id;  
            info.dwFlags = 0;
            #pragma warning(push)  
            #pragma warning(disable: 6320 6322)  
            __try
            {  
                RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);  
            }  
            __except (EXCEPTION_EXECUTE_HANDLER)
            {  
            }  
            #pragma warning(pop)  
            return true;
        }

    #elif __linux__

        uint64_t get_num_of_processors()
        {
            long res = sysconf(_SC_NPROCESSORS_ONLN);
            if(res >= 1)
                return static_cast<uint64_t>(res);
            else
                return 0;
        }

        bool set_thread_affinity(uint64_t i_mask)
        {
            cpu_set_t cpu_set;
            CPU_ZERO(&cpu_set);

            uint64_t cpu_mask = 1;
            for(int cpu_index = 0; cpu_index < CPU_SETSIZE; cpu_index++ )
            {
                if(cpu_index >= 64 || i_mask & cpu_mask)
                {
                    CPU_SET(cpu_index, &cpu_set);
                }
                cpu_mask <<= 1;
            }

            return sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) == 0;
        }

        bool supend_thread(std::thread & )
        {
            return false;
        }

        void resume_thread(std::thread &)
        {
        }

        bool set_thread_name(std::thread &, const char *)
        {
            return false;
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

        bool supend_thread(std::thread & )
        {
            return false;
        }

        void resume_thread(std::thread &)
        {
        }

        bool set_thread_name(std::thread &, const char *)
        {
            return false;
        }

    #endif
}
