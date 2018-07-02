
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
    struct f_istream
    {
        /** Pointer to the function that reads the target object. */
        void (*const m_read_func)(std::istream & i_istream, void * i_dest);

        /** Reads the target object from an input stream.
        @param i_source pointer to an instance of the target type. Can't be null.
            If the dynamic type of the pointed object is not the taget type (assigned
            by the function make), the behaviour is undefined. */
        void operator()(std::istream & i_istream, void * i_dest) const
        {
            DENSITY_ASSERT(i_dest != nullptr);
            (*m_read_func)(i_istream, i_dest);
        }

        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_istream make() noexcept
        {
            return f_istream{&read_func<TARGET_TYPE>};
        }

      private:
        template <typename TARGET_TYPE>
        static void read_func(std::istream & i_istream, void * i_dest)
        {
            const auto derived = detail::down_cast<TARGET_TYPE *>(i_dest);
            i_istream >> *derived;
        }
    };

    /** This feature writes an object to an std::ostream using the operator >> */
    struct f_ostream
    {
        /** Pointer to the function that writes the target object. */
        void (*const m_write_func)(std::ostream & i_ostream, const void * i_dest);

        /** Reads from an input stream the target object.
            @param i_source pointer to an instance of the target type. Can't be null.
                If the dynamic type of the pointed object is not the taget type (assigned
                by the function make), the behaviour is undefined. */
        void operator()(std::ostream & i_ostream, const void * i_dest) const
        {
            DENSITY_ASSERT(i_dest != nullptr);
            (*m_write_func)(i_ostream, i_dest);
        }

        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_ostream make() noexcept
        {
            return f_ostream{&write_func<TARGET_TYPE>};
        }

      private:
        template <typename TARGET_TYPE>
        static void write_func(std::ostream & i_ostream, const void * i_dest)
        {
            const auto derived = detail::down_cast<const TARGET_TYPE *>(i_dest);
            i_ostream << *derived;
        }
    };

} // namespace density
