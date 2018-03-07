
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
    namespace type_features
    {
        /** This feature reads an object from an std::istream with the operator >> */
        struct istream
        {
            using type = void * (*)(std::istream & i_istream, void * i_complete_dest);

            template <typename BASE, typename TYPE> struct Impl
            {
                static void invoke(std::istream & i_istream, void * i_complete_object)
                {
                    const auto base    = static_cast<BASE *>(i_complete_object);
                    const auto derived = detail::down_cast<TYPE *>(base);
                    i_istream >> *derived;
                }

                static const uintptr_t value;
            };
        };
        template <typename TYPE, typename BASE>
        const uintptr_t istream::Impl<TYPE, BASE>::value = reinterpret_cast<uintptr_t>(invoke);

        /** This feature writes an object to an std::ostream with the operator << */
        struct ostream
        {
            using type = void * (*)(std::ostream & i_ostream, const void * i_complete_object);

            template <typename BASE, typename TYPE> struct Impl
            {
                static void invoke(std::ostream & i_ostream, const void * i_complete_object)
                {
                    const auto base    = static_cast<const BASE *>(i_complete_object);
                    const auto derived = detail::down_cast<const TYPE *>(base);
                    i_ostream << *derived;
                }

                static const uintptr_t value;
            };
        };
        template <typename TYPE, typename BASE>
        const uintptr_t ostream::Impl<TYPE, BASE>::value = reinterpret_cast<uintptr_t>(invoke);

    } // namespace type_features

} // namespace density
