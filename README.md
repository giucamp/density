

Density Overview
================

What's Density
--------------
Density is a C++11 header-only library that provides heterogeneous containers and lifo memory management. The key ideas behind density are:

- allocating objects of **heterogeneous** type linearly and tightly in memory: this is beneficial for allocation time, memory locality and footprint. The inplace storage of containers is minimal (in particular in heterogeneous_array, that has the same size of a pointer).
- superseding the *fixed-then-dynamic storage* pattern. When you need storage N elements, with N known only at runtime, you may dedicate a fixed-sized storage big M. Then, at runtime, if N > M, you would allocate another storage on dynamic memory with size N, and leave the fixed storage unused. This pattern is used in typical implementation of std::any, std::function, and it is very frequent as optimization for production code when a temporary automatic storage is need. If this description was not clear, see [density explained in the real world](http://peggysansonetti.it/tech/density/html/md_todo_list.html).
- providing at least the **strong exception guarantee** for every single function (that is, if an exception is thrown while executing the function, the call has no visible side effects at all on the calling thread). Exceptions are the easiest, safest and fastest way of handling errors, and applications should have a reliable behavior even in case of exception.



Here is a summary of the containers provided by density:

Container       | Memory layout      | Replacement for...
--------------- | ------------------ | --------------------
[function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1function__queue_3_01RET__VAL_07PARAMS_8_8_8_08_4.html) | Paged | std::queue< std::function >
[small_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1small__function__queue_3_01RET__VAL_07PARAMS_8_8_8_08_4.html) | Monolithic | std::queue< std::function >
[heterogeneous_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1heterogeneous__queue.html)  | Paged | std::queue< std::any >
[small_heterogeneous_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1small__heterogeneous__queue.html) | Monolithic | std::queue< std::any >
[heterogeneous_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1heterogeneous__array.html)  | Monolithic | std::list< std::any >
[lifo_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1lifo__array.html) | Lifo monolithic | std::vector (on automatic storage) 


Heterogeneous containers
--------------
Currently density provides 3 heterogeneous containers, which have a similar template argument list:

    template < typename ELEMENT = void, typename VOID_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class heterogeneous_queue final;

    template <typename ELEMENT = void, typename UNTYPED_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class small_heterogeneous_queue final;

    template <typename ELEMENT = void, typename UNTYPED_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class heterogeneous_array final;

Template argument              | Description
-------------------------------|----------------
ELEMENT                        | Base type of the elements that are stored in the container. If it is void, the type of the elements is unconstrained (you can add an int, a std::string, a foo::Bike and a foo::Car). Otherwise, if you specify something like foo::Vehicle, you can add a foo::Bike, a foo::Car, but if you try to add an int you get a compile time error.
*_ALLOCATOR | For monolithic containers this parameter must model the [UntypedAllocator](http://peggysansonetti.it/tech/density/html/UntypedAllocator_concept.html) concept. For paged containers, it must model both [UntypedAllocator](http://peggysansonetti.it/tech/density/html/UntypedAllocator_concept.html) and [PagedAllocator](http://peggysansonetti.it/tech/density/html/PagedAllocator_concept.html).
RUNTIME_TYPE | Type responsible of type-erasing the elements. The builtin class [runtime_type](http://peggysansonetti.it/tech/density/html/classdensity_1_1runtime__type.html) allows to specify which features you want to be exposed by the elements (copy costruction, comparison, or a custom Update(float dt) method) by composition. Anyway, if you already have in your project a reflection or type-erasure mechanism, you can define a custom type modelling the [RuntimeType concept](http://peggysansonetti.it/tech/density/html/RuntimeType_concept.html).

Here is an example of how an heterogeneous_array can be created and iterated:

        struct Widget { virtual void draw() { /* ... */ } };
        struct TextWidget : Widget { virtual void draw() override { /* ... */ } };
        struct ImageWidget : Widget { virtual void draw() override { /* ... */ } };

        auto widgets = heterogeneous_array<Widget>::make(TextWidget(), ImageWidget(), TextWidget());
        for (auto & widget : widgets)
        {
            widget.draw();
        }

        widgets.push_back(TextWidget());

Function queues
--------------
A function queue is a queue in which every element is an object similar to an std::function. function_queue is just a wrapper around a private heterogeneous_queue that uses its type-erasure capability to store generic callable objects. Same for small_function_queue, which wraps a small_heterogeneous_queue.

A function queue may be used as command buffer for a rendering engine, in which every command has a different number (and type) of parameters. Thanks to the dense memory layout, a function_queue is [around four time faster](http://peggysansonetti.it/tech/density/html/func_queue_bench.html) than a std::queue< std::function >. 

Here is a very simple example of function_queue. For a more realistic example see the [producer consumer sample](http://peggysansonetti.it/tech/density/html/producer_consumer_sample.html). 

    auto print_func = [](const char * i_str) { std::cout << i_str; };
    
    function_queue<void()> queue_1;
    queue_1.push(std::bind(print_func, "hello "));
    queue_1.push([print_func]() { print_func("world!"); });
    queue_1.push([]() { std::cout << std::endl; });
    while (!queue_1.empty())
        queue_1.consume_front();
    
    function_queue<int(double, double)> queue_2;
    queue_2.push([](double a, double b) { return static_cast<int>(a + b); });
    std::cout << "a + b = " << queue_2.consume_front(40., 2.) << std::endl;

Note: with gcc, in the above example the line with std::bind raises a compile time error, as the return type is not noexcept-move-constructible, and density imposes that move constructor must be noexcept. This constraint may be relaxed in future versions. Anyway a call to std::bind can always be replaced with a lambda, which is also easier to use.

Monolithic and paged containers
--------------
Inside heterogeneous containers elements are packed together (like they were allocated by a linear allocator). 

- Monolithic containers allocate a single memory block (like an std::vector). To perform an insertion sometimes the container must reallocate the block, and the elements have to be moved.
- Paged containers allocate multiple memory blocks (the term page is used for these blocks, as the size of them is fixed and specified by the allocator). The size of a page can be something like 4 KiB, and may depend on the operating system.
A paged queue (like heterogeneous_queue or function_queue) pushes as many elements as possible in its *current page*. Whenever the element to push does not fit in the current page, the container just allocates a new page from its allocator, with no need to move any element. 
Allocating a fixed-size page is much easier than allocating a dynamic block. As default page allocator, density provides the class [void_allocator](http://peggysansonetti.it/tech/density/html/classdensity_1_1void__allocator.html), that uses a thread-local cache of 4 free pages. Pushing and peeking a page from this cache requires no thread synchronization. This is much better than allocating a memory block (and probably locking a mutex) for every element to push in the queue.

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
[UntypedAllocator](http://peggysansonetti.it/tech/density/html/UntypedAllocator_concept.html) | [void_allocator](http://peggysansonetti.it/tech/density/html/classdensity_1_1void__allocator.html)
[PagedAllocator](http://peggysansonetti.it/tech/density/html/PagedAllocator_concept.html) | [void_allocator](http://peggysansonetti.it/tech/density/html/classdensity_1_1void__allocator.html)

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

Important note: density has an extensive test suite, that includes an exhaustive test of the exception safeness. Anyway some functions may be still uncovered by the tests.

Future development
------------------
- Future version of density may provide an anti-slicing mechanism, to detect at compile-time copy-constructions or copy-assignments using as source a partial-type reference to an element of an heterogeneous containers. This feature will probably require a C++17 compiler.
- Density currently lacks an heterogeneous_stack and a function_stack.
- An undocumented lifo_any exists, but is not yet tested enough, and its usage is not recommended.
- As reported in the reference documentation, currently heterogeneous_array reallocates its memory block on every change (it does not handle an unused space, unlike std::vector). Any modifying operation on the array has linear complexity, so it is suitable for immutable or almost immutable containers. A major review is planned to make the complexity of most modifying operations constant amortized (like std::vector::push_back).

[Reference Documentation](http://peggysansonetti.it/tech/density/html/index.html)

[Github Repository](https://github.com/giucamp/density)

For any kind of support or feedback, mail to giu.campana@gmail.com

> Written with [StackEdit](https://stackedit.io/).
