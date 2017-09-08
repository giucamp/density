
#include <string>
#include <iostream>
#include <iterator>
#include <complex>
#include <string>
#include <chrono>
#include <vector>
#include <assert.h>
#include <density/function_queue.h>
#include "../density_tests/test_framework/progress.h"

// if assert expands to nothing, some local variable becomes unused
#if defined(_MSC_VER) && defined(NDEBUG)
	#pragma warning(push)
	#pragma warning(disable:4189) // local variable is initialized but not referenced
#endif

namespace density_tests
{

void func_queue_put_samples(std::ostream & i_ostream)
{
	PrintScopeDuration(i_ostream, "function queue put samples");

	using namespace density;

    {
        //! [function_queue push example 1]
    function_queue<void()> queue;
    queue.push( []{ std::cout << "Hello"; } );
    queue.push( []{ std::cout << " world"; } );
    queue.push( []{ std::cout << "!!!"; } );
    queue.push( []{ std::cout << std::endl; } );
    while( queue.try_consume() );
        //! [function_queue push example 1]
    }
    {
        //! [function_queue push example 2]
    double last_val = 1.;

    auto func = [&last_val] { 
        return last_val /= 2.; 
    };

    function_queue<double()> queue;
    for(int i = 0; i < 10; i++)
        queue.push(func);

    while (auto const return_value = queue.try_consume())
        std::cout << *return_value << std::endl;
        //! [function_queue push example 2]
    }
    {
        //! [function_queue emplace example 1]
    /* This local struct is unmovable and uncopyable, so emplace is the only
        option to add it to the queue. Note that operator () returns an int,
        but we add it to a void() function queue. This is ok, as we are just 
        discarding the return value. */
    struct Func
    {
        int const m_value;

        Func(int i_value) : m_value(i_value) {}

        Func(const Func &) = delete;
        Func & operator = (const Func &) = delete;

        int operator () () const
        { 
            std::cout << m_value << std::endl;
            return m_value;
        }
    };

    function_queue<void()> queue;
    queue.emplace<Func>(7);

    bool const invoked = queue.try_consume();
    assert(invoked);
        //! [function_queue emplace example 1]
        (void)invoked;
    }
    {
        //! [function_queue start_push example 1]
    struct Func
    {
        const char * m_string_1;
        const char * m_string_2;
        
        void operator () ()
        {
            std::cout << m_string_1 << std::endl;
            std::cout << m_string_2 << std::endl;
        }
    };

    function_queue<void()> queue;
    auto put = queue.start_push(Func{});
    put.element().m_string_1 = put.raw_allocate_copy("Hello world");
    put.element().m_string_2 = put.raw_allocate_copy("\t(I'm so happy)!!");
    put.commit();

    bool const invoked = queue.try_consume();
    assert(invoked);
        //! [function_queue start_push example 1]
        (void)invoked;
    }
    {
        //! [function_queue start_emplace example 1]
    struct Func
    {
        const char * m_string_1;
        const char * m_string_2;
        
        void operator () ()
        {
            std::cout << m_string_1 << std::endl;
            std::cout << m_string_2 << std::endl;
        }
    };

    function_queue<void()> queue;
    auto put = queue.start_emplace<Func>();
    put.element().m_string_1 = put.raw_allocate_copy("Hello world");
    put.element().m_string_2 = put.raw_allocate_copy("\t(I'm so happy)!!");
    put.commit();

    bool const invoked = queue.try_consume();
    assert(invoked);
        //! [function_queue start_emplace example 1]
        (void)invoked;
    }
}

void func_queue_reentrant_put_samples(std::ostream & i_ostream)
{
	PrintScopeDuration(i_ostream, "function queue reentrant put samples");

	using namespace density;

    {
        //! [function_queue reentrant_push example 1]
    function_queue<void()> queue;
    queue.reentrant_push( []{ std::cout << "Hello"; } );
    queue.reentrant_push( []{ std::cout << " world"; } );
    queue.reentrant_push( []{ std::cout << "!!!"; } );
    queue.reentrant_push( []{ std::cout << std::endl; } );
    while( queue.try_reentrant_consume() );
        //! [function_queue reentrant_push example 1]
    }
    {
        //! [function_queue reentrant_push example 2]
    double last_val = 1.;

    auto func = [&last_val] { 
        return last_val /= 2.; 
    };

    function_queue<double()> queue;
    for(int i = 0; i < 10; i++)
        queue.reentrant_push(func);

    while (auto const return_value = queue.try_reentrant_consume())
        std::cout << *return_value << std::endl;
        //! [function_queue reentrant_push example 2]
    }
    {
        //! [function_queue reentrant_emplace example 1]
    /* This local struct is unmovable and uncopyable, so emplace is the only
        option to add it to the queue. Note that operator () returns an int,
        but we add it to a void() function queue. This is ok, as we are just 
        discarding the return value. */
    struct Func
    {
        int const m_value;

        Func(int i_value) : m_value(i_value) {}

        Func(const Func &) = delete;
        Func & operator = (const Func &) = delete;

        int operator () () const
        { 
            std::cout << m_value << std::endl;
            return m_value;
        }
    };

    function_queue<void()> queue;
    queue.reentrant_emplace<Func>(7);

    bool const invoked = queue.try_reentrant_consume();
    assert(invoked);
        //! [function_queue reentrant_emplace example 1]
        (void)invoked;
    }
    {
        //! [function_queue start_reentrant_push example 1]
    struct Func
    {
        const char * m_string_1;
        const char * m_string_2;
        
        void operator () ()
        {
            std::cout << m_string_1 << std::endl;
            std::cout << m_string_2 << std::endl;
        }
    };

    function_queue<void()> queue;
    auto put = queue.start_reentrant_push(Func{});
    put.element().m_string_1 = put.raw_allocate_copy("Hello world");
    put.element().m_string_2 = put.raw_allocate_copy("\t(I'm so happy)!!");
    put.commit();

    bool const invoked = queue.try_reentrant_consume();
    assert(invoked);
        //! [function_queue start_reentrant_push example 1]
        (void)invoked;
    }
    {
        //! [function_queue start_reentrant_emplace example 1]
    struct Func
    {
        const char * m_string_1;
        const char * m_string_2;
        
        void operator () ()
        {
            std::cout << m_string_1 << std::endl;
            std::cout << m_string_2 << std::endl;
        }
    };

    function_queue<void()> queue;
    auto put = queue.start_reentrant_emplace<Func>();
    put.element().m_string_1 = put.raw_allocate_copy("Hello world");
    put.element().m_string_2 = put.raw_allocate_copy("\t(I'm so happy)!!");
    put.commit();

    bool const invoked = queue.try_reentrant_consume();
    assert(invoked);
        //! [function_queue start_reentrant_emplace example 1]
        (void)invoked;
    }
}

void func_queue_reentrant_consume_samples(std::ostream & i_ostream)
{
    using namespace density;

    {
        //! [function_queue try_consume example 1]
    std::vector<int> results;

    auto sum = [&results](int a, int b) { 
        results.push_back(a + b);
    };

    auto mul = [&results](int a, int b) { 
        results.push_back(a * b);
    };

    function_queue<void(int, int)> queue;
    queue.push(sum);
    queue.push(mul);

    while (auto const return_value = queue.try_consume(3, 4));
        //! [function_queue try_consume example 1]
    }

    {
        //! [function_queue try_reentrant_consume example 1]
    std::vector<int> results;

    auto sum = [&results](int a, int b) { 
        results.push_back(a + b);
    };

    auto mul = [&results](int a, int b) { 
        results.push_back(a * b);
    };

    function_queue<void(int, int)> queue;
    queue.push(sum);
    queue.push(mul);

    while (auto const return_value = queue.try_reentrant_consume(3, 4));
        //! [function_queue try_reentrant_consume example 1]
    }
}

void func_queue_samples(std::ostream & i_ostream)
{
    func_queue_put_samples(i_ostream);
    func_queue_reentrant_put_samples(i_ostream);
    func_queue_reentrant_consume_samples(i_ostream);
}

} // namespace density_tests

#if defined(_MSC_VER) && defined(NDEBUG)		
	#pragma warning(pop)
#endif
