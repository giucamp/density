cmake_minimum_required (VERSION 3.1)
project (density_test)

OPTION(DENSITY_DEBUG "Enable debug checks" OFF)
OPTION(ENABLE_TEST_DATA_STACK "Enable test data stack" OFF)

IF(DENSITY_DEBUG)
    MESSAGE("DENSITY_DEBUG is ON")
    ADD_DEFINITIONS(-DDENSITY_DEBUG)
    ADD_DEFINITIONS(-DDENSITY_DEBUG_INTERNAL)
ENDIF(DENSITY_DEBUG)

IF(ENABLE_TEST_DATA_STACK)
    MESSAGE("ENABLE_TEST_DATA_STACK is ON")
    ADD_DEFINITIONS(-DDENSITY_USER_DATA_STACK)
ENDIF(ENABLE_TEST_DATA_STACK)

include_directories(".")
include_directories("../")

SET(COMPILER_EXTRA "" CACHE STRING "additional compiler options")

# the compiler used for C++ files 
MESSAGE( STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} )
MESSAGE( STATUS "CMAKE_CXX_COMPILER_ID: " ${CMAKE_CXX_COMPILER_ID} )

IF(DEFINED ${CMAKE_BUILD_TYPE})
    IF(${CMAKE_BUILD_TYPE} STREQUAL Debug)
        message("debug mode")
        ADD_DEFINITIONS(-DDENSITY_DEBUG)
        ADD_DEFINITIONS(-DDENSITY_DEBUG_INTERNAL)
    ELSE(${CMAKE_BUILD_TYPE} STREQUAL Debug)
        message("release mode")
    ENDIF(${CMAKE_BUILD_TYPE} STREQUAL Debug)
ENDIF(DEFINED ${CMAKE_BUILD_TYPE})

add_executable(density_test
	../density/detail/function_runtime_type.h
	../density/detail/lf_queue_base.h
	../density/detail/lf_queue_head_multiple.h
	../density/detail/lf_queue_head_single.h
	../density/detail/lf_queue_tail_multiple_relaxed.h
	../density/detail/lf_queue_tail_multiple_seq_cst.h
	../density/detail/lf_queue_tail_single.h
	../density/detail/page_allocator.h
	../density/detail/page_stack.h
	../density/detail/singleton_ptr.h
	../density/detail/sp_queue_tail_multiple.h
	../density/detail/system_page_manager.h
	../density/detail/wf_page_stack.h
    ../density/detail/runtime_type_internals.h
	../density/conc_function_queue.h
	../density/conc_heter_queue.h
	../density/default_allocator.h
	../density/density_common.h
	../density/density_config.h
	../density/function_queue.h
	../density/heter_queue.h
	../density/io_runtimetype_features.h
	../density/lf_function_queue.h
	../density/lf_heter_queue.h
	../density/lifo.h
	../density/raw_atomic.h
	../density/runtime_type.h
	../density/sp_function_queue.h
	../density/sp_heter_queue.h
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
    tests/type_fetaures_tests.cpp
    tests/user_data_stack.cpp
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
	test_framework/allocator_stress_test.cpp
    test_settings.cpp
	main.cpp )

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	MESSAGE( STATUS "will compile with clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g ${COMPILER_EXTRA}")
	TARGET_LINK_LIBRARIES(density_test pthread)	
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	MESSAGE( STATUS "will compile with g++")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -rdynamic ${COMPILER_EXTRA}")
	TARGET_LINK_LIBRARIES(density_test pthread)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	MESSAGE( STATUS "will compile with msvc")
	add_compile_options(/bigobj)
endif()

install (TARGETS density_test DESTINATION bin)
