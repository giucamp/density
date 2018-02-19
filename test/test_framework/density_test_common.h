
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <typeinfo>
#include <string>
#include <chrono>
#include <density/default_allocator.h>
#include <sstream>
#include <string>
#include <cstdlib>
#if defined(__GNUC__) && !defined(_MSC_VER)
    #include <cxxabi.h>
#endif

#define DENSITY_TEST_ASSERT(expr)        if(!(expr)) density_tests::detail::assert_failed(__FILE__, __func__, __LINE__, #expr); else (void)0

namespace density_tests
{
    template <typename TYPE>
        std::string truncated_type_name(size_t i_max_size = 80)
    {
        #if defined(__GNUC__) && !defined(_MSC_VER)
            int status = 0;
            auto const demangled = abi::__cxa_demangle(typeid(TYPE).name(), 0, 0, &status);
            std::string name = status == 0 ? demangled : typeid(TYPE).name();
        #else
            std::string name = typeid(TYPE).name();
        #endif

        if (name.size() > i_max_size)
            name.resize(i_max_size);

        #if defined(__GNUC__) && !defined(_MSC_VER)
            free(demangled);
        #endif
        return name;
    }

    enum class QueueTesterFlags
    {
        eNone = 0,
        eTestExceptions = 1 << 1,
        eUseTestAllocators = 1 << 2,
        eReserveCoreToMainThread = 1 << 3,
        ePrintProgress = 1 << 4,
        eSuspender = 1 << 8,
    };

    constexpr QueueTesterFlags operator | (QueueTesterFlags i_first, QueueTesterFlags i_second) noexcept
    {
        return static_cast<QueueTesterFlags>(static_cast<int>(i_first) | static_cast<int>(i_second));
    }

    constexpr QueueTesterFlags operator & (QueueTesterFlags i_first, QueueTesterFlags i_second) noexcept
    {
        return static_cast<QueueTesterFlags>(static_cast<int>(i_first) & static_cast<int>(i_second));
    }

    constexpr bool operator && (QueueTesterFlags i_first, QueueTesterFlags i_second) noexcept
    {
        return (static_cast<int>(i_first) & static_cast<int>(i_second)) != 0;
    }

    namespace detail
    {
        void assert_failed(const char * i_source_file, const char * i_function, int i_line, const char * i_expr);
    }

    /** Move-only wrapper of default_allocator */
    struct move_only_void_allocator : density::default_allocator
    {
        move_only_void_allocator(int) {}

        move_only_void_allocator(move_only_void_allocator &&) = default;

        move_only_void_allocator & operator = (move_only_void_allocator &&) = default;

        move_only_void_allocator(const move_only_void_allocator&) = delete;

        move_only_void_allocator & operator = (const move_only_void_allocator&) = delete;

        void dummy_func()
        {

        }

        void const_dummy_func() const
        {

        }
    };

    enum FormatAlignment
    {
        eAlignLeft,
        eAlignCenter
    };

    /** Writes to a string a value using a fixed number of chars. Useful for tables. */
    template <typename TYPE>
        std::string format_fixed(const TYPE & i_value, size_t i_char_count, FormatAlignment i_alignment = eAlignCenter, char i_fill_char = ' ')
    {
        std::ostringstream out;
        out << i_value;
        auto string = out.str();
        switch (i_alignment)
        {
            case eAlignLeft:
            {
                string.resize(i_char_count, i_fill_char);
                break;
            }
            case eAlignCenter:
            {
                if (string.size() > i_char_count)
                {
                    // truncate
                    string.resize(i_char_count);
                }
                else
                {
                    // align
                    auto const padding = i_char_count - string.size();
                    auto const left_padding = padding / 2;
                    string.insert(0, left_padding, i_fill_char);
                    string.resize(i_char_count, i_fill_char);
                }
            }
            default:
                break;
        }        
        return string;
    }

    // old versions of libstdc++ don't define std::max_align_t
    #if defined(__GLIBCXX__)
        constexpr size_t MaxAlignment = alignof(max_align_t);
    #else
        constexpr size_t MaxAlignment = alignof(std::max_align_t);
    #endif
}
