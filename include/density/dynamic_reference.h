
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
    /** Provides the constexpr member constant value which is equal to true if
        the template argument is a specialization of dynamic_reference, and to 
        false otherwise */
    template <typename RUNTIME_TYPE> class dynamic_reference;
    template <typename> struct is_dynamic_reference : std::false_type
    {
    };
    template <typename RUNTIME_TYPE>
    struct is_dynamic_reference<dynamic_reference<RUNTIME_TYPE>> : std::true_type
    {
    };

    /** Holds a reference to an object whose type in unknown at compile time.
        This class template is an abstraction of a pair of void pointer and a runtime type.

        @tparam RUNTIME_TYPE Runtime-type object used to store the actual complete type of the target object.
            This type must satisfy the requirements of \ref RuntimeType_requirements "RuntimeType". The default is runtime_type.
    */
    template <typename RUNTIME_TYPE = runtime_type<>> class dynamic_reference
    {
      public:
        /** Deleted default constructor. */
        dynamic_reference() = delete;

        /** Constructs a dynamic_reference assigning a target object.

        \b Postcoditions:
            Given a type RT satisfying the requirements of \ref RuntimeType_requirements "RuntimeType",
            the following conditions hold:
            \snippet dynamic_reference_examples.cpp construct example 2
        \b Throws: unspecified */
        constexpr dynamic_reference(const RUNTIME_TYPE & i_type, void * i_address)
            : m_address(i_address), m_type(i_type)
        {
        }

        /** Constructs an instance of dynamic_reference bound to the specified target object.

            \b Postcoditions:
                Given a type RT satisfying the requirements of \ref RuntimeType_requirements "RuntimeType" and
                an object t, the following conditions hold:
                \snippet dynamic_reference_examples.cpp make_dynamic_type example 1
        */
#ifdef DOXYGEN_DOC_GENERATION
        template <typename TARGET_TYPE>
#else
        template <
          typename TARGET_TYPE,
          typename std::enable_if_t<!is_dynamic_reference<TARGET_TYPE>::value> * = nullptr>
#endif
        dynamic_reference(TARGET_TYPE & i_target_object)
            : dynamic_reference(RUNTIME_TYPE::template make<TARGET_TYPE>(), &i_target_object)
        {
        }

        /** Copy constructor.

        \b Postcoditions:
            Given a type RT satisfying the requirements of \ref RuntimeType_requirements "RuntimeType" and
            an object r of type dynamic_reference<RT>, the following conditions hold:
            \snippet dynamic_reference_examples.cpp copy construct example 1

        \b Throws: unspecified */
        constexpr dynamic_reference(const dynamic_reference &) = default;

        /** Deleted copy assignment. */
        dynamic_reference & operator=(const dynamic_reference & i_source) = delete;

        /** Returns a reference to the runtime type.

        \b Throws: nothing */
        constexpr const RUNTIME_TYPE & type() const noexcept { return m_type; }

        /** Returns the address of the target object. If the dynamic_reference is empty,
            the return value is nullptr.

        \b Throws: nothing */
        constexpr void * address() const noexcept { return m_address; }

        /** Returns whether the target type is bound to the provided compile-time type.
            
            \snippet dynamic_reference_examples.cpp is example 1

        \b Throws: nothing */
        template <typename TYPE> constexpr bool is() const noexcept { return m_type.is<TYPE>(); }

        /** Returns a reference to the target object, assuming that the target type is bound
            to the provided compile-time type.

            \b Precoditions:
               The behavior is undefined if any of these conditions is not satisfied:
                - The target type is not the provided compile-time type.

            \snippet dynamic_reference_examples.cpp as example 1

        \b Throws: nothing */
        template <typename TYPE> DENSITY_CPP14_CONSTEXPR TYPE & as() const noexcept
        {
            assert(m_type.is<TYPE>());
            return *static_cast<TYPE *>(m_address);
        }

      private:
        void * const       m_address = nullptr;
        RUNTIME_TYPE const m_type;
    };

} // namespace density
