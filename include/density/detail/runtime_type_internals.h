
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
        // Tuple_Merge_t< tuple<...>... >
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

        // Tuple_FindFirst< tuple<...>, TARGET_TYPE >::index
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

        // Tuple_Remove_t< tuple<...>, TARGET_TYPE >
        template <typename TUPLE, typename TARGET_TYPE> struct Tuple_Remove;
        template <typename TARGET_TYPE> struct Tuple_Remove<std::tuple<>, TARGET_TYPE>
        {
            using type = std::tuple<>;
        };
        template <typename FIRST_TYPE, typename... OTHER_TYPES, typename TARGET_TYPE>
        struct Tuple_Remove<std::tuple<FIRST_TYPE, OTHER_TYPES...>, TARGET_TYPE>
        {
            using type = Tuple_Merge_t<
              std::tuple<FIRST_TYPE>,
              typename Tuple_Remove<std::tuple<OTHER_TYPES...>, TARGET_TYPE>::type>;
        };
        template <typename... OTHER_TYPES, typename TARGET_TYPE>
        struct Tuple_Remove<std::tuple<TARGET_TYPE, OTHER_TYPES...>, TARGET_TYPE>
        {
            using type = typename Tuple_Remove<std::tuple<OTHER_TYPES...>, TARGET_TYPE>::type;
        };
        template <typename TUPLE, typename TARGET_TYPE>
        using Tuple_Remove_t = typename Tuple_Remove<TUPLE, TARGET_TYPE>::type;

        // Tuple_Diff_t<TUPLE_1, TUPLE_2>, -> TUPLE_1 - TUPLE_2
        template <typename TUPLE_1, typename TUPLE_2> struct Tuple_Diff;
        template <typename TUPLE_1, typename TUPLE_2>
        using Tuple_Diff_t = typename Tuple_Diff<TUPLE_1, TUPLE_2>::type;
        template <typename... TYPES_1> struct Tuple_Diff<std::tuple<TYPES_1...>, std::tuple<>>
        {
            using type = std::tuple<TYPES_1...>;
        };
        template <typename... TYPES_1, typename FIRST, typename... TYPES_2>
        struct Tuple_Diff<std::tuple<TYPES_1...>, std::tuple<FIRST, TYPES_2...>>
        {
            using type = Tuple_Diff_t<
              Tuple_Remove_t<std::tuple<TYPES_1...>, FIRST>,
              std::tuple<TYPES_2...>

              >;
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

        // FeatureTable
        template <typename FEATURE_TUPLE, typename TARGET_TYPE> struct FeatureTable
        {
            constexpr static FEATURE_TUPLE s_table =
              detail::MakeFeatureTable<FEATURE_TUPLE>::template make_table<TARGET_TYPE>();
        };
        template <typename FEATURE_TUPLE, typename TARGET_TYPE>
        constexpr FEATURE_TUPLE FeatureTable<FEATURE_TUPLE, TARGET_TYPE>::s_table;

    } // namespace detail

    struct
      f_none; /* this forward declaration allows feature_list to use f_none before it is defined. f_none
        is defined later to make the header more readable.*/

} // namespace density
