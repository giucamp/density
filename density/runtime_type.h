
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <functional> // for std::hash
#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace density
{
    /** namespace type_features */
    namespace type_features
    {
        /** This template represents a typelist. Every type of this list is a feature that can be used by a runtime_type.
            A feature_list is a composition of features that forms a complete type erasure.
            A feature is a struct that performs a part of type erasure on a type, and has this form:
            \code
            struct FeatureX
            {
                using type = ...;
                template <typename BASE, typename TYPE> struct Impl
                {
                    static constexpr type value = ...;
                };
            };
            \endcode
            A feature is a struct (or class) that doesn't depend on the type to erase. In contrast the inner type Impl
            depends on the type to erase, and on the base type. All the types to erase are covariant to
            the base type. If the base type is void any type can be erased. \n
            The value of the feature (a static const uintptr_t) in Impl stores a value that is required for
            to do the job on a type: in many cases it is a pointer to a function (like in the case of
            type_features::copy_construct or type_features::destroy). Anyway it
            may store a value, if it is small enough to fit in a uintptr_t (like in case of type_features::size, or
            type_features::alignment). \n
            The member 'type' is the real type of the value of the feature. The member function runtime_type::get_feature
            casts the value of the feature to this type before returning it.
            \snippet misc_examples.cpp feature_list example 1 */
        template <typename... FEATURES> struct feature_list
        {
            /** Constant that gives the number of features */
            static const size_t size = sizeof...(FEATURES);

            using tuple = std::tuple<typename FEATURES::type...>;
        };

        /** This template concatenates two feature_list, or a feature_list and a feature.
            @tparam FIRST feature_list to prepend
            @tparam SECOND feature_list or feature to append

            The inner member type is an alias for the concatenations of all features in the tempate arguments.
            The order of the features is preserved.

            The alias feature_concat_t can be used instead of feature_concat<...>::type:

            @code
            template <typename FIRST, typename SECOND>
                using feature_concat_t = typename feature_concat<FIRST, SECOND>::type;
            @endcode

            Example:

            \snippet misc_examples.cpp feature_concat example 1
        */
#ifndef DOXYGEN_DOC_GENERATION
        template <typename...> struct feature_concat;
        template <typename... FIRST_FEATURES, typename... SECOND_FEATURES>
        struct feature_concat<feature_list<FIRST_FEATURES...>, feature_list<SECOND_FEATURES...>>
#else
        template <typename FIRST, typename SECOND> struct feature_concat
#endif
        {
            using type = feature_list<FIRST_FEATURES..., SECOND_FEATURES...>;
        };
        template <typename... FIRST_FEATURES, typename SECOND_FEATURE>
        struct feature_concat<feature_list<FIRST_FEATURES...>, SECOND_FEATURE>
        {
            using type = feature_list<FIRST_FEATURES..., SECOND_FEATURE>;
        };

        template <typename FIRST, typename SECOND>
        using feature_concat_t = typename feature_concat<FIRST, SECOND>::type;

    } // namespace type_features

    namespace detail
    {
        template <typename FEATURE_LIST> struct feature_container;
        template <typename FIRST_FEATURE, typename... OTHER_FEATURES>
        struct feature_container<type_features::feature_list<FIRST_FEATURE, OTHER_FEATURES...>>
            : feature_container<type_features::feature_list<OTHER_FEATURES...>>
        {
            typename FIRST_FEATURE::type m_feature;

            using base = feature_container<type_features::feature_list<OTHER_FEATURES...>>;

            template <size_t>
            constexpr const typename FIRST_FEATURE::type & get_impl(std::true_type) const noexcept
            {
                return m_feature;
            }

            template <size_t INDEX>
            constexpr auto get_impl(std::false_type) const noexcept -> decltype(
              std::declval<const feature_container>().base::template get_impl<INDEX - 1>(
                std::integral_constant<bool, INDEX - 1 == 0>{}))
            {
                return base::get_impl<INDEX - 1>(std::integral_constant<bool, INDEX - 1 == 0>{});
            }

            static feature_container<type_features::feature_list<FIRST_FEATURE, OTHER_FEATURES...>>
              inst;

            template <size_t INDEX>
            constexpr auto get() const noexcept /*-> typename std::conditional<

              INDEX == 0,
              typename FIRST_FEATURE::type,
              decltype(
                std::declval<const feature_container>().template get_impl<INDEX>(std::false_type{}))

              >::type*/
            {
                static_assert(INDEX < sizeof...(OTHER_FEATURES) + 1, "feature not found");
                return get_impl<INDEX>(std::integral_constant<bool, INDEX == 0>{});
            }
        };
        template <> struct feature_container<type_features::feature_list<>>
        {
        };

        template <typename FEATURE_LIST, size_t INDEX> struct GetFeature;

        template <typename FIRST_FEATURE, typename... OTHER_FEATURES, size_t INDEX>
        struct GetFeature<
          feature_container<type_features::feature_list<FIRST_FEATURE, OTHER_FEATURES...>>,
          INDEX>
        {
            using type = typename std::conditional<
              INDEX == 0,
              FIRST_FEATURE,
              typename GetFeature<type_features::feature_list<OTHER_FEATURES...>, INDEX - 1>::
                type>::type;
        };

        template <size_t INDEX>
        struct GetFeature<feature_container<type_features::feature_list<>>, INDEX>
        {
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
        inline size_t invoke_hash_func_impl(const TYPE & i_object, std::true_type)
        {
            static_assert(
              std::is_same<decltype(hash_func(i_object)), size_t>::value,
              "if the hash_func() exits for this type, then it must return a size_t");
            return hash_func(i_object);
        }
        template <typename TYPE>
        inline size_t invoke_hash_func_impl(const TYPE & i_object, std::false_type)
        {
            return std::hash<TYPE>()(i_object);
        }
        template <typename TYPE> inline size_t invoke_hash(const TYPE & i_object)
        {
            return invoke_hash_func_impl(i_object, decltype(has_hash_func_impl<TYPE>(0))());
        }

        /* DERIVED * down_cast<DERIVED*>(BASE *i_base_ptr) - down cast from a base class to a derived, assuming
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

        /* FeatureTable<TYPE, feature_list<FEATURES...> >::s_table is a constexpr tuple of all the FEATURES::s_value */
        template <typename BASE, typename TYPE, typename FEATURE_LIST> struct FeatureTable;
        template <typename BASE, typename TYPE, typename... FEATURES>
        struct FeatureTable<BASE, TYPE, type_features::feature_list<FEATURES...>>
        {
            constexpr static typename type_features::feature_list<FEATURES...>::tuple s_table{
              (FEATURES::template Impl<BASE, TYPE>::value)...};
        };

        /* IndexOfFeature<0, TARGET_FEATURE, feature_list<FEATURES...> >::value is the index of TARGET_FEATURE in feature_list<FEATURES...>,
            or feature_list<FEATURES...>::size if TARGET_FEATURE is not present */
        template <size_t START_INDEX, typename TARGET_FEATURE, typename FEATURE_LIST>
        struct IndexOfFeature;
        template <size_t START_INDEX, typename TARGET_FEATURE, typename... OTHER_FEATURES>
        struct IndexOfFeature<
          START_INDEX,
          TARGET_FEATURE,
          type_features::feature_list<OTHER_FEATURES...>>
        {
            static const size_t value = START_INDEX;
        };
        template <
          size_t START_INDEX,
          typename TARGET_FEATURE,
          typename FIRST_FEATURE,
          typename... OTHER_FEATURES>
        struct IndexOfFeature<
          START_INDEX,
          TARGET_FEATURE,
          type_features::feature_list<FIRST_FEATURE, OTHER_FEATURES...>>
        {
            static const size_t value =
              std::is_same<TARGET_FEATURE, FIRST_FEATURE>::value
                ? START_INDEX
                : IndexOfFeature<
                    START_INDEX + 1,
                    TARGET_FEATURE,
                    type_features::feature_list<OTHER_FEATURES...>>::value;
        };

        // CopyConstructImpl<COMPLETE_TYPE>::invoke
        template <
          typename COMPLETE_TYPE,
          bool CAN = std::is_copy_constructible<COMPLETE_TYPE>::value>
        struct CopyConstructImpl;
        template <typename COMPLETE_TYPE> struct CopyConstructImpl<COMPLETE_TYPE, true>
        {
            static void * invoke(void * i_first, void * i_second) noexcept
            {
                DENSITY_ASSERT(i_first != nullptr && i_second != nullptr);
                return new (i_first) COMPLETE_TYPE(*static_cast<const COMPLETE_TYPE *>(i_second));
            }
        };
        template <typename COMPLETE_TYPE> struct CopyConstructImpl<COMPLETE_TYPE, false>
        {
            static void * invoke(void *, void *)
            {
                throw std::logic_error("copy-construction not supported");
            }
        };

        // MoveConstructImpl<COMPLETE_TYPE>::invoke
        template <
          typename COMPLETE_TYPE,
          bool CAN = std::is_nothrow_move_constructible<COMPLETE_TYPE>::value>
        struct MoveConstructImpl;
        template <typename COMPLETE_TYPE> struct MoveConstructImpl<COMPLETE_TYPE, true>
        {
            static void * invoke(void * i_first, void * i_second) noexcept
            {
                DENSITY_ASSERT(i_first != nullptr && i_second != nullptr);
                return new (i_first)
                  COMPLETE_TYPE(std::move(*static_cast<COMPLETE_TYPE *>(i_second)));
            }
        };
        template <typename COMPLETE_TYPE> struct MoveConstructImpl<COMPLETE_TYPE, false>
        {
            static void * invoke(void *, void *)
            {
                throw std::logic_error("move-construction not supported");
            }
        };

    } // namespace detail

    namespace type_features
    {
        /** This feature stores the size in the table of the type */
        struct size
        {
            using type = size_t;

            template <typename BASE, typename TYPE> struct Impl
            {
                static_assert(
                  sizeof(TYPE) < (std::numeric_limits<uintptr_t>::max)() / 4,
                  "Type with size >= 1/4 of the address space are not supported");
                // constraining the size of types allows to reduce the runtime checks to detect pointer arithmetic overflow
                constexpr static size_t value = sizeof(TYPE);
            };
        };

        /** This feature stores the alignment in the table of the type */
        struct alignment
        {
            using type = size_t;

            template <typename BASE, typename TYPE> struct Impl
            {
                static_assert(
                  alignof(TYPE) < (std::numeric_limits<uintptr_t>::max)() / 4,
                  "Type with alignment >= 1/4 of the address space are not supported");
                // constraining the alignment of types allows to reduce the runtime checks to detect pointer arithmetic overflow
                constexpr static size_t value = alignof(TYPE);
            };
        };

        /** This feature computes the hash of an object
            - If a the call hash_func(i_object) is legal, it is used to compute the hash. This function
                should be defined in the namespace that contains TYPE (it will use ADL). If such function exits,
                its return type must be size_t.
            - Otherwise std::hash<TYPE> is used to compute the hash */
        struct hash
        {
            using type = size_t (*)(const void * i_source);

            template <typename BASE, typename TYPE> struct Impl
            {
                constexpr static type value = invoke;

                static size_t invoke(const void * i_source)
                {
                    DENSITY_ASSERT(i_source != nullptr);
                    auto const base_ptr = static_cast<const BASE *>(i_source);
                    return detail::invoke_hash(*detail::down_cast<const TYPE *>(base_ptr));
                }
            };
        };

        /** This feature stores a pointer to a std::type_info associated to a type in the table of the type */
        struct rtti
        {
            using type = const std::type_info & (*)();

            template <typename BASE, typename TYPE> struct Impl
            {
                static const std::type_info & invoke() { return typeid(TYPE); }

                constexpr static type value = invoke;
            };
        };

        /** This feature stores a pointer to the default constructor in the table of the type.
            The effect of the feature is default-constructing an object of type TYPE, and returning a pointer (of type BASE) to it.
                @param i_complete_dest pointer to the storage in which the TYPE must be constructed.
                @return pointer to a subobject (of type BASE) of the new object. */
        struct default_construct
        {
            using type = void * (*)(void * i_complete_dest);

            template <typename BASE, typename TYPE> struct Impl
            {
                static void * invoke(void * i_complete_dest)
                {
                    DENSITY_ASSERT(i_complete_dest != nullptr);
                    BASE * const base_result = new (i_complete_dest) TYPE();
                    return base_result;
                }

                constexpr static type value = invoke;
            };
        };

        /** This feature stores a pointer to the copy constructor in the table of the type.
            The effect of the feature is copy-constructing an object of type TYPE, and returning a pointer (of type BASE) to it.
                @param i_complete_dest pointer to the storage in which the TYPE must be constructed.
                @param i_base_source pointer to a subobject (of type BASE) of an object whose complete
                    type is TYPE, that is to be used as source of the copy.
                @return pointer to a subobject (of type BASE) of the new object. */
        struct copy_construct
        {
            using type = void * (*)(void * i_complete_dest, const void * i_base_source);

            template <typename BASE, typename TYPE> struct Impl
            {
                static void * invoke(void * i_complete_dest, const void * i_base_source)
                {
                    DENSITY_ASSERT(i_base_source != nullptr);
                    auto const base_source = static_cast<const BASE *>(i_base_source);
                    // DENSITY_ASSERT( dynamic_cast<const TYPE*>(base_source) != nullptr ); to do: implement a detail::IsInstanceOf
                    BASE * const base_result =
                      new (i_complete_dest) TYPE(*detail::down_cast<const TYPE *>(base_source));
                    return base_result;
                }

                constexpr static type value = invoke;
            };
        };

        /** This feature stores a pointer to the move constructor in the table of the type.
            The effect of this feature move-constructing an object of type TYPE, and returning a pointer (of type BASE) to it.
                @param i_complete_dest pointer to the storage in which the TYPE must be constructed.
                @param i_base_source pointer to a subobject (of type BASE) of an object whose complete
                    type is TYPE, that is to be used as source of the move.
                @return pointer to a subobject (of type BASE) of the new object. */
        struct move_construct
        {
            using type = void * (*)(void * i_dest, void * i_source);

            template <typename BASE, typename TYPE> struct Impl
            {
                static void * invoke(void * i_complete_dest, void * i_base_source)
                {
                    DENSITY_ASSERT(i_complete_dest != nullptr && i_base_source != nullptr);

                    BASE * base_source = static_cast<BASE *>(i_base_source);
                    // DENSITY_ASSERT(dynamic_cast<const TYPE*>(base_source) != nullptr); to do: implement a detail::IsInstanceOf
                    BASE * base_result = new (i_complete_dest)
                      TYPE(std::move(*detail::down_cast<TYPE *>(base_source)));
                    return base_result;
                }
                constexpr static type value = invoke;
            };
        };

        /** This feature invokes a function object. The template parameter must be a callable type.
                @tparam CALLABLE signature of the object function

            Example:
            \snippet misc_examples.cpp type_features::invoke example 1
        */
#ifndef DOXYGEN_DOC_GENERATION
        template <typename> struct invoke;
        template <typename RET, typename... PARAMS> struct invoke<RET(PARAMS...)>
#else
        template <typename CALLABLE> struct invoke
#endif
        {
            using type = RET (*)(void * i_base_dest, PARAMS... i_params);

            template <typename BASE, typename TYPE> struct Impl
            {
                static RET invoke(void * i_base_dest, PARAMS... i_params)
                {
                    DENSITY_ASSERT(i_base_dest != nullptr);
                    const auto base_dest = static_cast<BASE *>(i_base_dest);
                    return (*detail::down_cast<TYPE *>(base_dest))(
                      std::forward<PARAMS>(i_params)...);
                }
                constexpr static type value = invoke;
            };
        };

        /** This feature invokes and destroys a function object. The template parameter must be a callable type.
            The effect of invoke_destroy is the same of the pair type_features::invoke and type_features::destroy. Anyway
            it is faster than using the latter features in sequence.
                @tparam CALLABLE signature of the object function.
            \sa type_features::invoke
            \sa type_features::destroy */
#ifndef DOXYGEN_DOC_GENERATION
        template <typename> struct invoke_destroy;
        template <typename RET, typename... PARAMS> struct invoke_destroy<RET(PARAMS...)>
#else
        template <typename CALLABLE> struct invoke_destroy
#endif
        {
            using type = RET (*)(void * i_base_dest, PARAMS... i_params);

            template <typename BASE, typename TYPE> struct Impl
            {
                static RET invoke_and_destroy(void * i_base_dest, PARAMS... i_params)
                {
                    return invoke_and_destroy_impl(
                      i_base_dest, std::is_void<RET>(), std::forward<PARAMS>(i_params)...);
                }
                constexpr static type value = invoke_and_destroy;

              private:
                static RET
                  invoke_and_destroy_impl(void * i_base_dest, std::false_type, PARAMS... i_params)
                {
                    const auto base_dest = static_cast<BASE *>(i_base_dest);
                    auto &&    result =
                      (*detail::down_cast<TYPE *>(base_dest))(std::forward<PARAMS>(i_params)...);
                    detail::down_cast<TYPE *>(base_dest)->TYPE::~TYPE();
                    return result;
                }
                static void
                  invoke_and_destroy_impl(void * i_base_dest, std::true_type, PARAMS... i_params)
                {
                    auto const base_dest = static_cast<BASE *>(i_base_dest);
                    auto const ptr       = detail::down_cast<TYPE *>(base_dest);
                    (*ptr)(std::forward<PARAMS>(i_params)...);
                    ptr->TYPE::~TYPE();
                }
            };
        };

        /** This feature aligns, invokes and destroys a function object. The template parameter must be a callable type.
            The effect of invoke_destroy is the same of the pair type_features::invoke and type_features::destroy. Anyway
            it is faster than using the latter features in sequence.
                @tparam CALLABLE signature of the object function.
            \sa type_features::invoke
            \sa type_features::destroy */
#ifndef DOXYGEN_DOC_GENERATION
        template <typename> struct align_invoke_destroy;
        template <typename RET, typename... PARAMS> struct align_invoke_destroy<RET(PARAMS...)>
#else
        template <typename CALLABLE> struct align_invoke_destroy
#endif
        {
            using type = RET (*)(void * i_base_dest, PARAMS... i_params);

            template <typename BASE, typename TYPE> struct Impl
            {
                static_assert(
                  std::is_void<BASE>::value, "align_invoke_destroy: BASE must be a void type");

                static RET align_and_invoke_and_destroy(void * i_base_dest, PARAMS... i_params)
                {
                    return align_and_invoke_and_destroy_impl(
                      i_base_dest, std::is_void<RET>(), std::forward<PARAMS>(i_params)...);
                }
                constexpr static type value = align_and_invoke_and_destroy;

              private:
                static RET align_and_invoke_and_destroy_impl(
                  void * i_base_dest, std::false_type, PARAMS... i_params)
                {
                    auto const base_dest = address_upper_align(i_base_dest, alignof(TYPE));
                    auto const derived   = detail::down_cast<TYPE *>(base_dest);
                    auto &&    result    = (*derived)(std::forward<PARAMS>(i_params)...);
                    derived->TYPE::~TYPE();
                    return result;
                }
                static void align_and_invoke_and_destroy_impl(
                  void * i_base_dest, std::true_type, PARAMS... i_params)
                {
                    auto const base_dest = address_upper_align(i_base_dest, alignof(TYPE));
                    auto const ptr       = detail::down_cast<TYPE *>(base_dest);
                    (*ptr)(std::forward<PARAMS>(i_params)...);
                    ptr->TYPE::~TYPE();
                }
            };
        };

        /** This feature stores a pointer to the destructor of a type.
                @param i_base_dest pointer to the object to be destroyed. */
        struct destroy
        {
            using type = void * (*)(void * i_dest);

            template <typename BASE, typename TYPE> struct Impl
            {
                static void * invoke(void * i_base_dest) noexcept
                {
                    const auto base_dest = static_cast<BASE *>(i_base_dest);
                    const auto derived   = detail::down_cast<TYPE *>(base_dest);
                    derived->TYPE::~TYPE();
                    return derived;
                }
                constexpr static type value = invoke;
            };
        };

        /** This feature stores a pointer to a function that compares two object of the type-erased type.
                @param i_first_base_object pointer to a subobject (of type BASE) of an object whose complete
                    type is TYPE, that is to be used as first operand.
                    @param i_second_base_object pointer to a subobject (of type BASE) of an object whose complete
                    type is TYPE, that is to be used as second operand.
                @return whether the objects compares equal. */
        struct equals
        {
            using type =
              bool (*)(const void * i_first_base_object, const void * i_second_base_object);

            template <typename BASE, typename TYPE> struct Impl
            {
                static bool
                  invoke(const void * i_first_base_object, const void * i_second_base_object)
                {
                    DENSITY_ASSERT(
                      i_first_base_object != nullptr && i_second_base_object != nullptr);
                    auto const base_first      = static_cast<const BASE *>(i_first_base_object);
                    auto const base_second     = static_cast<const BASE *>(i_second_base_object);
                    auto const complete_first  = detail::down_cast<const TYPE *>(base_first);
                    auto const complete_second = detail::down_cast<const TYPE *>(base_second);
                    return *complete_first == *complete_second;
                }

                constexpr static type value = invoke;
            };
        };

        /** This feature stores a pointer to a function that compares two object of the type-erased type.
                @param i_first_base_object pointer to a subobject (of type BASE) of an object whose complete
                    type is TYPE, that is to be used as first operand.
                    @param i_second_base_object pointer to a subobject (of type BASE) of an object whose complete
                    type is TYPE, that is to be used as second operand.
                @return whether the first object compares less than the first object. */
        struct less
        {
            using type =
              bool (*)(const void * i_first_base_object, const void * i_second_base_object);

            template <typename BASE, typename TYPE> struct Impl
            {
                static bool
                  invoke(const void * i_first_base_object, const void * i_second_base_object)
                {
                    DENSITY_ASSERT(
                      i_first_base_object != nullptr && i_second_base_object != nullptr);
                    auto const base_first      = static_cast<const BASE *>(i_first_base_object);
                    auto const base_second     = static_cast<const BASE *>(i_second_base_object);
                    auto const complete_first  = detail::down_cast<const TYPE *>(base_first);
                    auto const complete_second = detail::down_cast<const TYPE *>(base_second);
                    return *complete_first < *complete_second;
                }

                constexpr static type value = invoke;
            };
        };

    } // namespace type_features

    namespace detail
    {
        /* GetDefaultFeatures<TYPE>::type. Implementation of default_type_features */
        template <
          typename TYPE,
          bool CAN_COPY = std::is_copy_constructible<TYPE>::value || std::is_void<TYPE>::value,
          bool CAN_MOVE =
            std::is_nothrow_move_constructible<TYPE>::value || std::is_void<void>::value>
        struct GetDefaultFeatures;

        template <typename TYPE> struct GetDefaultFeatures<TYPE, false, false>
        {
            using type = type_features::feature_list<
              type_features::size,
              type_features::alignment,
              type_features::rtti,
              type_features::destroy>;
        };
        template <typename TYPE> struct GetDefaultFeatures<TYPE, true, false>
        {
            using type = type_features::feature_list<
              type_features::size,
              type_features::alignment,
              type_features::rtti,
              type_features::destroy,
              type_features::copy_construct>;
        };
        template <typename TYPE> struct GetDefaultFeatures<TYPE, false, true>
        {
            using type = type_features::feature_list<
              type_features::size,
              type_features::alignment,
              type_features::rtti,
              type_features::destroy,
              type_features::move_construct>;
        };
        template <typename TYPE> struct GetDefaultFeatures<TYPE, true, true>
        {
            using type = type_features::feature_list<
              type_features::size,
              type_features::alignment,
              type_features::rtti,
              type_features::destroy,
              type_features::move_construct,
              type_features::copy_construct>;
        };

    } // namespace detail

    namespace type_features
    {

        /** \class default_type_features
            This type alias template gives a feature_list for a given type.
                @tparam COMMON_TYPE type to use to select the features to include. \n


            - The result feature_list always includes: type_features::size, type_features::alignment, type_features::rtti, type_features::destroy.\n
            - If COMMON_TYPE is copy-constructible, copy_construct is included
            - If COMMON_TYPE is nothrow move-constructible, move_construct is included

            default_type_features_t is an alias for default_type_features<...>::type

            @code
            template <typename COMMON_TYPE>
                using default_type_features_t = typename default_type_features<COMMON_TYPE>::type;
            @endcode

            Note: A feature_list does not depend on a type. The template argument is used only to decide which features to include.
        */
#ifndef DOXYGEN_DOC_GENERATION
        template <typename COMMON_TYPE>
        using default_type_features = typename detail::GetDefaultFeatures<COMMON_TYPE>;
#else
        template <typename COMMON_TYPE> struct default_type_features
        {
            using type = default_type_features_t<COMMON_TYPE>;
        };
#endif

        /** Alias for default_type_features<COMMON_TYPE>::type */
        template <typename COMMON_TYPE>
        using default_type_features_t = typename default_type_features<COMMON_TYPE>::type;

    } // namespace type_features

    /*! \page RuntimeType_concept RuntimeType concept

        The RuntimeType concept provides at runtime data and functionalities specific to a type (the <em>target type</em>), like
        ctors, dtor, or retrieval of size and alignment.

        The target type is assigned with the make static function template (see below). A default constructed RuntimeType is empty,
        that is it has no target type. Trying to use any feature of the target type on an empty RuntimeType results is undefined behavior.

        <table>
        <tr><th style="width:600px">Requirement                      </th><th>Semantic</th></tr>
        <tr>
            <td>Non-throwing default constructor and destructor</td>
            <td>A default constructed RuntimeType is empty (not assigned to a target type).</td>
        </tr>
        <tr>
            <td>Copy constructor and copy assignment</td>
            <td>The destination RuntimeType gets the same target type of the source RuntimeType.</td>
        </tr>
        <tr>
            <td>Non-throwing move constructor and non-throwing move assignment</td>
            <td>The destination RuntimeType gets the target type of the source RuntimeType. The source becomes empty.</td>
        </tr>
        <tr>
            <td>Operators == and !=</td>
            <td>Checks for equality\\inequality. Two RuntimeType are equal if they have the same target type.</td>
        </tr>
        <tr>
            <td>Type alias: @code using common_type = [implementation defined] @endcode</td>
            <td>The type to which all the types handled by this RuntimeType are covariant. common_type can be void, is which case any target type is legal.</td>
        </tr>
        <tr>
            <td>Static function template: @code
                template <typename TARGET_TYPE>\n
                    static RuntimeType make() noexcept @endcode</td>
            <td>Returns a RuntimeType that has TARGET_TYPE as target type. The target type must be covariant to <em>common_type</em>, otherwise the
                behavior is undefined</td>
        </tr>
        <tr>
            <td>Member function: @code size_t size() const noexcept @endcode</td>
            <td>Equivalent to: @code return sizeof(TARGET_TYPE); @endcode. </td>
        </tr>
        <tr>
            <td>Member function: @code size_t alignment() const noexcept @endcode</td>
            <td>Equivalent to: @code return aignof(TARGET_TYPE); @endcode. </td>
        </tr>
        <tr>
            <td>Member function: @code common_type * default_construct(void * i_dest) const @endcode</td>
            <td>Equivalent to: @code return static_cast<common_type*>( new(i_dest) TARGET_TYPE() ); @endcode. </td>
        </tr>
        <tr>
            <td>Member function: @code common_type * copy_construct(void * i_dest, const common_type * i_source) const @endcode</td>
            <td>Equivalent to: @code return static_cast<common_type*>( new(i_dest) TARGET_TYPE(\n
                        *dynamic_cast<const TARGET_TYPE*>(i_source) ) ); @endcode. </td>
        </tr>
        <tr>
            <td>Member function: @code common_type * move_construct(void * i_dest, common_type * i_source) const noexcept @endcode</td>
            <td>Equivalent to: @code return static_cast<common_type*>( new(i_dest) TARGET_TYPE(\n
                        std::move(*dynamic_cast<TARGET_TYPE*>(i_source)) ) ); @endcode </td>
        </tr>
        <tr>
            <td>Member function: @code void * destroy(common_type * i_dest) const noexcept @endcode</td>
            <td>Equivalent to: @code dynamic_cast<TARGET_TYPE*>(i_dest)->~TARGET_TYPE::TARGET_TYPE();\nreturn dynamic_cast<TARGET_TYPE*>(i_dest); @endcode </td>
        </tr>
        <tr>
            <td>Member function: @code const std::type_info & type_info() const noexcept @endcode</td>
            <td>Equivalent to: @code return typeid(TARGET_TYPE); @endcode </td>
        </tr>
        </table>
    */

    /** Class template that performs type-erasure.
            @tparam COMMON_TYPE Type to which all type-erased types are covariant. If it is void, any type can be type-erased.
            @tparam FEATURE_LIST Type_features::feature_list that defines which type-features are type-erased. By default
                the feature_list is obtained with type_features::default_type_features. If this type is not a type_features::feature_list,
                a compile time error is reported.

        runtime_type meets the requirements of \ref RuntimeType_concept "RuntimeType".

        An instance of runtime_type binds at runtime to a target type. It can be used to construct, copy-construct, destroy, etc.,
        instances of the target types, depending on the features included on <code>FEATURE_LIST</code>. \n
        A runtime_type bound to a type can be created with the static function runtime_type::make. runtime_type has value semantic, and is copyable. \n
        A default-constructed runtime_type is empty: trying to use type-features of an empty runtime_type leads to undefined behavior.
        A runtime_type becomes empty is the member function \ref clear is called.

        runtime_type exposes a set of functions to manipulate instances of the target type. Furthermore it can be extended
        with any type feature built-in in density (for example type_features::equals or type_features::less). User-defined
        features are also supported, to add custom capabilities to the type erasure (see the examples below). \n
        Features can be queried with the function runtime_type::get_feature, specifying the requested feature at compile-time as template
        argument. The search is performed at compile time. If the requested feature is not included in <code>FEATURE_LIST</code>, a
        static_assert fails.

        \n\b Implementation runtime_type is implemented as a pointer to a pseudo vtable, that is a static array of feature
            values: for every feature in FEATURE_LIST there is an entry in this vtable. Most entries are pointer to functions.
            Anyway, some features (notably type_features::size and type_features::alignment) store a static const value.


        In this example an <code>std::string</code> is created and destroyed using a runtime_type.

        \snippet misc_examples.cpp runtime_type example 1

        This is an example of user-defined features: it calls a function named <code>update</code>, that takes as parameter a <code>float</code>.

        \snippet misc_examples.cpp runtime_type example 2

        The example below uses feature_call_update. Note that:
            - <code>ObjectA</code> and <code>ObjectB</code> are unrelated types (no common base class)
            - <code>ObjectA::update</code> and <code>ObjectB::update</code> are not virtual functions
            - If a <code>std::string</code> was added to the array, a compile time error would report that <code>std::string</code> has not an update function

        \snippet misc_examples.cpp runtime_type example 3

        Output:

        ~~~~~~~~~~~~~~
        ObjectA::update(0.0166667)
        ObjectB::update(0.0166667)
        ObjectB::update(0.0166667)
        ~~~~~~~~~~~~~~
    */
    template <
      typename COMMON_TYPE  = void,
      typename FEATURE_LIST = type_features::default_type_features_t<COMMON_TYPE>>
    class runtime_type
    {
      public:
        /** Alias for the template argument COMMON_TYPE */
        using common_type = COMMON_TYPE;

        /** Alias for the template argument FEATURE_LIST */
        using feature_list = FEATURE_LIST;

        /** Creates a runtime_type associated with the specified type. The latter is the target type.
                @tparam TYPE target type that is type-erased by the returned runtime_type. */
        template <typename TYPE> constexpr static runtime_type make() noexcept
        {
            return runtime_type(
              &detail::FeatureTable<common_type, typename std::decay<TYPE>::type, FEATURE_LIST>::
                s_table);
        }

        /** Construct an empty runtime_type not associated with any type. Trying to use any feature of an empty
            runtime_type leads to undefined behavior. */
        constexpr runtime_type() noexcept = default;

        /** Copy-constructs a runtime_type */
        constexpr runtime_type(const runtime_type &) noexcept = default;

        /** Copy-assigns a runtime_type. Self assignment (a = a) is not supported, and leads to undefined behavior. */
        constexpr runtime_type & operator=(const runtime_type &) noexcept = default;

        /** Returns whether this runtime_type is not bound to a target type */
        constexpr bool empty() const noexcept { return m_feature_table == nullptr; }

        /** Unbinds from a target. If the runtime_type was already empty this function has no effect. */
        constexpr void clear() noexcept { m_feature_table = nullptr; }

        /** Returns the size (in bytes) of the target type, which is always > 0. \n
            The effect of this function is the same of this code:
                @code
                    return sizeof(TARGET_TYPE);
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make).

            \n\b Requires:
                - the feature type_features::size must be included in the FEATURE_LIST \n
                - the runtime_type must be non-empty

            \n\b Throws: nothing */
        constexpr size_t size() const noexcept { return get_feature<type_features::size>(); }

        /** Returns the alignment (in bytes) of the target type, which is always an integer power of 2. \n

            The effect of this function is the same of this code:
                @code
                    return alignof(TARGET_TYPE);
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make).

            \n\b Requires:
                - the feature type_features::alignment must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: nothing */
        constexpr size_t alignment() const noexcept
        {
            return get_feature<type_features::alignment>();
        }

        /** Default constructs an instance of the target type on the specified uninitialized storage. \n
            The effect of this function is the same of this code:
                @code
                    return static_cast<common_type*>( new(i_dest) TARGET_TYPE() );
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make). Note that primitive
            types are initialized by this expression.
            @param i_dest pointer to a buffer in which the target type is inplace constructed. This buffer
                must be large at least as the result of runtime_type::size, and must be aligned at least according to runtime_type::alignment.
            @return pointer to the common_type subobject of the instance of TARGET_TYPE that has been constructed. Note: do not
                assume that the value of this pointer is the same of i_dest.

            \n\b Requires:
                - the feature type_features::default_construct must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: anything that the default constructor of the target type throws. */
        common_type * default_construct(void * i_dest) const
        {
            DENSITY_ASSERT(!empty());
            return static_cast<common_type *>(
              get_feature<type_features::default_construct>()(i_dest));
        }

        /** Copy constructs an instance of the target type on the specified uninitialized storage. \n
            The effect of this function is the same of this code:
                @code
                    return static_cast<common_type*>( new(i_dest) TARGET_TYPE(
                        *dynamic_cast<const TARGET_TYPE*>(i_source) ) );
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make). \n
            @param i_dest pointer to a buffer in which the target type is inplace constructed. This buffer
                must be large at least as the result of runtime_type::size, and must be aligned at least according to runtime_type::alignment.
            @param i_source pointer to a subobject common_type of an instance of TARGET_TYPE.
            @return pointer to the common_type subobject of the instance of TARGET_TYPE that has been constructed. Note: do not
                assume that the value of this pointer is the same of i_dest.

            \n\b Requires:
                - the feature type_features::copy_construct must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: anything that the copy constructor of the target type throws. */
        common_type * copy_construct(void * i_dest, const common_type * i_source) const
        {
            DENSITY_ASSERT(!empty());
            return static_cast<common_type *>(
              get_feature<type_features::copy_construct>()(i_dest, i_source));
        }

        /** Move constructs an instance of the target type on the specified uninitialized storage. \n
            The effect of this function is the same of this code:
                @code
                    return static_cast<common_type*>( new(i_dest) TARGET_TYPE(
                        std::move(*dynamic_cast<TARGET_TYPE*>(i_source)) ) );
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make). \n
            @param i_dest pointer to a buffer in which the target type is inplace constructed. This buffer
                must be large at least as the result of runtime_type::size, and must be aligned at least according to runtime_type::alignment.
            @param i_source pointer to a subobject common_type of an instance of TARGET_TYPE.
            @return pointer to the common_type subobject of the instance of TARGET_TYPE that has been constructed. Note: do not
                assume that the value of this pointer is the same of i_dest.

            \n\b Requires:
                - the feature type_features::move_construct must be included in the FEATURE_LIST
                - the runtime_type must be non-empty
            */
        common_type * move_construct(void * i_dest, common_type * i_source) const
        {
            DENSITY_ASSERT(!empty());
            return static_cast<common_type *>(
              get_feature<type_features::move_construct>()(i_dest, i_source));
        }

        /** Destroys an object of the target type through a pointer to the subobject common_type.

            The effect of this function is the same of this code:
                @code
                    dynamic_cast<TARGET_TYPE*>(i_source)->~TARGET_TYPE::TARGET_TYPE();
                    return dynamic_cast<TARGET_TYPE*>(i_source);
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make). \n

            @return pointer to the complete object that has been destroyed. This pointer can be used
                to deallocate a memory block on the heap.

            \n\b Requires:
                - the feature type_features::destroy must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: nothing. Destructors are required to be noexcept. */
        void * destroy(common_type * i_dest) const noexcept
        {
            DENSITY_ASSERT(!empty());
            return get_feature<type_features::destroy>()(i_dest);
        }

        /** Returns the std::type_info associated to the target type.

            The effect of this function is the same of this code:
                @code
                    return typeid(TARGET_TYPE);
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make). \n

            \n\b Requires:
                - the feature type_features::rtti must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: nothing. */
        const std::type_info & type_info() const noexcept
        {
            DENSITY_ASSERT(!empty());
            return get_feature<type_features::rtti>()();
        }

        /** Returns true if two instances of the target types compare equal.
            \n\b Throws: unspecified
        */
        bool are_equal(const common_type * i_first, const common_type * i_second) const
        {
            DENSITY_ASSERT(i_first != nullptr && i_second != nullptr && !empty());
            return get_feature<type_features::equals>()(i_first, i_second);
        }

        /** Returns the feature matching the specified type, if present. If the feature is not present, a static_assert fails.
            This function grant access to features that are not part of the interface of runtime_type.

            The search a the feature is done at compile time, so the complexity is alway constant.

            \n\b Requires:
                - the feature FEATURE must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: nothing. */
        template <typename FEATURE>
        constexpr const typename FEATURE::type & get_feature() const noexcept
        {
            return std::get<detail::IndexOfFeature<0, FEATURE, FEATURE_LIST>::value>(
              *m_feature_table);
        }

        /** Returns true whether this two runtime_type have the same target type. All empty runtime_type's compare equals.

            \n\b Requires: nothing

            \n\b Throws: nothing. */
        constexpr bool operator==(const runtime_type & i_other) const noexcept
        {
            return m_feature_table == i_other.m_feature_table;
        }

        /** Returns true whether this two runtime_type have different target types. All empty runtime_type's compare equals.

            \n\b Requires: nothing

            \n\b Throws: nothing. */
        constexpr bool operator!=(const runtime_type & i_other) const noexcept
        {
            return m_feature_table != i_other.m_feature_table;
        }

        /** Returns whether the target type of this runtime_type is exactly the one specified in the
            template parameter. Equivalent to *this == runtime_type::make<TYPE>() */
        template <typename TYPE> constexpr bool is() const noexcept
        {
            return m_feature_table == &detail::FeatureTable<
                                        common_type,
                                        typename std::decay<TYPE>::type,
                                        FEATURE_LIST>::s_table;
        }

        /** Returns an hash. This function is used for the partial specialization of std::hash
            for runtime_type. */
        size_t hash() const noexcept { return std::hash<const void *>()(m_feature_table); }

      private:
        constexpr runtime_type(const typename FEATURE_LIST::tuple * i_feature_table) noexcept
            : m_feature_table(i_feature_table)
        {
        }

      public:
        const typename FEATURE_LIST::tuple * m_feature_table{};
    };
} // namespace density

namespace std
{
    /** Partial specialization of std::hash to allow the use of density::runtime_type as key
        for unordered associative containers. */
    template <typename COMMON_TYPE, typename FEATURE_LIST>
    struct hash<density::runtime_type<COMMON_TYPE, FEATURE_LIST>>
    {
        size_t
          operator()(const density::runtime_type<COMMON_TYPE, FEATURE_LIST> & i_runtime_type) const
          noexcept
        {
            return i_runtime_type.hash();
        }
    };
} // namespace std
