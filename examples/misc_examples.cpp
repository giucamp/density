
#include <density/function_queue.h>
#include <density/conc_function_queue.h>
#include <density/lf_function_queue.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <string>

namespace density_tests
{

    void misc_examples()
    {
        using namespace density;

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
queue.push([](std::string i_prefix) {
    return i_prefix + "...";
});
            //! [function_queue example 3]
        }
            {
                //! [function_queue example 4]
struct Message
{
    const char * m_text;

    void operator () ()
    {
        std::cout << m_text << std::endl;
    }
};

function_queue<void()> queue;
    
auto transaction = queue.start_emplace<Message>();
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
atomic<bool> finished(false);

// this thread puts 10 commands
thread producer( [&]{
    for (int i = 0; i < 10; i++)
    {
        commands.push([] { cout << "Hi there..." << endl; }); 
        this_thread::sleep_for(chrono::milliseconds(10));
    }                
    finished.store(true);
});

// this thread consumes until finished is true
thread consumer( [&]{
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
using Lf_SpMc_FuncQueue = lf_function_queue<void(), void_allocator, function_standard_erasure, concurrency_single, concurrency_multiple>;
            
// multiple consumers, single producer:
using Lf_MpSc_FuncQueue = lf_function_queue<void(), void_allocator, function_standard_erasure, concurrency_multiple, concurrency_single>;

// multiple producer, multiple consumers (the default):
using Lf_MpMc_FuncQueue = lf_function_queue<void()>;
            //! [lf_function_queue cardinality example]
            Lf_SpMc_FuncQueue q1;
            Lf_MpSc_FuncQueue q2;
            Lf_MpMc_FuncQueue q3;
        }
    }
 
} // namespace density_tests
