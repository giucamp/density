
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "environment.h"
#include <sstream>
#include <vector>

#if defined( _WIN32 ) && !defined( __clang__ ) && !defined( __MINGW32__ )
    #define WIN32_WMI 0
#else
    #define WIN32_WMI 0
#endif

#if WIN32_WMI
    #include <Objbase.h>
    #include <Wbemidl.h>
    #include <atlbase.h>
    #pragma comment(lib, "Wbemuuid")
#endif

namespace density_bench
{

    #if WIN32_WMI

        #define TESTITY_COM_CALL(call)        { if(FAILED((call))) throw std::exception("Com call failed: " #call); }

        // BSTR - std::conversions - see http://stackoverflow.com/questions/6284524/bstr-to-stdstring-stdwstring-and-vice-versa
        std::string ConvertWCSToMBS(const wchar_t* pstr, long wslen)
        {
            int len = ::WideCharToMultiByte(CP_ACP, 0, pstr, wslen, NULL, 0, NULL, NULL);

            std::string dblstr(len, '\0');
            len = ::WideCharToMultiByte(CP_ACP, 0 /* no flags */,
                pstr, wslen /* not necessary NULL-terminated */,
                &dblstr[0], len,
                NULL, NULL /* no default char */);

            return dblstr;
        }
        std::string ConvertBSTRToMBS(BSTR bstr)
        {
            int wslen = ::SysStringLen(bstr);
            return ConvertWCSToMBS((wchar_t*)bstr, wslen);
        }

        class CoInitialize
        {
        public:
            CoInitialize()
            {
                CoInitializeEx(NULL, COINIT_MULTITHREADED); /* The second time this function is caled
                                                                on the same thread, it returns S_FALSE. */
                TESTITY_COM_CALL( CoInitializeSecurity(NULL, -1, NULL, NULL,
                    RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL) );
            }

            ~CoInitialize()
            {
                CoUninitialize();
            }
        };

        class WMIServices
        {
        public:
            WMIServices()
            {
                TESTITY_COM_CALL( CoCreateInstance( CLSID_WbemAdministrativeLocator, NULL,
                    CLSCTX_INPROC_SERVER,
                    IID_IWbemLocator, reinterpret_cast< void** >(&m_locator)) );

                TESTITY_COM_CALL(m_locator->ConnectServer( L"root\\cimv2", NULL, NULL, NULL,
                    WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                    NULL, NULL, &m_services) );
            }

            std::vector< CComPtr<IWbemClassObject> > execute_query(const wchar_t * i_query)
            {
                const CComBSTR query(i_query);
                CComPtr<IEnumWbemClassObject> enum_object;
                TESTITY_COM_CALL( m_services->ExecQuery(L"WQL", query, WBEM_FLAG_FORWARD_ONLY, NULL, &enum_object) );

                std::vector< CComPtr<IWbemClassObject> > result;
                bool redo = true;
                do {
                    const size_t tmp_object_capacity = 32;
                    IWbemClassObject * tmp_objects[tmp_object_capacity] = { NULL };
                    ULONG tmp_returned = 0;
                    enum_object->Next(WBEM_INFINITE, tmp_object_capacity, tmp_objects, &tmp_returned);
                    result.insert(result.end(), tmp_objects, tmp_objects + tmp_returned);
                    redo = tmp_returned == tmp_object_capacity;
                } while (redo);

                return std::move(result);
            }

            std::vector<std::string> get_properties(
                const wchar_t * i_query, const std::vector<const wchar_t *> & i_property_names)
            {
                auto query_results = execute_query(i_query);
                std::vector<std::string> value_strs;
                value_strs.resize(i_property_names.size());
                for (size_t property_index = 0; property_index < i_property_names.size(); property_index++)
                {
                    for (const auto & result : query_results)
                    {
                        auto & value_str = value_strs[property_index];
                        if (value_str.length() > 0)
                        {
                            value_str += ", ";
                        }

                        CComVariant value;
                        CIMTYPE value_type;
                        TESTITY_COM_CALL(result->Get(i_property_names[property_index], 0, &value, &value_type, NULL));
                        value.ChangeType(VT_BSTR);
                        value_str += ConvertBSTRToMBS(value.bstrVal);
                    }
                }
                return value_strs;
            }

            struct Property
            {
                std::string m_name;
                std::string m_value;
            };

            std::vector<Property> get_all_properties(const wchar_t * i_query)
            {
                std::vector<Property> props;
                auto query_results = execute_query(i_query);
                for (const auto & result : query_results)
                {
                    TESTITY_COM_CALL(result->BeginEnumeration(0));

                    for (;;)
                    {
                        CComBSTR prop_name;
                        CComVariant prop_value;
                        if(result->Next(0, &prop_name, &prop_value, NULL, NULL) != ERROR_SUCCESS)
                        {
                            break;
                        }
                        prop_value.ChangeType(VT_BSTR);

                        Property prop;
                        prop.m_name = ConvertBSTRToMBS(prop_name);
                        prop.m_value = ConvertBSTRToMBS(prop_value.bstrVal);
                        props.push_back(std::move(prop));
                    }

                    TESTITY_COM_CALL(result->EndEnumeration());
                }
                return std::move(props);
            }

        private:
            CoInitialize m_com_init;
            CComPtr<IWbemLocator> m_locator;
            CComPtr<IWbemServices> m_services;
        };
    #endif // #ifdef _WIN32

    Environment::Environment()
        : m_startup_clock(std::chrono::system_clock::now())
    {
        // detect the compiler
        std::ostringstream compiler;
        #ifdef _MSC_VER
                compiler << "MSC - " << _MSC_VER;
        #elif defined(__clang__)
                compiler << "Clang - " << __clang_major__ << '.' << __clang_minor__ << '.' << __clang_patchlevel__;
        #elif defined( __GNUC__ )
                compiler << "GCC - " << __GNUC__ << '.' << __GNUC_MINOR__ << '.' << __GNUC_PATCHLEVEL__;
        #else
                compiler << "unknown";
        #endif
        m_compiler = compiler.str();

        #if WIN32_WMI
            WMIServices wmi;

            // query system info
            auto proc_prop = wmi.get_properties(L"SELECT * FROM Win32_Processor", { L"Caption", L"Name", L"L2CacheSize" } );
            m_system_info = proc_prop[0] + " - " + proc_prop[1] + " - L2Cache(KiB) " + proc_prop[2];
            MEMORYSTATUSEX memory_status;
            ZeroMemory(&memory_status, sizeof(memory_status));
            memory_status.dwLength = sizeof(memory_status);
            if (GlobalMemoryStatusEx(&memory_status))
            {
                std::ostringstream memory_size;
                memory_size << ", System memory: " << memory_status.ullTotalPhys / (1024. * 1024.) << " GiB";
                m_system_info += memory_size.str();
            }

            // query os name
            auto os_prop = wmi.get_properties(L"SELECT * FROM Win32_OperatingSystem", { L"Caption" });
            m_operating_sytem = os_prop[0];
        #else
            m_operating_sytem = "unknown";
            m_system_info = "unknown";
        #endif
    }

} // namespace density_bench
