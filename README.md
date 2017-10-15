[![Build Status](https://travis-ci.org/giucamp/density.svg?branch=master)](https://travis-ci.org/giucamp/density)
[![Build status](https://ci.appveyor.com/api/projects/status/td8xk69gswc6vuct?svg=true)](https://ci.appveyor.com/project/GiuseppeCampana/density)
[![Coverage Status](https://coveralls.io/repos/github/giucamp/density/badge.svg)](https://coveralls.io/github/giucamp/density)

Density is a C++11 header-only library focused on paged memory management and concurrency, providing:

- lifo memory management
- heterogeneous queues
- function queues

Lifo memory management is provided with [lifo_array](http://giucamp.github.io/density/doc/html/classdensity_1_1lifo__array.html), [lifo_buffer](http://giucamp.github.io/density/doc/html/classdensity_1_1lifo__buffer.html), and [lifo_allocator](http://giucamp.github.io/density/doc/html/classdensity_1_1lifo__allocator.html).

<p align="right"><a href="http://giucamp.github.io/density/doc/html/index.html#lifo">More...</a></p>

Here is the table of all the available queues:

concurrency strategy|function queue|heterogeneous queue|Consumers cardinality|Producers cardinality
--------------- |------------------ |--------------------|--------------------|--------------------
single threaded   |[function_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1function__queue.html)      |[heter_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1heter__queue.html)| - | -
locking         |[conc_function_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1conc__function__queue.html) |[conc_hetr_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1conc__heter__queue.html)|multiple|multiple
lock-free       |[lf_function_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1lf__function__queue.html) |[lf_hetr_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1lf__heter__queue.html)|configurable|configurable
spin-locking    |[sp_function_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1sp__function__queue.html) |[sp_hetr_queue](http://giucamp.github.io/density/doc/html/classdensity_1_1sp__heter__queue.html)|configurable|configurable

<p align="right"><a href="http://giucamp.github.io/density/doc/html/index.html#queues">More...</a></p>

## Documentation
The [overview](http://giucamp.github.io/density/doc/html/index.html) should be enough for an effective use of the library. Of course there is a [reference](http://giucamp.github.io/density/doc/html/annotated.html) (generated from the master branch) for the details. The documentation includes the results of some benchmarks. 

## Support
This library is tested against these compilers:
- Msc (Visual Studio 2017)
- g++-4.9 and g++-7
- clang++-4.0 and clang++-5.0

## How to...
To use the library, just download or clone the repository, and then copy the directory "density" in your include paths. The library is composed only by headers, so there is nothing to build. The directory doc contains the html documentation.
In the test directory there is a test program you you can build with cmake or Visual Studio 2017.
In the bench directory there is a benchmark program you you can build with cmake or Visual Studio 2017.

<a href="mailto:giu.campana@gmail.com">[give feedback]</a>
