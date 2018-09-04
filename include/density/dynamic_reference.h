
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <type_traits>

namespace density
{
    /** Defines a cv-qualification, that describes if a type is const, volatile, both or none. */
    enum class cv_qual
    {
        no_qual             = 0, /**< no qualification */
        const_qual          = 1, /**< const qualification */
        volatile_qual       = 2, /**< volatile qualification */
        const_volatile_qual = 3, /**< const and volatile qualification */
    };

    /** Defines the member constant value that describes the level of cv-qualification of the template argument.

        Example:
            \snippet dynamic_reference_examples.cpp cv_qualifier example 1 */
    template <typename TYPE> struct cv_qual_of
    {
        static constexpr cv_qual value = cv_qual::no_qual;
    };
    template <typename TYPE> struct cv_qual_of<const TYPE>
    {
        static constexpr cv_qual value = cv_qual::const_qual;
    };
    template <typename TYPE> struct cv_qual_of<volatile TYPE>
    {
        static constexpr cv_qual value = cv_qual::volatile_qual;
    };
    template <typename TYPE> struct cv_qual_of<const volatile TYPE>
    {
        static constexpr cv_qual value = cv_qual::const_volatile_qual;
    };

    /** Returns true if the first operand is less than or equal to the second one according to the partial
        ordering of cv qualifications.

        - If the two operands are equal the return value is true
        - if the first is const and the second one is not, the return value is false.
        - if the first is volatile and the second one is not, the return value is false.

        Given two reference types T1 and T2, this function can be used to determine whether
        a variable of type T1 can be initialized with an expression of type T2:
            \snippet dynamic_reference_examples.cpp is_less_or_equal_cv_qualified example 2

        Example:
            \snippet dynamic_reference_examples.cpp is_less_or_equal_cv_qualified example 1 */
    constexpr bool is_less_or_equal_cv_qualified(cv_qual i_first, cv_qual i_second) noexcept
    {
        return (static_cast<unsigned>(i_first) & ~static_cast<unsigned>(i_second)) == 0;
    }

    /** Returns true if the first operand is less than the second one according to the partial
        ordering of cv qualifications.

        - If the two operands are equal the return value is false
        - if the first is const and the second one is not, the return value is false.
        - if the first is volatile and the second one is not, the return value is false.

        Example:
            \snippet dynamic_reference_examples.cpp is_less_cv_qualified example 1 */
    constexpr bool is_less_cv_qualified(cv_qual i_first, cv_qual i_second) noexcept
    {
        return i_first != i_second && is_less_or_equal_cv_qualified(i_first, i_second);
    }

    /** Adds to the first template argument the cv-qualification specified by the second template argument
        and defines a type member alias to it.
        
        Note: the template type alias \ref add_cv_qual_t is a short version for this type trait.
        
        Example:
            \snippet dynamic_reference_examples.cpp add_cv_qual example 1 */
    template <typename TYPE, cv_qual CV> struct add_cv_qual
    {
        using type = TYPE;
    };
    template <typename TYPE> struct add_cv_qual<TYPE, cv_qual::const_qual>
    {
        using type = const TYPE;
    };
    template <typename TYPE> struct add_cv_qual<TYPE, cv_qual::volatile_qual>
    {
        using type = volatile TYPE;
    };
    template <typename TYPE> struct add_cv_qual<TYPE, cv_qual::const_volatile_qual>
    {
        using type = const volatile TYPE;
    };

    /** Adds a cv-qualification to a type. Alias for add_cv_qual.

        Example:
            \snippet dynamic_reference_examples.cpp add_cv_qual example 2 */
    template <typename TYPE, cv_qual CV> using add_cv_qual_t = typename add_cv_qual<TYPE, CV>::type;

    template <typename RUNTIME_TYPE = runtime_type<>, cv_qual = cv_qual::no_qual>
    class dynamic_reference;

    /** Type alias template for the class template that sets the qv-qualification to cv_qual::const_qual. */
    template <typename RUNTIME_TYPE = runtime_type<>>
    using const_dynamic_reference = dynamic_reference<RUNTIME_TYPE, cv_qual::const_qual>;

    /** Type alias template for the class template that sets the qv-qualification to cv_qual::volatile_qual. */
    template <typename RUNTIME_TYPE = runtime_type<>>
    using volatile_dynamic_reference = dynamic_reference<RUNTIME_TYPE, cv_qual::volatile_qual>;

    /** Type alias template for the class template that sets the qv-qualification to cv_qual::const_volatile_qual. */
    template <typename RUNTIME_TYPE = runtime_type<>>
    using const_volatile_dynamic_reference =
      dynamic_reference<RUNTIME_TYPE, cv_qual::const_volatile_qual>;

    /** Provides the constexpr member constant value which is equal to true if
       the template argument is a specialization of dynamic_reference, and to 
       false otherwise. */
    template <typename> struct is_dynamic_reference : std::false_type
    {
    };
    template <typename RUNTIME_TYPE, cv_qual CV_QUAL>
    struct is_dynamic_reference<dynamic_reference<RUNTIME_TYPE, CV_QUAL>> : std::true_type
    {
    };

    /** Holds a reference to an object whose type in unknown at compile time.
        This class template is an abstraction of a pair of void pointer and a runtime type.

        @tparam RUNTIME_TYPE Runtime-type object used to store the actual complete type of the target object.
            This type must satisfy the requirements of \ref RuntimeType_requirements "RuntimeType". The default is runtime_type.

        @tparam CV_QUALIFIER CV-qualification of the reference. The default is cv_qual::no_qual.
    */
    template <typename RUNTIME_TYPE, cv_qual CV_QUALIFIER> class dynamic_reference
    {
      public:
        /** Deleted default constructor. */
        dynamic_reference() = delete;

        /** Constructs an instance of dynamic_reference bound to the specified target object.

            This constructor is excluded from the overload set if the type of the target type is less
                cv-qualified than the reference:            
            \snippet dynamic_reference_examples.cpp construct example 2

            \b Postcoditions:
                Given a type RT satisfying the requirements of \ref RuntimeType_requirements "RuntimeType" and
                an object t, the following conditions hold:
            \snippet dynamic_reference_examples.cpp construct example 1

        \b Throws: unspecified */
#ifdef DOXYGEN_DOC_GENERATION
        template <typename TARGET_TYPE>
#else
        template <
          typename TARGET_TYPE,
          typename std::enable_if<
            !is_dynamic_reference<TARGET_TYPE>::value &&
            is_less_or_equal_cv_qualified(cv_qual_of<TARGET_TYPE>::value, CV_QUALIFIER)>::type * =
            nullptr>
#endif
        dynamic_reference(TARGET_TYPE & i_target_object)
            : dynamic_reference(
                RUNTIME_TYPE::template make<typename std::remove_cv<TARGET_TYPE>::type>(),
                &i_target_object)
        {
        }

        /** Constructs a dynamic_reference assigning a target object.

        \b Postcoditions:
            Given a type RT satisfying the requirements of \ref RuntimeType_requirements "RuntimeType",
            the following conditions hold:
            \snippet dynamic_reference_examples.cpp construct2 example 1

        \b Throws: unspecified */
        constexpr dynamic_reference(
          const RUNTIME_TYPE & i_target_type, add_cv_qual_t<void, CV_QUALIFIER> * i_target_address)
            : m_address(i_target_address), m_type(i_target_type)
        {
        }

        /** Generalized copy constructor. The template argument CV_QUALIFIER of this template
            specialization can differ from the source:

            \snippet dynamic_reference_examples.cpp copy construct example 2

        \b Postcoditions:
            Given an object r of type dynamic_reference<*>, the following conditions hold:
            \snippet dynamic_reference_examples.cpp copy construct example 1

        \b Throws: unspecified */
#ifdef DOXYGEN_DOC_GENERATION
        template <cv_qual OTHER_CV>
#else
        template <
          cv_qual OTHER_CV,
          typename std::enable_if<is_less_or_equal_cv_qualified(OTHER_CV, CV_QUALIFIER)>::type * =
            nullptr>
#endif
        constexpr dynamic_reference(const dynamic_reference<RUNTIME_TYPE, OTHER_CV> & i_source)
            : dynamic_reference(i_source.type(), i_source.address())

        {
        }

        /** Deleted copy assignment. */
        dynamic_reference & operator=(const dynamic_reference & i_source) = delete;

        /** Returns a reference to the runtime type.

        \b Throws: nothing */
        constexpr const RUNTIME_TYPE & type() const noexcept { return m_type; }

        /** Returns the address of the target object. If the dynamic_reference is empty,
            the return value is nullptr.

        \b Throws: nothing */
        constexpr add_cv_qual_t<void, CV_QUALIFIER> * address() const noexcept { return m_address; }

        /** Returns whether the target type is bound to the provided compile-time type.
            
            Example:
            \snippet dynamic_reference_examples.cpp is example 1

        \b Throws: nothing */
        template <typename TYPE> constexpr bool is() const noexcept
        {
            return is_less_or_equal_cv_qualified(CV_QUALIFIER, cv_qual_of<TYPE>::value) &&
                   m_type.template is<TYPE>();
        }

        /** Returns a reference to the target object, assuming that the target type is bound
            to the provided compile-time type.

            \b Precoditions:
               The behavior is undefined if any of these conditions is not satisfied:
                - The target type is not the provided compile-time type.

            Example:
            \snippet dynamic_reference_examples.cpp as example 1

        \b Throws: nothing */
        template <typename TYPE>
        DENSITY_CPP14_CONSTEXPR add_cv_qual_t<TYPE, CV_QUALIFIER> & as() const noexcept
        {
            assert(m_type.template is<TYPE>());
            return *static_cast<add_cv_qual_t<TYPE, CV_QUALIFIER> *>(m_address);
        }

      private:
        add_cv_qual_t<void, CV_QUALIFIER> * const m_address = nullptr;
        RUNTIME_TYPE const                        m_type;
    };

} // namespace density
