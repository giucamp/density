
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>

namespace density
{
    namespace detail
    {
        // Tuple_Merge_t
        template <typename... TUPLES> struct Tuple_Merge;
        template <> struct Tuple_Merge<>
        {
            using type = std::tuple<>;
        };
        template <typename... TYPES_1> struct Tuple_Merge<std::tuple<TYPES_1...>>
        {
            using type = std::tuple<TYPES_1...>;
        };
        template <typename... TYPES_1, typename... TYPES_2, typename... OTHER_TUPLES>
        struct Tuple_Merge<std::tuple<TYPES_1...>, std::tuple<TYPES_2...>, OTHER_TUPLES...>
        {
            using type =
              typename Tuple_Merge<std::tuple<TYPES_1..., TYPES_2...>, OTHER_TUPLES...>::type;
        };
        template <typename... TUPLES> using Tuple_Merge_t = typename Tuple_Merge<TUPLES...>::type;

        // Tuple_FindFirst
        template <typename TUPLE, typename TARGET_TYPE> struct Tuple_FindFirst;
        template <typename TARGET_TYPE> struct Tuple_FindFirst<std::tuple<>, TARGET_TYPE>
        {
            constexpr static size_t index = 0;
        };
        template <typename FIRST_TYPE, typename... TYPES, typename TARGET_TYPE>
        struct Tuple_FindFirst<std::tuple<FIRST_TYPE, TYPES...>, TARGET_TYPE>
        {
            constexpr static size_t index =
              (std::is_same<FIRST_TYPE, TARGET_TYPE>::value)
                ? 0
                : (Tuple_FindFirst<std::tuple<TYPES...>, TARGET_TYPE>::index + 1);
        };

        // MakeFeatureTable<tuple<...>>::make_table<COMMON_ANCESTOR>
        template <typename TUPLE> struct MakeFeatureTable;
        template <typename... TYPES> struct MakeFeatureTable<std::tuple<TYPES...>>
        {
            template <typename TARGET_TYPE> constexpr static std::tuple<TYPES...> make_table()
            {
                return std::tuple<TYPES...>{TYPES::template make<TARGET_TYPE>()...};
            }
        };

    } // namespace detail

} // namespace density
