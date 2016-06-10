

Density Overview
================

What's Density
==============

Density is a C++11 header-only library that provides heterogeneous containers and lifo memory management. The key ideas are:
- allocating objects of heterogeneous type linearly and tightly in memory: this is beneficial for construction time, access memory locality, and global memory footprint.
- superseding the *fixed-then-dynamic storage* pattern: that is, when you need to store N elements, you dedicate a fast-to-allocate fixed-sized storage big M. Then, when using it, if N > M, you allocate another storage on dynamic memory with size N, and leave the fixed storage unused. This pattern is used in typical implementation of std::any, std::function, and is a very frequent as optimization for production code when a temporary automatic storage is need. density provides a modern replacement for the C-ish and non-standard [alloca](http://man7.org/linux/man-pages/man3/alloca.3.html), similar to C99 variable lenght automatic arrays, but more C++ish.
- providing at least the strong exception guarantee for every single function. Exceptions are the easiest, safest and fastest way of handling errors, and applications should have a reliable behavior even in case of exception.

dense_list<>
============

Heterogeneous containers are very frequent in production code. Using just the standard library, the best way to model a polymorphic container is probably a vector of smart pointers.
density provides dense_list as an alternative, a container whose elements can be of different type:

			struct Widget { virtual void draw() { /* ... */ } };
			struct TextWidget : Widget { virtual void draw() override { /* ... */ } };
			struct ImageWidget : Widget { virtual void draw() override { /* ... */ } };

			auto widgets = dense_list<Widget>::make(TextWidget(), ImageWidget(), TextWidget());
			for (auto & widget : widgets)
			{
				widget.draw();
			}
			
This snippet creates and draws a list of 3 widgets. Widget is the *element base type*. Trying to add a type not covariant to Widget will cause the failure of a static cast.
Elements can be added/removed to/from a dense_list in any time just like a std::vector:

    widgets.push_back(TextWidget());

Anyway the current implementation of dense_list does not handle unused capacity, so insertions and removals always cause a reallocation and have linear complexity, in contrast to the constant amortized time of many vector functions of std::vector. 
dense_list is recommended over a vector of smart pointers when the container is never or not-so-often modified, and when a compact memory layout is desired. A dense_list has the same inline storage of a raw pointer (unless you explicitly specify an allocator with a state), and does not allocate memory when empty.

If the element base type is void (the default), then any type can be added to the list:

	auto list = dense_list<>::make(3 + 5, string("abc"), 42.f);
			list.push_front(wstring(L"ABC"));
			for (auto it = list.begin(); it != list.end(); it++)
			{
				cout << it.complete_type().type_info().name() << endl;
			} 
			
This snippet creates a list with 3 elements, and then adds another one in front. Then the type of every element is written on std::cout. With a dense_list<void> range loops cannot be used, because the indirection of the iterator returns void. Anyway iterator.element() returns a void pointer to the complete element.
In the last snippet the function complete_type() of the iterartor is used. It returns a density::runtime_type, an object wich is used as type eraser: it is able to construct, destroy, copy, move, retrieve size and alignment of a type. The function type_info() of runtime_type returns the [std::type_info](http://en.cppreference.com/w/cpp/types/type_info) associated to the type. With the latter the user may match the type in a switch-like construct, and little more. Actually, even the names of the type, printed in the snippet, are useless, since the compiler has no requirements at all about the returned string.
density containers allows to customize the type used to do type-erasure. Here is the declaration of dense_list:

    template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
        class dense_list final;

The third template parameter can be used to specify a type other than the class template density::runtime_type, given that this type meets the requirements of RuntimeType. A custom RUNTIME_TYPE alows to store custom information associate with the type. For example you may provide the capability of streaming objects to a std::ostream. Or, if your codebase is using a reflection mechanism, your custom RUNTIME_TYPE may wrap a pointer to your custom type_info. Actually type erasure is a subset of reflection.
 

heterogeneous queues
====================

Queues are special case containers. They restrict the modifications so that additions of new elements can be done only at the end, and removals can be done only at the beginning.
density provides two similar containrser for queue: 

        template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
            class dense_queue final
    template < typename ELEMENT = void, typename PAGE_ALLOCATOR = page_allocator<std::allocator<ELEMENT>>, typename RUNTIME_TYPE = runtime_type<ELEMENT> >
            class paged_queue final;
The difference between the two is the memory management: 
- dense_queue uses a monolithic memory block, handling a capacity just like a vector does, and reallocating the whole block when it runs out of space. The memory block is managed like a ring buffer, so that insertions don't require to move all the existing elements.
- paged_queue uses a page-based memory management: it allocates fixed-size pages and, when it runs out of space, it just need to allocate a new page, without moving any existing element (like dense_queue does). Furthermore, page-based memory management very powerful: it allows fast allocation and deallocations, thread-local and unsynchronized unused page stores. Every page internally is handled like a ring-buffer. Iterators are invalidated only when the pointed element is erased.
Both dense_queue and paged_queue provides forward iteration only: no constant-time access by index is possible. dense_queue is recommended for small queues, while paged_queue is best suited to middle and large sizes.
A first application of dense_queue and paged_queue is built-in in density:

    template < typename FUNCTION > class dense_function_queue final;
    template < typename FUNCTION > class paged_function_queue final;

These two containers acts like queues of std::function objects, but are based on dense_queue and paged_queue:

			paged_function_queue<void()> queue_1;
			queue_1.push([]() { std::cout << "hello!" << std::endl; });
			queue_1.consume_front();

			paged_function_queue<int(double, double)> queue_2;
			queue_2.push([](double a, double b) { return static_cast<int>(a + b); });
			std::cout << queue_2.consume_front(40., 2.) << std::endl;

A dense_list has a very small inline storage: that it is guaranteed to be big like a pointer, provided that the allocator is stateless. Only in the. This is in contrast with common implementations of std::vector, that are big as 3 pointers.
 (sizeof(dense_list) ==sizeof(void*)). Anyway, if you specify a custom allocator, and this allocator.


Lifo data structures
====================

The first time a thread use a lifo data structure, density associates to it a *data stack*, that is an allocator in which only the most recently allocated block can be reallocated\deallocated. This LIFO constraint allows a straightforward and fast memory management. The data stack is a paged data structure (just like paged_list): pages are allocated when needed, and released soon when they become unused. The current implementation does not have a thread local cache of pages, but next releases will.
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
lifo_array cannot be neither moved nor copied.

    template <typename TYPE, typename LIFO_ALLOCATOR = thread_lifo_allocator<>>
    		class lifo_array final;

By default lifo_array allocates on the data stack, but a different lifo allocator may be specified. For example, one may create a lifo_allocator parallel and indipendent from the thread local data stack.

lifo_buffer<>
-------------
A lifo_buffer is not a container, it is more low-level. It provides a dynamic raw storage. Unlike lifo_array, lifo_buffer allow reallocation (that is changing the size and the alignment of the buffer).

    void string_io()
   	{
   		using namespace std;
   		vector<string> strings{ "string", "long string", "very long string", "much longer string!!!!!!" };
   		uint32_t len = 0;
   
   		auto file = tmpfile();
   		for (const auto & str : strings)
   		{
   			len = static_cast<uint32_t>( str.length() + 1);
   			fwrite(&len, sizeof(len), 1, file);
   			fwrite(str.c_str(), 1, len, file);
   		}
   
   		rewind(file);
   		lifo_buffer<> buff(8);
   		while (fread(&len, sizeof(len), 1, file) == 1)
   		{
   			buff.resize(len);
   			fread(buff.data(), len, 1, file);
   			cout << static_cast<char*>(buff.data()) << endl;
   		}
   
   		fclose(file);
   	}
Like lifo_array, lifo_buffer instances must be LIFO consistent, otherwise the behavior of the program is undefined.

> Written with [StackEdit](https://stackedit.io/).
