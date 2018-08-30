
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/dynamic_reference.h>
#include <density/runtime_type.h>
#include <istream>
#include <ostream>

namespace density
{
    /** This feature reads an object from an std::istream using the operator >> */
    class f_istream
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_istream make() noexcept
        {
            return f_istream{&invoke<TARGET_TYPE>};
        }

        /** Reads the target object from an input stream.
            @param i_source pointer to an instance of the target type. Can't be null.
                If the dynamic type of the pointed object is not the target type (assigned
                by the function make), the behavior is undefined. */
        void operator()(std::istream & i_istream, void * i_dest) const
        {
            DENSITY_ASSUME(i_dest != nullptr);
            (*m_function)(i_istream, i_dest);
        }

      private:
        using Function = void (*)(std::istream & i_istream, void * i_dest);
        Function const m_function;
        constexpr f_istream(Function i_function) noexcept : m_function(i_function) {}
        template <typename TARGET_TYPE> static void invoke(std::istream & i_istream, void * i_dest)
        {
            const auto derived = static_cast<TARGET_TYPE *>(i_dest);
            i_istream >> *derived;
        }
    };

    /** This feature writes an object to an std::ostream using the operator >> */
    class f_ostream
    {
      public:
        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static f_ostream make() noexcept
        {
            return f_ostream{&invoke<TARGET_TYPE>};
        }

        /** Reads from an input stream the target object.
            @param i_source pointer to an instance of the target type. Can't be null.
                If the dynamic type of the pointed object is not the target type (assigned
                by the function make), the behavior is undefined. */
        void operator()(std::ostream & i_ostream, const void * i_dest) const
        {
            DENSITY_ASSUME(i_dest != nullptr);
            (*m_function)(i_ostream, i_dest);
        }

      private:
        using Function = void (*)(std::ostream & i_ostream, const void * i_dest);
        Function const m_function;
        constexpr f_ostream(Function i_function) noexcept : m_function(i_function) {}
        template <typename TARGET_TYPE>
        static void invoke(std::ostream & i_ostream, const void * i_dest)
        {
            const auto derived = static_cast<const TARGET_TYPE *>(i_dest);
            i_ostream << *derived;
        }
    };

    /** Reads the target object of a dynamic_reference that supports the feature f_ostream from an std::istream.
        If the feature f_ostream is supported by the runtime_type, this function does not
        participate in overload in overload resolution.
        Note: the dynamic_reference must be bound to a valid target of the correct type before the function is called.

        \b Precoditions:
            The behavior is undefined if any of these conditions is not satisfied:
            - The dynamic_reference is empty.

        \b Example:
            \snippet dynamic_reference_examples.cpp istream example 1

        \b Throws: unspecified */
    template <
      typename... FEATURES,
      typename = std::enable_if<has_features<feature_list<FEATURES...>, f_istream>::value>::type>
    inline std::istream & operator>>(
      std::istream & i_source_stream, const dynamic_reference<runtime_type<FEATURES...>> & i_ptr)
    {
        i_ptr.type().get_feature<f_istream>()(i_source_stream, i_ptr.address());
        return i_source_stream;
    }

    /** Writes the target object of a dynamic_reference that supports the feature f_ostream to an std::ostream.
        If the feature f_ostream is supported by the runtime_type, this function does not 
        participate in overload in overload resolution.

        \b Precoditions:
            The behavior is undefined if any of these conditions is not satisfied:
            - The dynamic_reference is empty.

        \b Example:
            \snippet dynamic_reference_examples.cpp ostream example 1

        \b Throws: unspecified */
    template <
      typename... FEATURES,
      typename = std::enable_if<has_features<feature_list<FEATURES...>, f_ostream>::value>::type>
    inline std::ostream & operator<<(
      std::ostream & i_dest_stream, const dynamic_reference<runtime_type<FEATURES...>> & i_ref)
    {
        DENSITY_ASSERT(!i_ref.empty());
        i_ref.type().get_feature<f_ostream>()(i_dest_stream, i_ref.address());
        return i_dest_stream;
    }

} // namespace density
