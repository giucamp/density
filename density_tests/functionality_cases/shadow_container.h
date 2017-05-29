
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <testity/testity_common.h>
#include <random>
#include <deque>
#include <string>
#include <typeinfo>
#include <iostream>

namespace density_tests
{
    using namespace testity;

    /** A ShadowContainer keeps information about a dense container, to easy unit testing. It keeps a std::type_info
        and an hash for every element in the dense container. The dense container is owned externally.
        The units test performs the same operations on the dense container and on the shadow container, and checks
        for consistency. If an exception is thrown during an operation on a dense container, the shadow container can
        be used to check the strong exception guarantee. */
    template <typename DENSE_CONTAINER>
        class ShadowContainer
    {
    private:
        struct Element
        {
            const std::type_info * m_type_info;
            size_t m_object_hash;
        };

    public:

        /* Exception thrown as result to an exception occurred during the update of the shadow container.
            Handlers for of this exception can't check the tested container against the shadow container. */
        class BasicGuaranteeException : public TestException
        {
        public:

            BasicGuaranteeException(std::string i_what)
                : m_what(std::move(i_what)) {}

            const char* what() const noexcept override
                { return m_what.c_str(); }

        private:
            std::string m_what;
        };

        ShadowContainer() {}

        ShadowContainer(const DENSE_CONTAINER & i_container)
        {
            const auto end_it = i_container.end();
            for (const auto it = i_container.begin(); it != end_it; it++ )
            {
                auto hasher = it->curr_type().template get_feature<density::type_features::hash>();
                const auto & type_info = it->complete_type().type_info().type_info();
                m_deque.push_back(Element(type_info, hasher(it->element_ptr()) ));
            }
        }

        void check_equal(const DENSE_CONTAINER & i_container) const
        {
            const auto container_is_empty = i_container.empty();
            TESTITY_ASSERT(container_is_empty == m_deque.empty());
            TESTITY_ASSERT(container_is_empty == (i_container.begin() == i_container.end()));

            auto const dist = std::distance(i_container.begin(), i_container.end());
            TESTITY_ASSERT(dist >= 0 && static_cast<size_t>(dist) == m_deque.size());

            size_t index = 0;
            const auto end_it = i_container.end();
            for (auto it = i_container.begin(); it != end_it; it++)
            {
                auto hasher = it.complete_type().template get_feature<density::type_features::hash>();
                const auto & type_info = it.complete_type().type_info();
                const auto & deq_entry = m_deque[index];
                const auto hash = hasher(it.element_ptr());
                TESTITY_ASSERT(type_info == *deq_entry.m_type_info
                    && hash == deq_entry.m_object_hash );
                index++;
            }

            TESTITY_ASSERT(index == m_deque.size());
        }

        void compare_at(size_t i_at, const typename DENSE_CONTAINER::runtime_type & i_type, const void * i_element)
        {
            TESTITY_ASSERT(i_at < m_deque.size());
            TESTITY_ASSERT(*m_deque[i_at].m_type_info == i_type.type_info());
            const auto element_hash = i_type.template get_feature<density::type_features::hash>()(i_element);
            TESTITY_ASSERT(element_hash == m_deque[i_at].m_object_hash);
        }

        void compare_front(const typename DENSE_CONTAINER::runtime_type & i_type, const void * i_element)
        {
            TESTITY_ASSERT(m_deque.size() > 0);
            compare_at(0, i_type, i_element);
        }

        void compare_back(const typename DENSE_CONTAINER::runtime_type & i_type, const void * i_element)
        {
            TESTITY_ASSERT(m_deque.size() > 0);
            compare_at(m_deque.size() - 1, i_type, i_element);
        }

        template <typename TYPE>
            void insert_at(size_t i_at, const TYPE & i_element, size_t i_count = 1)
        {
            try
            {
                using runtime_type = typename DENSE_CONTAINER::runtime_type;
                const auto type = runtime_type::template make<TYPE>();
                Element new_element{ &type.type_info(), type.template get_feature<density::type_features::hash>()(
                    static_cast<const typename DENSE_CONTAINER::common_type*>(&i_element) ) };
                m_deque.insert(m_deque.begin() + i_at, i_count, new_element);
            }
            catch (...)
            {
                throw BasicGuaranteeException("insert_at failed");
            }
        }

        template <typename TYPE>
            void push_back(const TYPE & i_element)
        {
            insert_at(m_deque.size(), i_element, 1);
        }

        template <typename TYPE>
            void push_front(const TYPE & i_element)
        {
            insert_at(0, i_element, 1);
        }

        void erase_at(size_t i_at, size_t i_count = 1)
        {
            try
            {
                TESTITY_ASSERT(m_deque.size() >= i_at + i_count);
                m_deque.erase(m_deque.begin() + i_at, m_deque.begin() + i_at + i_count);
            }
            catch (...)
            {
                throw BasicGuaranteeException("erase failed");
            }
        }

        void pop_back()
        {
            m_deque.pop_back();
        }

        void pop_front()
        {
            m_deque.pop_front();
        }

        bool empty() const { return m_deque.empty(); }

        typename std::deque<Element>::size_type size() const { return m_deque.size(); }

    private:
        std::deque<Element> m_deque;
    };

} // namespace density_tests
