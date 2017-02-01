
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/function_queue.h>
#include <density/lifo.h>
#include <string>
#include <functional> // for std::bind
#include <iostream>
#include <utility>

namespace heter_queue_samples
{
    void run()
    {

    {
        //! [heter_queue push example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    queue.push(std::string("abc")); // move-construct

    std::wstring s(L"def");
    queue.push(s); // copy-construct

    assert(std::distance(queue.begin(), queue.end()) == 2);

    queue.consume([](runtime_type<> i_type, void * i_element_ptr) {
        assert(i_type == runtime_type<>::make<std::string>());
        assert(*static_cast<std::string*>(i_element_ptr) == "abc");
    });

    queue.consume([](runtime_type<> i_type, void * i_element_ptr) {
        assert(i_type == runtime_type<>::make<std::wstring>());
        assert(*static_cast<std::wstring*>(i_element_ptr) == L"def");
    });

    assert(queue.empty());

        //! [heter_queue push example 1]
    }
    {
        //! [heter_queue emplace example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    // insert a zero
    queue.emplace<int>();

    // insert "oooooooo"
    queue.emplace<std::string>(8, 'o');

    // insert {5, 10.}
    queue.emplace<std::pair<int,double>>(5, 10.);

    assert(std::distance(queue.begin(), queue.end()) == 3);

    // consume the elements

    int sum = 0, count = 0;
    for (auto v : queue)
    {
        if (v.first == runtime_type<>::make<int>())
        {
            sum += *static_cast<int*>(v.second);
            count++;
        }
    }
    assert(sum == 0 && count == 1);

    assert( queue.begin().complete_type() == runtime_type<>::make<int>()
        && *static_cast<int*>(queue.begin().element_ptr()) == 0);

    queue.pop();

    assert(queue.begin().complete_type() == runtime_type<>::make<std::string>()
        && *static_cast<std::string*>(queue.begin().element_ptr()) == "oooooooo");

    queue.pop();

    using Pair = std::pair<int, double>;
    assert(queue.begin().complete_type() == runtime_type<>::make<Pair>()
        && *static_cast<Pair*>(queue.begin().element_ptr()) == Pair(5, 10.));

    queue.pop();

    assert(queue.empty());

        //! [heter_queue emplace example 1]
    }
        {
        //! [heter_queue dyn_push example 1]

    using namespace density;
    using namespace type_features;
    using rt = runtime_type<void, feature_concat_t<default_type_features_t<void>, default_construct>>;
    heterogeneous_queue<void, rt> queue;
    queue.dyn_push(rt::make<int>());
    queue.dyn_push(rt::make<std::string>());
    queue.dyn_push(rt::make<std::wstring>());

    assert(std::distance(queue.begin(), queue.end()) == 3);

    queue.consume([](rt i_type, void * i_element_ptr) {
        assert(i_type == rt::make<int>());
        assert(*static_cast<int*>(i_element_ptr) == 0);
    });

    queue.consume([](rt i_type, void * i_element_ptr) {
        assert(i_type == rt::make<std::string>());
        assert(*static_cast<std::string*>(i_element_ptr) == "");
    });

    queue.consume([](rt i_type, void * i_element_ptr) {
        assert(i_type == rt::make<std::wstring>());
        assert(*static_cast<std::wstring*>(i_element_ptr) == L"");
    });

    assert(queue.empty());

        //! [heter_queue dyn_push example 1]
    }
    {
        //! [heter_queue dyn_push_copy example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    std::string s("abc");
    auto const source_ptr = static_cast<const void*>(&s);
    auto const type = runtime_type<>::make<std::string>();
    queue.dyn_push_copy(type, source_ptr);

    assert(s == "abc");

    assert(std::distance(queue.begin(), queue.end()) == 1);

    queue.consume([](runtime_type<> i_type, void * i_element_ptr) {
        assert(i_type == runtime_type<>::make<std::string>());
        assert(*static_cast<std::string*>(i_element_ptr) == "abc");
    });

    assert(queue.empty());


        //! [heter_queue dyn_push_copy example 1]
    }
    {
        //! [heter_queue dyn_push_move example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    std::string s("abc");
    auto const type = runtime_type<>::make<std::string>();
    queue.dyn_push_move(type, static_cast<void*>(&s)); // move-construct

    assert(std::distance(queue.begin(), queue.end()) == 1);

    queue.consume([](runtime_type<> i_type, void * i_element_ptr) {
        assert(i_type == runtime_type<>::make<std::string>());
        assert(*static_cast<std::string*>(i_element_ptr) == "abc");
    });

    assert(queue.empty());


    // s now has valid but indeterminate content

        //! [heter_queue dyn_push_move example 1]
    }
    {
        //! [heter_queue start_push example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    {
        struct Message
        {
            const char * m_message = nullptr;
            ~Message()
            {
                if (m_message)
                {
                    std::cout << m_message << std::endl;
                }
            }
        };
        auto transaction = queue.start_push(Message{});
        transaction.element_ptr()->m_message = transaction.raw_allocate_copy("abc");
        transaction.commit();
    }

        //! [heter_queue start_push example 1]
    }
    {
        //! [heter_queue start_dyn_push example 1]

    using namespace density;
    using namespace type_features;

    // by default runtime_type does not include the feature default_construct, so we have to add it
    using rt = runtime_type<void, feature_concat_t<default_type_features_t<void>, default_construct>>;
    heterogeneous_queue<void, rt> queue;

    auto const type = rt::make<std::string>();
    queue.start_dyn_push(type).commit(); // move-construct


        //! [heter_queue start_dyn_push example 1]
    }

    {
        //! [heter_queue start_dyn_push_copy example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    auto const type = runtime_type<>::make<std::string>();
    std::string str("hello");
    auto transaction = queue.start_dyn_push_copy(type, &str); // copy-construct
    transaction.commit();

    auto it = queue.begin();
    assert(it.complete_type() == type);
    std::cout << *static_cast<std::string*>(it.element_ptr()) << " world!" << std::endl;

        //! [heter_queue start_dyn_push_copy example 1]
    }

    {
        //! [heter_queue start_dyn_push_move example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    auto const type = runtime_type<>::make<std::string>();
    std::string str("hello");
    auto transaction = queue.start_dyn_push_move(type, &str); // move-construct
    transaction.commit();

    auto it = queue.begin();
    assert(it.complete_type() == type);
    std::cout << *static_cast<std::string*>(it.element_ptr()) << " world!" << std::endl;

        //! [heter_queue start_dyn_push_move example 1]
    }

    {
        //! [heter_queue consume example 1]

    using namespace density;
    heterogeneous_queue<> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);

    // consume returns the whatever the provided function object returns, in this case void
    queue.consume([](runtime_type<> i_type, void * i_element_ptr) {
        assert(i_type == runtime_type<>::make<int>());
        std::cout << "The first element is " << *static_cast<int*>(i_element_ptr) << std::endl;
    });

    int sum = 0;
    while (!queue.empty())
    {
        // in this case consume returns an in
        sum += queue.consume([](runtime_type<> i_type, void * i_element_ptr) {
            assert(i_type == runtime_type<>::make<int>());
            return *static_cast<int*>(i_element_ptr);
        });
    }
    std::cout << "The sum of the others is " << sum << std::endl;

        //! [heter_queue consume example 1]
    }

        {
        //! [heter_queue consume_if_any example 1]

    using namespace density;
    heterogeneous_queue<> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);

    // this lambda returns int
    auto return_lamda = [](runtime_type<> i_type, void * i_element_ptr) {
        assert(i_type == runtime_type<>::make<int>());
        return *static_cast<int*>(i_element_ptr);
    };

    int sum = 0;
    while (!queue.empty())
    {
        // consume_if_any returns an optional<int>
        auto res = queue.consume_if_any(return_lamda);
        if(res)
            sum += *res;
        else
        {
            assert(queue.empty());
        }
    }
    std::cout << "The sum is " << sum << std::endl;

    // this lambda returns void
    auto print_lamda = [](runtime_type<> i_type, void * i_element_ptr) {
        assert(i_type == runtime_type<>::make<int>());
        std::cout << "The element is " << *static_cast<int*>(i_element_ptr) << std::endl;
    };

    /* with a void function object, consume_if_any returns an optional<$unspeified_pod_type$>. This return
        value can be used only to test an element was consumed */
    using ret_val = std::decay<decltype(*queue.consume_if_any(print_lamda))>::type;
    static_assert(std::is_pod<ret_val>::value, "not a pod");

    queue.push(10);
    if (queue.consume_if_any(print_lamda))
        std::cout << "Consumed" << std::endl;
    else
        std::cout << "Not consumed" << std::endl;

        //! [heter_queue consume_if_any example 1]
    }

    } // void run()

} // namespace heter_queue_samples
