
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/runtime_type.h>
#include <istream>
#include <ostream>

namespace density
{
    /** This feature reads an object from an std::istream using the operator >> */
    class f_istream
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_istream make() noexcept
        {
            return f_istream{&invoke<TARGET_TYPE>};
        }

        /** Reads the target object from an input stream.
            @param i_source pointer to an instance of the target type. Can't be null.
                If the dynamic type of the pointed object is not the taget type (assigned
                by the function make), the behaviour is undefined. */
        void operator()(std::istream & i_istream, void * i_dest) const
        {
            DENSITY_ASSUME(i_dest != nullptr);
            (*m_function)(i_istream, i_dest);
        }

      private:
        using Function = void (*)(std::istream & i_istream, void * i_dest);
        Function const m_function;
        constexpr f_istream(Function i_function) noexcept : m_function(i_function) {}
        template <typename TARGET_TYPE> static void invoke(std::istream & i_istream, void * i_dest)
        {
            const auto derived = static_cast<TARGET_TYPE *>(i_dest);
            i_istream >> *derived;
        }
    };

    /** This feature writes an object to an std::ostream using the operator >> */
    class f_ostream
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_ostream make() noexcept
        {
            return f_ostream{&invoke<TARGET_TYPE>};
        }

        /** Reads from an input stream the target object.
            @param i_source pointer to an instance of the target type. Can't be null.
                If the dynamic type of the pointed object is not the taget type (assigned
                by the function make), the behaviour is undefined. */
        void operator()(std::ostream & i_ostream, const void * i_dest) const
        {
            DENSITY_ASSUME(i_dest != nullptr);
            (*m_function)(i_ostream, i_dest);
        }

      private:
        using Function = void (*)(std::ostream & i_ostream, const void * i_dest);
        Function const m_function;
        constexpr f_ostream(Function i_function) noexcept : m_function(i_function) {}
        template <typename TARGET_TYPE>
        static void invoke(std::ostream & i_ostream, const void * i_dest)
        {
            const auto derived = static_cast<const TARGET_TYPE *>(i_dest);
            i_ostream << *derived;
        }
    };

} // namespace density
