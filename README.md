

Density Overview
================
What's Density
--------------
Density is a C++ header-only library that provides heterogeneous containers and lifo memory management. The key idea of density is allocating objects tightly in memory: this benefits both construction time and access memory locality.
Motivations
-------------------
Heterogeneous containers are very frequent in production code. The best standard way to model them probably is a vector of smart pointers (or, if elements are not all covariant to a common type, a vector of any's, either using std::any of C++17, or boost::any). While this approach allows easy and fast insertion and removals, it fragments the memory, since every object is allocated in a different memory block.
Another very common need is a queue of type-erased functions. In this case a vector or deque of std::function will do, but it may both fragment memory or waste space inside the storage of the container.
A third and yet common need is an automatic storage whose size is known only at runtime. C99 provides variable-length automatic arrays, that is:

    void do_word(int i_length)
    {
    	int temp_storage[i_length];
    	...

In C+++, alloca, an old-fashioned compiler extension, provides a similar functionality, but it is not standard, and may result in stack overflow if the requested space is huge.

Heterogeneous lists
------------------
density provides dense_list, a container whose elements can be of different type:

    auto list = dense_list<>::make(3, std::string("abc"), 5.f);
 This code creates a list.
Heterogeneous queue and function queue.
A dense_list has a very small inline storage: that it is guaranteed to be big like a pointer. Only in the. This is in constast with common implemenatuion of std::vector
 (sizeof(dense_list) ==sizeof(void*)). Anyway, if you specify a custom allocator, and this allocator.


Lifo memory management
----------------------

 lifo_array and lifo_buffer provide lifo memory management.

Exception guarantees
--------------------
Exceptions are the best way of handling errors in C++.
All the classes (and class templates) of density provide either the no-throw exception guarantee (no exception is possibly thrown), or the strong exception guarantee (if an exception is raised during a function, the call has no visible effect). 

> Written with [StackEdit](https://stackedit.io/).
