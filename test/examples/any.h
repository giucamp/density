
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/runtime_type.h>
#include <typeinfo>

namespace density_examples
{
    class bad_any_cast : public std::bad_cast
    {
    };

    //! [any]
    template <typename... FEATURES> class any
    {
      public:
        using type_type = density::runtime_type<FEATURES...>;

        constexpr any() noexcept = default;

        template <typename TYPE> any(const TYPE & i_source) : m_type(type_type::template make<TYPE>())
        {
            allocate();
            deallocate_on_failure([&] { m_type.copy_construct(m_object, &i_source); });
        }

        any(const any & i_source) : m_type(i_source.m_type)
        {
            allocate();
            deallocate_on_failure([&] { m_type.copy_construct(m_object, i_source.m_object); });
        }

        any(any && i_source) noexcept : m_type(i_source.m_type), m_object(i_source.m_object)
        {
            i_source.m_object = nullptr;
            i_source.m_type.clear();
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
            if (m_object == nullptr)
                return true;
            return m_type.are_equal(m_object, i_source.m_object);
        }

        bool operator!=(const any & i_source) const noexcept { return !operator==(i_source); }

      private:
        void allocate() { m_object = density::aligned_allocate(m_type.size(), m_type.alignment()); }

        void deallocate() noexcept
        {
            density::aligned_deallocate(m_object, m_type.size(), m_type.alignment());
        }

        template <typename FUNC> void deallocate_on_failure(const FUNC & i_func)
        {
            try
            {
                i_func();
            }
            catch (...)
            {
                if (m_object != nullptr)
                    deallocate();
                throw;
            }
        }

      private:
        type_type m_type;
        void *    m_object{nullptr};
    };
    //! [any]

    template <typename DEST_TYPE, typename... FEATURES>
    DEST_TYPE any_cast(const any<FEATURES...> & i_source);

    template <typename DEST_TYPE, typename... FEATURES>
    DEST_TYPE any_cast(any<FEATURES...> && i_source);

    template <typename DEST_TYPE, typename... FEATURES>
    const DEST_TYPE * any_cast(const any<FEATURES...> * i_source) noexcept;

    template <typename DEST_TYPE, typename... FEATURES>
    DEST_TYPE * any_cast(any<FEATURES...> * i_source) noexcept;

} // namespace density_examples
