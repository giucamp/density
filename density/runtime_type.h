
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/detail/runtime_type_internals.h>
#include <functional> // for std::hash
#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace density
{
    /** Type-list class template that specifies which features a runtime_type captures from a type (the target type).

        A feature is a class that capture and exposes a specific feature of a target type, without 
        depending from it at compile time. Most times they use a pointer to a function (that is like
        an entry in a vtable), but they may hold just data (like f_size and f_alignment do). 

        Each type in the list is either:
        - a type modeling the [TypeFeature](TypeFeature_concept.html) concept, like a built-in type fetaure (f_size, f_alignment, 
            f_default_construct, f_copy_construct, f_destroy, ...), or a user defined type feature.
        - a nested feature_list
        - the special feature f_none

        feature_list provides a template alias to an std::tuple, that is used by runtime_type
        as pseudo vtable. Actually a runtime_type is a pointer to a static constexpr instance of this tuple.
        \snippet runtime_type_examples.cpp feature_list example 1
        For the composition of the tuple:
        - nested features are flatened
        - duplicates are removed (only the first occurrence of a fetaure is considered)
        - any occurrence of f_none is stripped out
        \snippet runtime_type_examples.cpp feature_list example 2
        If two feature_list produce the same tuple they are similar:
        \snippet runtime_type_examples.cpp feature_list example 3 */
    template <typename... FEATURES> struct feature_list;
    template <> struct feature_list<> /* an empty feature_list gives an empty tuple */
    {
        template <typename COMMON_ANCESTOR> using tuple = std::tuple<>;
    };
    template <typename FIRST_FEATURE, typename... OTHER_FEATURES>
    struct feature_list<FIRST_FEATURE, OTHER_FEATURES...> /* the generic case, to 
        handle normal features */
    {
        template <typename COMMON_ANCESTOR>
        using tuple = detail::Tuple_Merge_t<

          // add FIRST_FEATURE to the tuple
          std::tuple<typename FIRST_FEATURE::template type<COMMON_ANCESTOR>>,

          /* use recursively feature_list to add the other features, but removes 
             FIRST_FEATURE from the result */
          detail::Tuple_Remove_t<
            typename feature_list<OTHER_FEATURES...>::template tuple<COMMON_ANCESTOR>,
            typename FIRST_FEATURE::template type<COMMON_ANCESTOR>>

          >;
    };
    template <typename... GROUPED_FEATURES, typename... OTHER_FEATURES>
    struct feature_list<feature_list<GROUPED_FEATURES...>, OTHER_FEATURES...> /* handle 
        nesting of feature_list's */
    {
        template <typename COMMON_ANCESTOR>
        using tuple = detail::Tuple_Merge_t<

          // add to the tuple GROUPED_FEATURES (the nested fetaures)
          typename feature_list<GROUPED_FEATURES...>::template tuple<COMMON_ANCESTOR>,

          // add to the tuple the remaining features with the fetaures already in GROUPED_FEATURES removed
          detail::Tuple_Diff_t<
            typename feature_list<OTHER_FEATURES...>::template tuple<COMMON_ANCESTOR>,
            typename feature_list<GROUPED_FEATURES...>::template tuple<COMMON_ANCESTOR>>

          >;
    };
    template <typename... OTHER_FEATURES>
    struct feature_list<f_none, OTHER_FEATURES...> /* strip f_none from the feature list*/
    {
        template <typename COMMON_ANCESTOR>
        using tuple = typename feature_list<OTHER_FEATURES...>::template tuple<COMMON_ANCESTOR>;
    };

    /** Tag type that is ignored in a feature list. This peseudo-feature is usefull to conditionally add features
        to a feature_list, for example using std::conditional. For example f_none is used in the definition of 
        default_type_features. */
    struct f_none
    {
    };

    struct f_size
    {
        /** This type-feature gives the size of the target type
        @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's */
        template <typename COMMON_ANCESTOR> struct type
        {
            size_t const m_size;

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                // constraining the size of types allows to reduce the runtime checks to detect pointer arithmetic overflow
                static_assert(
                  sizeof(TARGET_TYPE) < (std::numeric_limits<uintptr_t>::max)() / 4,
                  "Type with size >= 1/4 of the address space are not supported");

                return type{sizeof(TARGET_TYPE)};
            }

            /** Returns the size of the target type. */
            constexpr size_t operator()() const noexcept { return m_size; }
        };
    };

    struct f_alignment
    {
        /** This type-feature gives the alignment of the target type
        @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's */
        template <typename COMMON_ANCESTOR> struct type
        {
            size_t const m_alignment;

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                // constraining the alignment of types allows to reduce the runtime checks to detect pointer arithmetic overflow
                static_assert(
                  alignof(TARGET_TYPE) < (std::numeric_limits<uintptr_t>::max)() / 4,
                  "Type with alignment >= 1/4 of the address space are not supported");

                return type{alignof(TARGET_TYPE)};
            }

            /** Returns the alignment of the target type. */
            constexpr size_t operator()() const noexcept { return m_alignment; }
        };
    };

    struct f_hash
    {
        /** This feature computes the hash of an instance of the target type
        @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's
        - If a function hash_func(const TARGET_TYPE & i_object) exists, it is used to compute the hash. This function
            should be defined in the namespace that contains TARGET_TYPE (it will use ADL). If such function exits,
            its return type must be size_t and must be noexcept.
        - Otherwise std::hash<TARGET_TYPE> is used to compute the hash */
        template <typename COMMON_ANCESTOR> struct type
        {
            /** Pointer to the function that computes the hash */
            size_t (*const m_hash_func)(const COMMON_ANCESTOR * i_source) DENSITY_CPP17_NOEXCEPT;

            /** Computes the hash of an instance of the target type object. 
            @param i_source pointer to an instance of the target type. Can't be null.
                If the dynamic type of the pointed object is not the taget type (assigned
                by the function make), the behaviour is undefined. */
            size_t operator()(const COMMON_ANCESTOR * i_source) const noexcept
            {
                return (*m_hash_func)(i_source);
            }

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                return type{&hash_func<TARGET_TYPE>};
            }

          private:
            template <typename TARGET_TYPE>
            static size_t hash_func(const COMMON_ANCESTOR * i_source) noexcept
            {
                DENSITY_ASSERT(i_source != nullptr);
                return detail::invoke_hash(*detail::down_cast<const TARGET_TYPE *>(i_source));
            }
        };
    };

    struct f_rtti
    {
        /** This feature returns the std::type_info associated to the target type.
        @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's */
        template <typename COMMON_ANCESTOR> struct type
        {
            /** Pointer to the function that returns the std::type_info of the object */
            std::type_info const & (*const m_type_info_func)() DENSITY_CPP17_NOEXCEPT;

            /** Returns the std::type_info of the target type. */
            std::type_info const & operator()() const noexcept { return (*m_type_info_func)(); }

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                return type{&get_type_info_func<TARGET_TYPE>};
            }

          private:
            template <typename TARGET_TYPE>
            static const std::type_info & get_type_info_func() noexcept
            {
                return typeid(TARGET_TYPE);
            }
        };
    };

    struct f_default_construct
    {
        /** Constructs to an uninitialized memory buffer a value-initialized instance of the target type.
        @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's */
        template <typename COMMON_ANCESTOR> struct type
        {
            /** Pointer to the function that value-constructs the target object. */
            COMMON_ANCESTOR * (*const m_construct_func)(void * i_dest);

            /** Constructs in an uninitialized memory buffer a value-initialized instance of the target type.
            @param i_dest where the target object must be constructed. Can't be null. If the buffer 
                pointed by this parameter does not respect the size and alignment of the target type,
                the behaviour is undefined.
            @return A pointer to the COMMON_ANCESTOR subobject of the constructed object. Note: the
                value of this pointer may or may not be the same of the input patrameter. */
            COMMON_ANCESTOR * operator()(void * i_dest) const
            {
                return (*m_construct_func)(i_dest);
            }

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                return type{&construct_func<TARGET_TYPE>};
            }

          private:
            template <typename TARGET_TYPE> static COMMON_ANCESTOR * construct_func(void * i_dest)
            {
                DENSITY_ASSERT(i_dest != nullptr);
                COMMON_ANCESTOR * const result = new (i_dest) TARGET_TYPE();
                return result;
            }
        };
    };

    struct f_copy_construct
    {
        /** Copy-constructs to an uninitialized memory buffer an instance of the target type.
            @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's */
        template <typename COMMON_ANCESTOR> struct type
        {
            /** Pointer to the function that copy-constructs the target object. */
            COMMON_ANCESTOR * (*const m_construct_func)(
              void * i_dest, const COMMON_ANCESTOR * i_source);

            /** Copy-constructs in an uninitialized memory buffer an instance of the target type.
            @param i_dest where the target object must be constructed. Can't be null. If the buffer
                pointed by this parameter does not respect the size and alignment of the target type,
                the behaviour is undefined.
            @param i_source pointer to the source object. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined.
            @return A pointer to the COMMON_ANCESTOR subobject of the constructed object. Note: the
                value of this pointer may or may not be the same of the input patrameter. */
            COMMON_ANCESTOR * operator()(void * i_dest, const COMMON_ANCESTOR * i_source) const
            {
                return (*m_construct_func)(i_dest, i_source);
            }

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                return type{&copy_construct_func<TARGET_TYPE>};
            }

          private:
            template <typename TARGET_TYPE>
            static COMMON_ANCESTOR *
              copy_construct_func(void * i_dest, const COMMON_ANCESTOR * i_source)
            {
                DENSITY_ASSERT(i_dest != nullptr && i_source != nullptr);
                const TARGET_TYPE &     source = *detail::down_cast<const TARGET_TYPE *>(i_source);
                COMMON_ANCESTOR * const result = new (i_dest) TARGET_TYPE(source);
                return result;
            }
        };
    };

    struct f_move_construct
    {
        /** Move-constructs to an uninitialized memory buffer an instance of the target type.
            @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's */
        template <typename COMMON_ANCESTOR> struct type
        {
            /** Pointer to the function that move-constructs the target object. */
            COMMON_ANCESTOR * (*const m_construct_func)(void * i_dest, COMMON_ANCESTOR * i_source);

            /** Move-constructs in an uninitialized memory buffer an instance of the target type.
            @param i_dest where the target object must be constructed. Can't be null. If the buffer
                pointed by this parameter does not respect the size and alignment of the target type,
                the behaviour is undefined.
            @param i_source pointer to the source object. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined.
            @return A pointer to the COMMON_ANCESTOR subobject of the constructed object. Note: the
                value of this pointer may or may not be the same of the input patrameter. */
            COMMON_ANCESTOR * operator()(void * i_dest, COMMON_ANCESTOR * i_source) const
            {
                return (*m_construct_func)(i_dest, i_source);
            }

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                return type{&move_construct_func<TARGET_TYPE>};
            }

          private:
            template <typename TARGET_TYPE>
            static COMMON_ANCESTOR * move_construct_func(void * i_dest, COMMON_ANCESTOR * i_source)
            {
                DENSITY_ASSERT(i_dest != nullptr && i_source != nullptr);
                TARGET_TYPE &           source = *detail::down_cast<TARGET_TYPE *>(i_source);
                COMMON_ANCESTOR * const result = new (i_dest) TARGET_TYPE(std::move(source));
                return result;
            }
        };
    };

    struct f_destroy
    {
        /** Destroys an instance of the target type, and returns the address of the complete type.
        @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's */
        template <typename COMMON_ANCESTOR> struct type
        {
            /** Pointer to the function that move-constructs the target object. */
            void * (*const m_destroy_func)(COMMON_ANCESTOR * i_object)DENSITY_CPP17_NOEXCEPT;

            /** Destroys an instance of the target type.
            @param i_object pointer to the object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined.
            @return The address of the complete object that has been destroyed. */
            void * operator()(COMMON_ANCESTOR * i_object) const noexcept
            {
                return (*m_destroy_func)(i_object);
            }

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                return type{&destroy_func<TARGET_TYPE>};
            }

          private:
            template <typename TARGET_TYPE> static void * destroy_func(COMMON_ANCESTOR * i_object)
            {
                DENSITY_ASSERT(i_object != nullptr);
                TARGET_TYPE * obj = detail::down_cast<TARGET_TYPE *>(i_object);
                obj->TARGET_TYPE::~TARGET_TYPE();
                return obj;
            }
        };
    };

    struct f_equals
    {
        /** Compares two objects for equality. The target type must have an operator == .
        @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's */
        template <typename COMMON_ANCESTOR> struct type
        {
            /** Pointer to the function that compares two target objects. */
            bool (*const m_compare_func)(
              COMMON_ANCESTOR const * i_first, COMMON_ANCESTOR const * i_second);

            /** Returns whether two target object compare equal.
            @param i_first pointer to the first object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined.
            @param i_second pointer to the second object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined. */
            bool operator()(COMMON_ANCESTOR const * i_first, COMMON_ANCESTOR const * i_second) const
            {
                return (*m_compare_func)(i_first, i_second);
            }

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                return type{&compare_equal<TARGET_TYPE>};
            }

          private:
            template <typename TARGET_TYPE>
            static bool
              compare_equal(COMMON_ANCESTOR const * i_first, COMMON_ANCESTOR const * i_second)
            {
                DENSITY_ASSERT(i_first != nullptr && i_second != nullptr);
                auto const & first  = *detail::down_cast<TARGET_TYPE const *>(i_first);
                auto const & second = *detail::down_cast<TARGET_TYPE const *>(i_second);
                bool const   result = first == second;
                return result;
            }
        };
    };

    struct f_less
    {
        /** Compares two objects. The target type must have an operator < .
            @tparam COMMON_ANCESTOR void or common base class of all the TARGET_TYPE's */
        template <typename COMMON_ANCESTOR> struct type
        {
            /** Pointer to the function that compares two target objects. */
            bool (*const m_compare_func)(
              COMMON_ANCESTOR const * i_first, COMMON_ANCESTOR const * i_second);

            /** Returns whether the first object compares less than the second object.
            @param i_first pointer to the first object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined.
            @param i_second pointer to the second object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined. */
            bool operator()(COMMON_ANCESTOR const * i_first, COMMON_ANCESTOR const * i_second) const
            {
                return (*m_compare_func)(i_first, i_second);
            }

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                return type{&compare_less<TARGET_TYPE>};
            }

          private:
            template <typename TARGET_TYPE>
            static bool
              compare_less(COMMON_ANCESTOR const * i_first, COMMON_ANCESTOR const * i_second)
            {
                DENSITY_ASSERT(i_first != nullptr && i_second != nullptr);
                auto const & first  = *detail::down_cast<TARGET_TYPE const *>(i_first);
                auto const & second = *detail::down_cast<TARGET_TYPE const *>(i_second);
                bool const   result = first < second;
                return result;
            }
        };
    };

    /** \class default_type_features
        This type alias template gives a feature_list for a given type.
            @tparam COMMON_TYPE type to use to select the features to include. \n


        - The result feature_list always includes: f_size, f_alignment, f_rtti, f_destroy.\n
        - If COMMON_TYPE is copy-constructible, f_copy_construct is included
        - If COMMON_TYPE is nothrow move-constructible, f_move_construct is included

        default_type_features_t is an alias for default_type_features<...>::type

        @code
        template <typename COMMON_TYPE>
            using default_type_features_t = typename default_type_features<COMMON_TYPE>::type;
        @endcode

        Note: A feature_list does not depend on a type. The template argument is used only to decide which features to include.
    */
    template <typename COMMON_TYPE>
    using default_type_features = feature_list<
      f_size,
      f_alignment,
      typename std::conditional<
        std::is_copy_constructible<COMMON_TYPE>::value || std::is_void<COMMON_TYPE>::value,
        f_copy_construct,
        f_none>::type,
      typename std::conditional<
        std::is_move_constructible<COMMON_TYPE>::value || std::is_void<COMMON_TYPE>::value,
        f_move_construct,
        f_none>::type,
      f_rtti,
      f_destroy>;

    namespace detail
    {
        /* FeaturesToTuple - this trait gives the fetaure tuple associated to a type list introducing the
            special case of empty feature list, that resolves to default_type_features. */
        template <typename COMMON_TYPE, typename... FEATURES> struct FeaturesToTuple
        {
            using type = typename feature_list<FEATURES...>::template tuple<COMMON_TYPE>;
        };
        template <typename COMMON_TYPE> struct FeaturesToTuple<COMMON_TYPE>
        {
            using type = typename default_type_features<COMMON_TYPE>::template tuple<COMMON_TYPE>;
        };

        template <typename FEATURE_TUPLE, typename TARGET_TYPE> struct FeatureTable
        {
            constexpr static FEATURE_TUPLE s_table = detail::MakeFeatureTable<
              FEATURE_TUPLE>::template make_table<typename std::decay<TARGET_TYPE>::type>();
        };
        template <typename FEATURE_TUPLE, typename TARGET_TYPE>
        constexpr FEATURE_TUPLE FeatureTable<FEATURE_TUPLE, TARGET_TYPE>::s_table;

    } // namespace detail

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

    /*! \page TypeFeature_concept TypeFeature concept

        A type feature is a class that capture and exposes a specific feature of a target type, without
        depending from it at compile time. Most times they use a pointer to a function (that is like
        an entry in a vtable), but they may hold just data (like f_size and f_alignment do).
        
        A class modeling this concept has this form:
        \code
        struct f_feature
        {
            template <typename COMMON_ANCESTOR> struct type
            {
                // returns an instance of this feature bound to a specific target type
                template <typename TARGET_TYPE>
                    constexpr static type make() noexcept;

                // invokes the feature. The return type and the parameters depends on the specific feature
                ... operator()(...) const;
            };
        };
        \endcode
    */

    /** Class template that performs type-erasure.
            @tparam COMMON_TYPE Type to which all type-erased types are covariant. If it is void, any type can be type-erased.
            @tparam FEATURES... list of features to be captures from the target type. 
            
            Type_features::feature_list that defines which type-features are type-erased. By default
                the feature_list is obtained with default_type_features. If this type is not a feature_list,
                a compile time error is reported.

        runtime_type meets the requirements of \ref RuntimeType_concept "RuntimeType".

        An instance of runtime_type binds at runtime to a target type. It can be used to construct, copy-construct, destroy, etc.,
        instances of the target types, depending on the features included on <code>FEATURE_LIST</code>. \n
        A runtime_type bound to a type can be created with the static function runtime_type::make. runtime_type has value semantic, and is copyable. \n
        A default-constructed runtime_type is empty: trying to use type-features of an empty runtime_type leads to undefined behavior.
        A runtime_type becomes empty is the member function \ref clear is called.

        runtime_type exposes a set of functions to manipulate instances of the target type. Furthermore it can be extended
        with any type feature built-in in density (for example f_equals or f_less). User-defined
        features are also supported, to add custom capabilities to the type erasure (see the examples below). \n
        Features can be queried with the function runtime_type::get_feature, specifying the requested feature at compile-time as template
        argument. The search is performed at compile time. If the requested feature is not included in <code>FEATURE_LIST</code>, a
        static_assert fails.

        \n\b Implementation runtime_type is implemented as a pointer to a pseudo vtable, that is a static array of feature
            values: for every feature in FEATURE_LIST there is an entry in this vtable. Most entries are pointer to functions.
            Anyway, some features (notably f_size and f_alignment) store a static const value.


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
    template <typename COMMON_TYPE = void, typename... FEATURES> class runtime_type
    {
      public:
        /** Alias for the template argument COMMON_TYPE */
        using common_type = COMMON_TYPE;

        /** Alias for the template argument FEATURE_LIST */
        //using feature_list = FEATURE_LIST;

        using tuple = typename detail::FeaturesToTuple<COMMON_TYPE, FEATURES...>::type;

        /** Creates a runtime_type associated with the specified type. The latter is the target type.
                @tparam TYPE target type that is type-erased by the returned runtime_type. */
        template <typename TARGET_TYPE> constexpr static runtime_type make() noexcept
        {
            return runtime_type(&detail::FeatureTable<tuple, TARGET_TYPE>::s_table);
        }

        /** Construct an empty runtime_type not associated with any type. Trying to use any feature of an empty
            runtime_type leads to undefined behavior. */
        constexpr runtime_type() noexcept = default;

        /** Generalized copy constructor.

            This constructor does not partecipate in oveload resolution unless runtime_type::tuple
            and runtime_type<COMMON_TYPE, OTHER_FEATURES...>::tuple are the same (that is the
            feature lists of the two runtime_type are similar).
            
            \snippet runtime_type_examples.cpp runtime_type copy example 1 */
        template <typename... OTHER_FEATURES>
        constexpr runtime_type(
          const runtime_type<COMMON_TYPE, OTHER_FEATURES...> & i_source
#ifndef DOXYGEN_DOC_GENERATION
          ,
          typename std::enable_if<
            std::is_same<
              typename runtime_type::tuple,
              typename runtime_type<COMMON_TYPE, OTHER_FEATURES...>::tuple>::value,
            int>::type = 0
#endif
          ) noexcept
            : m_feature_table(i_source.m_feature_table)
        {
        }

        /** Generalized copy assignment

            This function does not partecipate in oveload resolution unless runtime_type::tuple
            and runtime_type<COMMON_TYPE, OTHER_FEATURES...>::tuple are the same (that is the
            feature lists of the two runtime_type are similar).

            If the compiler conforms to C++14 (in particular __cpp_constexpr >= 201304) this 
            function is constexpr.

            \snippet runtime_type_examples.cpp runtime_type assign example 1 */
        template <typename... OTHER_FEATURES>
        DENSITY_CPP14_CONSTEXPR
#ifndef DOXYGEN_DOC_GENERATION
          typename std::enable_if<
            std::is_same<
              typename runtime_type::tuple,
              typename runtime_type<COMMON_TYPE, OTHER_FEATURES...>::tuple>::value,
            runtime_type>::type &
#else
          runtime_type &
#endif
          operator=(const runtime_type<COMMON_TYPE, OTHER_FEATURES...> & i_source) noexcept
        {
            m_feature_table = i_source.m_feature_table;
            return *this;
        }

        /** Returns whether this runtime_type is not bound to a target type */
        constexpr bool empty() const noexcept { return m_feature_table == nullptr; }

        /** Unbinds from a target. If the runtime_type was already empty this function has no effect. */
        DENSITY_CPP14_CONSTEXPR void clear() noexcept { m_feature_table = nullptr; }

        /** Returns the size (in bytes) of the target type, which is always > 0. \n
            The effect of this function is the same of this code:
                @code
                    return sizeof(TARGET_TYPE);
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make).

            \n\b Requires:
                - the feature f_size must be included in the FEATURE_LIST \n
                - the runtime_type must be non-empty

            \n\b Throws: nothing */
        constexpr size_t size() const noexcept { return get_feature<f_size>()(); }

        /** Returns the alignment (in bytes) of the target type, which is always an integer power of 2. \n

            The effect of this function is the same of this code:
                @code
                    return alignof(TARGET_TYPE);
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make).

            \n\b Requires:
                - the feature f_alignment must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: nothing */
        constexpr size_t alignment() const noexcept { return get_feature<f_alignment>()(); }

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
                - the feature f_default_construct must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: anything that the default constructor of the target type throws. */
        common_type * default_construct(void * i_dest) const
        {
            DENSITY_ASSERT(!empty());
            return get_feature<f_default_construct>()(i_dest);
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
                - the feature f_copy_construct must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: anything that the copy constructor of the target type throws. */
        common_type * copy_construct(void * i_dest, const common_type * i_source) const
        {
            DENSITY_ASSERT(!empty());
            return get_feature<f_copy_construct>()(i_dest, i_source);
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
                - the feature f_move_construct must be included in the FEATURE_LIST
                - the runtime_type must be non-empty
            */
        common_type * move_construct(void * i_dest, common_type * i_source) const
        {
            DENSITY_ASSERT(!empty());
            return get_feature<f_move_construct>()(i_dest, i_source);
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
                - the feature f_destroy must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: nothing. Destructors are required to be noexcept. */
        void * destroy(common_type * i_dest) const noexcept
        {
            DENSITY_ASSERT(!empty());
            return get_feature<f_destroy>()(i_dest);
        }

        /** Returns the std::type_info associated to the target type.

            The effect of this function is the same of this code:
                @code
                    return typeid(TARGET_TYPE);
                @endcode
            where TARGET_TYPE is the target type (see the static member function runtime_type::make). \n

            \n\b Requires:
                - the feature f_rtti must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: nothing. */
        const std::type_info & type_info() const noexcept
        {
            DENSITY_ASSERT(!empty());
            return get_feature<f_rtti>()();
        }

        /** Returns true if two instances of the target types compare equal.
            \n\b Throws: unspecified
        */
        bool are_equal(const common_type * i_first, const common_type * i_second) const
        {
            DENSITY_ASSERT(i_first != nullptr && i_second != nullptr && !empty());
            return get_feature<f_equals>()(i_first, i_second);
        }

        /** Returns the feature matching the specified type, if present. If the feature is not present, a static_assert fails.
            This function grant access to features that are not part of the interface of runtime_type.

            The search a the feature is done at compile time, so the complexity is alway constant.

            \n\b Requires:
                - the feature FEATURE must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: nothing. */
        template <typename FEATURE>
        const typename FEATURE::template type<COMMON_TYPE> & get_feature() const noexcept
        {
            return std::get<
              detail::Tuple_FindFirst<tuple, typename FEATURE::template type<COMMON_TYPE>>::index>(
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
            template parameter. Equivalent to *this == runtime_type::make<TARGET_TYPE>() */
        template <typename TARGET_TYPE> constexpr bool is() const noexcept
        {
            return m_feature_table == &detail::FeatureTable<tuple, TARGET_TYPE>::s_table;
        }

        /** Returns an hash. This function is used for the partial specialization of std::hash
            for runtime_type. */
        size_t hash() const noexcept { return std::hash<const void *>()(m_feature_table); }

      private:
        constexpr runtime_type(const tuple * i_feature_table) noexcept
            : m_feature_table(i_feature_table)
        {
        }

      public:
        const tuple * m_feature_table{};
    };
} // namespace density

namespace std
{
    /** Partial specialization of std::hash to allow the use of density::runtime_type as key
        for unordered associative containers. */
    template <typename COMMON_TYPE, typename... FEATURES>
    struct hash<density::runtime_type<COMMON_TYPE, FEATURES...>>
    {
        size_t
          operator()(const density::runtime_type<COMMON_TYPE, FEATURES...> & i_runtime_type) const
          noexcept
        {
            return i_runtime_type.hash();
        }
    };

} // namespace std
