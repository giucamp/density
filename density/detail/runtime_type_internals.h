
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

        /* size_t invoke_hash(const TYPE & i_object) - Computes the hash of an object.
            - If a the call hash_func(i_object) is legal, it is used to compute the hash. This function
            should be defined in the namespace that contains TYPE (it will use ADL). If such function exits,
            its return type must be size_t.
            - Otherwise std::hash<TYPE> is used to compute the hash
        see http://stackoverflow.com/questions/257288/is-it-possible-to-write-a-c-template-to-check-for-a-functions-existence */
        template <typename> struct sfinae_true : std::true_type
        {
        };
        template <typename TYPE>
        static sfinae_true<decltype(hash_func(std::declval<TYPE>()))> has_hash_func_impl(int);
        template <typename TYPE> static std::false_type               has_hash_func_impl(long);
        template <typename TYPE>
        inline size_t invoke_hash_func_impl(const TYPE & i_object, std::true_type) noexcept
        {
            static_assert(
              std::is_same<decltype(hash_func(i_object)), size_t>::value,
              "if hash_func() exits for this type, then it must return a size_t");
            return hash_func(i_object);
        }
        template <typename TYPE>
        inline size_t invoke_hash_func_impl(const TYPE & i_object, std::false_type) noexcept
        {
            return std::hash<TYPE>()(i_object);
        }
        template <typename TYPE> inline size_t invoke_hash(const TYPE & i_object) noexcept
        {
            return invoke_hash_func_impl(i_object, decltype(has_hash_func_impl<TYPE>(0))());
        }

        /* DERIVED * down_cast<DERIVED*>(BASE *i_base_ptr) - casts from a base class to a derived, assuming
            that the cast is legal. A static_cast is used if it is possible. Otherwise, if a virtual base is
            involved, dynamic_cast is used. */
        template <typename DERIVED, typename BASE>
        static sfinae_true<decltype(static_cast<DERIVED>(std::declval<BASE>()))>
          can_static_cast_impl(int);
        template <typename DERIVED, typename BASE>
        static std::false_type can_static_cast_impl(long);
        template <typename DERIVED, typename BASE>
        inline DERIVED down_cast_impl(BASE i_base_ptr, std::true_type)
        {
            return static_cast<DERIVED>(i_base_ptr);
        }
        template <typename DERIVED, typename BASE>
        inline DERIVED down_cast_impl(BASE i_base_ptr, std::false_type)
        {
            return dynamic_cast<DERIVED>(i_base_ptr);
        }
        template <typename DERIVED, typename BASE> inline DERIVED down_cast(BASE i_base_ptr)
        {
            static_assert(std::is_pointer<DERIVED>::value, "DERIVED must be a pointer");
            static_assert(std::is_pointer<BASE>::value, "BASE must be a pointer");

            using BaseNaked = typename std::decay<typename std::remove_pointer<BASE>::type>::type;
            using DerivedNaked =
              typename std::decay<typename std::remove_pointer<DERIVED>::type>::type;
            static_assert(
              std::is_same<BaseNaked, void>::value ||
                std::is_same<BaseNaked, DerivedNaked>::value ||
                std::is_base_of<BaseNaked, DerivedNaked>::value,
              "*BASE must be void, DERIVED or a base of *DERIVED");

            return down_cast_impl<DERIVED>(
              i_base_ptr, decltype(can_static_cast_impl<DERIVED, BASE>(0))());
        }

    } // namespace detail

} // namespace density
