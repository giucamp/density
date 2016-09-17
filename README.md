

Density Overview
================

What's Density
--------------
Density is a C++11 header-only library that provides heterogeneous containers and lifo memory management. The key ideas behind density are:
- allocating objects of **heterogeneous** type linearly and tightly in memory: this is beneficial for construction time, access memory locality, and global memory footprint. A very fast function queue is provided, which performs much better than a standard vector of functions (see the [banchmarks](http://peggysansonetti.it/tech/density/html/func_queue_bench.html)).
- superseding the *fixed-then-dynamic storage* pattern, that is: when you need to store N elements, you dedicate a fast-to-allocate fixed-sized storage big M. Then, when using it, if N > M, you allocate another storage on dynamic memory with size N, and leave the fixed storage unused. This pattern is used in typical implementation of std::any, std::function, and is a very frequent as optimization for production code when a temporary automatic storage is need. density provides [lifo_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1lifo__array.html), a modern replacement for the C-ish and non-standard [alloca](http://man7.org/linux/man-pages/man3/alloca.3.html), similar to C99 variable lenght automatic arrays, but more C++ish.
- providing at least the **strong exception guarantee** for every single function (that is, if an exception is thrown while executing the function, the call has no visible side effects at all on the calling thread). Exceptions are the easiest, safest and fastest way of handling errors, and applications should have a reliable behavior even in case of exception.

Here is a summary of the containers provided by density:

Container       | Memory layout| Description
--------------- | -------------
[heterogeneous_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1heterogeneous__array.html)  | Monolithic | Array of elements with unrelated types
[heterogeneous_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1heterogeneous__queue.html)  | Paged | Queue of elements with unrelated types
[small_heterogeneous_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1small__heterogeneous__queue.html) | Monolithic | Queue of elements with unrelated types
[function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1function__queue_3_01RET__VAL_07PARAMS_8_8_8_08_4.html) | Paged | Queue of std::function-like elements
[small_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1small__function__queue_3_01RET__VAL_07PARAMS_8_8_8_08_4.html) | Monolithic | Queue of std::function-like elements
[lifo_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1lifo__array.html) | Lifo monolithic | Homogeneous useful as lightweight automatic storage 



Heterogeneous containers
========================

heterogeneous_array<>
------------
Heterogeneous containers are very frequent in object-oriented designs. Using just the standard library, the best way to model a polymorphic container is probably a vector of smart pointers.
As alternative, density provides [heterogeneous_array](http://peggysansonetti.it/tech/density/html/classdensity_1_1heterogeneous__array.html), a container whose elements can be of different type:

			struct Widget { virtual void draw() { /* ... */ } };
			struct TextWidget : Widget { virtual void draw() override { /* ... */ } };
			struct ImageWidget : Widget { virtual void draw() override { /* ... */ } };

			auto widgets = dense_list<Widget>::make(TextWidget(), ImageWidget(), TextWidget());
			for (auto & widget : widgets)
			{
				widget.draw();
			}
			
This snippet creates and draws a list of 3 widgets. The first template argument (in this case Widget) is the *element base type*. Trying to add a type not covariant to Widget will cause the failure of a static assert.
Elements can be added/removed to/from a dense_list in any time just like a std::vector:

    widgets.push_back(TextWidget());

All the elements in a dense_list are allocated linearly in the same memory block.
dense_list does not handle unused capacity, so insertions and removals always cause a reallocation and have linear complexity, in contrast to the constant amortized time of many vector functions of std::vector. 
dense_list is recommended over a vector of smart pointers when the container is never or not-so-often modified, and when a compact memory layout is desired. A dense_list has the same inline storage of a raw pointer (unless you explicitly specify an allocator with a state). This is in contrast with common implementations of std::vector, that are big as 3 pointers. Finally a dense_list does not allocate memory when empty. 

If the element base type is void (the default), then any type can be added to the list:

	auto list = heterogeneous_array<>::make(3 + 5, string("abc"), 42.f);
	list.push_front(wstring(L"ABC"));
	for (auto it = list.begin(); it != list.end(); it++)
	{
		cout << it.complete_type().type_info().name() << endl;
	} 
			
This snippet creates a list with 3 elements, and then adds another one at the beginning. Then the type of every element is written on std::cout. With a heterogeneous_array<void> range loops cannot be used, because the indirection of the iterator returns void. The expression iterator.element() returns a void pointer to the complete element.
In the last snippet the function complete_type() of the iterartor is used. It returns a density::runtime_type, an object which is used as "type eraser": it is able to construct, destroy, copy, move, retrieve size and alignment of a type. The function type_info() of runtime_type returns the [std::type_info](http://en.cppreference.com/w/cpp/types/type_info) associated to the type. With the latter the user can discover the actual complete type of every element, and may match the type in a switch-like construct. Unfortunately this is almost everything you can do with the type_info. Even the names of the type, printed in the last snippet, are compiler dependent and unreliable.
If you need to include additional information about the complete type of the elements, you can: density containers allow to customize the type used to do type-erasure. Here is the declaration of dense_list:

    template <typename ELEMENT = void, typename UNTYPED_ALLOCATOR = void_allocator, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
            class heterogeneous_array final;

The third template parameter can be used to specify a type other than the class template [runtime_type](http://peggysansonetti.it/tech/density/html/classdensity_1_1runtime__type.html), given that this type meets the requirements of RuntimeType. For example you may provide the capability of streaming objects to a std::ostream. Or, if your codebase is using a reflection mechanism, your custom RUNTIME_TYPE may wrap a pointer to your custom type_info-like class. Type erasure is a subset of reflection; density, by default stores about a type only what it needs to handle the lifetime of elements in a container.
 
heterogeneous queues
--------------------
Queues are special case containers. They restrict the modifications so that additions of new elements can be done only at the end, and removals can be done only at the beginning.
density provides two similar containrser for queue: 

        template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
            class dense_queue final
    template < typename ELEMENT = void, typename PAGE_ALLOCATOR = page_allocator<std::allocator<ELEMENT>>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
            class paged_queue final;
The difference between the two is the memory management: 
- [dense_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1dense__queue.html) uses a monolithic memory block, handling a capacity just like a vector does, and reallocating the whole block when it runs out of space. The memory block is managed like a ring buffer, so that insertions don't require to move all the existing elements.
- [paged_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1paged__queue.html) uses a page-based memory management: it allocates fixed-size pages and, when it runs out of space, it just need to allocate a new page, without moving any existing element (like dense_queue does). Page-based memory management is very powerful: it allows fast allocation and deallocations, and thread-local and unsynchronized unused page depots. Every page of a paged_queue is handled internally like a ring-buffer. Iterators are invalidated only when the pointed element is erased.
Both dense_queue and paged_queue provide forward iteration only: no constant-time access by index is possible. dense_queue is recommended for small queues, while paged_queue is best suited to middle and large sizes.

An example of application of dense_queue and paged_queue is built-in in density: [dense_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1dense__function__queue_3_01RET__VAL_07PARAMS_8_8_8_08_4.html) and [paged_function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1paged__function__queue_3_01RET__VAL_07PARAMS_8_8_8_08_4.html).
These two containers acts like queues of std::function objects, but are based on dense_queue and paged_queue:

			auto print_func = [](const char * i_str) { std::cout << i_str; };

			paged_function_queue<void()> queue_1;
			queue_1.push(std::bind(print_func, "hello "));
			queue_1.push([print_func]() { print_func("world!"); });
			queue_1.push([]() { std::cout << std::endl; });
			while (!queue_1.empty())
				queue_1.consume_front();

			paged_function_queue<int(double, double)> queue_2;
			queue_2.push([](double a, double b) { return static_cast<int>(a + b); });
			std::cout << "a + b = " << queue_2.consume_front(40., 2.) << std::endl;

Lifo data structures
====================
The first time a thread use a lifo data structure, density associates to it a *data stack*, that is an allocator in which only the most recently allocated block can be reallocated\deallocated. This LIFO constraint allows a straightforward and fast memory management. The data stack is a paged data structure (just like paged_list): pages are allocated when needed, and released soon when they become unused. 
The data stack can be used with the class thread_lifo_allocator. 

lifo_array<>
------------
lifo_array is the C++ version of variable length automatic array of C99:

		void dijkstra_path_find(const GraphNode * i_nodes, size_t i_node_count, size_t i_initial_node_index)
		{
			lifo_array<float> min_distance(i_node_count, std::numeric_limits<float>::max() );
			min_distance[i_initial_node_index] = 0.f;

			// ...

The size of the array is provided at construction time, and cannot be changed. lifo_array allocates its element contiguously in memory in the *data stack*. As a consequence a lifo_array:
- must be destroyed by the same thread that created it
- must be LIFO consistent: when the array is destroyed, its memory block must be the most recently allocated on the data stack
A violation of any of these two constraints leads to undefined behavior. To avoid it safely a stricter and simple constraint is highly recommended: 
- lifo_array are instantiated only in the automatic storage (on the callstack).
Instancing lifo_array's on the callstack is totally safe, and there is possibly no way to  mess with the LIFO constraint.
lifo_array cannot be neither moved nor copied. This is the declaration:

    template <typename TYPE, typename LIFO_ALLOCATOR = thread_lifo_allocator<>>
    		class lifo_array final;

By default lifo_array allocates on the data stack, but a different lifo allocator may be specified. For example, one may create a lifo_allocator parallel and indipendent from the thread local data stack.

lifo_buffer<>
-------------
A lifo_buffer is not a container, it's more low-level. It provides a dynamic raw storage. Unlike lifo_array, lifo_buffer allows reallocation (that is changing the size and the alignment of the buffer), but only of the last created lifo_buffer (otherwise the behavior is undefined).

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
