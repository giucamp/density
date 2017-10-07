cmake_minimum_required (VERSION 3.1)
project (density_test)

set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(".")
include_directories("../")

# the compiler used for C++ files 
MESSAGE( STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} )
MESSAGE( STATUS "CMAKE_CXX_COMPILER_ID: " ${CMAKE_CXX_COMPILER_ID} )

add_executable(density_bench
    bench_framework/performance_test.cpp
    bench_framework/test_session.cpp
    bench_framework/test_tree.cpp
    tests/lifo_tests.cpp
    tests/muilti_thread_tests.cpp
    tests/single_thread_tests.cpp
    main.cpp )

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	MESSAGE( STATUS "will compile with clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	TARGET_LINK_LIBRARIES(density_test pthread)	
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	MESSAGE( STATUS "will compile with g++")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	TARGET_LINK_LIBRARIES(density_test pthread)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	MESSAGE( STATUS "will compile with msvc")
	add_compile_options(/bigobj)
endif()

install (TARGETS density_test DESTINATION bin)
