

Density Overview
================

What's Density
--------------
Density is a C++11 header-only library that provides heterogeneous containers and lifo memory management. The key ideas behind density are:

- allocating objects of **heterogeneous** type linearly and tightly in memory: this is beneficial for construction time, access memory locality, and global memory footprint. A very fast function queue is provided, which performs much better than a standard vector of functions (see the [banchmarks](http://peggysansonetti.it/tech/density/html/func_queue_bench.html)). The inplace storage of containers is minimal (for example a heterogeneous_array has the same size of a pointer).
- superseding the *fixed-then-dynamic storage* pattern, that is: when you need to store N elements, you dedicate a fast-to-allocate fixed-sized storage big M. Then, when using it, if N > M, you allocate another storage on dynamic memory with size N, and leave the fixed storage unused. This pattern is used in typical implementation of std::any, std::function, and is a very frequent as optimization for production code when a temporary automatic storage is need. density provides [lifo_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1lifo__array.html), a modern replacement for the C-ish and non-standard [alloca](http://man7.org/linux/man-pages/man3/alloca.3.html), similar to C99 variable lenght automatic arrays, but more C++ish.
- providing at least the **strong exception guarantee** for every single function (that is, if an exception is thrown while executing the function, the call has no visible side effects at all on the calling thread). Exceptions are the easiest, safest and fastest way of handling errors, and applications should have a reliable behavior even in case of exception.

Here is a summary of the containers provided by density:

Container       | Memory layout      | Replacement for...
--------------- | ------------------ | --------------------
[function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1function__queue_3_01RET__VAL_07PARAMS_8_8_8_08_4.html) | Paged | std::queue< std::function >
[small_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1small__function__queue_3_01RET__VAL_07PARAMS_8_8_8_08_4.html) | Monolithic | std::queue< std::function >
[heterogeneous_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1heterogeneous__queue.html)  | Paged | std::queue< std::any >
[small_heterogeneous_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1small__heterogeneous__queue.html) | Monolithic | std::queue< std::any >
[heterogeneous_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1heterogeneous__array.html)  | Monolithic | std::list< std::any >
[lifo_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1lifo__array.html) | Lifo monolithic | Homogeneous useful as lightweight automatic storage 



Heterogeneous containers
========================
Currently density provides 3 heterogeneous containers, which have a similar template argument list:

    template < typename ELEMENT = void, typename VOID_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class heterogeneous_queue final;

    template <typename ELEMENT = void, typename UNTYPED_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class small_heterogeneous_queue final;

    template <typename ELEMENT = void, typename UNTYPED_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class heterogeneous_array final;

Template argument              |Meaning
-------------------------------|----------------
ELEMENT                        | Base type of the elements that are stored in the container. If it is void, the type of the elements is unconstrained (you can add an int, a std::string, a foo::Bike and a foo::Car). Otherwise, if you specify something like foo::Vehicle, you can add a foo::Bike, a foo::Car, but if you try to add an int you get a compile time error.
*_ALLOCATOR | For monolithic containers this parameter must model the [UntypedAllocator](http://peggysansonetti.it/tech/density/html/UntypedAllocator_concept.html) concept. For paged container, it must model both [UntypedAllocator](http://peggysansonetti.it/tech/density/html/UntypedAllocator_concept.html) and [PagedAllocator](http://peggysansonetti.it/tech/density/html/PagedAllocator_concept.html).
RUNTIME_TYPE | Type responsible of type-erasing the elements. The builtin class [runtime_type](http://peggysansonetti.it/tech/density/html/classdensity_1_1runtime__type.html) allows to specify which features you want to be exposed by the elements (copy costruction, comparison, or a custom Update(float dt) method) by composition. Anyway, if you already have in your project a reflection or type-erasure mechanism, you can define a custom type modelling the [RuntimeType concept](http://peggysansonetti.it/tech/density/html/RuntimeType_concept.html).

Monolithic and Paged containers
========================
Inside heterogeneous containers elements are packed together (like they were allocated by a linear allocator). 

Monolithic containers allocate a single memory block (like an std::vector). To perform an insertion sometimes the container must reallocate the block, and the elements have to be moved.

Paged containers allocate multiple memory blocks (the term page is used for these blocks, as the size of them is fixed and specified by the allocator). The size of a page can be something like 4 KiB, and may depend on the operating system.
A paged queue (like heterogeneous_queue or function_queue) pushes as many elements as possible in its *current page*. Whenever the element to push does not fit in the current page, it just allocates a new page from its allocator, with no need to move any element. 
Allocating a fixed-size page is much easier than allocating a dynamic block. As default page allocator, density provides the class [void_allocator](http://peggysansonetti.it/tech/density/html/classdensity_1_1void__allocator.html), that uses a thread-local cache of 4 free pages. Pushing and peeking a page from this cache requires no thread synchronization. This is much better than allocating a memory block (and probably locking a mutex) for every element to push in the queue, and that's the reason why function_queue is [four time faster]((http://peggysansonetti.it/tech/density/html/func_queue_bench.html)) than a std::queue< std::function >.

Lifo data structures
====================
[lifo_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1lifo__array.html) is a modern C++ version of variable length automatic array of C99:

		void dijkstra_path_find(const GraphNode * i_nodes, size_t i_node_count, size_t i_initial_node_index)
		{
			lifo_array<float> min_distance(i_node_count, std::numeric_limits<float>::max() );
			min_distance[i_initial_node_index] = 0.f;

			// ...

The size of the array is provided at construction time, and cannot be changed. lifo_array allocates its element contiguously in memory in the **data stack** of the calling thread. 
The data stack is a thread-local storage managed by a lifo allocator.  By "lifo" (last-in-first-out) we mean that only the most recently allocated block can be deallocated. If the lifo constraint is violated, the behaviour is undefined (defining DENSITY_DEBUG as non-zero in density_common.h enables a debug check, but beaware of the [one definition rule](https://en.wikipedia.org/wiki/One_Definition_Rule)). 
If you declare a lifo_array locally to a function, you don't have to worry about the lifo-constraint, because C++ is designed so that automatic objects are destroyed in lifo order. If the lifo_array is a non-static member of a struct\class allocated on the callstack, no problem.

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

The producer-consumer sample shows a more realistic example of how to use a lifo_buffer<>.

Benchmarks
----------
- [Function queue](http://peggysansonetti.it/tech/density/html/func_queue_bench.html)
- [Widget list](http://peggysansonetti.it/tech/density/html/wid_list_iter_bench.html) 	
- [Comparaison benchmarks with boost::any](http://peggysansonetti.it/tech/density/html/any_bench.html)
- [Automatic variable-length array](http://peggysansonetti.it/tech/density/html/lifo_array_bench.html)	
 

Samples
-------
- [A producer-consumer sample](http://peggysansonetti.it/tech/density/html/producer_consumer_sample.html)
- [A sample with runtime_type](http://peggysansonetti.it/tech/density/html/runtime_type_sample.html)	
- [A lifo sample](http://peggysansonetti.it/tech/density/html/lifo_sample.html)	
- [A sample with function queues](http://peggysansonetti.it/tech/density/html/function_queue_sample.html)

Future development
------------------

- Currently an element of an heterogeneous dense container (using the builtin runtime_type) has a space overhead of 3 pointers. One of these pointers is the layout of the runtime_type In the next releases this overhead will probably be reduced.
- There is no easy way to move elements away from heterogeneous containers. It is possible, using a lifo_buffer for temporary storage (see the producer-consumer sample), but it takes more coding than it should, and it's not automatically exception safe. Future versions will probably introduce a lifo_object that will do this job. 
- Future version of density may provide an anti-slicing mechanism, to detect at compile_time copy-constructions or copy-assignments using as source a partial-type reference to an element of an heterogeneous dense containers.

[Reference Documentation](http://peggysansonetti.it/tech/density/html/index.html)

giu.campana@gmail.com

> Written with [StackEdit](https://stackedit.io/).
