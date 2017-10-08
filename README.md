
## Build status

[![Build Status](https://travis-ci.org/giucamp/density.svg?branch=master)](https://travis-ci.org/giucamp/density)
[![Build status](https://ci.appveyor.com/api/projects/status/td8xk69gswc6vuct?svg=true)](https://ci.appveyor.com/project/GiuseppeCampana/density)

Density is a C++11 header-only library focused on paged memory management and concurrency. 

- lifo memory management
- heterogeneous queues
- function queues

concurrency strategy|function queue|heterogeneous queue|Consumers cardinality|Producers cardinality
--------------- |------------------ |--------------------|--------------------|--------------------
single threaded   |[function_queue](classdensity_1_1function__queue.html)      |[heter_queue](classdensity_1_1heter__queue.html)| - | -
locking         |[conc_function_queue](classdensity_1_1conc__function__queue.html) |[conc_hetr_queue](classdensity_1_1conc__heter__queue.html)|multiple|multiple
lock-free       |[lf_function_queue](classdensity_1_1lf__function__queue.html) |[lf_hetr_queue](classdensity_1_1lf__heter__queue.html)|configurable|configurable
spin-locking    |[sp_function_queue](classdensity_1_1sp__function__queue.html) |[sp_hetr_queue](classdensity_1_1sp__heter__queue.html)|configurable|configurable

Lifo memory management is prvided with [lifo_array](classdensity_1_1lifo__array.html), [lifo_buffer](classdensity_1_1lifo__buffer.html), and [lifo_allocator](classdensity_1_1lifo__allocator.html)

[Overview](http://giucamp.github.io/density/doc/html/intro.html)
[Reference](http://giucamp.github.io/density/doc/html/annotated.html) - generated from the master branch

This library is tested against these compilers:
- Msc (Visual Studio 2017)
- g++-4.9 and g++-7
- clang++-4.0 and clang++-5.0
