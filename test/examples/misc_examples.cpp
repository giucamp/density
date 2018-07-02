
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//

#include <atomic>
#include <cstdlib>
#include <density/conc_function_queue.h>
#include <density/function_queue.h>
#include <density/lf_function_queue.h>
#include <iostream>
#include <string>
#include <thread>
namespace density_tests
{
    //! [runtime_type example 2]

    /* This feature calls an update function on any object. The update has not to be virtual, as
        type erasure already is a kind of virtualization. */
    struct feature_call_update
    {
        using type = void (*)(void * i_dest, float i_elapsed_time);

        template <typename BASE, typename TYPE> struct Impl
        {
            static void invoke(void * i_base_dest, float i_elapsed_time) noexcept
            {
                const auto base_dest = static_cast<BASE *>(i_base_dest);
                static_cast<TYPE *>(base_dest)->update(i_elapsed_time);
            }
            static const uintptr_t value;
        };
    };
    template <typename TYPE, typename BASE>
    const uintptr_t
      feature_call_update::Impl<TYPE, BASE>::value = reinterpret_cast<uintptr_t>(invoke);

    //! [runtime_type example 2]

    void misc_examples()
    {
        using namespace density;

        {
            //! [feature_list example 1]
            using namespace type_features;
            using MyFeatures = feature_list<default_construct, size, alignment>;
            //! [feature_list example 1]
            MyFeatures unused;
            (void)unused;
        }

        {
            //! [feature_concat example 1]
            using namespace type_features;
            using MyPartialFeatures = feature_list<default_construct, size>;
            using MyFeatures        = feature_concat_t<MyPartialFeatures, alignment>;
            using MyFeatures1       = feature_concat_t<MyPartialFeatures, feature_list<alignment>>;
            static_assert(std::is_same<MyFeatures, MyFeatures1>::value, "");
            //! [feature_concat example 1]
        }

        {
            //! [type_features::invoke example 1]

            using namespace type_features;

            // This feature allows to call a function object passing a std::string
            using MyInvoke = invoke<void(std::string)>;

            // we need MyInvoke and the standard features (size, alignment, ...)
            using MyFeatures = feature_concat_t<default_type_features_t<void>, MyInvoke>;

            // create a queue
            using QueueOfInvokables =
              heter_queue<void, runtime_type<void, MyFeatures>, default_allocator>;
            QueueOfInvokables my_queue;
            my_queue.push([](std::string s) { std::cout << s << std::endl; });

            // invoke my_queue.begin()
            my_queue.begin()->first.get_feature<MyInvoke>()(my_queue.begin()->second, "hello!");
            //! [type_features::invoke example 1]
        }

        {
            //! [runtime_type example 1]

            using namespace type_features;

            using MyRTType = runtime_type<void, feature_list<default_construct, destroy, size>>;

            MyRTType type = MyRTType::make<std::string>();

            void * buff = malloc(type.size());

            type.default_construct(buff);

            // now buff points to a valid std::string
            *static_cast<std::string *>(buff) = "hello world!";

            type.destroy(buff);

            free(buff);

            //! [runtime_type example 1]
        }


        {
            //! [runtime_type example 3]

            struct ObjectA
            {
                void update(float i_elapsed_time)
                {
                    std::cout << "ObjectA::update(" << i_elapsed_time << ")" << std::endl;
                }
            };

            struct ObjectB
            {
                void update(float i_elapsed_time)
                {
                    std::cout << "ObjectB::update(" << i_elapsed_time << ")" << std::endl;
                }
            };

            using namespace type_features;

            // concatenates feature_call_update to the default features (destroy, size, alignment)
            using MyFeatures = feature_concat_t<default_type_features_t<void>, feature_call_update>;

            // create a queue with 3 objects
            heter_queue<void, runtime_type<void, MyFeatures>, default_allocator> my_queue;
            my_queue.emplace<ObjectA>();
            my_queue.emplace<ObjectB>();
            my_queue.emplace<ObjectB>();

            // call update on all the objects
            auto const end_it = my_queue.end();
            for (auto it = my_queue.begin(); it != end_it; ++it)
            {
                auto const update_func = it->first.get_feature<feature_call_update>();
                update_func(it->second, 1.f / 60.f);
            }
            //! [runtime_type example 3]
        }

        {
            //! [function_queue example 1]
            // put a lambda
            function_queue<void()> queue;
            queue.push([] { std::cout << "Printing..." << std::endl; });

            // we can have a capture of any size
            double pi = 3.1415;
            queue.push([pi] { std::cout << pi << std::endl; });

            // now we execute the functions
            int executed = 0;
            while (queue.try_consume())
                executed++;
            //! [function_queue example 1]
            (void)executed;
        }
        {
            //! [function_queue example 2]
            function_queue<void()> queue;
            queue.push([] { std::cout << "Printing..." << std::endl; });

            auto print_func = [](const char * i_str) { std::cout << i_str; };
            queue.push(std::bind(print_func, "ello "));
            queue.push([print_func]() { print_func("world!"); });
            queue.push([]() { std::cout << std::endl; });

            while (!queue.try_consume())
                ;

            function_queue<int(double, double)> other_queue;
            other_queue.push([](double a, double b) { return static_cast<int>(a + b); });
            //! [function_queue example 2]
        }

        {
            //! [function_queue example 3]
            function_queue<std::string(const char * i_prefix)> queue;
            queue.push([](std::string i_prefix) { return i_prefix + "..."; });
            //! [function_queue example 3]
        }
        {
            //! [function_queue example 4]
            struct Message
            {
                const char * m_text;

                void operator()() { std::cout << m_text << std::endl; }
            };

            function_queue<void()> queue;

            auto transaction             = queue.start_emplace<Message>();
            transaction.element().m_text = transaction.raw_allocate_copy("Hello world");
            transaction.commit();

            bool const invoked = queue.try_consume();
            assert(invoked);
            //! [function_queue example 4]
            (void)invoked;
        }
        {
            //! [conc_function_queue example 1]
            using namespace std;
            conc_function_queue<void()> commands;
            atomic<bool>                finished(false);

            // this thread puts 10 commands
            thread producer([&] {
                for (int i = 0; i < 10; i++)
                {
                    commands.push([] { cout << "Hi there..." << endl; });
                    this_thread::sleep_for(chrono::milliseconds(10));
                }
                finished.store(true);
            });

            // this thread consumes until finished is true
            thread consumer([&] {
                while (!finished)
                {
                    commands.try_consume();
                    this_thread::sleep_for(chrono::milliseconds(10));
                }
            });

            producer.join();
            consumer.join();
            //! [conc_function_queue example 1]
        }

        {
            //! [lf_function_queue cardinality example]
            // single producer, multiple consumers:
            using Lf_SpMc_FuncQueue = lf_function_queue<
              void(),
              default_allocator,
              function_standard_erasure,
              concurrency_single,
              concurrency_multiple>;

            // multiple consumers, single producer:
            using Lf_MpSc_FuncQueue = lf_function_queue<
              void(),
              default_allocator,
              function_standard_erasure,
              concurrency_multiple,
              concurrency_single>;

            // multiple producer, multiple consumers (the default):
            using Lf_MpMc_FuncQueue = lf_function_queue<void()>;
            //! [lf_function_queue cardinality example]
            Lf_SpMc_FuncQueue q1;
            Lf_MpSc_FuncQueue q2;
            Lf_MpMc_FuncQueue q3;
        }
    }

} // namespace density_tests
