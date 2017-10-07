[![Build Status](https://travis-ci.org/giucamp/density.svg?branch=master)](https://travis-ci.org/giucamp/density)
[![Build status](https://ci.appveyor.com/api/projects/status/td8xk69gswc6vuct?svg=true)](https://ci.appveyor.com/project/GiuseppeCampana/density)

Density Overview
----------------
Density is a C++11 header-only library focused on paged memory management. providing a rich set of highly configurable heterogeneous queues.

concurrency strategy|function queue|heterogeneous queue|Consumers cardinality|Producers cardinality
--------------- |------------------ |--------------------|--------------------|--------------------
single threaded   |[function_queue](classdensity_1_1function__queue.html)      |[heter_queue](classdensity_1_1heter__queue.html)| - | -
locking         |[conc_function_queue](classdensity_1_1conc__function__queue.html) |[conc_hetr_queue](classdensity_1_1conc__heter__queue.html)|multiple|multiple
lock-free       |[lf_function_queue](classdensity_1_1lf__function__queue.html) |[lf_hetr_queue](classdensity_1_1lf__heter__queue.html)|configurable|configurable
spin-locking    |[sp_function_queue](classdensity_1_1sp__function__queue.html) |[sp_hetr_queue](classdensity_1_1sp__heter__queue.html)|configurable|configurable


All function queues have a common interface, and so all heterogeneous queues. Some queues have specific extensions: an example is heter_queue supporting iteration, or lock-free and spin-locking queues supporting try_* functions with parametric progress guarantee.
A lifo memory management system is provided too, built upon the paged management.

About function queues
--------------
A function queue is an heterogeneous FIFO pseudo-container that stores callable objects, each with a different type, but all invokable with the same signature.
Foundamentaly a funcion queue is a queue of `std::function`-like objects that uses an in-page linear allocation for the storage of the captures.

    // put a lambda in the queue
    function_queue<void()> queue;            
    queue.push([] { std::cout << "Printing..." << std::endl; });
    
    // we can have a capture of any size
    double pi = 3.1415;
    queue.push([pi] { std::cout << pi << std::endl; });
    
    // now we execute all the functions
    int executed = 0;
    while (queue.try_consume())
        executed++;

If the return type of the signature is `void`, the function `try_consume` returns a boolean indicating whether a function has been called. Otherwise it returns an `optional` containing the return value of the function, or empty if no function was present.
Any callable object can be pushed on the queue: a lambda, the result of an `std::bind`, a (possibly local) class defining an operator (), a pointer to member function or variable. The actual signature of the callable does not need to match the one of the queue, as long as an implicit conversion exists:

    function_queue<std::string(const char * i_prefix)> queue;
    queue.push([](std::string i_prefix) {
        return i_prefix + "...";
    });

All queues in density have a very simple implementation: a *tail pointer*, an *head pointer*, and a set of memory pages. Paging is a kind of quantization of memory: all pages have a fixed size (by default slightly less than 64 kibibyte) and a fixed alignment (by default 64 kibibyte), so that the allocator can be simple and efficient. 
Values are arranged in the queue by *linear allocation*: allocating a value means just adding its size to the tail pointer. If the new tail pointer would point outside the last page, a new page is allocated, and the linear allocation is performed from there. In case of very huge values, only a pointer is allocated in the pages, and the actual storage for the value is allocated in the heap.

[Continue...](intro.html)

> Written with [StackEdit](https://stackedit.io/).

