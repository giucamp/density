
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/small_function_queue.h>
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

    std::string s("def");
    queue.push(s); // copy-construct

        //! [heter_queue push example 1]
    }
    {
        //! [heter_queue emplace example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    // insert an int
    queue.emplace<int>();

    // check the type and the value
    auto it = queue.cbegin();
    assert(it.complete_type() == runtime_type<>::make<int>() );
    assert(*static_cast<const int*>(it.element()) == 0);

    queue.emplace<std::string>("abc"); // move-construct

    std::string s("def");
    queue.emplace<std::string>(s); // copy-construct

        //! [heter_queue emplace example 1]
    (void)it;
    }
    {
        //! [heter_queue dyn_push_copy example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    std::string s("abc");
    auto const source_ptr = static_cast<const void*>(&s);
    auto const type = runtime_type<>::make<std::string>();
    queue.dyn_push_copy(type, source_ptr); // move-construct


        //! [heter_queue dyn_push_copy example 1]
    }
    {
        //! [heter_queue dyn_push_move example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    std::string s("abc");
    auto const type = runtime_type<>::make<std::string>();
    queue.dyn_push_move(type, static_cast<void*>(&s)); // move-construct

    std::cout << "This string has valid but indeterminate content: " << s << std::endl;


        //! [heter_queue dyn_push_move example 1]
    }
    {
        //! [heter_queue begin_push example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    queue.begin_push(std::string("abc")).commit(); // move-construct

    std::string s("def");
    queue.begin_push(s).commit(); // copy-construct

        //! [heter_queue begin_push example 1]
    }
    {
        //! [heter_queue begin_dyn_push example 1]

    using namespace density;
    using namespace type_features;

    // by default runtime_type does not include the feature default_construct, so we have to add it
    using rt = runtime_type<void, feature_concat_t<default_type_features_t<void>, default_construct>>;
    heterogeneous_queue<void, rt> queue;

    auto const type = rt::make<std::string>();
    queue.begin_dyn_push(type).commit(); // move-construct


        //! [heter_queue begin_dyn_push example 1]
    }

    {
        //! [heter_queue begin_dyn_push_copy example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    auto const type = runtime_type<>::make<std::string>();
    std::string str("hello");
    auto transaction = queue.begin_dyn_push_copy(type, &str); // copy-construct
    transaction.commit();

    auto it = queue.begin();
    assert(it.complete_type() == type);
    std::cout << *static_cast<std::string*>(it.element()) << " world!" << std::endl;

        //! [heter_queue begin_dyn_push_copy example 1]
    }

    {
        //! [heter_queue begin_dyn_push_move example 1]

    using namespace density;
    heterogeneous_queue<> queue;

    auto const type = runtime_type<>::make<std::string>();
    std::string str("hello");
    auto transaction = queue.begin_dyn_push_move(type, &str); // move-construct
    transaction.commit();

    auto it = queue.begin();
    assert(it.complete_type() == type);
    std::cout << *static_cast<std::string*>(it.element()) << " world!" << std::endl;

        //! [heter_queue begin_dyn_push_move example 1]
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
