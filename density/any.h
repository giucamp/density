
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/void_allocator.h>
#include <density/runtime_type.h>

namespace density
{
    /** Class with value semantics that can contain an object of any type (similar to std::any)
    */
    template <typename COMMON_TYPE = void, typename VOID_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>>
        class any : VOID_ALLOCATOR
    {
    public:

        using common_type = COMMON_TYPE;
        using allocator_type = VOID_ALLOCATOR;
        using runtime_type = RUNTIME_TYPE;

        /** Creates an any bound to an instance of TYPE, constructing it in place with the provided construction arguments.
                @param i_args construction arguments to forward to the constructor of TYPE
                @return an instance of any bound to an instance of TYPE
            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
        Example:
            \snippet misc_samples.cpp any make example 1 */
        template <typename TYPE, typename ... CONSTRUCTION_ARGS>
            static any make(CONSTRUCTION_ARGS && ...i_args)
        {
            return make_with_alloc<TYPE>(allocator_type(), std::forward<CONSTRUCTION_ARGS>(i_args)...);
        }

        /** Creates an any bound to an instance of TYPE, constructing it in place with the provided construction arguments.
                @param i_allocator source to use to move-construct the allocator of the result
                @param i_args construction arguments to forward to the constructor of TYPE
                @return an instance of any bound to an instance of TYPE
            \n\b Throws: unspecified
            \n\b Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
        Example:
            \snippet misc_samples.cpp any make example 3 */
        template <typename TYPE, typename ... CONSTRUCTION_ARGS>
            static any make_with_alloc(allocator_type && i_allocator, CONSTRUCTION_ARGS && ...i_args)
        {
            auto const memory_block = i_allocator.allocate(sizeof(TYPE), alignof(TYPE));
            try
            {
                COMMON_TYPE * const object = new(memory_block) TYPE(std::forward<CONSTRUCTION_ARGS>(i_args)...);
                return any( object,
                    RUNTIME_TYPE::template make<TYPE>(),
                    std::move(i_allocator));
            }
            catch (...)
            {
                i_allocator.deallocate(memory_block, sizeof(TYPE), alignof(TYPE));
                throw;
            }
        }

        /** Creates an any bound to an instance of TYPE, constructing it in place with the provided construction arguments.
                @param i_allocator source to use to copy-construct the allocator of the result
                @param i_args construction arguments to forward to the constructor of TYPE
                @return an instance of any bound to an instance of TYPE
            \n\b Throws: unspecified
            \n\b Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
        Example:
            \snippet misc_samples.cpp any make example 2 */
        template <typename TYPE, typename ... CONSTRUCTION_ARGS>
            static any make_with_alloc(const allocator_type & i_allocator, CONSTRUCTION_ARGS && ...i_args)
        {
            return make_with_alloc<TYPE>(allocator_type(i_allocator), std::forward<CONSTRUCTION_ARGS>(i_args)...);
        }

        /** Default constructor. A default constructed any is empty.
            \n\b Throws: anything the default constructor of the allocator throws.
            \n\b Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
        any()
            : m_object(nullptr)
        {
        }

        /** Copy constructor. The constructed any is bound to a copy of the object bound to source.
            \n\b Throws: anything the default constructor of the allocator throws.
            \n\b Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
        any(const any & i_source)
            : VOID_ALLOCATOR(static_cast<const VOID_ALLOCATOR&>(i_source)), m_type(i_source.m_type)
        {
            auto const size = i_source.m_type.size();
            auto const alignment = i_source.m_type.alignment();
            auto const memory_block = allocator_type::allocate(size, alignment);
            try
            {
                m_object = m_type.copy_construct(memory_block, i_source.m_object);
            }
            catch (...)
            {
                allocator_type::deallocate(memory_block, size, alignment);
                throw;
            }
        }

        /** Move constructor. After the call the source has a valid but unspecified content. */
        any(any && i_source) noexcept
            : any()
        {
            swap(*this, i_source);
        }

        /** Copy assignment.
            \n\b Throws: unspecified
            \n\b Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
            http://stackoverflow.com/questions/3279543/what-is-the-copy-and-swap-idiom
        */
        any & operator = (const any & i_source)
        {
            any copy(i_source);
            swap(*this, copy);
            return *this;
        }

        /** Move assignment. After the call the source has a valid but unspecified content.
            \n\b Throws: nothing */
        any & operator = (any && i_source) noexcept
        {
            swap(*this, i_source);
            return *this;
        }

        ~any()
        {
            if (m_object != nullptr)
            {
                auto const old_memory_block = m_type.destroy(m_object);
                auto const old_size = m_type.size();
                auto const old_alignment = m_type.alignment();
                internal_get_allocator_ref().deallocate(old_memory_block, old_size, old_alignment);
            }
        }

        /**
            http://stackoverflow.com/questions/5695548/public-friend-swap-member-function
        */
        friend inline void swap(any & i_first, any & i_second) noexcept
        {
            using std::swap;
            swap(i_first.internal_get_allocator_ref(), i_second.internal_get_allocator_ref());
            swap(i_first.m_object, i_second.m_object);
            swap(i_first.m_type, i_second.m_type);
        }

        bool has_value() const noexcept
        {
            return m_object != nullptr;
        }

        COMMON_TYPE * object_ptr() const noexcept
        {
            return m_object;
        }

        const RUNTIME_TYPE & type() const noexcept
        {
            return m_type;
        }

        allocator_type get_allocator()
        {
            return *this;
        }

        const allocator_type & get_allocator_ref() const
        {
            return *this;
        }

        bool operator == (const any & i_other) const noexcept
        {
            if (m_type != i_other.m_type)
            {
                return false;
            }

            if (m_object == i_other.m_object) // they may be both empty
            {
                DENSITY_ASSERT_INTERNAL(m_object == nullptr);
                return true;
            }

            return m_type.template get_feature<type_features::equals>()(m_object, i_other.m_object);
        }

        bool operator != (const any & i_other) const noexcept
            { return !operator == (i_other); }

        void ckeck_invariants() const
        {
            //
            DENSITY_ASSERT_INTERNAL( (m_type == RUNTIME_TYPE()) == (m_object == nullptr) );
        }

    private:

        any(COMMON_TYPE * i_object, RUNTIME_TYPE && i_type, allocator_type && i_allocator) noexcept
            : allocator_type(std::move(i_allocator)), m_object(i_object), m_type(std::move(i_type))
        {
        }

        allocator_type & internal_get_allocator_ref() noexcept
        {
            return *this;
        }

    private:
        COMMON_TYPE * m_object;
        RUNTIME_TYPE m_type;
    };


    template <typename COMMON_TYPE, typename VOID_ALLOCATOR, typename RUNTIME_TYPE>
        inline void swap(any<COMMON_TYPE, VOID_ALLOCATOR, RUNTIME_TYPE> & i_first,
                         any<COMMON_TYPE, VOID_ALLOCATOR, RUNTIME_TYPE> & i_second) noexcept
    {
        i_first.swap(i_second);
    }
}
