
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/density_common.h"
#include "../testity/testing_utils.h"
#include <random>
#include <deque>
#include <string>
#include <typeinfo>
#include <iostream>

namespace density
{
    namespace tests
    {
        using namespace testity;

        /* This class is used to test density containers. It owns an object which allocates memory with TestAllocator. In this
            way a TestObjectBase which is not correctly destroyed, will cause a memory leak that will be detected during unit tests. */
        class TestObjectBase
        {
        public:

            TestObjectBase(std::mt19937 & i_random)
                : m_hash(std::allocate_shared<size_t>(TestAllocator<size_t>(), std::uniform_int_distribution<size_t>()(i_random))) {}

            TestObjectBase(size_t i_val)
                : m_hash(std::allocate_shared<size_t>(TestAllocator<size_t>(), i_val)) {}

            TestObjectBase(const TestObjectBase & i_source)
                : m_hash(std::allocate_shared<size_t>(TestAllocator<size_t>(), *i_source.m_hash))
            {
                DENSITY_TEST_ASSERT(i_source.m_check_word == 3232);
            }

            TestObjectBase(TestObjectBase && i_source) noexcept
                : m_hash(std::move(i_source.m_hash))
            {
                DENSITY_TEST_ASSERT(i_source.m_check_word == 3232);
                i_source.m_hash = nullptr;
            }

            TestObjectBase & operator = (const TestObjectBase & i_source)
            {
                DENSITY_TEST_ASSERT(m_check_word == 3232 && i_source.m_check_word == 3232);
                m_hash = std::allocate_shared<size_t>(TestAllocator<size_t>(), *i_source.m_hash);
                return *this;
            }

            TestObjectBase & operator = (TestObjectBase && i_source) noexcept
            {
                DENSITY_TEST_ASSERT(m_check_word == 3232 && i_source.m_check_word == 3232);
                m_hash = std::move(i_source.m_hash);
                i_source.m_hash = nullptr;
                return *this;
            }

            ~TestObjectBase()
            {
                m_check_word = 1239873;
            }

            bool operator == (const TestObjectBase & i_other) const
            {
                DENSITY_TEST_ASSERT(m_check_word == 3232 && i_other.m_check_word == 3232);
                return *m_hash == *i_other.m_hash;
            }

            bool operator != (const TestObjectBase & i_other) const
            {
                DENSITY_TEST_ASSERT(m_check_word == 3232 && i_other.m_check_word == 3232);
                return *m_hash != *i_other.m_hash;
            }

            size_t hash() const
            {
                DENSITY_TEST_ASSERT(m_check_word == 3232);
                return m_hash != nullptr ? *m_hash : 0;
            }

        private:
            std::shared_ptr<size_t> m_hash;
            int m_check_word = 3232;
        };

        /* Returns the hash of a TestObjectBase. This function is compliant with FeatureHash */
        inline size_t hash_func(const TestObjectBase & i_object)
        {
            return i_object.hash();
        }

        /* Copyable class deriving from CopyableTestObject. */
        class CopyableTestObject : public TestObjectBase
        {
        public:

            CopyableTestObject(std::mt19937 & i_random) : TestObjectBase(i_random) { exception_check_point(); }
            CopyableTestObject(const TestObjectBase & i_source) : TestObjectBase(i_source) {}

            virtual void f() {}

            // copy
            CopyableTestObject(const CopyableTestObject & i_source)
                : TestObjectBase((exception_check_point(), i_source))
            {

            }
            CopyableTestObject & operator = (const CopyableTestObject & i_source)
            {
                exception_check_point();
                TestObjectBase::operator = (i_source);
                exception_check_point();
                return *this;
            }

            // move (no except)
            CopyableTestObject(CopyableTestObject && i_source) noexcept = default;
            CopyableTestObject & operator = (CopyableTestObject && i_source) noexcept = default;

            bool operator == (const CopyableTestObject & i_other) const
                { return TestObjectBase::operator == (i_other); }

            bool operator != (const CopyableTestObject & i_other) const
                { return TestObjectBase::operator != (i_other); }
        };

        /* Movable class deriving from CopyableTestObject. */
        class MovableTestObject : public TestObjectBase
        {
        public:

            MovableTestObject(std::mt19937 & i_random) : TestObjectBase(i_random) { exception_check_point(); }
            MovableTestObject(const TestObjectBase & i_source) : TestObjectBase(i_source) {}

            virtual void f() {}

            // copy
            MovableTestObject(const MovableTestObject & i_source) = delete;
            MovableTestObject & operator = (const MovableTestObject & i_source) = delete;

            // move (no except)
            MovableTestObject(MovableTestObject && i_source) noexcept = default;
            MovableTestObject & operator = (MovableTestObject && i_source) noexcept = default;

            bool operator == (const MovableTestObject & i_other) const
                { return TestObjectBase::operator == (i_other); }

            bool operator != (const MovableTestObject & i_other) const
                { return TestObjectBase::operator != (i_other); }
        };

        /* AlignedRandomStorage  */
        template <size_t SIZE, size_t ALIGNMENT>
            struct alignas(ALIGNMENT) AlignedRandomStorage
        {
        public:

            AlignedRandomStorage(std::mt19937 & i_random)
            {
                for (size_t i = 0; i < SIZE; i++)
                {
                    m_data[i] = static_cast<unsigned char>( std::uniform_int_distribution<unsigned>()(i_random) );
                }
            }

            size_t hash() const
            {
                DENSITY_TEST_ASSERT(m_check_word_1 == 7865);

                size_t result = 0;
                for (size_t i = 0; i < SIZE; i++)
                {
                    result = result * 33 + m_data[i];
                }
                return result;
            }

        private:
            unsigned char m_data[SIZE];
            const int m_check_word_1 = 7865;
        };

        /** Returns the hash of an AlignedRandomStorage. This function is compliant with FeatureHash */
        template <size_t SIZE, size_t ALIGNMENT>
            inline size_t hash_func(const AlignedRandomStorage<SIZE, ALIGNMENT> & i_object)
        {
            return i_object.hash();
        }

        class ComplexTypeBase : public virtual CopyableTestObject
        {
        public:
            ComplexTypeBase(std::mt19937 & i_random)
                : CopyableTestObject(i_random),
                  m_hash(std::uniform_int_distribution<size_t>(0, 7)(i_random))
                    { }

            virtual size_t hash() const
            {
                DENSITY_TEST_ASSERT(m_check_word_2 == 7865);
                return m_hash ^ CopyableTestObject::hash();
            }

        private:
            size_t m_hash;
            const int m_check_word_2 = 7865;
        };

        class ComplexType_A : public virtual ComplexTypeBase
        {
        public:
            ComplexType_A(std::mt19937 & i_random)
                : CopyableTestObject(i_random),
                  ComplexTypeBase(i_random),
                  m_hash(std::uniform_int_distribution<size_t>(0, 7)(i_random))
                    { }

            virtual size_t hash() const
            {
                DENSITY_TEST_ASSERT(m_check_word_3 == 1543);
                return m_hash ^ ComplexTypeBase::hash();
            }

        private:
            size_t m_hash;
            const int m_check_word_3 = 1543;
        };

        class ComplexType_B : public virtual ComplexTypeBase
        {
        public:
            ComplexType_B(std::mt19937 & i_random)
                : CopyableTestObject(i_random),
                  ComplexTypeBase(i_random),
                  m_hash(std::uniform_int_distribution<size_t>(0, 7)(i_random))
                    { }

            virtual size_t hash() const
            {
                DENSITY_TEST_ASSERT(m_check_word_4 == 12465);
                return m_hash ^ ComplexTypeBase::hash();
            }

        private:
            size_t m_hash;
            const int m_check_word_4 = 12465;
        };

        class ComplexType_C : public ComplexType_A, public ComplexType_B
        {
        public:
            ComplexType_C(std::mt19937 & i_random)
                : CopyableTestObject(i_random), ComplexTypeBase(i_random), ComplexType_A(i_random), ComplexType_B(i_random),
                  m_hash(std::uniform_int_distribution<size_t>(0, 7)(i_random))
                    { }

            virtual size_t hash() const
            {
                return m_hash ^ ComplexTypeBase::hash();
            }

        private:
            size_t m_hash;
        };

        /** Returns the hash of an ComplexTypeBase. This function is compliant with FeatureHash */
        inline size_t hash_func(const ComplexTypeBase & i_object)
        {
            return i_object.hash();
        }

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
                size_t m_hash;
            };

        public:

            /* Exception thrown as result to an exception occurred during the update of the shadow container.
                Handlers for of this exception can't check the tested container against the shadow container. */
            struct BasicGuaranteeException : TestException
            {
                using TestException::TestException;
            };

            ShadowContainer() {}

            ShadowContainer(const DENSE_CONTAINER & i_container)
            {
                const auto end_it = i_container.end();
                for (const auto it = i_container.begin(); it != end_it; it++ )
                {
                    auto hasher = it->curr_type().get_feature<FeatureHash>();
                    const auto & type_info = it->complete_type().type_info().type_info();
                    m_deque.push_back(Element(type_info, hasher(it->element()) ));
                }
            }

            void compare_all(const DENSE_CONTAINER & i_container)
            {
                const auto container_is_empty = i_container.empty();
                DENSITY_TEST_ASSERT(container_is_empty == m_deque.empty());
                DENSITY_TEST_ASSERT(container_is_empty == (i_container.begin() == i_container.end()));

                const auto end_it1 = i_container.end();
                for (auto it = i_container.begin(); it != end_it1; it++)
                {
                    it.complete_type();
                }

                auto const dist = std::distance(i_container.begin(), i_container.end());
                DENSITY_TEST_ASSERT(dist >= 0 && static_cast<size_t>(dist) == m_deque.size());

                size_t index = 0;
                const auto end_it = i_container.end();
                for (auto it = i_container.begin(); it != end_it; it++)
                {
                    auto hasher = it.complete_type().template get_feature<type_features::FeatureHash>();
                    const auto & type_info = it.complete_type().type_info();
                    const auto & deq_entry = m_deque[index];
                    const auto hash = hasher(it.element());
                    DENSITY_TEST_ASSERT(type_info == *deq_entry.m_type_info
                        && hash == deq_entry.m_hash );
                    index++;
                }

                DENSITY_TEST_ASSERT(index == m_deque.size());
            }

            void compare_at(size_t i_at, const typename DENSE_CONTAINER::runtime_type & i_type, const void * i_element)
            {
                DENSITY_TEST_ASSERT(i_at < m_deque.size());
                DENSITY_TEST_ASSERT(*m_deque[i_at].m_type_info == i_type.type_info());
                const auto element_hash = i_type.template get_feature<type_features::FeatureHash>()(i_element);
                DENSITY_TEST_ASSERT(element_hash == m_deque[i_at].m_hash);
            }

            void compare_front(const typename DENSE_CONTAINER::runtime_type & i_type, const void * i_element)
            {
                DENSITY_TEST_ASSERT(m_deque.size() > 0);
                compare_at(0, i_type, i_element);
            }

            void compare_back(const typename DENSE_CONTAINER::runtime_type & i_type, const void * i_element)
            {
                DENSITY_TEST_ASSERT(m_deque.size() > 0);
                compare_at(m_deque.size() - 1, i_type, i_element);
            }

            template <typename TYPE>
                void insert_at(size_t i_at, const TYPE & i_element, size_t i_count = 1)
            {
                try
                {
                    using runtime_type = typename DENSE_CONTAINER::runtime_type;
                    const auto type = runtime_type::template make<TYPE>();
                    Element new_element{ &type.type_info(), type.template get_feature<type_features::FeatureHash>()(
                        static_cast<const typename DENSE_CONTAINER::value_type*>(&i_element) ) };
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
                    DENSITY_TEST_ASSERT(m_deque.size() >= i_at + i_count);
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

        /** This class template represent a test session to be run on a container implementation.
            The tested container is tested to fulfill the strong exception guarantee: whether or
            not an exception is thrown during a test case, the DENSE_CONTAINER is compared to the
            shadow container. */
        template <typename DENSE_CONTAINER>
            class ContainerTest
        {
        public:

            using TestCaseFunction = std::function<void(std::mt19937 & i_random)>;

            ContainerTest(std::string i_name)
                : m_name(i_name + " with " + typeid(typename DENSE_CONTAINER::value_type).name()), m_total_probability(0.)
            {
            }

            void add_test_case(const char * i_name, std::function< void(std::mt19937 & i_random) > && i_function,
                double i_probability = 1. )
            {
                m_total_probability += i_probability;
                m_test_cases.push_back({i_name, std::move(i_function), i_probability});
            }

            void run(std::mt19937 & i_random)
            {
                std::cout << "Running the test " << m_name << std::endl;
                unsigned step_count = std::uniform_int_distribution<unsigned>(0, 1000)(i_random);
                for (unsigned step_index = 0; step_index < step_count; step_index++)
                {
                    step(i_random);
                }
                print_stats("test completed");
            }

            void step(std::mt19937 & i_random)
            {
                // pick a random test from m_test_cases, and execute it
                auto target_random_value = std::uniform_real_distribution<double>(0., m_total_probability)(i_random);
                double cumulative = 0.;
                for (auto & test_case : m_test_cases)
                {
                    cumulative += test_case.m_probability;
                    if (target_random_value < cumulative)
                    {
                        //std::cout << "\ttest case: " << test_case.m_name << ", size: " << m_shadow_container.size() << std::endl;
                        try
                        {
                            test_case.m_function(i_random);
                            test_case.m_executions++;
                        }
                        catch (typename ShadowContainer<DENSE_CONTAINER>::BasicGuaranteeException)
                        {
                            // the shadow container could not be updated
                            print_stats("BasicGuaranteeException raised");
                            throw;
                        }
                        catch (...)
                        {
                            print_stats("exception raised");
                            compare();
                            throw;
                        }
                        break;
                    }
                }

                compare();
            }

            void set_custom_check(const std::function<void()> & i_custom_check)
            {
                m_custom_check = i_custom_check;
            }

            // check for equality m_dense_container and shadow_container()
            void compare()
            {
                if (m_custom_check)
                    m_custom_check();
                m_shadow_container.compare_all(m_dense_container);
            }

            void print_stats(const char * i_msg) const
            {
                std::cout << "\t" << i_msg << ", container size: " << m_shadow_container.size() << std::endl;
                for (const auto & test_case : m_test_cases)
                {
                    std::cout << "\ttest case: " << test_case.m_name << " times: " << test_case.m_executions << std::endl;
                }
                std::cout << std::endl;
            }

            const DENSE_CONTAINER & dense_container() const { return m_dense_container; }
            DENSE_CONTAINER & dense_container() { return m_dense_container; }

            ShadowContainer<DENSE_CONTAINER> & shadow_container() { return m_shadow_container; }
            const ShadowContainer<DENSE_CONTAINER> & shadow_container() const { return m_shadow_container; }

        private: // data members
            NoLeakScope m_no_leak_scope;
            DENSE_CONTAINER m_dense_container;
            ShadowContainer<DENSE_CONTAINER> m_shadow_container;
            std::string m_name;
            std::function<void()> m_custom_check;

            struct TestCase
            {
                TestCase(std::string i_name, TestCaseFunction i_function, double i_probability)
                    : m_name(std::move(i_name)), m_function(std::move(i_function)), m_probability(i_probability), m_executions(0)
                {
                }

                std::string m_name;
                TestCaseFunction m_function;
                double m_probability;
                uint64_t m_executions;
            };
            std::vector<TestCase> m_test_cases;
            double m_total_probability;
        }; // class template ContainerTest

        template <typename DENSE_CONTAINER>
            void add_test_case_copy_and_assign(ContainerTest<DENSE_CONTAINER> & i_test, double i_probability)
        {
            i_test.add_test_case("copy_and_assign", [&i_test](std::mt19937 & /*i_random*/) {

                auto & dense_container = i_test.dense_container();

                /* Assign dense_container from a copy of itself. dense_container and shadow_container() must preserve the equality. */
                auto copy = dense_container;
                dense_container = copy;

                // check the size with the iterators
                const auto size_1 = std::distance(dense_container.cbegin(), dense_container.cend());
                const auto size_2 = std::distance(copy.cbegin(), copy.cend());
                DENSITY_TEST_ASSERT(size_1 == size_2);

                // move construct dense_container to tmp
                auto tmp(std::move(dense_container));
                DENSITY_TEST_ASSERT(dense_container.empty());
                const auto size_3 = std::distance(tmp.cbegin(), tmp.cend());
                DENSITY_TEST_ASSERT(size_1 == size_3);

                // move assign tmp to dense_container
                dense_container = std::move(tmp);
                const auto size_4 = std::distance(dense_container.cbegin(), dense_container.cend());
                DENSITY_TEST_ASSERT(size_1 == size_4);
            }, i_probability);
        }

    } // namespace detail

} // namespace density
