
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <iostream>
#include <iterator>
#include <complex>
#include <string>
#include <chrono>
#include <assert.h>
#include <density/heter_queue.h>
#include <density/io_runtimetype_features.h>
#include "test_framework/progress.h"

// if assert expands to nothing, some local variable becomes unused
#if defined(_MSC_VER) && defined(NDEBUG)
    #pragma warning(push)
    #pragma warning(disable:4189) // local variable is initialized but not referenced
#endif

namespace density_tests
{
    uint32_t compute_checksum(const void * i_data, size_t i_lenght)
    {
        uint32_t checksum = 0;
        auto const data = static_cast<const unsigned char*>(i_data);
        for (size_t i = 0; i < i_lenght; i++)
        {
            checksum += data[i];
        }
        return checksum;
    }

void heterogeneous_queue_put_samples()
{
    using namespace density;
{
    heter_queue<> queue;

    //! [heter_queue push example 1]
    queue.push(12);
    queue.push(std::string("Hello world!!"));
    //! [heter_queue push example 1]

    //! [heter_queue emplace example 1]
    queue.emplace<int>();
    queue.emplace<std::string>(12, '-');
    //! [heter_queue emplace example 1]

{
    //! [heter_queue start_push example 1]
    auto put = queue.start_push(12);
    put.element() += 2;
    put.commit(); // commits a 14
    //! [heter_queue start_push example 1]
}
{
    //! [heter_queue start_emplace example 1]
    auto put = queue.start_emplace<std::string>(4, '*');
    put.element() += "****";
    put.commit(); // commits a "********"
    //! [heter_queue start_emplace example 1]
}
}
{
    //! [heter_queue dyn_push example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    auto const type = MyRunTimeType::make<int>();
    queue.dyn_push(type); // appends 0
    //! [heter_queue dyn_push example 1]
}
{
    //! [heter_queue dyn_push_copy example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    std::string const source("Hello world!!");
    auto const type = MyRunTimeType::make<decltype(source)>();
    queue.dyn_push_copy(type, &source);
    //! [heter_queue dyn_push_copy example 1]
}
{
    //! [heter_queue dyn_push_move example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    std::string source("Hello world!!");
    auto const type = MyRunTimeType::make<decltype(source)>();
    queue.dyn_push_move(type, &source);
    //! [heter_queue dyn_push_move example 1]
}

{
    //! [heter_queue start_dyn_push example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    auto const type = MyRunTimeType::make<int>();
    auto put = queue.start_dyn_push(type);
    put.commit();
    //! [heter_queue start_dyn_push example 1]
}
{
    //! [heter_queue start_dyn_push_copy example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    std::string const source("Hello world!!");
    auto const type = MyRunTimeType::make<decltype(source)>();
    auto put = queue.start_dyn_push_copy(type, &source);
    put.commit();
    //! [heter_queue start_dyn_push_copy example 1]
}
{
    //! [heter_queue start_dyn_push_move example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    std::string source("Hello world!!");
    auto const type = MyRunTimeType::make<decltype(source)>();
    auto put = queue.start_dyn_push_move(type, &source);
    put.commit();
    //! [heter_queue start_dyn_push_move example 1]
}
}

void heterogeneous_queue_put_transaction_samples()
{
    using namespace density;
    using namespace type_features;
{
    //! [heter_queue put_transaction default_construct example 1]
    heter_queue<>::put_transaction<> transaction;
    assert(transaction.empty());
    //! [heter_queue put_transaction default_construct example 1]
}
{
    //! [heter_queue put_transaction copy_construct example 1]
    static_assert(!std::is_copy_constructible<heter_queue<>::put_transaction<>>::value, "");
    static_assert(!std::is_copy_constructible<heter_queue<int>::put_transaction<>>::value, "");
    //! [heter_queue put_transaction copy_construct example 1]
}
{
    //! [heter_queue put_transaction copy_assign example 1]
    static_assert(!std::is_copy_assignable<heter_queue<>::put_transaction<>>::value, "");
    static_assert(!std::is_copy_assignable<heter_queue<int>::put_transaction<>>::value, "");
    //! [heter_queue put_transaction copy_assign example 1]
}
{
    //! [heter_queue put_transaction move_construct example 1]
    heter_queue<> queue;
    auto transaction1 = queue.start_push(1);

    // move from transaction1 to transaction2
    auto transaction2(std::move(transaction1));
    assert(transaction1.empty());
    assert(transaction2.element() == 1);

    // commit transaction2
    transaction2.commit();
    assert(transaction2.empty());

    //! [heter_queue put_transaction move_construct example 1]

    //! [heter_queue put_transaction move_construct example 2]
    // put_transaction<void> can be move constructed from any put_transaction<T>
    static_assert(std::is_constructible<heter_queue<>::put_transaction<void>, heter_queue<>::put_transaction<void> &&>::value, "");
    static_assert(std::is_constructible<heter_queue<>::put_transaction<void>, heter_queue<>::put_transaction<int> &&>::value, "");

    // put_transaction<T> can be move constructed only from put_transaction<T>
    static_assert(!std::is_constructible<heter_queue<>::put_transaction<int>, heter_queue<>::put_transaction<void> &&>::value, "");
    static_assert(!std::is_constructible<heter_queue<>::put_transaction<int>, heter_queue<>::put_transaction<float> &&>::value, "");
    static_assert(std::is_constructible<heter_queue<>::put_transaction<int>, heter_queue<>::put_transaction<int> &&>::value, "");
    //! [heter_queue put_transaction move_construct example 2]
}
{
    //! [heter_queue put_transaction move_assign example 1]
    heter_queue<> queue;
    auto transaction1 = queue.start_push(1);

    heter_queue<>::put_transaction<> transaction2;
    transaction2 = std::move(transaction1);
    assert(transaction1.empty());
    transaction2.commit();
    assert(transaction2.empty());
    //! [heter_queue put_transaction move_assign example 1]

    //! [heter_queue put_transaction move_assign example 2]
    // put_transaction<void> can be move assigned from any put_transaction<T>
    static_assert(std::is_assignable<heter_queue<>::put_transaction<void>, heter_queue<>::put_transaction<void> &&>::value, "");
    static_assert(std::is_assignable<heter_queue<>::put_transaction<void>, heter_queue<>::put_transaction<int> &&>::value, "");

    // put_transaction<T> can be move assigned only from put_transaction<T>
    static_assert(!std::is_assignable<heter_queue<>::put_transaction<int>, heter_queue<>::put_transaction<void> &&>::value, "");
    static_assert(!std::is_assignable<heter_queue<>::put_transaction<int>, heter_queue<>::put_transaction<float> &&>::value, "");
    static_assert(std::is_assignable<heter_queue<>::put_transaction<int>, heter_queue<>::put_transaction<int> &&>::value, "");
    //! [heter_queue put_transaction move_assign example 2]
}
{
    //! [heter_queue put_transaction raw_allocate example 1]
    heter_queue<> queue;
    struct Msg
    {
        std::chrono::high_resolution_clock::time_point m_time = std::chrono::high_resolution_clock::now();
        size_t m_len = 0;
        void * m_data = nullptr;
    };

    auto post_message = [&queue](const void * i_data, size_t i_len) {
        auto transaction = queue.start_emplace<Msg>();
        transaction.element().m_len = i_len;
        transaction.element().m_data = transaction.raw_allocate(i_len, 1);
        memcpy(transaction.element().m_data, i_data, i_len);

        assert(!transaction.empty()); // a put transaction is not empty if it's bound to an element being put
        transaction.commit();
        assert(transaction.empty()); // the commit makes the transaction empty
    };

    auto const start_time = std::chrono::high_resolution_clock::now();

    auto consume_all_msgs = [&queue, &start_time] {
        while (auto consume = queue.try_start_consume())
        {
            auto const checksum = compute_checksum(consume.element<Msg>().m_data, consume.element<Msg>().m_len);
            std::cout << "Message with checksum " << checksum << " at ";
            std::cout << (consume.element<Msg>().m_time - start_time).count() << std::endl;
            consume.commit();
        }
    };

    int msg_1 = 42, msg_2 = 567;
    post_message(&msg_1, sizeof(msg_1));
    post_message(&msg_2, sizeof(msg_2));

    consume_all_msgs();
    //! [heter_queue put_transaction raw_allocate example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue put_transaction raw_allocate_copy example 1]
    struct Msg
    {
        size_t m_len = 0;
        char * m_chars = nullptr;
    };
    auto post_message = [&queue](const char * i_data, size_t i_len) {
        auto transaction = queue.start_emplace<Msg>();
        transaction.element().m_len = i_len;
        transaction.element().m_chars = transaction.raw_allocate_copy(i_data, i_data + i_len);
        memcpy(transaction.element().m_chars, i_data, i_len);
        transaction.commit();
    };
    //! [heter_queue put_transaction raw_allocate_copy example 1]
    (void)post_message;
}
{
    heter_queue<> queue;
    //! [heter_queue put_transaction raw_allocate_copy example 2]
    struct Msg
    {
        char * m_chars = nullptr;
    };
    auto post_message = [&queue](const std::string & i_string) {
        auto transaction = queue.start_emplace<Msg>();
        transaction.element().m_chars = transaction.raw_allocate_copy(i_string);
        transaction.commit();
    };
    //! [heter_queue put_transaction raw_allocate_copy example 2]
    (void)post_message;
}
{
    heter_queue<> queue;
    //! [heter_queue put_transaction empty example 1]
    heter_queue<>::put_transaction<> transaction;
    assert(transaction.empty());

    transaction = queue.start_push(1);
    assert(!transaction.empty());
    //! [heter_queue put_transaction empty example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue put_transaction swap example 1]
    auto transaction_1 = queue.start_push(1);
    assert(!transaction_1.empty());
    heter_queue<>::put_transaction<int> transaction_2;
    swap(transaction_1, transaction_2);
    assert(transaction_1.empty());
    //! [heter_queue put_transaction swap example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue put_transaction operator_bool example 1]
    heter_queue<>::put_transaction<> transaction;
    assert(!transaction);

    transaction = queue.start_push(1);
    assert(transaction);
    //! [heter_queue put_transaction operator_bool example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue put_transaction queue example 1]
    heter_queue<>::put_transaction<> transaction;
    assert(transaction.queue() == nullptr);

    transaction = queue.start_push(1);
    assert(transaction.queue() == &queue);
    //! [heter_queue put_transaction queue example 1]
}
{
    //! [heter_queue put_transaction cancel example 1]
    heter_queue<> queue;

    // start and cancel a put
    assert(queue.empty());
    auto put = queue.start_push(42);
    /* assert(queue.empty()); <- this assert would trigger an undefined behavior, because it would access
        the queue during a non-reentrant put transaction. */
    assert(!put.empty());
    put.cancel();
    assert(queue.empty() && put.empty());

    // start and commit a put
    put = queue.start_push(42);
    put.commit();
    assert(std::distance(queue.cbegin(), queue.cend()) == 1);
    //! [heter_queue put_transaction cancel example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue put_transaction element_ptr example 1]
    int value = 42;
    auto put = queue.start_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);
    assert(*static_cast<int*>(put.element_ptr()) == 42);
    std::cout << "Putting an " << put.complete_type().type_info().name() << "..." << std::endl;

    //! [heter_queue put_transaction element_ptr example 1]

    //! [heter_queue put_transaction element_ptr example 2]
    auto put_1 = queue.start_push(1);
    assert(*static_cast<int*>(put_1.element_ptr()) == 1); // this is fine
    assert(put_1.element() == 1); // this is better
    //! [heter_queue put_transaction element_ptr example 2]
}
{
    heter_queue<> queue;
    //! [heter_queue put_transaction complete_type example 1]
    int value = 42;
    auto put = queue.start_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);
    assert(put.complete_type().is<int>());
    std::cout << "Putting an " << put.complete_type().type_info().name() << "..." << std::endl;
    //! [heter_queue put_transaction complete_type example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue put_transaction destroy example 1]
    queue.start_push(42); /* this transaction is destroyed without being committed,
                            so it gets canceled automatically. */
    //! [heter_queue put_transaction destroy example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue typed_put_transaction element example 1]

    int value = 42;
    auto untyped_put = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);

    auto typed_put = queue.start_reentrant_push(42.);

    /* typed_put = std::move(untyped_put); <- this would not compile: can't assign an untyped
        transaction to a typed transaction */

    assert(typed_put.element() == 42.);

    //! [heter_queue typed_put_transaction element example 1]
}

}

void heterogeneous_queue_consume_operation_samples()
{
    using namespace density;
    using namespace type_features;
    {
        
//! [heter_queue consume_operation consume_1 example 1]
    heter_queue<> queue;
    queue.push(42);
    queue.emplace<std::string>("Hello world!");
    queue.push(42.);

    while (auto consume = queue.try_start_consume())
    {
        if(consume.complete_type().is<int>())
            std::cout << "Found an int: " << consume.element<int>() << std::endl;
        else if(consume.complete_type().is<std::string>())
            std::cout << "Found a string: " << consume.element<std::string>() << std::endl;
        consume.commit();
    }
//! [heter_queue consume_operation consume_1 example 1]
}
{
//! [heter_queue consume_operation consume_1 example 2]
    heter_queue<> queue;
    queue.push(42);
    queue.emplace<std::string>("Hello world!");
    queue.push(42.);

    heter_queue<>::consume_operation consume;
    while (queue.try_start_consume(consume))
    {
        if(consume.complete_type().is<int>())
            std::cout << "Found an int: " << consume.element<int>() << std::endl;
        else if(consume.complete_type().is<std::string>())
            std::cout << "Found a string: " << consume.element<std::string>() << std::endl;
        consume.commit();
    }
//! [heter_queue consume_operation consume_1 example 2]
    }

    {
        heter_queue<> queue;
//! [heter_queue consume_operation default_construct example 1]
        heter_queue<>::consume_operation consume;
        assert(consume.empty());
//! [heter_queue consume_operation default_construct example 1]
    }

    //! [heter_queue consume_operation copy_construct example 1]
    static_assert(!std::is_copy_constructible<heter_queue<>::consume_operation>::value, "");
    //! [heter_queue consume_operation copy_construct example 1]

    //! [heter_queue consume_operation copy_assign example 1]
    static_assert(!std::is_copy_assignable<heter_queue<>::consume_operation>::value, "");
    //! [heter_queue consume_operation copy_assign example 1]

{
    //! [heter_queue consume_operation move_construct example 1]
    heter_queue<> queue;

    queue.push(42);
    auto consume = queue.try_start_consume();

    auto consume_1 = std::move(consume);
    assert(consume.empty() && !consume_1.empty());
    consume_1.commit();
    //! [heter_queue consume_operation move_construct example 1]
}
{
    //! [heter_queue consume_operation move_assign example 1]
    heter_queue<> queue;

    queue.push(42);
    queue.push(43);
    auto consume = queue.try_start_consume();

    heter_queue<>::consume_operation consume_1;
    consume_1 = std::move(consume);
    assert(consume.empty() && !consume_1.empty());
    consume_1.commit();
    //! [heter_queue consume_operation move_assign example 1]
}
{
    //! [heter_queue consume_operation destroy example 1]
    heter_queue<> queue;
    queue.push(42);

    // this consumed is started and destroyed before being committed, so it has no observable effects
    queue.try_start_consume();
    //! [heter_queue consume_operation destroy example 1]
}
{
    //! [heter_queue consume_operation empty example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::consume_operation consume;
    assert(consume.empty());
    consume = queue.try_start_consume();
    assert(!consume.empty());
    //! [heter_queue consume_operation empty example 1]
}
{
    //! [heter_queue consume_operation operator_bool example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::consume_operation consume;
    assert(consume.empty() == !consume);
    consume = queue.try_start_consume();
    assert(consume.empty() == !consume);
    //! [heter_queue consume_operation operator_bool example 1]
}
{
    //! [heter_queue consume_operation queue example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::consume_operation consume;
    assert(consume.empty() && !consume && consume.queue() == nullptr);
    consume = queue.try_start_consume();
    assert(!consume.empty() && !!consume && consume.queue() == &queue);
    //! [heter_queue consume_operation queue example 1]
}
{
    //! [heter_queue consume_operation commit_nodestroy example 1]
    heter_queue<> queue;
    queue.emplace<std::string>("abc");

    heter_queue<>::consume_operation consume = queue.try_start_consume();
    consume.complete_type().destroy(consume.element_ptr());

    // the string has already been destroyed. Calling commit would trigger an undefined behavior
    consume.commit_nodestroy();
    //! [heter_queue consume_operation commit_nodestroy example 1]
}
{
    //! [heter_queue consume_operation cancel example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::consume_operation consume = queue.try_start_consume();
    consume.cancel();

    // there is still a 42 in the queue
    assert(std::distance(queue.cbegin(), queue.cend()) == 1);
    //! [heter_queue consume_operation cancel example 1]
}
{
    //! [heter_queue consume_operation complete_type example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::consume_operation consume = queue.try_start_consume();
    assert(consume.complete_type().is<int>());
    assert(consume.complete_type() == runtime_type<>::make<int>()); // same to the previous assert
    consume.commit();

    assert(std::distance(queue.cbegin(), queue.cend()) == 0);
    //! [heter_queue consume_operation complete_type example 1]
}
{
    //! [heter_queue consume_operation element_ptr example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::consume_operation consume = queue.try_start_consume();
    ++*static_cast<int*>(consume.element_ptr());
    assert(consume.element<int>() == 43);
    consume.commit();
    //! [heter_queue consume_operation element_ptr example 1]
}
{
    //! [heter_queue consume_operation swap example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::consume_operation consume_1 = queue.try_start_consume();
    heter_queue<>::consume_operation consume_2;
    {
        using namespace std;
        swap(consume_1, consume_2);
    }
    assert(consume_2.complete_type().is<int>());
    assert(consume_2.complete_type() == runtime_type<>::make<int>()); // same to the previous assert
    assert(consume_2.element<int>() == 42);
    consume_2.commit();

    assert(std::distance(queue.cbegin(), queue.cend()) == 0);
    //! [heter_queue consume_operation swap example 1]
}
{
    //! [heter_queue consume_operation unaligned_element_ptr example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::consume_operation consume = queue.try_start_consume();
    bool const is_overaligned = alignof(int) > heter_queue<>::min_alignment;
    void * const unaligned_ptr = consume.unaligned_element_ptr();
    int * element_ptr;
    if (is_overaligned)
    {
        element_ptr = static_cast<int*>(address_upper_align(unaligned_ptr, alignof(int)));
    }
    else
    {
        assert(unaligned_ptr == consume.element_ptr());
        element_ptr = static_cast<int*>(unaligned_ptr);
    }
    assert(address_is_aligned(element_ptr, alignof(int)));
    std::cout << "An int: " << *element_ptr << std::endl;
    consume.commit();
    //! [heter_queue consume_operation unaligned_element_ptr example 1]
}
{
    //! [heter_queue consume_operation element example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::consume_operation consume = queue.try_start_consume();
    assert(consume.complete_type().is<int>());
    std::cout << "An int: " << consume.element<int>() << std::endl;
    /* std::cout << "An float: " << consume.element<float>() << std::endl; this would
        trigger an undefined behavior, because the element is not a float */
    consume.commit();
    //! [heter_queue consume_operation element example 1]
}
}

void heterogeneous_queue_reentrant_put_samples()
{
    using namespace density;
{
    heter_queue<> queue;

    //! [heter_queue reentrant put example 1]
    auto put_1 = queue.start_reentrant_push(12);
    auto put_2 = queue.start_reentrant_emplace<std::string>("Hello ");
    auto put_3 = queue.start_reentrant_push(3.14f);
    put_3.commit();
    put_1.cancel();
    put_2.element() += "world!!!!";
    put_2.commit();
    //! [heter_queue reentrant put example 1]
}
{
    

    //! [heter_queue transfer]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
    
    heter_queue<void, MyRunTimeType> queue_1;
    queue_1.emplace<std::string>("Hello!");
    
    heter_queue<void, MyRunTimeType> queue_2;
    auto consume = queue_1.try_start_consume();
    queue_2.dyn_push_move(consume.complete_type(), consume.element_ptr());
    consume.commit();

    consume = queue_2.try_start_consume();
    assert(consume && consume.complete_type().is<std::string>());
    assert(consume.element<std::string>() == "Hello!");
    consume.commit();

    assert(queue_1.empty() && queue_2.empty());
    //! [heter_queue transfer]
}
{
    heter_queue<> queue;

    //! [heter_queue reentrant_push example 1]
    queue.reentrant_push(12);
    queue.reentrant_push(std::string("Hello world!!"));
    //! [heter_queue reentrant_push example 1]

    //! [heter_queue reentrant_emplace example 1]
    queue.reentrant_emplace<int>();
    queue.reentrant_emplace<std::string>(12, '-');
    //! [heter_queue reentrant_emplace example 1]

{
    //! [heter_queue start_reentrant_push example 1]
    auto put = queue.start_reentrant_push(12);
    put.element() += 2;
    put.commit(); // commits a 14
    //! [heter_queue start_reentrant_push example 1]
}
{
    //! [heter_queue start_reentrant_emplace example 1]
    auto put = queue.start_reentrant_emplace<std::string>(4, '*');
    put.element() += "****";
    put.commit(); // commits a "********"
    //! [heter_queue start_reentrant_emplace example 1]
}
}
{
    //! [heter_queue reentrant_dyn_push example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    auto const type = MyRunTimeType::make<int>();
    queue.reentrant_dyn_push(type); // appends 0
    //! [heter_queue reentrant_dyn_push example 1]
}
{
    //! [heter_queue reentrant_dyn_push_copy example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    std::string const source("Hello world!!");
    auto const type = MyRunTimeType::make<decltype(source)>();
    queue.reentrant_dyn_push_copy(type, &source);
    //! [heter_queue reentrant_dyn_push_copy example 1]
}
{
    //! [heter_queue reentrant_dyn_push_move example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    std::string source("Hello world!!");
    auto const type = MyRunTimeType::make<decltype(source)>();
    queue.reentrant_dyn_push_move(type, &source);
    //! [heter_queue reentrant_dyn_push_move example 1]
}

{
    //! [heter_queue start_reentrant_dyn_push example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<default_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    auto const type = MyRunTimeType::make<int>();
    auto put = queue.start_reentrant_dyn_push(type);
    put.commit();
    //! [heter_queue start_reentrant_dyn_push example 1]
}
{
    //! [heter_queue start_reentrant_dyn_push_copy example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<copy_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    std::string const source("Hello world!!");
    auto const type = MyRunTimeType::make<decltype(source)>();
    auto put = queue.start_reentrant_dyn_push_copy(type, &source);
    put.commit();
    //! [heter_queue start_reentrant_dyn_push_copy example 1]
}
{
    //! [heter_queue start_reentrant_dyn_push_move example 1]
    using namespace type_features;
    using MyRunTimeType = runtime_type<void, feature_list<move_construct, destroy, size, alignment>>;
    heter_queue<void, MyRunTimeType> queue;

    std::string source("Hello world!!");
    auto const type = MyRunTimeType::make<decltype(source)>();
    auto put = queue.start_reentrant_dyn_push_move(type, &source);
    put.commit();
    //! [heter_queue start_reentrant_dyn_push_move example 1]
}
}

void heterogeneous_queue_reentrant_put_transaction_samples()
{
    using namespace density;
    using namespace type_features;
{
    //! [heter_queue reentrant_put_transaction default_construct example 1]
    heter_queue<>::reentrant_put_transaction<> transaction;
    assert(transaction.empty());
    //! [heter_queue reentrant_put_transaction default_construct example 1]
}
{
    //! [heter_queue reentrant_put_transaction copy_construct example 1]
    static_assert(!std::is_copy_constructible<heter_queue<>::reentrant_put_transaction<>>::value, "");
    static_assert(!std::is_copy_constructible<heter_queue<int>::reentrant_put_transaction<>>::value, "");
    //! [heter_queue reentrant_put_transaction copy_construct example 1]
}
{
    //! [heter_queue reentrant_put_transaction copy_assign example 1]
    static_assert(!std::is_copy_assignable<heter_queue<>::reentrant_put_transaction<>>::value, "");
    static_assert(!std::is_copy_assignable<heter_queue<int>::reentrant_put_transaction<>>::value, "");
    //! [heter_queue reentrant_put_transaction copy_assign example 1]
}
{
    //! [heter_queue reentrant_put_transaction move_construct example 1]
    heter_queue<> queue;
    auto transaction1 = queue.start_reentrant_push(1);

    // move from transaction1 to transaction2
    auto transaction2(std::move(transaction1));
    assert(transaction1.empty());
    assert(transaction2.element() == 1);

    // commit transaction2
    transaction2.commit();
    assert(transaction2.empty());

    //! [heter_queue reentrant_put_transaction move_construct example 1]

    //! [heter_queue reentrant_put_transaction move_construct example 2]
    // reentrant_put_transaction<void> can be move constructed from any reentrant_put_transaction<T>
    static_assert(std::is_constructible<heter_queue<>::reentrant_put_transaction<void>, heter_queue<>::reentrant_put_transaction<void> &&>::value, "");
    static_assert(std::is_constructible<heter_queue<>::reentrant_put_transaction<void>, heter_queue<>::reentrant_put_transaction<int> &&>::value, "");

    // reentrant_put_transaction<T> can be move constructed only from reentrant_put_transaction<T>
    static_assert(!std::is_constructible<heter_queue<>::reentrant_put_transaction<int>, heter_queue<>::reentrant_put_transaction<void> &&>::value, "");
    static_assert(!std::is_constructible<heter_queue<>::reentrant_put_transaction<int>, heter_queue<>::reentrant_put_transaction<float> &&>::value, "");
    static_assert(std::is_constructible<heter_queue<>::reentrant_put_transaction<int>, heter_queue<>::reentrant_put_transaction<int> &&>::value, "");
    //! [heter_queue reentrant_put_transaction move_construct example 2]
}
{
    //! [heter_queue reentrant_put_transaction move_assign example 1]
    heter_queue<> queue;
    auto transaction1 = queue.start_reentrant_push(1);

    heter_queue<>::reentrant_put_transaction<> transaction2;
    transaction2 = queue.start_reentrant_push(1);
    transaction2 = std::move(transaction1);
    assert(transaction1.empty());
    transaction2.commit();
    assert(transaction2.empty());
    //! [heter_queue reentrant_put_transaction move_assign example 1]

    //! [heter_queue reentrant_put_transaction move_assign example 2]
    // reentrant_put_transaction<void> can be move assigned from any reentrant_put_transaction<T>
    static_assert(std::is_assignable<heter_queue<>::reentrant_put_transaction<void>, heter_queue<>::reentrant_put_transaction<void> &&>::value, "");
    static_assert(std::is_assignable<heter_queue<>::reentrant_put_transaction<void>, heter_queue<>::reentrant_put_transaction<int> &&>::value, "");

    // reentrant_put_transaction<T> can be move assigned only from reentrant_put_transaction<T>
    static_assert(!std::is_assignable<heter_queue<>::reentrant_put_transaction<int>, heter_queue<>::reentrant_put_transaction<void> &&>::value, "");
    static_assert(!std::is_assignable<heter_queue<>::reentrant_put_transaction<int>, heter_queue<>::reentrant_put_transaction<float> &&>::value, "");
    static_assert(std::is_assignable<heter_queue<>::reentrant_put_transaction<int>, heter_queue<>::reentrant_put_transaction<int> &&>::value, "");
    //! [heter_queue reentrant_put_transaction move_assign example 2]

}
{
    //! [heter_queue reentrant_put_transaction raw_allocate example 1]
    heter_queue<> queue;
    struct Msg
    {
        std::chrono::high_resolution_clock::time_point m_time = std::chrono::high_resolution_clock::now();
        size_t m_len = 0;
        void * m_data = nullptr;
    };

    auto post_message = [&queue](const void * i_data, size_t i_len) {
        auto transaction = queue.start_reentrant_emplace<Msg>();
        transaction.element().m_len = i_len;
        transaction.element().m_data = transaction.raw_allocate(i_len, 1);
        memcpy(transaction.element().m_data, i_data, i_len);

        assert(!transaction.empty()); // a put transaction is not empty if it's bound to an element being put
        transaction.commit();
        assert(transaction.empty()); // the commit makes the transaction empty
    };

    auto const start_time = std::chrono::high_resolution_clock::now();

    auto consume_all_msgs = [&queue, &start_time] {
        while (auto consume = queue.try_start_reentrant_consume())
        {
            auto const checksum = compute_checksum(consume.element<Msg>().m_data, consume.element<Msg>().m_len);
            std::cout << "Message with checksum " << checksum << " at ";
            std::cout << (consume.element<Msg>().m_time - start_time).count() << std::endl;
            consume.commit();
        }
    };

    int msg_1 = 42, msg_2 = 567;
    post_message(&msg_1, sizeof(msg_1));
    post_message(&msg_2, sizeof(msg_2));

    consume_all_msgs();
    //! [heter_queue reentrant_put_transaction raw_allocate example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue reentrant_put_transaction raw_allocate_copy example 1]
    struct Msg
    {
        size_t m_len = 0;
        char * m_chars = nullptr;
    };
    auto post_message = [&queue](const char * i_data, size_t i_len) {
        auto transaction = queue.start_reentrant_emplace<Msg>();
        transaction.element().m_len = i_len;
        transaction.element().m_chars = transaction.raw_allocate_copy(i_data, i_data + i_len);
        memcpy(transaction.element().m_chars, i_data, i_len);
        transaction.commit();
    };
    //! [heter_queue reentrant_put_transaction raw_allocate_copy example 1]
    (void)post_message;
}
{
    heter_queue<> queue;
    //! [heter_queue reentrant_put_transaction raw_allocate_copy example 2]
    struct Msg
    {
        char * m_chars = nullptr;
    };
    auto post_message = [&queue](const std::string & i_string) {
        auto transaction = queue.start_reentrant_emplace<Msg>();
        transaction.element().m_chars = transaction.raw_allocate_copy(i_string);
        transaction.commit();
    };
    //! [heter_queue reentrant_put_transaction raw_allocate_copy example 2]
    (void)post_message;
}
{
    heter_queue<> queue;
    //! [heter_queue reentrant_put_transaction empty example 1]
    heter_queue<>::reentrant_put_transaction<> transaction;
    assert(transaction.empty());

    transaction = queue.start_reentrant_push(1);
    assert(!transaction.empty());
    //! [heter_queue reentrant_put_transaction empty example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue reentrant_put_transaction operator_bool example 1]
    heter_queue<>::reentrant_put_transaction<> transaction;
    assert(!transaction);

    transaction = queue.start_reentrant_push(1);
    assert(transaction);
    //! [heter_queue reentrant_put_transaction operator_bool example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue reentrant_put_transaction queue example 1]
    heter_queue<>::reentrant_put_transaction<> transaction;
    assert(transaction.queue() == nullptr);

    transaction = queue.start_reentrant_push(1);
    assert(transaction.queue() == &queue);
    //! [heter_queue reentrant_put_transaction queue example 1]
}
{
    //! [heter_queue reentrant_put_transaction cancel example 1]
    heter_queue<> queue;

    // start and cancel a put
    assert(queue.empty());
    auto put = queue.start_reentrant_push(42);
    /* assert(queue.empty()); <- this assert would trigger an undefined behavior, because it would access
        the queue during a non-reentrant put transaction. */
    assert(!put.empty());
    put.cancel();
    assert(queue.empty() && put.empty());

    // start and commit a put
    put = queue.start_reentrant_push(42);
    put.commit();
    assert(std::distance(queue.cbegin(), queue.cend()) == 1);
    //! [heter_queue reentrant_put_transaction cancel example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue reentrant_put_transaction element_ptr example 1]
    int value = 42;
    auto put = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);
    assert(*static_cast<int*>(put.element_ptr()) == 42);
    std::cout << "Putting an " << put.complete_type().type_info().name() << "..." << std::endl;

    //! [heter_queue reentrant_put_transaction element_ptr example 1]

    //! [heter_queue reentrant_put_transaction element_ptr example 2]
    auto put_1 = queue.start_reentrant_push(1);
    assert(*static_cast<int*>(put_1.element_ptr()) == 1); // this is fine
    assert(put_1.element() == 1); // this is better
    //! [heter_queue reentrant_put_transaction element_ptr example 2]
}
{
    heter_queue<> queue;
    //! [heter_queue reentrant_put_transaction complete_type example 1]
    int value = 42;
    auto put = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);
    assert(put.complete_type().is<int>());
    std::cout << "Putting an " << put.complete_type().type_info().name() << "..." << std::endl;
    //! [heter_queue reentrant_put_transaction complete_type example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue reentrant_put_transaction destroy example 1]
    queue.start_reentrant_push(42);/* this transaction is destroyed without being committed,
                            so it gets canceled automatically. */
    //! [heter_queue reentrant_put_transaction destroy example 1]
}
{
    heter_queue<> queue;
    //! [heter_queue reentrant_typed_put_transaction element example 1]

    int value = 42;
    auto untyped_put = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<decltype(value)>(), &value);

    auto typed_put = queue.start_reentrant_push(42.);

    /* typed_put = std::move(untyped_put); <- this would not compile: can't assign an untyped
        transaction to a typed transaction */

    assert(typed_put.element() == 42.);

    //! [heter_queue reentrant_typed_put_transaction element example 1]
}
}

void heterogeneous_queue_reentrant_consume_operation_samples()
{
    using namespace density;
    using namespace type_features;

    {
        heter_queue<> queue;
//! [heter_queue reentrant_consume_operation default_construct example 1]
        heter_queue<>::reentrant_consume_operation consume;
        assert(consume.empty());
//! [heter_queue reentrant_consume_operation default_construct example 1]
    }

    //! [heter_queue reentrant_consume_operation copy_construct example 1]
    static_assert(!std::is_copy_constructible<heter_queue<>::reentrant_consume_operation>::value, "");
    //! [heter_queue reentrant_consume_operation copy_construct example 1]

    //! [heter_queue reentrant_consume_operation copy_assign example 1]
    static_assert(!std::is_copy_assignable<heter_queue<>::reentrant_consume_operation>::value, "");
    //! [heter_queue reentrant_consume_operation copy_assign example 1]

{
    //! [heter_queue reentrant_consume_operation move_construct example 1]
    heter_queue<> queue;

    queue.push(42);
    auto consume = queue.try_start_reentrant_consume();

    auto consume_1 = std::move(consume);
    assert(consume.empty() && !consume_1.empty());
    consume_1.commit();
    //! [heter_queue reentrant_consume_operation move_construct example 1]
}
{
    //! [heter_queue reentrant_consume_operation move_assign example 1]
    heter_queue<> queue;

    queue.push(42);
    queue.push(43);
    auto consume = queue.try_start_reentrant_consume();

    heter_queue<>::reentrant_consume_operation consume_1;
    consume = queue.try_start_reentrant_consume();
    consume_1 = std::move(consume);
    assert(consume.empty() && !consume_1.empty());
    consume_1.commit();
    //! [heter_queue reentrant_consume_operation move_assign example 1]
}
{
    //! [heter_queue reentrant_consume_operation destroy example 1]
    heter_queue<> queue;
    queue.push(42);

    // this consumed is started and destroyed before being committed, so it has no observable effects
    queue.try_start_reentrant_consume();
    //! [heter_queue reentrant_consume_operation destroy example 1]
}
{
    //! [heter_queue reentrant_consume_operation empty example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume;
    assert(consume.empty());
    consume = queue.try_start_reentrant_consume();
    assert(!consume.empty());
    //! [heter_queue reentrant_consume_operation empty example 1]
}
{
    //! [heter_queue reentrant_consume_operation operator_bool example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume;
    assert(consume.empty() == !consume);
    consume = queue.try_start_reentrant_consume();
    assert(consume.empty() == !consume);
    //! [heter_queue reentrant_consume_operation operator_bool example 1]
}
{
    //! [heter_queue reentrant_consume_operation queue example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume;
    assert(consume.empty() && !consume && consume.queue() == nullptr);
    consume = queue.try_start_reentrant_consume();
    assert(!consume.empty() && !!consume && consume.queue() == &queue);
    //! [heter_queue reentrant_consume_operation queue example 1]
}
{
    //! [heter_queue reentrant_consume_operation commit_nodestroy example 1]
    heter_queue<> queue;
    queue.emplace<std::string>("abc");

    heter_queue<>::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
    consume.complete_type().destroy(consume.element_ptr());

    // the string has already been destroyed. Calling commit would trigger an undefined behavior
    consume.commit_nodestroy();
    //! [heter_queue reentrant_consume_operation commit_nodestroy example 1]
}
{
    //! [heter_queue reentrant_consume_operation swap example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume_1 = queue.try_start_reentrant_consume();
    heter_queue<>::reentrant_consume_operation consume_2;
    {
        using namespace std;
        swap(consume_1, consume_2);
    }
    assert(consume_2.complete_type().is<int>());
    assert(consume_2.complete_type() == runtime_type<>::make<int>()); // same to the previous assert
    assert(consume_2.element<int>() == 42);
    consume_2.commit();

    assert(std::distance(queue.cbegin(), queue.cend()) == 0);
    //! [heter_queue reentrant_consume_operation swap example 1]
}
{
    //! [heter_queue reentrant_consume_operation cancel example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
    consume.cancel();

    // there is still a 42 in the queue
    assert(std::distance(queue.cbegin(), queue.cend()) == 1);
    //! [heter_queue reentrant_consume_operation cancel example 1]
}
{
    //! [heter_queue reentrant_consume_operation complete_type example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
    assert(consume.complete_type().is<int>());
    assert(consume.complete_type() == runtime_type<>::make<int>()); // same to the previous assert
    consume.commit();

    assert(std::distance(queue.cbegin(), queue.cend()) == 0);
    //! [heter_queue reentrant_consume_operation complete_type example 1]
}
{
    //! [heter_queue reentrant_consume_operation element_ptr example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
    ++*static_cast<int*>(consume.element_ptr());
    assert(consume.element<int>() == 43);
    consume.commit();
    //! [heter_queue reentrant_consume_operation element_ptr example 1]
}
{
    //! [heter_queue reentrant_consume_operation unaligned_element_ptr example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
    bool const is_overaligned = alignof(int) > heter_queue<>::min_alignment;
    void * const unaligned_ptr = consume.unaligned_element_ptr();
    int * element_ptr;
    if (is_overaligned)
    {
        element_ptr = static_cast<int*>(address_upper_align(unaligned_ptr, alignof(int)));
    }
    else
    {
        assert(unaligned_ptr == consume.element_ptr());
        element_ptr = static_cast<int*>(unaligned_ptr);
    }
    assert(address_is_aligned(element_ptr, alignof(int)));
    std::cout << "An int: " << *element_ptr << std::endl;
    consume.commit();
    //! [heter_queue reentrant_consume_operation unaligned_element_ptr example 1]
}
{
    //! [heter_queue reentrant_consume_operation element example 1]
    heter_queue<> queue;
    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume = queue.try_start_reentrant_consume();
    assert(consume.complete_type().is<int>());
    std::cout << "An int: " << consume.element<int>() << std::endl;
    /* std::cout << "An float: " << consume.element<float>() << std::endl; this would
        trigger an undefined behavior, because the element is not a float */
    consume.commit();
    //! [heter_queue reentrant_consume_operation element example 1]
}
}

void heterogeneous_queue_samples_1()
{
    //! [heter_queue example 3]
    using namespace density;
    using namespace type_features;

    /* a runtime_type is internally like a pointer to a v-table, but it can
        contain functions or data (like in the case of size and alignment). */
    using MyRunTimeType = runtime_type<void, feature_list<default_construct, copy_construct, destroy, size, alignment, ostream, istream, rtti>>;

    heter_queue<void, MyRunTimeType> queue;
    queue.push(4);
    queue.push(std::complex<double>(1., 4.));
    queue.emplace<std::string>("Hello!!");
    // queue.emplace<std::thread>(); - This would not compile because std::thread does not have a << operator for streams

    // consume all the elements
    while (auto consume = queue.try_start_consume())
    {
        /* this is like: give me the function at the 6-th row in the v-table. The type ostream
            is converted to an index at compile time. */
        auto const ostream_feature = consume.complete_type().get_feature<ostream>();

        ostream_feature(std::cout, consume.element_ptr()); // this invokes the feature
        std::cout << "\n";
        consume.commit();  // don't forget the commit, otherwise the element will remain in the queue
    }
    //! [heter_queue example 3]

    //! [heter_queue example 4]
    // this local function reads from std::cin an object of a given type and puts it in the queue
    auto ask_and_put = [&](const MyRunTimeType & i_type) {

        // for this we exploit the feature rtti that we have included in MyRunTimeType
        std::cout << "Enter a " << i_type.type_info().name() << std::endl;

        auto const istream_feature = i_type.get_feature<istream>();

        auto put = queue.start_dyn_push(i_type);
        istream_feature(std::cin, put.element_ptr());

        /* if an exception is thrown before the commit, the put is canceled without ever
            having observable side effects. */
        put.commit();
    };

    ask_and_put(MyRunTimeType::make<int>());
    ask_and_put(MyRunTimeType::make<std::string>());
    //! [heter_queue example 4]
}


void heterogeneous_queue_samples(std::ostream & i_ostream)
{
    PrintScopeDuration dur(i_ostream, "heterogeneous queue samples");

    using namespace density;
    using namespace type_features;

{
    //! [heter_queue put example 1]
    heter_queue<> queue;
    queue.push(19); // the parameter can be an l-value or an r-value
    queue.emplace<std::string>(8, '*'); // pushes "********"
    //! [heter_queue put example 1]

    //! [heter_queue example 2]
    auto consume = queue.try_start_consume();
    int my_int = consume.element<int>();
    consume.commit();

    consume = queue.try_start_consume();
    std::string my_string = consume.element<std::string>();
    consume.commit();
    //! [heter_queue example 3
    (void)my_int;
    (void)my_string;
}

{
    //! [heter_queue put example 2]
    heter_queue<> queue;
    struct MessageInABottle
    {
        const char * m_text = nullptr;
    };
    auto transaction = queue.start_emplace<MessageInABottle>();
    transaction.element().m_text = transaction.raw_allocate_copy("Hello world!");
    transaction.commit();
    //! [heter_queue put example 2]

    //! [heter_queue consume example 1]
    auto consume = queue.try_start_consume();
    if (consume.complete_type().is<std::string>())
    {
        std::cout << consume.element<std::string>() << std::endl;
    }
    else if (consume.complete_type().is<MessageInABottle>())
    {
        std::cout << consume.element<MessageInABottle>().m_text << std::endl;
    }
    consume.commit();
    //! [heter_queue consume example 1]

    //! [heter_queue iterators example 1]
    heter_queue<> queue_1, queue_2;
    queue_1.push(42);
    assert(queue_1.end() == queue_2.end() && queue_1.end() == heter_queue<>::const_iterator() );

    for (const auto & value : queue_1)
    {
        assert(value.first.is<int>());
        assert(*static_cast<int*>(value.second) == 42);
        *static_cast<int*>(value.second) = 0;
    }
    //! [heter_queue iterators example 1]
}

{
    //! [heter_queue default_construct example 1]
    heter_queue<> queue;
    assert(queue.empty());
    assert(std::distance(queue.begin(), queue.end()) == 0);
    assert(queue.cbegin() == queue.cend());
    //! [heter_queue default_construct example 1]
}
{
    //! [heter_queue copy_construct example 1]
    using MyRunTimeType = runtime_type<void, feature_list<default_construct, copy_construct, destroy, size, alignment, equals>>;

    heter_queue<void, MyRunTimeType> queue;
    queue.push(std::string());
    queue.push(std::make_pair(4., 1));

    heter_queue<void, MyRunTimeType> queue_1(queue);
    assert(queue == queue_1);
    //! [heter_queue copy_construct example 1]
}
{
    //! [heter_queue move_construct example 1]
    using MyRunTimeType = runtime_type<void, feature_list<default_construct, copy_construct, destroy, size, alignment, equals>>;

    heter_queue<void, MyRunTimeType> queue;
    queue.push(std::string());
    queue.push(std::make_pair(4., 1));

    heter_queue<void, MyRunTimeType> queue_1(std::move(queue));

    assert(queue.empty());
    assert(std::distance(queue.begin(), queue.end()) == 0);
    assert(queue.cbegin() == queue.cend());

    assert(!queue_1.empty());
    assert(std::distance(queue_1.begin(), queue_1.end()) == 2);
    assert(queue_1.cbegin() != queue_1.cend());
    //! [heter_queue move_construct example 1]
}
{
    //! [heter_queue construct_copy_alloc example 1]
    default_allocator allocator;
    heter_queue<> queue(allocator);
    //! [heter_queue construct_copy_alloc example 1]
}
{
    //! [heter_queue construct_move_alloc example 1]
    default_allocator allocator;
    heter_queue<> queue(std::move(allocator));
    //! [heter_queue construct_move_alloc example 1]
}
{
    //! [heter_queue copy_assign example 1]
    using MyRunTimeType = runtime_type<void, feature_list<default_construct, copy_construct, destroy, size, alignment, equals>>;

    heter_queue<void, MyRunTimeType> queue;
    queue.push(std::string());
    queue.push(std::make_pair(4., 1));

    heter_queue<void, MyRunTimeType> queue_1;
    queue_1 = queue;
    assert(queue == queue_1);
    //! [heter_queue copy_assign example 1]
}
{
    //! [heter_queue move_assign example 1]
    using MyRunTimeType = runtime_type<void, feature_list<default_construct, copy_construct, destroy, size, alignment, equals>>;

    heter_queue<void, MyRunTimeType> queue;
    queue.push(std::string());
    queue.push(std::make_pair(4., 1));

    heter_queue<void, MyRunTimeType> queue_1;
    queue_1 = std::move(queue);

    assert(queue.empty());
    assert(std::distance(queue.begin(), queue.end()) == 0);
    assert(queue.cbegin() == queue.cend());

    assert(!queue_1.empty());
    assert(std::distance(queue_1.begin(), queue_1.end()) == 2);
    assert(queue_1.cbegin() != queue_1.cend());
    //! [heter_queue move_assign example 1]
}
{
    //! [heter_queue get_allocator example 1]
    heter_queue<> queue;
    assert(queue.get_allocator() == default_allocator());
    //! [heter_queue get_allocator example 1]
}
{
    //! [heter_queue get_allocator_ref example 1]
    heter_queue<> queue;
    assert(queue.get_allocator_ref() == default_allocator());
    //! [heter_queue get_allocator_ref example 1]
}
{
    //! [heter_queue get_allocator_ref example 2]
    heter_queue<> queue;
    auto const & queue_ref = queue;
    assert(queue_ref.get_allocator_ref() == default_allocator());
    //! [heter_queue get_allocator_ref example 2]
    (void)queue_ref;
}
{
    //! [heter_queue swap example 1]
    heter_queue<> queue, queue_1;
    queue.push(1);
    swap(queue, queue_1);

    assert(queue.empty());
    assert(std::distance(queue.begin(), queue.end()) == 0);
    assert(queue.cbegin() == queue.cend());

    assert(!queue_1.empty());
    assert(std::distance(queue_1.begin(), queue_1.end()) == 1);
    assert(queue_1.cbegin() != queue_1.cend());
    //! [heter_queue swap example 1]
}
{
    //! [heter_queue swap example 2]
    heter_queue<> queue, queue_1;
    queue.push(1);
    {
        using namespace std;
        swap(queue, queue_1);
    }
    assert(queue.empty());
    assert(std::distance(queue.begin(), queue.end()) == 0);
    assert(queue.cbegin() == queue.cend());

    assert(!queue_1.empty());
    assert(std::distance(queue_1.begin(), queue_1.end()) == 1);
    assert(queue_1.cbegin() != queue_1.cend());
    //! [heter_queue swap example 2]
}
{
    //! [heter_queue empty example 1]
    heter_queue<> queue;
    assert(queue.empty());
    queue.push(1);
    assert(!queue.empty());
    //! [heter_queue empty example 1]
}
{
    //! [heter_queue clear example 1]
    heter_queue<> queue;
    queue.push(1);
    queue.clear();
    assert(queue.empty());
    //! [heter_queue clear example 1]
}
{
    //! [heter_queue pop example 1]
    heter_queue<> queue;
    queue.push(1);
    queue.push(2);

    queue.pop();
    auto consume = queue.try_start_consume();
    assert(consume.element<int>() == 2);
    consume.commit();
    //! [heter_queue pop example 1]
}
{
    //! [heter_queue try_pop example 1]
    heter_queue<> queue;

    bool pop_result = queue.try_pop();
    assert(pop_result == false);

    queue.push(1);
    queue.push(2);

    pop_result = queue.try_pop();
    assert(pop_result == true);
    auto consume = queue.try_start_consume();
    assert(consume.element<int>() == 2);
    consume.commit();
    //! [heter_queue try_pop example 1]
    (void)pop_result;
}
{
    //! [heter_queue try_start_consume example 1]
    heter_queue<> queue;

    auto consume_1 = queue.try_start_consume();
    assert(!consume_1);

    queue.push(42);

    auto consume_2 = queue.try_start_consume();
    assert(consume_2);
    assert(consume_2.element<int>() == 42);
    consume_2.commit();
    //! [heter_queue try_start_consume example 1]
}
{
    //! [heter_queue try_start_consume_ example 1]
    heter_queue<> queue;

    heter_queue<>::consume_operation consume_1;
    bool const bool_1 = queue.try_start_consume(consume_1);
    assert(!bool_1 && !consume_1);

    queue.push(42);

    heter_queue<>::consume_operation consume_2;
    auto bool_2 = queue.try_start_consume(consume_2);
    assert(consume_2 && bool_2);
    assert(consume_2.element<int>() == 42);
    consume_2.commit();
    //! [heter_queue try_start_consume_ example 1]
    (void)bool_1;
    (void)bool_2;
}
{
    heter_queue<> queue;

    //! [heter_queue reentrant example 1]
    // start 3 reentrant put transactions
    auto put_1 = queue.start_reentrant_push(1);
    auto put_2 = queue.start_reentrant_emplace<std::string>("Hello world!");
    double pi = 3.14;
    auto put_3 = queue.start_reentrant_dyn_push_copy(runtime_type<>::make<double>(), &pi);
    assert(queue.empty()); // the queue is still empty, because no transaction has been committed

    // commit and start consuming "Hello world!"
    put_2.commit();
    auto consume2 = queue.try_start_reentrant_consume();
    assert(!consume2.empty() && consume2.complete_type().is<std::string>());

    // commit and start consuming 1
    put_1.commit();
    auto consume1 = queue.try_start_reentrant_consume();
    assert(!consume1.empty() && consume1.complete_type().is<int>());

    // cancel 3.14, and commit the consumes
    put_3.cancel();
    consume1.commit();
    consume2.commit();
    assert(queue.empty());

    //! [heter_queue reentrant example 1]
}
{
    //! [heter_queue reentrant_pop example 1]
    heter_queue<> queue;
    queue.push(1);
    queue.push(2);

    queue.reentrant_pop();
    auto consume = queue.try_start_consume();
    assert(consume.element<int>() == 2);
    consume.commit();
    //! [heter_queue reentrant_pop example 1]
}
{
    //! [heter_queue try_reentrant_pop example 1]
    heter_queue<> queue;

    bool pop_result = queue.try_reentrant_pop();
    assert(pop_result == false);

    queue.push(1);
    queue.push(2);

    pop_result = queue.try_reentrant_pop();
    assert(pop_result == true);
    auto consume = queue.try_start_reentrant_consume();
    assert(consume.element<int>() == 2);
    consume.commit();
    //! [heter_queue try_reentrant_pop example 1]
    (void)pop_result;
}
{
    //! [heter_queue try_start_reentrant_consume example 1]
    heter_queue<> queue;

    auto consume_1 = queue.try_start_reentrant_consume();
    assert(!consume_1);

    queue.push(42);

    auto consume_2 = queue.try_start_reentrant_consume();
    assert(consume_2);
    assert(consume_2.element<int>() == 42);
    consume_2.commit();
    //! [heter_queue try_start_reentrant_consume example 1]
}
{
    //! [heter_queue try_start_reentrant_consume_ example 1]
    heter_queue<> queue;

    heter_queue<>::reentrant_consume_operation consume_1;
    bool const bool_1 = queue.try_start_reentrant_consume(consume_1);
    assert(!bool_1 && !consume_1);

    queue.push(42);

    heter_queue<>::reentrant_consume_operation consume_2;
    auto bool_2 = queue.try_start_reentrant_consume(consume_2);
    assert(consume_2 && bool_2);
    assert(consume_2.element<int>() == 42);
    consume_2.commit();
    //! [heter_queue try_start_reentrant_consume_ example 1]
    (void)bool_1;
    (void)bool_2;
}

    // this samples uses std::cout and std::cin
    // heterogeneous_queue_samples_1();

    heterogeneous_queue_put_samples();

    heterogeneous_queue_put_transaction_samples();

    heterogeneous_queue_consume_operation_samples();

    heterogeneous_queue_reentrant_put_samples();

    heterogeneous_queue_reentrant_put_transaction_samples();

    heterogeneous_queue_reentrant_consume_operation_samples();
}



} // namespace density_tests

#if defined(_MSC_VER) && defined(NDEBUG)
    #pragma warning(pop)
#endif
