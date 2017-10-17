
branch          |build status
--------------- |------------------
master|[![Build Status](https://travis-ci.org/giucamp/density.svg?branch=master)](https://travis-ci.org/giucamp/density) [![Build status](https://ci.appveyor.com/api/projects/status/td8xk69gswc6vuct?svg=true)](https://ci.appveyor.com/project/GiuseppeCampana/density) [![Coverage Status](https://coveralls.io/repos/github/giucamp/density/badge.svg?branch=master)](https://coveralls.io/github/giucamp/density?branch=master)
develop|[![Build Status](https://travis-ci.org/giucamp/density.svg?branch=develop)](https://travis-ci.org/giucamp/density)[![Build status](https://ci.appveyor.com/api/projects/status/td8xk69gswc6vuct/branch/develop?svg=true)](https://ci.appveyor.com/project/GiuseppeCampana/density/branch/develop)[![Coverage Status](https://coveralls.io/repos/github/giucamp/density/badge.svg?branch=develop)](https://coveralls.io/github/giucamp/density?branch=develop)

Density is a C++11 header-only library focused on paged memory management and concurrency, providing:

* [lifo memory management](http://giucamp.github.io/density/doc/html/index.html#lifo):
  - [lifo_array](http://giucamp.github.io/density/doc/html/classdensity_1_1lifo__array.html)
  - [lifo_buffer](http://giucamp.github.io/density/doc/html/classdensity_1_1lifo__buffer.html)
  - [lifo_allocator](http://giucamp.github.io/density/doc/html/classdensity_1_1lifo__allocator.html)
* [heterogeneous queues and function queues](http://giucamp.github.io/density/doc/html/index.html#queues):

concurrency strategy|function queue|heterogeneous queue|Consumers cardinality|Producers cardinality
--------------- |------------------ |--------------------|--------------------|--------------------
single threaded   |[function_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1function__queue.html)      |[heter_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1heter__queue.html)| - | -
locking         |[conc_function_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1conc__function__queue.html) |[conc_hetr_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1conc__heter__queue.html)|multiple|multiple
lock-free       |[lf_function_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1lf__function__queue.html) |[lf_hetr_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1lf__heter__queue.html)|configurable|configurable
spin-locking    |[sp_function_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1sp__function__queue.html) |[sp_hetr_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1sp__heter__queue.html)|configurable|configurable

## Documentation
The [overview](http://giucamp.github.io/density/doc/html/index.html) should be enough for an effective use of the library. Of course there is a [reference](http://giucamp.github.io/density/doc/html/annotated.html) (generated from the master branch) for the details. The documentation includes the results of some benchmarks. 

## Support
Supported compilers:
- Msc (Visual Studio 2017)
- g++-4.9 and g++-7
- clang++-4.0 and clang++-5.0

The current version has not been tested on achitectures with weak memory ordering, so relaxed atomic operations are disabled with a global compile-time constant. Future versions will exploit relaxed atomics.

## How to...
To use the library, just download or clone the repository, and then copy the directory "density" in your include paths. The library is composed only by headers, so there is nothing to build. The directory doc contains the html documentation.

In the test directory there is a test program that you can build with cmake or Visual Studio 2017. Read [here](http://giucamp.github.io/density/doc/html/test_bench.html) for the details.

In the bench directory there is a benchmark program you you can build with cmake or Visual Studio 2017.

[This document](http://giucamp.github.io/density/doc/html/implementation.pdf) describes the implementation of the library.

<a href="mailto:giu.campana@gmail.com">[give feedback]</a>
