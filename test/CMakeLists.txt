cmake_minimum_required (VERSION 3.1)
project (density_test)

set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(".")
include_directories("../")

SET(COMPILER_EXTRA "" CACHE STRING "additional compiler options")

# the compiler used for C++ files 
MESSAGE( STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} )
MESSAGE( STATUS "CMAKE_CXX_COMPILER_ID: " ${CMAKE_CXX_COMPILER_ID} )

add_executable(density_test 
	examples/concurrent_heterogeneous_queue_examples.cpp
	examples/conc_func_queue_examples.cpp
	examples/func_queue_examples.cpp
	examples/heterogeneous_queue_examples.cpp
	examples/lf_func_queue_examples.cpp
	examples/lf_heterogeneous_queue_examples.cpp
	examples/misc_examples.cpp
	examples/lifo_examples.cpp
	examples/sp_func_queue_examples.cpp
	examples/sp_heterogeneous_queue_examples.cpp
	tests/generic_tests/conc_heter_queue_generic_tests.cpp
	tests/generic_tests/heter_queue_generic_tests.cpp
	tests/generic_tests/lf_heter_queue_generic_tests_relaxed.cpp
	tests/generic_tests/lf_heter_queue_generic_tests_seqcst.cpp
	tests/generic_tests/queue_generic_tests.cpp
	tests/generic_tests/sp_heter_queue_generic_tests_seqcst.cpp
	tests/concurrent_heterogeneous_queue_basic_tests.cpp
	tests/heterogeneous_queue_basic_tests.cpp
	tests/lf_heterogeneous_queue_basic_tests.cpp
	tests/load_unload_tests.cpp
	tests/sp_heterogeneous_queue_basic_tests.cpp
	tests/lifo_tests.cpp
	test_framework/density_test_common.cpp
	test_framework/dynamic_type.cpp
	test_framework/exception_tests.cpp
	test_framework/histogram.cpp
	test_framework/line_updater_stream_adapter.cpp
	test_framework/progress.cpp
	test_framework/shared_block_registry.cpp
	test_framework/test_allocators.cpp
	test_framework/test_objects.cpp
	test_framework/threading_extensions.cpp
	main.cpp )

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	MESSAGE( STATUS "will compile with clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${COMPILER_EXTRA}")
	TARGET_LINK_LIBRARIES(density_test pthread)	
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	MESSAGE( STATUS "will compile with g++")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${COMPILER_EXTRA}")
	TARGET_LINK_LIBRARIES(density_test pthread)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	MESSAGE( STATUS "will compile with msvc")
	add_compile_options(/bigobj)
endif()

install (TARGETS density_test DESTINATION bin)
