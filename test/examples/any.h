
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../test_framework/density_test_common.h"
//
#include <density/io_runtimetype_features.h>
#include <density/runtime_type.h>
#include <ostream>
#include <typeinfo>

namespace density_examples
{
    class bad_any_cast : public std::bad_cast
    {
    };

    //! [any 1]
    template <typename... FEATURES> class any
    {
      public:
        using runtime_type      = density::runtime_type<FEATURES...>;
        using feature_list_type = typename runtime_type::feature_list_type;
        using tuple_type        = typename feature_list_type::tuple_type;

        constexpr any() noexcept = default;

        template <typename TYPE>
        any(const TYPE & i_source) : m_type(runtime_type::template make<TYPE>())
        {
            allocate();
            deallocate_if_excepts([&] { m_type.copy_construct(m_object, &i_source); });
        }

        any(const any & i_source) : m_type(i_source.m_type)
        {
            allocate();
            deallocate_if_excepts([&] { m_type.copy_construct(m_object, i_source.m_object); });
        }

        any(any && i_source) noexcept : m_type(i_source.m_type), m_object(i_source.m_object)
        {
            i_source.m_object = nullptr;
            i_source.m_type.clear();
        }

        template <typename CONSTRUCTION_FUNC>
        any(const runtime_type & i_type, CONSTRUCTION_FUNC && i_construction_func) : m_type(i_type)
        {
            allocate();
            deallocate_if_excepts(
              [&] { std::forward<CONSTRUCTION_FUNC>(i_construction_func)(m_object); });
        }

        any & operator=(any i_source) noexcept
        {
            swap(*this, i_source);
            return *this;
        }

        ~any()
        {
            if (m_object != nullptr)
            {
                m_type.destroy(m_object);
                deallocate();
            }
        }

        friend void swap(any & i_first, any & i_second) noexcept
        {
            std::swap(i_first.m_type, i_second.m_type);
            std::swap(i_first.m_object, i_second.m_object);
        }

        bool has_value() const noexcept { return m_object != nullptr; }

        const std::type_info & type() const noexcept
        {
            return m_type.empty() ? typeid(void) : m_type.type_info();
        }

        template <typename DEST_TYPE> friend DEST_TYPE any_cast(const any & i_source)
        {
            if (i_source.m_type.template is<DEST_TYPE>())
                return *static_cast<DEST_TYPE *>(i_source.m_object);
            else
                throw bad_any_cast();
        }

        template <typename DEST_TYPE> friend DEST_TYPE any_cast(any && i_source)
        {
            if (i_source.m_type.template is<DEST_TYPE>())
                return std::move(*static_cast<DEST_TYPE *>(i_source.m_object));
            else
                throw bad_any_cast();
        }

        template <typename DEST_TYPE>
        friend const DEST_TYPE * any_cast(const any * i_source) noexcept
        {
            if (i_source->m_type.template is<DEST_TYPE>())
                return static_cast<const DEST_TYPE *>(i_source->m_object);
            else
                return nullptr;
        }

        template <typename DEST_TYPE> friend DEST_TYPE * any_cast(any * i_source) noexcept
        {
            if (i_source->m_type.template is<DEST_TYPE>())
                return static_cast<DEST_TYPE *>(i_source->m_object);
            else
                return nullptr;
        }

        bool operator==(const any & i_source) const noexcept
        {
            if (m_type != i_source.m_type)
                return false;
            return m_object == nullptr || m_type.are_equal(m_object, i_source.m_object);
        }

        bool operator!=(const any & i_source) const noexcept { return !operator==(i_source); }

        template <typename FEATURE> const FEATURE & get_type_feature() const noexcept
        {
            return m_type.template get_feature<FEATURE>();
        }

        void *       object_ptr() noexcept { return m_object; }
        const void * object_ptr() const noexcept { return m_object; }

        const runtime_type & get_runtime_type() const noexcept { return m_type; }

      private:
        void allocate() { m_object = density::aligned_allocate(m_type.size(), m_type.alignment()); }

        void deallocate() noexcept
        {
            density::aligned_deallocate(m_object, m_type.size(), m_type.alignment());
        }

        template <typename FUNC> void deallocate_if_excepts(const FUNC & i_func)
        {
            try
            {
                i_func();
            }
            catch (...)
            {
                if (m_object != nullptr)
                    deallocate();
                m_type.clear();
                throw;
            }
        }

      private:
        runtime_type m_type;
        void *       m_object{nullptr};
    };

    // any_cast is already defined, but we have to declare it too
    template <typename DEST_TYPE, typename... FEATURES>
    DEST_TYPE any_cast(const any<FEATURES...> & i_source);
    template <typename DEST_TYPE, typename... FEATURES>
    DEST_TYPE any_cast(any<FEATURES...> && i_source);
    template <typename DEST_TYPE, typename... FEATURES>
    const DEST_TYPE * any_cast(const any<FEATURES...> * i_source) noexcept;
    template <typename DEST_TYPE, typename... FEATURES>
    DEST_TYPE * any_cast(any<FEATURES...> * i_source) noexcept;

    //! [any 1]

    //! [any 2]
    template <typename... FEATURES>
    std::ostream & operator<<(std::ostream & i_dest, const any<FEATURES...> & i_any)
    {
        using namespace density;
        static_assert( // for simplcity we don't SFINAE
          density::has_features<feature_list<FEATURES...>, f_ostream>::value,
          "The provided any leaks the fetaure f_ostream");

        if (i_any.has_value())
            i_any.template get_type_feature<f_ostream>()(i_dest, i_any.object_ptr());
        else
            i_dest << "[empty]";
        return i_dest;
    }
    //! [any 2]

    //! [any 3]
    struct f_sum
    {
        template <typename TARGET_TYPE> constexpr static f_sum make() noexcept
        {
            return f_sum{&invoke<TARGET_TYPE>};
        }

        void operator()(void * i_dest, void const * i_first, void const * i_second) const noexcept
        {
            (*m_function)(i_dest, i_first, i_second);
        }

        void (*m_function)(void * i_dest, void const * i_first, void const * i_second);

        template <typename TARGET_TYPE>
        static void invoke(void * i_dest, void const * i_first, void const * i_second)
        {
            auto const & first  = *static_cast<TARGET_TYPE const *>(i_first);
            auto const & second = *static_cast<TARGET_TYPE const *>(i_second);
            new (i_dest) TARGET_TYPE(first + second);
        }
    };

    template <typename... FEATURES>
    any<FEATURES...> operator+(const any<FEATURES...> & i_first, const any<FEATURES...> & i_second)
    {
        using namespace density;
        static_assert( // for simplcity we don't SFINAE
          density::has_features<feature_list<FEATURES...>, f_sum>::value,
          "The provided any leaks the fetaure sum_impl");

        if (i_first.type() != i_second.type() || i_first.type() == typeid(void))
            throw std::runtime_error("Mismatching types");

        return any<FEATURES...>(i_first.get_runtime_type(), [&](void * i_dest) {
            i_first.template get_type_feature<f_sum>()(
              i_dest, i_first.object_ptr(), i_second.object_ptr());
        });
    }
    //! [any 3]

} // namespace density_examples
