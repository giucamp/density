
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "..\dense_list.h"
#include "..\testing_utils.h"
#include "container_test.h"
#include <vector>
#include <list>

namespace density
{
    namespace detail
    {
		class TestString : public std::basic_string<char, std::char_traits<char>, TestAllocator<char> >
		{
		public:
			TestString() = default;
			TestString(const char * i_str) : std::basic_string<char, std::char_traits<char>, TestAllocator<char> >(i_str) {}

			TestString(const TestString&) = default;
			TestString & operator = (const TestString&) = default;

			TestString(TestString&&) noexcept = default;
			TestString & operator = (TestString&&) noexcept = default;

			virtual ~TestString() {}
		};

        namespace DenseListTest
        {
            using TestDenseListString = DenseList< TestString, TestAllocator<TestString> >;

            void dense_list_test_insert(TestDenseListString i_list, size_t i_at, size_t i_count)
            {
                using namespace DenseListTest;

                std::vector<TestString> vec(i_list.begin(), i_list.end());

                auto const res1 = i_list.insert(std::next(i_list.cbegin(), i_at), i_count, TestString("42"));
                auto const res2 = vec.insert(std::next(vec.cbegin(), i_at), i_count, TestString("42"));
                std::vector<TestString> vec_1(i_list.begin(), i_list.end());
                DENSITY_TEST_ASSERT(vec == vec_1);

                auto const dist1 = std::distance(i_list.begin(), res1);
                auto const dist2 = std::distance(vec.begin(), res2);
                DENSITY_TEST_ASSERT(dist1 == dist2);
            }

            #ifdef _MSC_VER
                #pragma warning(push)
                #pragma warning(disable: 4324) // structure was padded due to alignment specifier
            #endif

            #if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below

                struct StructB_1{ char m_member = 42; std::aligned_storage<1, 1> m_aligned; };
                struct StructB_2 { char m_member = 42; std::aligned_storage<1, 2> m_aligned; };
                struct StructB_4 { int m_member = 42; std::aligned_storage<1, 4> m_aligned; };
                struct StructB_8 { int m_member = 42; std::aligned_storage<1, 8> m_aligned; };
                struct StructB_16 { int m_member = 42; std::aligned_storage<1, 16> m_aligned; };
                struct StructB_32 { int m_member = 42; std::aligned_storage<1, 32> m_aligned; };
                struct StructB_64 { int m_member = 42; std::aligned_storage<1, 64> m_aligned; };
                struct StructB_128 { int m_member = 42; std::aligned_storage<1, 128> m_aligned; };
                struct StructB_256 { int m_member = 42; std::aligned_storage<1, 256> m_aligned; };

                template <typename BASE> struct StructA_1 : BASE { std::aligned_storage<1, 1 > m_aligned; };
                template <typename BASE> struct StructA_2 : BASE { std::aligned_storage<1, 2 > m_aligned; };
                template <typename BASE> struct StructA_4 : BASE { std::aligned_storage<1, 4 > m_aligned; };
                template <typename BASE> struct StructA_8 : BASE { std::aligned_storage<1, 8 > m_aligned; };
                template <typename BASE> struct StructA_16 : BASE { std::aligned_storage<1, 16 > m_aligned; };
                template <typename BASE> struct StructA_32 : BASE { std::aligned_storage<1, 32 > m_aligned; };
                template <typename BASE> struct StructA_64 : BASE { std::aligned_storage<1, 64 > m_aligned; };
                template <typename BASE> struct StructA_128 : BASE { std::aligned_storage<1, 128 > m_aligned; };
                template <typename BASE> struct StructA_256 : BASE { std::aligned_storage<1, 256 > m_aligned; };

            #else

                struct alignas(1) StructB_1{ char m_member = 42; };
                struct alignas(2) StructB_2 { char m_member = 42; };
                struct alignas(4) StructB_4 { int m_member = 42; };
                struct alignas(8) StructB_8 { int m_member = 42; };
                struct alignas(16) StructB_16 { int m_member = 42; };
                struct alignas(32) StructB_32 { int m_member = 42; };
                struct alignas(64) StructB_64 { int m_member = 42; };
                struct alignas(128) StructB_128 { int m_member = 42; };
                struct alignas(256) StructB_256 { int m_member = 42; };

                template <size_t VALUE, typename BASE> struct AlignHelper {
                    static const size_t value = VALUE > std::alignment_of<BASE>::value ? VALUE : std::alignment_of<BASE>::value;
                };

                template <typename BASE> struct alignas(AlignHelper<1, BASE>::value) StructA_1 : BASE { };
                template <typename BASE> struct alignas(AlignHelper<2, BASE>::value) StructA_2 : BASE { };
                template <typename BASE> struct alignas(AlignHelper<4, BASE>::value) StructA_4 : BASE { };
                template <typename BASE> struct alignas(AlignHelper<8, BASE>::value) StructA_8 : BASE { };
                template <typename BASE> struct alignas(AlignHelper<16, BASE>::value) StructA_16 : BASE { };
                template <typename BASE> struct alignas(AlignHelper<32, BASE>::value) StructA_32 : BASE { };
                template <typename BASE> struct alignas(AlignHelper<64, BASE>::value) StructA_64 : BASE { };
                template <typename BASE> struct alignas(AlignHelper<128, BASE>::value) StructA_128 : BASE { };
                template <typename BASE> struct alignas(AlignHelper<256, BASE>::value) StructA_256 : BASE { };

            #endif

            #ifdef _MSC_VER
                #pragma warning(pop)
            #endif

            void test1()
            {
                NoLeakScope leak_detector;

                const auto list = TestDenseListString::make();
                static_assert(sizeof(list) == sizeof(void*) * 1, "If the allocator is stateless DenseList is documented to be big as one pointers");
                DENSITY_TEST_ASSERT(list.begin() == list.end());
                DENSITY_TEST_ASSERT(list.size() == 0);
                DENSITY_TEST_ASSERT(list == TestDenseListString());

                // check copy constructor and copy assignment
                auto list_1 = list;
                DENSITY_TEST_ASSERT(list == list_1);
                auto list_2 = list_1;
                static_assert( std::is_copy_constructible<TestDenseListString::value_type>::value, "");
                //static_assert( (TestDenseListString::runtime_type::s_caps & ElementTypeCaps::copy_only) == TestDenseListString::runtime_type::s_caps, "");
                list_2 = list_1;
                DENSITY_TEST_ASSERT(list == list_2);

                // check move constructor and move assignment
                auto list_3 = std::move(list_1);
                DENSITY_TEST_ASSERT(list == list_3);
                auto list_4 = list_2;
                list_4 = std::move(list_2);
                DENSITY_TEST_ASSERT(list == list_4);
            }

            void test2()
            {
                NoLeakScope leak_detector;

                const auto list = TestDenseListString::make(TestString("1"), TestString("2"), TestString("3"));
                DENSITY_TEST_ASSERT(*std::next(list.begin(), 0) == "1");
                DENSITY_TEST_ASSERT(*std::next(list.begin(), 1) == "2");
                DENSITY_TEST_ASSERT(*std::next(list.begin(), 2) == "3");
                DENSITY_TEST_ASSERT(std::next(list.begin(), 2) != list.end());
                DENSITY_TEST_ASSERT(std::next(list.begin(), 3) == list.end());
                DENSITY_TEST_ASSERT(list.size() == 3);
                DENSITY_TEST_ASSERT(list != TestDenseListString());

                for (size_t i = 0; i <= list.size(); i++)
                {
                    for (size_t j = 0; j < 4; j++)
                    {
                        dense_list_test_insert(list, i, j);
                    }
                }

                // check copy constructor and copy assignment
                auto list_1 = list;
                DENSITY_TEST_ASSERT(list == list_1);
                auto list_2 = list_1;
                list_2 = list_1;
                DENSITY_TEST_ASSERT(list == list_2);

                // check move constructor and move assignment
                auto list_3 = std::move(list_1);
                DENSITY_TEST_ASSERT(list == list_3);
                auto list_4 = list_2;
                list_4 = std::move(list_2);
                DENSITY_TEST_ASSERT(list == list_4);

                // test erase
                for (size_t i = 0; i <= list.size(); i++)
                {
                    for (size_t j = i; j <= list.size(); j++)
                    {
                        auto list_5 = list;
                        std::vector<TestString> vec(list_5.begin(), list_5.end());
                        const auto vec_res = vec.erase(std::next(vec.begin(), i), std::next(vec.begin(), j));
                        const auto lst_res = list_5.erase(std::next(list_5.begin(), i), std::next(list_5.begin(), j));
                        std::vector<TestString> vec1(list_5.begin(), list_5.end());
                        DENSITY_TEST_ASSERT(vec == vec1);

                        const auto lst_dist = std::distance(list_5.begin(), lst_res);
                        const auto vec_dist = std::distance(vec.begin(), vec_res);
                        DENSITY_TEST_ASSERT(lst_dist == vec_dist);
                    }
                }
            }

            template <typename BASE_CLASS>
                void typed_alignment_test()
            {
                using List = DenseList< BASE_CLASS, TestAllocator<BASE_CLASS> >;
                
                std::vector<List> lists = {
                    List::make(),
                    List::make(StructA_16<BASE_CLASS>()),
                    List::make(StructA_16<BASE_CLASS>(), StructA_32<BASE_CLASS>()),
                    List::make(
                        StructA_16<BASE_CLASS>(),
                        StructA_8<BASE_CLASS>(),
                        StructA_256<BASE_CLASS>(),
                        StructA_64<BASE_CLASS>(),
                        StructA_4<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_1<BASE_CLASS>(),
                        StructA_2<BASE_CLASS>(),
                        StructA_32<BASE_CLASS>()),
                    List::make(
                        StructA_16<BASE_CLASS>(),
                        StructA_8<BASE_CLASS>(),
                        StructA_256<BASE_CLASS>(),
                        StructA_8<BASE_CLASS>(),
                        StructA_64<BASE_CLASS>(),
                        StructA_4<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_1<BASE_CLASS>(),
                        StructA_1<BASE_CLASS>(),
                        StructA_1<BASE_CLASS>(),
                        StructA_2<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_64<BASE_CLASS>(),
                        StructA_4<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_1<BASE_CLASS>(),
                        StructA_1<BASE_CLASS>(),
                        StructA_1<BASE_CLASS>(),
                        StructA_2<BASE_CLASS>(),
                        StructA_16<BASE_CLASS>(),
                        StructA_32<BASE_CLASS>()) };

                lists.insert(lists.begin() + lists.size()/2, 20, List::make(StructA_256<BASE_CLASS>(), StructA_32<BASE_CLASS>()) );

                for (const auto & list : lists)
                {
                    for (const auto & element : list)
                    {
                        DENSITY_TEST_ASSERT(element.m_member == 42);
                    }
                }
            }

            void test3()
            {
                NoLeakScope leak_detector;

                typed_alignment_test<StructB_1>();
                typed_alignment_test<StructB_2>();
                typed_alignment_test<StructB_4>();
                typed_alignment_test<StructB_8>();
                typed_alignment_test<StructB_16>();
                typed_alignment_test<StructB_32>();
                typed_alignment_test<StructB_64>();
                typed_alignment_test<StructB_128>();
                typed_alignment_test<StructB_256>();
            }

            class Moveable
            {
                public:
                    Moveable(int){}
                    Moveable(Moveable &&) DENSITY_NOEXCEPT {}
                    Moveable & operator = (Moveable &&) DENSITY_NOEXCEPT { return *this; }
                    Moveable(const Moveable &) = delete;
                    Moveable & operator = (const Moveable &) = delete;
            };

            void test4()
            {
                #if !defined(_MSC_VER) || _MSC_VER >= 1900 // disable for Visual Studio 2013 and below
                    NoLeakScope leak_detector;
                    using List = DenseList< Moveable, TestAllocator<Moveable> >;
                    List::make(Moveable(1), Moveable(2));
                #endif
            }

            template <typename ELEMENT, typename ACTION_ON_STD_LIST, typename ACTION_ON_DENSE_LIST>
                void test_operation_with_exceptions(
                    const DenseList< ELEMENT, TestAllocator<ELEMENT> > & i_dense_list, 
                    ACTION_ON_DENSE_LIST i_action_on_dense_list, ACTION_ON_STD_LIST i_action_std_list )
            {
                // dense_list_copy = i_dense_list
                DenseList< ELEMENT, TestAllocator<ELEMENT> > dense_list_copy;
                for (const auto & el : i_dense_list)
                {
                    const typename ELEMENT::UnderlyingClass & elu = el;
                    dense_list_copy.push_back(ELEMENT(elu));
                }

                // std_list_copy = i_dense_list
                std::list<ELEMENT> std_list_copy;
                for (const auto & el : i_dense_list)
                {
                    const typename ELEMENT::UnderlyingClass & elu = el;
                    std_list_copy.push_back(ELEMENT(elu));
                }

                // apply the action on the dense list
                try
                {
                    i_action_on_dense_list(dense_list_copy);
                }
                catch (...)
                {
                    // ...check the strong exception guarantee: no changes is the list

                    // std_list_copy_2 = i_dense_list
                    std::list<ELEMENT> std_list_copy_2;
                    for (const auto & el : i_dense_list)
                    {
                        const typename ELEMENT::UnderlyingClass & elu = el;
                        std_list_copy_2.push_back(ELEMENT(elu));
                    }

                    DENSITY_TEST_ASSERT(std_list_copy == std_list_copy_2);

                    throw;
                }
                
                // appy the action on the list
                i_action_std_list(std_list_copy);

                // compare std_list_copy and dense_list_copy (should be equal)
                {
                    std::list<ELEMENT> dense_list_post_copy;
                    for (const auto & el : dense_list_copy)
                    {
                        const typename ELEMENT::UnderlyingClass & elu = el;
                        dense_list_post_copy.push_back(ELEMENT(elu));
                    }
                    DENSITY_TEST_ASSERT(dense_list_post_copy == std_list_copy);
                }
            }

            template <bool CAN_COPY_ELEMENTS, typename LIST> struct TestWithExceptionsOnList;
                template <typename LIST> struct TestWithExceptionsOnList<false, LIST>
            {
                // test on list with non-copyable elements
                static void do_it(const LIST & i_list)
                {
                    using Element = typename LIST::value_type;
                    using UndelyingElement = typename LIST::value_type::UnderlyingClass;
                    Element new_element;
                    const UndelyingElement & copy_source = new_element;

                    // test push_back( Element && new_element )
                    test_operation_with_exceptions(i_list,
                        [&copy_source](LIST & i_container) {
                            i_container.push_back(Element(copy_source)); },
                        [&copy_source](std::list<Element> & i_container) {
                            i_container.push_back(Element(copy_source)); }
                        );

                    // test push_front( Element && new_element )
                    test_operation_with_exceptions(i_list,
                        [&copy_source](LIST & i_container) {
                            i_container.push_front(Element(copy_source)); },
                        [&copy_source](std::list<Element> & i_container) {
                            i_container.push_front(Element(copy_source)); }
                        );

                    // test pop_back()
                    test_operation_with_exceptions(i_list,
                        [](LIST & i_container) {
                            i_container.pop_back(); },
                        [](std::list<Element> & i_container) {
                            i_container.pop_back(); }
                        );

                    // test pop_front()
                    test_operation_with_exceptions(i_list,
                        [](LIST & i_container) {
                            i_container.pop_front(); },
                        [](std::list<Element> & i_container) {
                            i_container.pop_front(); }
                        );

                    auto const size = i_list.size();
                    for (size_t from = 0; from <= size; from++)
                    {
                        // test insert( iterator at, Element && new_element )
                        /*test_operation_with_exceptions(i_list,
                            [from, &copy_source](LIST & i_container) {
                                i_container.insert(std::next(i_container.begin(), from), Element(copy_source)); },
                            [from, &copy_source](std::list<Element> & i_container) {
                                i_container.insert(std::next(i_container.begin(), from), Element(copy_source)); }
                            );*/

                        // test erase( iterator at )
                        if (from < size)
                        {
                            test_operation_with_exceptions(i_list,
                                [from](LIST & i_container) {
                                    i_container.erase(std::next(i_container.begin(), from)); },
                                [from](std::list<Element> & i_container) {
                                    i_container.erase(std::next(i_container.begin(), from)); }
                                );
                        }

                        for (size_t to = from; to <= size; to++)
                        {
                            // test erase( iterator at, size_t count, const Element & new_element )
                            test_operation_with_exceptions(i_list,
                                [from, to](LIST & i_container) {
                                    i_container.erase(std::next(i_container.begin(), from), std::next(i_container.begin(), to)); },
                                [from, to](std::list<Element> & i_container) {
                                    i_container.erase(std::next(i_container.begin(), from), std::next(i_container.begin(), to)); }
                                );
                        }
                    }
                }
            };
            template <typename LIST> struct TestWithExceptionsOnList<true, LIST>
            {
                // test on list with copyable elements
                static void do_it(const LIST & i_list)
                {
                    TestWithExceptionsOnList<false, LIST>::do_it(i_list);
                    
                    using Element = typename LIST::value_type;
                    Element new_element;

                    // test push_back( const Element & new_element )
                    test_operation_with_exceptions(i_list,
                        [new_element](LIST & i_container) {
                            i_container.push_back(new_element); },
                        [new_element](std::list<Element> & i_container) {
                            i_container.push_back(new_element); }
                        );

                    // test push_front( const Element & new_element )
                    test_operation_with_exceptions(i_list,
                        [new_element](LIST & i_container) {
                            i_container.push_front(new_element); },
                        [new_element](std::list<Element> & i_container) {
                            i_container.push_front(new_element); }
                        );

                    auto const size = i_list.size();
                    for (size_t from = 0; from <= size; from++)
                    {
                        // test insert( iterator at, const Element & new_element )
                        test_operation_with_exceptions(i_list,
                            [from, new_element](LIST & i_container) {
                                i_container.insert(std::next(i_container.begin(), from), new_element); },
                            [from, new_element](std::list<Element> & i_container) {
                                i_container.insert(std::next(i_container.begin(), from), new_element); }
                            );

                        for (size_t to = from; to <= size; to++)
                        {
                            // test insert( iterator at, size_t count, const Element & new_element )
                            test_operation_with_exceptions(i_list,
                                [from, to, new_element](LIST & i_container) {
                                    i_container.insert(std::next(i_container.begin(), from), to - from, new_element); },
                                [from, to, new_element](std::list<Element> & i_container) {
                                    i_container.insert(std::next(i_container.begin(), from), to - from, new_element); }
                                );
                        }
                    }
                }
            };


            template <typename ELEMENT>
                void test_with_exceptions_typed()
            {
                using Element = ELEMENT;
                using List = DenseList< Element, TestAllocator<Element> >;

                auto list = List::make(Element(), Element(), Element());
                TestWithExceptionsOnList<std::is_copy_constructible<typename List::value_type>::value, List>::do_it(list);
            }

            void test_with_exceptions()
            {
                //test_with_exceptions_typed<MovableTestObject>();
                //test_with_exceptions_typed<CopyableTestObject>();
            }

            void test_void_dense_list()
            {
                /*auto void_list = DenseList<void>::template make(1,2,3);
                int sum = 0;
                for (auto it = void_list.begin(); it != void_list.end(); it++)
                {
                    sum += *(int*)it.curr_element();
                }
                (void)sum;*/
            }
        }
    }

    void list_test()
    {
        detail::DenseListTest::test_void_dense_list();

        detail::DenseListTest::test1();
        detail::DenseListTest::test2();
        detail::DenseListTest::test3();
        detail::DenseListTest::test4();

        run_exception_stress_test(&detail::DenseListTest::test_with_exceptions);
    }

}
