[![Build Status](https://travis-ci.org/giucamp/density.svg?branch=master)](https://travis-ci.org/giucamp/density)

Density Overview
----------------
Density is a C++11 header-only library focused on paged memory management. providing a rich set of highly configurable heterogeneous queues.
concurrency strategy|function queue|heterogeneous queue|Consumers cardinality|Producers cardinality
--------------- |------------------ |--------------------|--------------------|--------------------
single threaded   |[function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1function__queue.html)      |[heter_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1heter__queue.html)| - | -
locking         |[conc_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1conc__function__queue.html) |[conc_hetr_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1conc__heter__queue.html)|multiple|multiple
lock-free       |[lf_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1lf__function__queue.html) |[lf_hetr_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1lf__heter__queue.html)|configurable|configurable
spin-locking    |[sp_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1sp__function__queue.html) |[sp_hetr_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1sp__heter__queue.html)|configurable|configurable

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


Transactional puts and raw allocations
------------------------------------

The functions `queue::push` and `function_queue::emplace` append a callable at the end of the queue. This is the quick way of doing a *put transaction*. We can have more control breaking it using the **start_*** put functions:

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

The start_* put functions return a [put_transaction](http://peggysansonetti.it/tech/density/html/classdensity_1_1heter__queue_1_1put__transaction.html) by which the caller can:

 - access or alter the object being pushed before it becomes observable
 - [commit](http://peggysansonetti.it/tech/density/html/classdensity_1_1heter__queue_1_1put__transaction.html#a96491d550e91a5918050bfdafe43a72c) the transaction so that the element becomes observable to consumers (the `put_transaction` becomes empty).
 - [cancel](http://peggysansonetti.it/tech/density/html/classdensity_1_1heter__queue_1_1put__transaction.html#a209a4e37e25451c144910d6f6aa4911e) the transaction, in which case the transaction is discarded with no observable side effects (the `put_transaction` becomes empty).
 - allocate *raw memory blocks*, that is uninitialized memory, or arrays of a trivially destructible type (`char`s, `float`s) that are linearly allocated in the pages, right after the last allocated value. There is no function to deallocate raw blocks: they are automatically deallocated after the associated element has been canceled or consumed.

When a non-empty `put_transaction` is destroyed, the bound transaction is canceled. As a consequence, if the `raw_allocate_copy` in the code above throws an exception, the transaction is discarded with no side effects.

Raw memory blocks are handled in the same way of canceled and consumed values (they are referred as *dead* values). Internally the storage of dead values is deallocated when the whole page is returned to the page allocator, but this is an implementation detail. When a consume is committed, the head pointer is advanced, skipping any dead element.

Internally instant puts are implemented in terms of transactional puts, so there is no performance difference between them:

    // ... = omissis
    template <...> void emplace(...)
    {
    	start_emplace(...).commit();
    }
Consume operations have the `start_*` variant in heterogeneous queues (but not in function queues). Anyway this operation is not a transaction, as the element disappears from the queue when the operation starts, and will reappear if the operation is canceled.

Reentrancy
----------
During a put or a consume operation an heterogeneous or function queue is not in a consistent state *for the caller thread*. So accessing the queue in any way, in the between of a start_* function and the cancel/commit, causes undefined behavior. Also accessing the queue from the constructor of an element, or from the invocation of a callable of a function queue, causes undefined behavior.
Anyway in this time span the queue is not in an inconsistent state for the other threads, provided that the queue is concurrency-enabled. This may appear weird, but think to the queues protected by a mutex (`conc_function_queue` and `conc_heter_queue`): reentrancy would mean a double lock by the same thread, which causes undefined behaviour (unless the mutex is recursive, which is not). On the other hand other threads can access the queue: they will eventually block in a lock. Single threaded queues also exploit non-reentrancy to do some minor optimizations (internally they set the transaction as committed when it is started, so that the commit does not write the control bit again).
Anyway, especially for consumers, reentrancy is sometimes necessary: a callable object, during the invocation, may need to push another callable object to the same queue. For every put or consume function, in every queue, there is a reentrant variant.

    lf_function_queue<void()> queue;

    auto func1 = [&queue] { 
        std::cout << (queue.empty() ? "The queue is empty" : "The queue is not empty") << std::endl;
    };

    auto func2 = [&queue, func1] { 
        queue.push(func1);
    };

    queue.push(func1);
    queue.push(func2);

    /* The callable objects we are going to invoke will access the queue, so we
        must use a reentrant consume. Note: during the invoke of the last function
        the queue is empty to any observer. */
    while (queue.try_reentrant_consume());
    
Output:

    The queue is not empty
    The queue is empty

When doing reentrant operations,`conc_function_queue` and `conc_heter_queue` lock the mutex once when starting, and once when committing or canceling. In the middle of the operation the mutex is not locked.

Relaxed guarantees
------------
Function queues use type erasure to handle callable objects of heterogeneous types. By default two operations are captured for each element type: invoke-destroy, and just-destroy.
The third template parameter of all function queues is an enum of type [function_type_erasure](http://peggysansonetti.it/tech/density/html/namespacedensity.html#a80100b808e35e98df3ffe74cc2293309) that controls the type erasure: the value function_manual_clear excludes the second operation, so that the function queue will not be able to destroy a callable without invoking it. This gives a performance benefit, at the price that the queue can't be cleared, and that the user must ensure that the queue is empty when destroyed.
Internally, in manual-clean mode, the layout of a value in the function queue is composed by:

 - an overhead pointer (that points to the next value, and keeps the state of the value in the least significant bits)
 - the runtime type, actually a pointer to the invoke-destroy function
 - the eventual capture

So if you put a captureless lambda or a pointer to a function, you are advancing the tail pointer by the space required by 2 pointers.
Anyway lock-free queues and spin-locking queues align their values to [density::concurrent_alignment](http://peggysansonetti.it/tech/density/html/namespacedensity.html#ae8f72b2dd386b61bf0bc4f30478c2941), so they are less dense than the other queues.

All queues but `function_queue` and `heter_queue` are concurrency enabled. By default they allow multiple producers and multiple consumers.
The class templates [lf_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1lf__function__queue.html), [lf_hetr_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1lf__heter__queue.html), [sp_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1sp__function__queue.html) and [sp_hetr_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1sp__heter__queue.html) allow to specify, with 2 independent template arguments of type [concurrency_cardinality](http://peggysansonetti.it/tech/density/html/namespacedensity.html#aeef74ec0c9bea0ed2bc9802697c062cb), whether multiple threads are allowed to produce, and whether multiple threads are allowed to consume:

    // single producer, multiple consumers:
    using Lf_SpMc_FuncQueue = lf_function_queue<void(), void_allocator, function_standard_erasure, concurrency_single, concurrency_multiple>;
                
    // multiple consumers, single producer:
    using Lf_MpSc_FuncQueue = lf_function_queue<void(), void_allocator, function_standard_erasure, concurrency_multiple, concurrency_single>;
    
    // multiple producer, multiple consumers (the default):
    using Lf_MpMc_FuncQueue = lf_function_queue<void()>;

When dealing with a multiple producers, the tail pointer is an atomic variable. Otherwise it is a plain variable.
When dealing with a multiple consumers, the head pointer is an atomic variable. Otherwise it is a plain variable.

The class templates [lf_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1lf__function__queue.html) and [lf_hetr_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1lf__heter__queue.html) allow a further optimization with a template argument of type [consistency_model](http://peggysansonetti.it/tech/density/html/namespacedensity.html#ad5d59321f5f1b9a040c6eb9bc500a051): by default the queue is sequential consistent (that is all threads observe the operations happening in the same order). If `consistency_relaxed`is specified, this guarantee is removed, with a great performance benefit.

For all queues, the functions `try_consume` and `try_reentrant_consume` have 2 variants:

- one returning a consume operation. If the queue was empty, an empty consume is returned.
- one taking a reference to a consume as parameter and returning a boolean, raccomanded if a thread performs many consecutive consumes.

There is no functional difference between the two consumes. Anyway,
currently only for lock-free and spin-locking queues supporting multi-producers, the second consume can be much faster. The reason has to do with the way they ensure that a consumer does not deallocate a page while another consumer is reading it.
When a consumer needs to access a page, it increments a ref-count in the page (it *pins* the page), to notify to the allocator that it is using it. When it has finished, the consumer decrements the ref-count (it *unpins* the page).
If a thread performs many consecutive consumes, it will ends up doing many atomic increments and decrements of the same page (that is a somewhat expensive operation). Since pin\unpin logic is encapsulated in the consume_operation, if the consumer thread keeps the consume_operation alive, pinning and unpinning will be performed only in case of page switch.
Note: a forgotten consume_operation which has pinned a page prevents the page from being recycled by the page allocator, even if it was deallocated by other consumers.  

heterogeneous queues
--------------------
Every function queue is actually an adaptor for the corresponding heterogeneous pseudo-container.  Heterogeneous queues have pit and consume functions, just like function queues, but elements are not required to be callable objects.

    heter_queue<> queue;
    queue.push(19); // the parameter can be an l-value or an r-value
    queue.emplace<std::string>(8, '*'); // pushes "********"

The first 3 template parameters are the same for all the heterogeneous queues.

Template parameter|Type|Meaning|Constraint|Default
--------|-----------|---------|------
`COMMON_TYPE`|type|Common type of elements|Must decay to itself (see [std::decay](http://en.cppreference.com/w/cpp/types/decay))|`void`
`RUNTIME_TYPE`|type|Type eraser type|Must model [RuntimeType](http://peggysansonetti.it/tech/density/html/RuntimeType_concept.html)|[runtime_type](http://peggysansonetti.it/tech/density/html/classdensity_1_1runtime__type.html)
`ALLOCATOR_TYPE`|type|Allocator|Must model both [PageAllocator](http://peggysansonetti.it/tech/density/html/PagedAllocator_concept.html) and [UntypedAllocator](http://peggysansonetti.it/tech/density/html/UntypedAllocator_concept.html)|[void_allocator](http://peggysansonetti.it/tech/density/html/namespacedensity.html#a06c6ce21f0d3cede79e2b503a90b731e)

An element can be pushed on a queue if its address is is implicitly convertible to `COMMON_TYPE*`. By default any type is allowed in the queue.
The `RUNTIME_TYPE` type allows much more customization than the [function_type_erasure](http://peggysansonetti.it/tech/density/html/namespacedensity.html#a80100b808e35e98df3ffe74cc2293309) template parameter of fumnction queues. Even using the buillt-in [runtime_type](http://peggysansonetti.it/tech/density/html/classdensity_1_1runtime__type.html) you can select which operations the elements of the queue should support, and add your own.

Lifo data structures
--------------
Density provides two lifo data structures: lifo_array and lifo_buffer.

[lifo_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1lifo__array.html) is a modern C++ version of the variable length automatic arrays of C99. It is is an alternative to the non-standard [alloca](http://man7.org/linux/man-pages/man3/alloca.3.html) function, much more C++ish.

		void dijkstra_path_find(const GraphNode * i_nodes, size_t i_node_count, size_t i_initial_node_index)
		{
			lifo_array<float> min_distance(i_node_count, std::numeric_limits<float>::max() );
			min_distance[i_initial_node_index] = 0.f;

			// ...

The size of the array is provided at construction time, and cannot be changed. lifo_array allocates its element contiguously in memory in the **data stack** of the calling thread. 
The data stack is a thread-local storage managed by a lifo allocator.  By "lifo" (last-in-first-out) we mean that only the most recently allocated block can be deallocated. If the lifo constraint is violated, the behaviour of the program is undefined (defining DENSITY_DEBUG as non-zero in density_common.h enables a debug check, but beaware of the [one definition rule](https://en.wikipedia.org/wiki/One_Definition_Rule)). 
If you declare a lifo_array locally to a function, you don't have to worry about the lifo-constraint, because C++ is designed so that automatic objects are destroyed in lifo order. Even a lifo_array as non-static member of a struct/class allocated on the callstack is safe.

A [lifo_buffer](http://peggysansonetti.it/tech/density/html/classdensity_1_1lifo__buffer.html) is not a container, it's more low-level. It provides a dynamic raw storage. Unlike lifo_array, lifo_buffer allows reallocation (that is changing the size and the alignment of the buffer), but only of the last created lifo_buffer (otherwise the behavior is undefined).

    void string_io()
    {
        using namespace std;
        vector<string> strings{ "string", "long string", "very long string", "much longer string!!!!!!" };
        uint32_t len = 0;

		// for each string, write a length-chars pair on a temporary file
        #ifndef _MSC_VER
            auto file = tmpfile();
        #else
            FILE * file = nullptr;
            tmpfile_s(&file);
        #endif
        for (const auto & str : strings)
        {
            len = static_cast<uint32_t>( str.length() + 1);
            fwrite(&len, sizeof(len), 1, file);
            fwrite(str.c_str(), 1, len, file);
        }

		// now we read what we have written
        rewind(file);
        lifo_buffer<> buff;
        while (fread(&len, sizeof(len), 1, file) == 1)
        {
            buff.resize(len);
            fread(buff.data(), len, 1, file);
            cout << static_cast<char*>(buff.data()) << endl;
        }

        fclose(file);
    }

Concepts
----------

Concept     | Modeled in density by
------------|--------------
[RuntimeType](http://peggysansonetti.it/tech/density/html/RuntimeType_concept.html) | [runtime_type](http://peggysansonetti.it/tech/density/html/classdensity_1_1runtime__type.html)
[UntypedAllocator](http://peggysansonetti.it/tech/density/html/UntypedAllocator_concept.html) | [void_allocator](http://peggysansonetti.it/tech/density/html/namespacedensity.html#a06c6ce21f0d3cede79e2b503a90b731e), [basic_void_allocator](http://peggysansonetti.it/tech/density/html/classdensity_1_1basic__void__allocator.html)
[PagedAllocator](http://peggysansonetti.it/tech/density/html/PagedAllocator_concept.html) | [void_allocator](http://peggysansonetti.it/tech/density/html/namespacedensity.html#a06c6ce21f0d3cede79e2b503a90b731e), [basic_void_allocator](http://peggysansonetti.it/tech/density/html/classdensity_1_1basic__void__allocator.html)

Samples
-------
- [A producer-consumer sample](http://peggysansonetti.it/tech/density/html/producer_consumer_sample.html)
- [A sample with runtime_type](http://peggysansonetti.it/tech/density/html/runtime_type_sample.html)	
- [A lifo sample](http://peggysansonetti.it/tech/density/html/lifo_sample.html)	
- [A sample with function queues](http://peggysansonetti.it/tech/density/html/function_queue_sample.html)

Benchmarks
----------
- [Function queue](http://peggysansonetti.it/tech/density/html/func_queue_bench.html)
- [Widget list](http://peggysansonetti.it/tech/density/html/wid_list_iter_bench.html) 	
- [Comparaison benchmarks with boost::any](http://peggysansonetti.it/tech/density/html/any_bench.html)
- [Automatic variable-length array](http://peggysansonetti.it/tech/density/html/lifo_array_bench.html)	
 
Usage and release notes
----------
Density is an header-only library, so you don't have to compile anything: just include the headers you need. You can get the latest revision from the master branch of the [github repository](https://github.com/giucamp/density).

Density is known to compile with:

- msc (Visual Studio 2015 - update 2)
- clang 3.7 (Visual Studio 2015 - update 2)
- gcc 4.9.3 (MinGW)
 
Anyway it should compile with any C++11 compliant compiler. If you compile density with any other compiler, please let me know.


[Reference Documentation](http://peggysansonetti.it/tech/density/html/index.html)

[Github Repository](https://github.com/giucamp/density)

For any kind of support or feedback, mail to giu.campana@gmail.com

> Written with [StackEdit](https://stackedit.io/).

