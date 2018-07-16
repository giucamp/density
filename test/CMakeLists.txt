cmake_minimum_required (VERSION 3.1)
project (density_test)

OPTION(DENSITY_DEBUG "Enable debug checks" OFF)
OPTION(TEST_DATA_STACK "Enable test data stack" OFF)
OPTION(THREAD_SANITIZER "Enable thread sanitizer" OFF)
OPTION(CODE_COVERAGE "Enable code coverage" OFF)

MESSAGE( STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} )
MESSAGE( STATUS "CMAKE_CXX_COMPILER_ID: " ${CMAKE_CXX_COMPILER_ID} )

if(NOT "${CMAKE_CXX_STANDARD}")
  set(CMAKE_CXX_STANDARD 11)
  MESSAGE("c++ standard = ${CMAKE_CXX_STANDARD} (Defaulted)")
else()
  MESSAGE("c++ standard = ${CMAKE_CXX_STANDARD}")
endif()


IF(DENSITY_DEBUG)
    MESSAGE("DENSITY_DEBUG is ON")
    ADD_DEFINITIONS(-DDENSITY_DEBUG)
    ADD_DEFINITIONS(-DDENSITY_DEBUG_INTERNAL)
ENDIF()

IF(TEST_DATA_STACK)
    MESSAGE("TEST_DATA_STACK is ON")
    ADD_DEFINITIONS(-DDENSITY_USER_DATA_STACK)
ENDIF()

include_directories(".")
include_directories("../include")

SET(COMPILER_EXTRA "" CACHE STRING "additional compiler options")

IF(DEFINED ${CMAKE_BUILD_TYPE})
    IF(${CMAKE_BUILD_TYPE} STREQUAL Debug)
        message("debug mode")
        ADD_DEFINITIONS(-DDENSITY_DEBUG)
        ADD_DEFINITIONS(-DDENSITY_DEBUG_INTERNAL)
    ELSE()
        message("release mode")
    ENDIF()
ENDIF()

add_executable(density_test
	../include/density/detail/function_runtime_type.h
	../include/density/detail/lf_queue_base.h
	../include/density/detail/lf_queue_head_multiple.h
	../include/density/detail/lf_queue_head_single.h
	../include/density/detail/lf_queue_tail_multiple_relaxed.h
	../include/density/detail/lf_queue_tail_multiple_seq_cst.h
	../include/density/detail/lf_queue_tail_single.h
	../include/density/detail/page_allocator.h
	../include/density/detail/page_stack.h
	../include/density/detail/singleton_ptr.h
	../include/density/detail/sp_queue_tail_multiple.h
	../include/density/detail/system_page_manager.h
	../include/density/detail/wf_page_stack.h
    ../include/density/detail/runtime_type_internals.h
	../include/density/conc_function_queue.h
	../include/density/conc_heter_queue.h
	../include/density/default_allocator.h
	../include/density/density_common.h
	../include/density/density_config.h
	../include/density/function_queue.h
	../include/density/heter_queue.h
	../include/density/io_runtimetype_features.h
	../include/density/lf_function_queue.h
	../include/density/lf_heter_queue.h
	../include/density/lifo.h
	../include/density/raw_atomic.h
	../include/density/runtime_type.h
	../include/density/sp_function_queue.h
	../include/density/sp_heter_queue.h
	examples/concurrent_heterogeneous_queue_examples.cpp
	examples/conc_func_queue_examples.cpp
	examples/func_queue_examples.cpp
	examples/heterogeneous_queue_examples.cpp
	examples/lf_func_queue_examples.cpp
	examples/lf_heterogeneous_queue_examples.cpp
	examples/misc_examples.cpp
	examples/lifo_examples.cpp
    examples/any.h
    examples/any_tests.cpp
    examples/runtime_type_examples.cpp
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

MESSAGE( STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	MESSAGE( STATUS "will compile with clang")
	IF(THREAD_SANITIZER)
		MESSAGE( STATUS "THREAD SANITIZER IS ON")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
	ENDIF()
	TARGET_LINK_LIBRARIES(density_test pthread)	
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	MESSAGE( STATUS "will compile with g++")
	IF(THREAD_SANITIZER)
		MESSAGE( STATUS "THREAD SANITIZER IS ON")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fuse-ld=gold")
	ENDIF()
	IF(CODE_COVERAGE)
		MESSAGE( STATUS "CODE COVERAGE IS ON")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
	ENDIF()
	TARGET_LINK_LIBRARIES(density_test pthread)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	MESSAGE( STATUS "will compile with msvc")
	add_compile_options(/bigobj)
endif()

install (TARGETS density_test DESTINATION bin)
