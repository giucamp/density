

Density Overview
----------------
Density is a C++11 header-only library providing page based memory management, lifo memory management, and a rich set of highly configurable heterogeneous queues based on paged memory management.

concurrency strategy|function queue|heterogeneous queue|Consumers cardinality|Producers cardinality
--------------- |------------------ |--------------------|--------------------|--------------------
single threaded   |[function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1function__queue.html)      |[heter_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1heter__queue.html)| - | -
locking         |[conc_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1conc__function__queue.html) |[conc_hetr_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1conc__heter__queue.html)|multiple|multiple
lock-free       |[lf_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1lf__function__queue.html) |[lf_hetr_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1lf__heter__queue.html)|configurable|configurable
spin-locking    |[sp_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1sp__function__queue.html) |[sp_hetr_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1sp__heter__queue.html)|configurable|configurable

All function queues have a common interface, and so all heterogeneous queues. Some queues have specific extensions: an example is heter_queue supporting iteration, or lock-free and spin-locking queues supporting try_* functions with parametric progress guarantee.

About function queues
--------------
A function queue is an heterogeneous FIFO pseudo-container that stores callable objects, each with a different type, but all invokable with the same signature.
Foundamentaly a funcion queue is a queue of `std::function`-like objects that uses linear allocation for the storage of the captures.

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

The start_* put functions return a [put_transaction](http://peggysansonetti.it/tech/density/html/classdensity_1_1heter__queue_1_1put__transaction.html) that:

 - can be used to access or alter the object being pushed before it becomes observable
 - can [commit](http://peggysansonetti.it/tech/density/html/classdensity_1_1heter__queue_1_1put__transaction.html#a96491d550e91a5918050bfdafe43a72c) the transaction so that the element becomes observable to consumers, and the `put_transaction` becomes empty.
 - can [cancel](http://peggysansonetti.it/tech/density/html/classdensity_1_1heter__queue_1_1put__transaction.html#a209a4e37e25451c144910d6f6aa4911e) the transaction, in which case the transaction is discarded with no observable side effects, and the `put_transaction` becomes empty.
 - can allocate *raw memory blocks*, that is arrays of a trivially destructible type (`char`s, `float`s) that are linearly allocated in the pages, right after the last allocated value. There is no function to deallocate raw blocks: they are automatically deallocated after the associated element has been canceled or consumed.

When a non-empty `put_transaction` is destroyed, the bound transaction is canceled. As a consequence, if the `raw_allocate_copy` in the code above throws, the transaction is discarded with no side effects.

Raw memory blocks are handled in the same way of canceled and consumed values (they are referred as *dead* values). Internally the storage of dead values is deallocated when the whole page is returned to the page allocator, but this is an implementation detail. When a consume is committed, the head pointer is advanced, skipping any dead element.

Internally instant puts are implemented in terms of transactional puts, so there is no performance difference between them:

    // ... = omissis
    template <...> void emplace(...)
    {
    	start_emplace(...).commit();
    }
Consume operations have the `start_*` variant only in heterogeneous queues (not in function queues). Anyway this operation is not a transaction, as the element disappears from the queue when the operation starts, and will reappear if the operation is canceled.

Reentrancy
----------
During a put or a consume operation an heterogeneous or function queue is not in a consistent state *for the caller thread*. So accessing the queue in any way, in the between of a start_* function and the cancel/commit, causes undefined behaviour. Also accessing the queue from the constructor of an element, or from the invokation of a callable of a function queue, causes undefined behaviour.
Anyway in this time span the queue is not in an inconsistent state for the other threads, provided that the queue is concurrency-enabled. This may appear weird, but think to the queues protected by a mutex (`conc_function_queue` and `conc_heter_queue`): reentrancy would mean a double lock by the same thread, which causes undefined behaviour (unless the mutex is recursive, which is not). On the other hand other threads can access the queue: they will eventually block in a lock. Single threaded queues also exploit non-reentrancy to do some minor optimizations.
Anyway, expecially for consumers, sometimes reentrancy is necessary: a callable object, during the ivokation, may need to push another callable object to the same queue. For every put or consume function, in every queue, there is a reentrant variant.

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
Anyway the 3th template parameter of all function queues is an enum of type [function_type_erasure](http://peggysansonetti.it/tech/density/html/namespacedensity.html#a80100b808e35e98df3ffe74cc2293309) that can be used to exclude the second operation, so that the function queue will not be able to destroy a callable without invoking it. This gives a performance benefit, at the price that the queue can't be cleared, and that the user must ensure that the queue is empty when destroyed.
In the manual clean mode the layout of a value in the function queue is composed by:

 - an overhead pointer (that points to the next value, and keeps the state of the value in the least significant bits)
 - the runtime type, actually a pointer to the invoke-destroy function
 - the eventual capture

Anyway lock-free queues and spin-locking queues align their values to [density::concurrent_alignment](http://peggysansonetti.it/tech/density/html/namespacedensity.html#ae8f72b2dd386b61bf0bc4f30478c2941), so they are less dense than the other queues.

All queues but `function_queue` and `heter_queue` are concurrency enabled. 
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

Naming convensions
------------------

Functions starting with the try_ prefix provide a progress guarantee specified by a parameter of type progress_guarantee. Try-functions are always noexcept, but they are subject to failure: the return value is a boolean or an object implicitly convertible to a boolean. Non try-functions completes the requesdsted operation or throw an exception (usually because they fail to allocate memory).
Functions containing "reentrant" in their name allow safe recursion: during the call, the target data structure is in consistent state and other function can be called on it. This may look unusual, but often when invoking an element of a function queue the caller has no idea of what the function will do: the function may push another function on the same queue.
Functions containing "start" in their name begin an operation that expects to be committed or canceled. The return value is an object.

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

