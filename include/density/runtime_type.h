
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
    /** Type-list class template that can be used to specify which features a runtime_type captures from the target type. 

        Each type in the template arguments is either:
        - a type satisfying the requirements of [TypeFeature](TypeFeature_concept.html) concept, like a built-in type fetaure
          (f_size, f_alignment, f_default_construct, f_copy_construct, f_move_construct, f_destroy, f_hash, f_rtti, f_equal,
          f_less, f_istream, f_ostream), or a user defined type feature.
        - a nested feature_list
        - the special tag type f_none

        \snippet runtime_type_examples.cpp feature_list example 1

        feature_list provides a template alias to an [std::tuple](https://it.cppreference.com/w/cpp/utility/tuple). An instance of
        this tuple is a pseudo-vtable associated to the target type.
        
        \snippet runtime_type_examples.cpp feature_list example 2

        For the composition of the tuple:
        - nested features are flatened
        - duplicates are removed (only the first occurrence of a fetaure is considered)
        - any occurrence of the pseudo-feature f_none is stripped out
        
        \snippet runtime_type_examples.cpp feature_list example 3 
        
        Two feature lists are equivalent if they prduce the same feature tuple:

        \snippet runtime_type_examples.cpp feature_list example 4 */
    template <typename... FEATURES> struct feature_list;
    template <> struct feature_list<> /* an empty feature_list gives an empty tuple */
    {
        using tuple_type = std::tuple<>;
    };
    template <typename FIRST_FEATURE, typename... OTHER_FEATURES>
    struct feature_list<FIRST_FEATURE, OTHER_FEATURES...> /* the generic case, to 
        handle types satisfying [TypeFeature](TypeFeature_concept.html) */
    {
        static_assert(
          !std::is_void<FIRST_FEATURE>::value, "[cv-qualified] void does not satisfy TypeFeature");

        static_assert(
          std::is_literal_type<FIRST_FEATURE>::value, "a TypeFeature must be a literal type");

        using tuple_type = detail::Tuple_Merge_t<

          // add FIRST_FEATURE to the tuple
          std::tuple<FIRST_FEATURE>,

          /* use recursively feature_list to add the other features, but removes 
             FIRST_FEATURE from the result */
          detail::
            Tuple_Remove_t<typename feature_list<OTHER_FEATURES...>::tuple_type, FIRST_FEATURE>>;
    };
    template <typename... GROUPED_FEATURES, typename... OTHER_FEATURES>
    struct feature_list<feature_list<GROUPED_FEATURES...>, OTHER_FEATURES...> /* handle 
        nesting of feature_list's */
    {
        using tuple_type = detail::Tuple_Merge_t<

          // add to the tuple GROUPED_FEATURES (the nested fetaures)
          typename feature_list<GROUPED_FEATURES...>::tuple_type,

          // add to the tuple the remaining features with the fetaures already in GROUPED_FEATURES removed
          detail::Tuple_Diff_t<
            typename feature_list<OTHER_FEATURES...>::tuple_type,
            typename feature_list<GROUPED_FEATURES...>::tuple_type>

          >;
    };
    template <typename... OTHER_FEATURES>
    struct feature_list<f_none, OTHER_FEATURES...> /* strip f_none from the feature list*/
    {
        using tuple_type = typename feature_list<OTHER_FEATURES...>::tuple_type;
    };

    /** Tag type that can be included in a feature_list without affecting the feature tuple. This peseudo-feature 
        is usefull to conditionally add features to a feature_list, for example using
        [std::conditional](https://en.cppreference.com/w/cpp/types/conditional).
        
        In this code f_none is used to define a conditional type list.

        \snippet runtime_type_examples.cpp feature_list example 6 */
    struct f_none
    {
    };

    /** This type-feature gives the size of the target type */
    class f_size
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_size make() noexcept
        {
            // constraining the size of types allows to reduce the runtime checks to detect pointer arithmetic overflow
            static_assert(
              sizeof(TARGET_TYPE) < (std::numeric_limits<uintptr_t>::max)() / 4,
              "Type with size >= 1/4 of the address space are not supported");

            return f_size{sizeof(TARGET_TYPE)};
        }

        /** Returns the size of the target type. */
        DENSITY_NODISCARD constexpr size_t operator()() const noexcept { return m_size; }

      private:
        constexpr f_size(size_t i_size) noexcept : m_size(i_size) {}
        size_t const m_size;
    };

    /** This type-feature gives the alignment of the target type */
    class f_alignment
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_alignment make() noexcept
        {
            // constraining the alignment of types allows to reduce the runtime checks to detect pointer arithmetic overflow
            static_assert(
              alignof(TARGET_TYPE) < (std::numeric_limits<uintptr_t>::max)() / 4,
              "Type with alignment >= 1/4 of the address space are not supported");

            return f_alignment{alignof(TARGET_TYPE)};
        }

        /** Returns the alignment of the target type. */
        DENSITY_NODISCARD constexpr size_t operator()() const noexcept { return m_alignment; }

      private:
        constexpr f_alignment(size_t i_alignment) noexcept : m_alignment(i_alignment) {}
        size_t const m_alignment;
    };

    /** This feature invokes [std::hash](https://en.cppreference.com/w/cpp/utility/hash) on an 
        instance of the target type. Specializations of std::hash must be nothrow default
        constructible and nothrow invokable. */
    class f_hash
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_hash make() noexcept
        {
            return f_hash{&invoke<TARGET_TYPE>};
        }

        /** Computes the hash of an instance of the target type object.
            @param i_source pointer to an instance of the target type. Can't be null.
                If the dynamic type of the pointed object is not the taget type (assigned
                by the function make), the behaviour is undefined. */
        DENSITY_NODISCARD size_t operator()(const void * i_source) const noexcept
        {
            return (*m_function)(i_source);
        }

      private:
        using Function = size_t (*)(const void * i_source) DENSITY_CPP17_NOEXCEPT;
        Function const m_function;
        constexpr f_hash(Function i_function) noexcept : m_function(i_function) {}
        template <typename TARGET_TYPE> static size_t invoke(const void * i_source) noexcept
        {
            DENSITY_ASSERT(i_source != nullptr);
            static_assert(
              noexcept(std::hash<TARGET_TYPE>()(*static_cast<const TARGET_TYPE *>(i_source))),
              "Specializations of std::hash must be nothrow constructible and invokable");
            return std::hash<TARGET_TYPE>()(*static_cast<const TARGET_TYPE *>(i_source));
        }
    };

    /** This feature returns the std::type_info associated to the target type. */
    class f_rtti
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_rtti make() noexcept
        {
            return f_rtti{&invoke<TARGET_TYPE>};
        }

        /** Returns the std::type_info of the target type. */
        DENSITY_NODISCARD std::type_info const & operator()() const noexcept
        {
            return (*m_function)();
        }

      private:
        using Function = std::type_info const & (*)() DENSITY_CPP17_NOEXCEPT;
        Function const m_function;
        constexpr f_rtti(Function i_function) noexcept : m_function(i_function) {}
        template <typename TARGET_TYPE> static const std::type_info & invoke() noexcept
        {
            return typeid(TARGET_TYPE);
        }
    };

    /** [Value-initializes](https://en.cppreference.com/w/cpp/language/value_initialization) an
        instance of the target type in a user specified storage buffer. The target type must satisfy
        the requirements of [DefaultConstructible](https://en.cppreference.com/w/cpp/named_req/DefaultConstructible). */
    class f_default_construct
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_default_construct make() noexcept
        {
            return f_default_construct{&invoke<TARGET_TYPE>};
        }

        /** Constructs in an uninitialized memory buffer a value-initialized instance of the target type.
                @param i_dest where the target object must be constructed. Can't be null. If the buffer 
                pointed by this parameter does not respect the size and alignment of the target type,
                the behaviour is undefined. */
        void operator()(void * i_dest) const { (*m_function)(i_dest); }

      private:
        using Function = void (*)(void * i_dest);
        Function const m_function;
        constexpr f_default_construct(Function i_function) : m_function(i_function) {}
        template <typename TARGET_TYPE> static void invoke(void * i_dest)
        {
            DENSITY_ASSERT(i_dest != nullptr);
            new (i_dest) TARGET_TYPE();
        }
    };

    /** Copy-initializes an instance of the target type in a user specified storage buffer. The target type
        must satisfy the requirements of [CopyConstructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible).*/
    class f_copy_construct
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_copy_construct make() noexcept
        {
            return f_copy_construct{&invoke<TARGET_TYPE>};
        }

        /** Copy-constructs in an uninitialized memory buffer an instance of the target type.
            @param i_dest where the target object must be constructed. Can't be null. If the buffer
                pointed by this parameter does not respect the size and alignment of the target type,
                the behaviour is undefined.
            @param i_source pointer to the source object. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined. */
        void operator()(void * i_dest, const void * i_source) const
        {
            (*m_function)(i_dest, i_source);
        }

      private:
        using Function = void (*)(void * i_dest, const void * i_source);
        Function const m_function;
        constexpr f_copy_construct(Function i_function) : m_function(i_function) {}
        template <typename TARGET_TYPE> static void invoke(void * i_dest, const void * i_source)
        {
            DENSITY_ASSERT(i_dest != nullptr && i_source != nullptr);
            const TARGET_TYPE & source = *static_cast<const TARGET_TYPE *>(i_source);
            new (i_dest) TARGET_TYPE(source);
        }
    };

    /** Move-constructs to an uninitialized memory buffer an instance of the target type. The target type
        must satisfy the requirements of [MoveConstructible](https://en.cppreference.com/w/cpp/named_req/MoveConstructible). */
    struct f_move_construct
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_move_construct make() noexcept
        {
            return f_move_construct{&invoke<TARGET_TYPE>};
        }

        /** Move-constructs in an uninitialized memory buffer an instance of the target type.
            @param i_dest where the target object must be constructed. Can't be null. If the buffer
                pointed by this parameter does not respect the size and alignment of the target type,
                the behaviour is undefined.
            @param i_source pointer to the source object. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined. */
        void operator()(void * i_dest, void * i_source) const { (*m_function)(i_dest, i_source); }

      private:
        using Function = void (*)(void * i_dest, void * i_source);
        Function const m_function;
        constexpr f_move_construct(Function i_function) : m_function(i_function) {}
        template <typename TARGET_TYPE> static void invoke(void * i_dest, void * i_source)
        {
            DENSITY_ASSERT(i_dest != nullptr && i_source != nullptr);
            TARGET_TYPE & source = *static_cast<TARGET_TYPE *>(i_source);
            new (i_dest) TARGET_TYPE(std::move(source));
        }
    };

    /** Destroys an instance of the target type, and returns the address of the complete type. The target type
        must satisfy the requirements of [Destructible](https://en.cppreference.com/w/cpp/named_req/Destructible). */
    class f_destroy
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_destroy make() noexcept
        {
            return f_destroy{&invoke<TARGET_TYPE>};
        }

        /** Destroys an instance of the target type.
            @param i_object pointer to the object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined.
            @return The address of the complete object that has been destroyed. */
        void operator()(void * i_object) const noexcept { return (*m_function)(i_object); }

      private:
        using Function = void (*)(void * i_dest) DENSITY_CPP17_NOEXCEPT;
        Function const m_function;
        constexpr f_destroy(Function i_function) : m_function(i_function) {}
        template <typename TARGET_TYPE> static void invoke(void * i_object) noexcept
        {
            DENSITY_ASSERT(i_object != nullptr);
            TARGET_TYPE * obj = static_cast<TARGET_TYPE *>(i_object);
            static_assert(
              noexcept(obj->TARGET_TYPE::~TARGET_TYPE()),
              "TARGET_TYPE must be nothrow destructible");
            obj->TARGET_TYPE::~TARGET_TYPE();
        }
    };

    /** Compares two objects for equality. The target type must satisfy the requirements of 
        [EqualityComparable](https://en.cppreference.com/w/cpp/named_req/EqualityComparable). */
    class f_equal
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_equal make() noexcept
        {
            return f_equal{&invoke<TARGET_TYPE>};
        }

        /** Returns whether two target object compare equal.
            @param i_first pointer to the first object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined.
            @param i_second pointer to the second object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined. */
        bool operator()(void const * i_first, void const * i_second) const noexcept
        {
            return (*m_function)(i_first, i_second);
        }

      private:
        using Function = bool (*)(void const * i_first, void const * i_second)
          DENSITY_CPP17_NOEXCEPT;
        Function const m_function;
        constexpr f_equal(Function i_function) : m_function(i_function) {}
        template <typename TARGET_TYPE>
        static bool invoke(void const * i_first, void const * i_second)
        {
            DENSITY_ASSERT(i_first != nullptr && i_second != nullptr);
            auto const & first  = *static_cast<TARGET_TYPE const *>(i_first);
            auto const & second = *static_cast<TARGET_TYPE const *>(i_second);
            bool const   result = first == second;
            return result;
        }
    };

    /** Compares two objects. The target type must satisfy the requirements of 
        [LessThanComparable](https://en.cppreference.com/w/cpp/named_req/LessThanComparable). */
    class f_less
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_less make() noexcept
        {
            return f_less{&invoke<TARGET_TYPE>};
        }

        /** Returns whether the first object compares less than the second object.
            @param i_first pointer to the first object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined.
            @param i_second pointer to the second object to destroy. Can't be null. If the dynamic type of the
                pointed object is not the taget type (assigned by the function make), the behaviour is
                undefined. */
        bool operator()(void const * i_first, void const * i_second) const
        {
            return (*m_function)(i_first, i_second);
        }

      private:
        using Function = bool (*)(void const * i_first, void const * i_second)
          DENSITY_CPP17_NOEXCEPT;
        Function const m_function;
        constexpr f_less(Function i_function) : m_function(i_function) {}
        template <typename TARGET_TYPE>
        static bool invoke(void const * i_first, void const * i_second)
        {
            DENSITY_ASSERT(i_first != nullptr && i_second != nullptr);
            auto const & first  = *static_cast<TARGET_TYPE const *>(i_first);
            auto const & second = *static_cast<TARGET_TYPE const *>(i_second);
            bool const   result = first < second;
            return result;
        }
    };

    /** feature_list used by default by runtime_type */
    using default_type_features =
      feature_list<f_size, f_alignment, f_copy_construct, f_move_construct, f_rtti, f_destroy>;

    /** Traits that checks whether a feature_list or a runtime_type (the first template parameter)
        contains all of the target fetaures (from the second template parameter on). If this condition 
        is satisfied, or if no target feature is provided, this type inehrits from 
        [std::true_type](https://en.cppreference.com/w/cpp/types/integral_constant).
        Otherwise it inehrits from [std::false_type](https://en.cppreference.com/w/cpp/types/integral_constant).
    
        The following example shows has_features used on a feature_list:
        \snippet runtime_type_examples.cpp has_features example 1
        
        The following example shows has_features used on a runtime_type:
        \snippet runtime_type_examples.cpp has_features example 2
    */
    template <typename FEATURE_LIST, typename... TARGET_FEATURES> struct has_features;
    template <typename FEATURE_LIST>
    struct has_features<FEATURE_LIST> : std::integral_constant<bool, true>
    {
    };
    template <typename FEATURE_LIST, typename FIRST_FEATURE, typename... OTHER_FEATURES>
    struct has_features<FEATURE_LIST, FIRST_FEATURE, OTHER_FEATURES...>
        : std::integral_constant<
            bool,

            (detail::Tuple_FindFirst<typename FEATURE_LIST::tuple_type, FIRST_FEATURE>::index <
             std::tuple_size<typename FEATURE_LIST::tuple_type>::value) &&
              has_features<FEATURE_LIST, OTHER_FEATURES...>::value

            >
    {
    };

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

        Requirements
        -------------------------
        A type <code>F</code> is a TypeFeature if:
        - <code>F</code> satisfies [LiteralType](https://en.cppreference.com/w/cpp/named_req/LiteralType)
        - <code>F::make<T>()</code> is valid as constant expression that returns an instance of 
            <code>F</code> bound to the target type <code>T</code>

        Notes
        -------------------------
        A type feature is a class that captures and exposes a specific property or action of a target 
        type, without depending from it at compile time. Most times features store a pointer to a function
        (that is like an entry in a vtable), but they may hold just data (like f_size and f_alignment do).
    
        Usually a TypeFeature has this form:

        \code
        struct f_feature
        {
            // returns an instance of this feature bound to a specific target type
            template <typename TARGET_TYPE>
                constexpr static type make() noexcept;

            // invokes the feature. The return type and the parameters depends on the specific feature
            ... operator()(...) const;
        };
        \endcode

        The static function template make creates an instance of the feature bound to a type. The function
        call operator invokes the fetaure.  

        The following snippet shows the synopsis of f_size, f_default_construct and f_equal.

        \code
        struct f_size
        {
            template <typename TARGET_TYPE>
                constexpr static f_size make() noexcept;

            constexpr size_t operator()() const noexcept;
        };

        struct f_default_construct
        {
            template <typename TARGET_TYPE>
                constexpr static f_default_construct make() noexcept;

            void operator()(void * i_dest) const;
        };

        struct f_equal
        {
            template <typename TARGET_TYPE>
                constexpr static f_equal make() noexcept;
            bool operator()(void const * i_first, void const * i_second) const;
        };
        \endcode
    */

    /** Class template that performs [type-erasure](https://en.wikipedia.org/wiki/Type_erasure). A runtime_type can be empty, 
        or can be bound to a target type, from which it captures and exposes the supported type features. It is copyable and
        trivially destructible. A specialization of runtime_type satisfies the requirements of \ref RuntimeType_concept "RuntimeType".
            @tparam FEATURES... list of features to be captures from the target type. 

        <i>Implementation note</i>:
        runtime_type is actually a wrapper around a pointer to a static constexpr instance of the tuple of the suppported features. 
        It is actually a generalization of the pointer to the v-table in a polymorphic type.

        Like in a feature_list, each type in the template arguments pack is either:
        - a type satisfying the requirements of [TypeFeature](TypeFeature_concept.html) concept, like a built-in type fetaure
          (one of f_size, f_alignment, f_default_construct, f_copy_construct, f_move_construct, f_destroy, f_hash, f_rtti, f_equal,
          f_less, f_istream, f_ostream), or a user defined type feature
        - a nested feature_list, which is expanded. Feature lists can be nested to any level, and they are always flatened
        - the special tag type f_none, which is ignored

        Furthermore duplicate features are removed (only the first occurrence of a fetaure is considered). If after all these 
        transformations two runtime_type specializations have the same feature_list, they are copyable and assignable each others:

        \snippet runtime_type_examples.cpp runtime_type example 1
        
        This example shows a very simple usage of runtime_type:
        \snippet runtime_type_examples.cpp runtime_type example 3

        Every type feature is associated with a concept. If a target type does not satisfy the syntactic requirements of all the features
        supported by the runtime_type, the function <code>make</code> fails to specialize, and a compile error is reported. For example
        we can't bind an instance of the <code>RuntimeType</code> of the example above to <code>std::lock_guard</code>, because 
        <code>RuntimeType</code> has the feature <code>f_default_construct</code>, but <code>std::lock_guard</code> is not default 
        constructible.

        Any type feature can be retrieved with the member function template <code>get_feature</code>. Anyway a set of convenience
        member function is avaiable for the most common features: \ref size, \ref alignment, \ref default_construct, \ref copy_construct, 
        \ref move_construct, \ref destroy, \ref type_info, \ref are_equal.

        Managing instances of the target type directly is difficult and requires very low-level code: instances are managed by
        void pointers, they must explictly allocated, constructed, destroyed and deallocated. Indeed runtime_type is not intended to be
        used directly in this way, but it should instead used by library code to provide high-level features.
        
        In the following example runtime_type is used to implement a class template very similar to 
        [std::any](https://en.cppreference.com/w/cpp/utility/any), which can be customized specifying which features must 
        be captured from the underlying object.
            
        \snippet any.h any 1

        This example shows how a non-intrusive serialization <code>operator << </code>for the above <code>any</code> can be easly implemented
        exploiting the feature built-in f_ostream:

        \snippet any.h any 2 
        
        This example defines the feature <code>f_sum</code> and then overloads the operator <code>operator +</code> between two <code>any</code>'s
        
        \snippet any.h any 3 */
    template <typename... FEATURES> class runtime_type
    {
      public:
        /** feature_list associated to the template arguments. If to template arguments is provided, \ref default_type_features is used. */
        using feature_list_type = typename std::conditional<
          (sizeof...(FEATURES) > 0),
          feature_list<FEATURES...>,
          default_type_features>::type;

        /** Alias for <code>feature_list_type::tuple_type</code>, a specialization of 
            [std::tuple](https://en.cppreference.com/w/cpp/utility/tuple) that contains all the features.
            \snippet runtime_type_examples.cpp runtime_type tuple_type example 1 
            <i>Implementation note</i>: runtime_type is actually a wrapper around a pointer to a static 
            constexpr instance of this tuple. */
        using tuple_type = typename feature_list_type::tuple_type;

        /** Creates a runtime_type boudnto a target type.
                @tparam TARGET_TYPE type to bind to the returned runtime_type. 

        \b Postcoditions:
            Given a specialization of runtime_type R and type T, the following conditions hold:
            \snippet runtime_type_examples.cpp runtime_type make example 1
        \b Throws: nothing */
        template <typename TARGET_TYPE> constexpr static runtime_type make() noexcept
        {
            return runtime_type(&detail::FeatureTable<tuple_type, TARGET_TYPE>::s_table);
        }

        /** Construct an empty runtime_type not associated with any type. Trying to use any feature of an empty
            runtime_type leads to undefined behavior.
            
        \b Postcoditions:
            Given a specialization of runtime_type R and type T, the following conditions hold:
            \snippet runtime_type_examples.cpp runtime_type construct example 1
        \b Throws: nothing */
        constexpr runtime_type() noexcept = default;

        /** Generalized copy constructor.
            This constructor does not partecipate in oveload resolution unless <code>runtime_type::tuple</code>
            and <code>runtime_type<OTHER_FEATURES...>::tuple</code> are the same (that is the
            feature lists of the two runtime_type are equivalent).

            \b Throws: nothing
            
            \snippet runtime_type_examples.cpp runtime_type copy example 1 */
        template <typename... OTHER_FEATURES>
        constexpr runtime_type(
          const runtime_type<OTHER_FEATURES...> & i_source
#ifndef DOXYGEN_DOC_GENERATION
          ,
          typename std::enable_if<
            std::is_same<
              typename runtime_type::tuple_type,
              typename runtime_type<OTHER_FEATURES...>::tuple_type>::value,
            int>::type = 0
#endif
          ) noexcept
            : m_feature_table(i_source.m_feature_table)
        {
        }

        /** Generalized copy assignment.
            This function does not partecipate in oveload resolution unless <code>runtime_type::tuple</code>
            and <code>runtime_type<OTHER_FEATURES...>::tuple</code> are the same (that is the
            feature lists of the two runtime_type are equivalent).

            If the compiler conforms to C++14 (in particular <code>__cpp_constexpr >= 201304</code>) this 
            function is constexpr.

            \b Throws: nothing

            \snippet runtime_type_examples.cpp runtime_type assign example 1 */
        template <typename... OTHER_FEATURES>
        DENSITY_CPP14_CONSTEXPR
#ifndef DOXYGEN_DOC_GENERATION
          typename std::enable_if<
            std::is_same<
              typename runtime_type::tuple_type,
              typename runtime_type<OTHER_FEATURES...>::tuple_type>::value,
            runtime_type>::type &
#else
          runtime_type &
#endif
          operator=(const runtime_type<OTHER_FEATURES...> & i_source) noexcept
        {
            m_feature_table = i_source.m_feature_table;
            return *this;
        }

        /** Returns whether this runtime_type is not bound to a target type.
            
            \b Throws: nothing */
        constexpr bool empty() const noexcept { return m_feature_table == nullptr; }

        /** Unbinds from a target. If the runtime_type was already empty this function has no effect. 
        
            If the compiler conforms to C++14 (in particular <code>__cpp_constexpr >= 201304</code>) this
            function is constexpr.

            \b Throws: nothing */
        DENSITY_CPP14_CONSTEXPR void clear() noexcept { m_feature_table = nullptr; }

        /** Invokes the feature f_size and returns the size of the target type.
            Equivalent to <code>get_feature<f_size>()()</code>.

            The effect of this function is the same of this code:
            @code
                return sizeof(TARGET_TYPE);
            @endcode

            \b Precoditions:
                - If the runtime_type is empty the behaviour is undefined.

            \b Postcoditions:
                - The return value is above zero.

            \b Requires:
                - If the feature f_size is not included in the FEATURE_LIST a compile 
                    error is reported (this function is not SFINAE-friendly).

            \b Throws: nothing */
        constexpr size_t size() const noexcept { return get_feature<f_size>()(); }

        /** Invokes the feature f_alignment and returns the alignment of the target type.
            Equivalent to <code>get_feature<f_alignment>()()</code>.

            The effect of this function is the same of this code:
            @code
                return alignof(TARGET_TYPE);
            @endcode

            \b Precoditions:
                - If the runtime_type is empty the behaviour is undefined.

            \b Postcoditions:
                - The return value is above zero.

            \b Requires:
                - If the feature f_alignment is not included in the FEATURE_LIST a compile
                    error is reported (this function is not SFINAE-friendly).

            \b Throws: nothing */
        constexpr size_t alignment() const noexcept { return get_feature<f_alignment>()(); }

        /** Invokes the feature f_default_construct to [value-initialize](https://en.cppreference.com/w/cpp/language/value_initialization)
            an instance of the target type.
            Equivalent to <code>get_feature<f_default_construct>()(i_dest)</code>.
                @param i_dest pointer to the destination buffer in which the target type is inplace constructed. 

            The effect of this function is the same of this code:
                @code
                    new(i_dest) TARGET_TYPE();
                @endcode

            Note that if TARGET_TYPE is not a class type, it is zero-initialized.

            \b Precoditions:
                The behaviour is undefined if either:
                - The runtime_type is empty
                - The destination buffer is null
                - The destination buffer is not large at least as the result of runtime_type::size, or is 
                  not aligned at least according to runtime_type::alignment

            \b Postcoditions:
                - The destination buffer contains an instance of the TARGET_TYPE.

            \b Requires:
                - If the feature f_default_construct is not included in the FEATURE_LIST a compile
                    error is reported (this function is not SFINAE-friendly).

            \b Throws: anything that the constructor of the target type throws. */
        void default_construct(void * i_dest) const
        {
            DENSITY_ASSERT(!empty());
            get_feature<f_default_construct>()(i_dest);
        }

        /** Invokes the feature f_copy_construct to copy-construct an instance of the target type.
            Equivalent to <code>get_feature<f_copy_construct>()(i_dest, i_source)</code>.
                @param i_dest pointer to the destination buffer in which the target type is inplace constructed.
                @param i_source pointer to an instance of the target type to be used as source for the copy.

            The effect of this function is the same of this code:
                @code
                    new(i_dest) TARGET_TYPE( *static_cast<const TARGET_TYPE*>(i_source) );
                @endcode

            \b Precoditions:
                The behaviour is undefined if either:
                - The runtime_type is empty
                - The destination pointer or the source pointer are null
                - The destination buffer is not large at least as the result of runtime_type::size, or is
                  not aligned at least according to runtime_type::alignment
                - The source pointer does not point to an object whose dynamic type is the target type

            \b Postcoditions:
                - The destination buffer contains an instance of the TARGET_TYPE.

            \b Requires:
                - If the feature f_copy_construct is not included in the FEATURE_LIST a compile
                    error is reported (this function is not SFINAE-friendly).

            \b Throws: anything that the copy constructor of the target type throws. */
        void copy_construct(void * i_dest, const void * i_source) const
        {
            DENSITY_ASSERT(!empty());
            get_feature<f_copy_construct>()(i_dest, i_source);
        }

        /** Invokes the feature f_move_construct to move-construct an instance of the target type.
            Equivalent to <code>get_feature<f_move_construct>()(i_dest, i_source)</code>.
                @param i_dest pointer to the destination buffer in which the target type is inplace constructed.
                @param i_source pointer to an instance of the target type to be used as source for the move

            The effect of this function is the same of this code:
                @code
                    new(i_dest) TARGET_TYPE( std::move(*static_cast<TARGET_TYPE*>(i_source)) );
                @endcode

            \b Precoditions:
                The behaviour is undefined if either:
                - The runtime_type is empty
                - The destination pointer or the source pointer are null
                - The destination buffer is not large at least as the result of runtime_type::size, or is
                  not aligned at least according to runtime_type::alignment
                - The source pointer does not point to an object whose dynamic type is the target type

            \b Postcoditions:
                - The destination buffer contains an instance of the TARGET_TYPE.
                - The source buffer contains a moved-from instance of the target type.

            \b Requires:
                - If the feature f_move_construct is not included in the FEATURE_LIST a compile
                    error is reported (this function is not SFINAE-friendly).

            \b Throws: anything that the move constructor of the target type throws. */
        void move_construct(void * i_dest, void * i_source) const
        {
            DENSITY_ASSERT(!empty());
            get_feature<f_move_construct>()(i_dest, i_source);
        }

        /** Invokes the feature f_destroy to destroy an instance of the target type.
            Equivalent to <code>get_feature<f_destroy>()(i_dest)</code>.
                @param i_dest pointer to an instance of the target type to be destroyed.

            The effect of this function is the same of this code:
                @code
                    static_cast<TARGET_TYPE*>(i_source)->~TARGET_TYPE::TARGET_TYPE();
                @endcode

            \b Precoditions:
                The behaviour is undefined if either:
                - The runtime_type is empty
                - The destination pointer is null or does not point to an object whose dynamic type is the target type

            \b Postcoditions:
                - The destination buffer does not contain an instance of the TARGET_TYPE.

            \b Requires:
                - If the feature f_destroy is not included in the FEATURE_LIST a compile
                    error is reported (this function is not SFINAE-friendly).

            \b Throws: nothing. */
        void destroy(void * i_dest) const noexcept
        {
            DENSITY_ASSERT(!empty());
            get_feature<f_destroy>()(i_dest);
        }

        /** Invokes the feature f_rtti to return the [std::type_info](https://en.cppreference.com/w/cpp/types/type_info) of
            the target type.
            Equivalent to <code>get_feature<f_rtti>()()</code>.

            The effect of this function is the same of this code:
                @code
                    return typeid(TARGET_TYPE);
                @endcode

            \b Precoditions:
                The behaviour is undefined if either:
                - The runtime_type is empty

            \b Requires:
                - If the feature f_rtti is not included in the FEATURE_LIST a compile
                    error is reported (this function is not SFINAE-friendly).

            \b Throws: nothing. */
        const std::type_info & type_info() const noexcept
        {
            DENSITY_ASSERT(!empty());
            return get_feature<f_rtti>()();
        }

        /** Invokes the feature f_equal to compare two instances of the target type.
            Equivalent to <code>get_feature<f_equal>()(i_first, i_second)</code>.

            The effect of this function is the same of this code:
                @code
                    return *static_cast<const TARGET_TYPE*>(i_first) == *static_cast<const TARGET_TYPE*>(i_second);
                @endcode

            \b Precoditions:
                The behaviour is undefined if either:
                - The runtime_type is empty
                - The first pointer is null or does not point to an object whose dynamic type is the target type
                - The second pointer is null or does not point to an object whose dynamic type is the target type

            \b Requires:
                - If the feature f_equal is not included in the FEATURE_LIST a compile
                    error is reported (this function is not SFINAE-friendly).

            \b Throws: nothing. */
        bool are_equal(const void * i_first, const void * i_second) const noexcept
        {
            DENSITY_ASSERT(i_first != nullptr && i_second != nullptr && !empty());
            return get_feature<f_equal>()(i_first, i_second);
        }

        /** Returns the feature matching the specified type, if present. If the feature is not present, a static_assert fails.
            This function grant access to features that are not part of the interface of runtime_type.

            The search a the feature is done at compile time, so the complexity is alway constant.

            \n\b Requires:
                - the feature FEATURE must be included in the FEATURE_LIST
                - the runtime_type must be non-empty

            \n\b Throws: nothing. */
        template <typename FEATURE> const FEATURE & get_feature() const noexcept
        {
            static_assert(has_features<feature_list_type, FEATURE>::value, "feature not found");
            return std::get<detail::Tuple_FindFirst<tuple_type, FEATURE>::index>(*m_feature_table);
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
            return m_feature_table == &detail::FeatureTable<tuple_type, TARGET_TYPE>::s_table;
        }

        /** Returns an hash. This function is used for the partial specialization of std::hash
            for runtime_type. */
        size_t hash() const noexcept { return std::hash<const void *>()(m_feature_table); }

      private:
        constexpr runtime_type(const tuple_type * i_feature_table) noexcept
            : m_feature_table(i_feature_table)
        {
        }

      private:
#ifndef DOXYGEN_DOC_GENERATION
        template <typename...> friend class runtime_type;
#endif
        const tuple_type * m_feature_table{nullptr};
    };
} // namespace density

namespace std
{
    /** Partial specialization of std::hash to allow the use of density::runtime_type as key
        for unordered associative containers. */
    template <typename... FEATURES> struct hash<density::runtime_type<FEATURES...>>
    {
        size_t operator()(const density::runtime_type<FEATURES...> & i_runtime_type) const noexcept
        {
            return i_runtime_type.hash();
        }
    };

} // namespace std
