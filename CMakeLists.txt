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

include_directories("test")
include_directories("include")

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
	include/density/detail/function_runtime_type.h
	include/density/detail/lf_queue_base.h
	include/density/detail/lf_queue_head_multiple.h
	include/density/detail/lf_queue_head_single.h
	include/density/detail/lf_queue_tail_multiple_relaxed.h
	include/density/detail/lf_queue_tail_multiple_seq_cst.h
	include/density/detail/lf_queue_tail_single.h
	include/density/detail/page_allocator.h
	include/density/detail/page_stack.h
	include/density/detail/singleton_ptr.h
	include/density/detail/sp_queue_tail_multiple.h
	include/density/detail/system_page_manager.h
	include/density/detail/wf_page_stack.h
	include/density/detail/runtime_type_internals.h
	include/density/conc_function_queue.h
	include/density/conc_heter_queue.h
	include/density/default_allocator.h
	include/density/density_common.h
	include/density/density_config.h
	include/density/function_queue.h
	include/density/heter_queue.h
	include/density/io_runtimetype_features.h
	include/density/lf_function_queue.h
	include/density/lf_heter_queue.h
	include/density/lifo.h
	include/density/raw_atomic.h
	include/density/runtime_type.h
    include/density/dynamic_reference.h
	include/density/sp_function_queue.h
	include/density/sp_heter_queue.h
	test/examples/conc_queue_examples.cpp
	test/examples/conc_func_queue_examples.cpp
	test/examples/func_queue_examples.cpp
	test/examples/heter_queue_examples.cpp
	test/examples/lf_func_queue_examples.cpp
	test/examples/lf_queue_examples.cpp
	test/examples/misc_examples.cpp
	test/examples/overview_examples.cpp
	test/examples/lifo_examples.cpp
	test/examples/any.h
	test/examples/any_tests.cpp
	test/examples/runtime_type_examples.cpp
	test/examples/sp_func_queue_examples.cpp
	test/examples/sp_queue_examples.cpp
    test/examples/dynamic_reference_examples.cpp
	test/tests/generic_tests/conc_heter_queue_generic_tests.cpp
	test/tests/generic_tests/heter_queue_generic_tests.cpp
	test/tests/generic_tests/lf_heter_queue_generic_tests_relaxed.cpp
	test/tests/generic_tests/lf_heter_queue_generic_tests_seqcst.cpp
	test/tests/generic_tests/queue_generic_tests.cpp
	test/tests/generic_tests/sp_heter_queue_generic_tests_seqcst.cpp
	test/tests/concurrent_heterogeneous_queue_basic_tests.cpp
	test/tests/heterogeneous_queue_basic_tests.cpp
	test/tests/lf_heterogeneous_queue_basic_tests.cpp
	test/tests/load_unload_tests.cpp
	test/tests/sp_heterogeneous_queue_basic_tests.cpp
	test/tests/lifo_tests.cpp
	test/tests/type_fetaures_tests.cpp
	test/tests/user_data_stack.cpp
	test/test_framework/density_test_common.cpp
	test/test_framework/dynamic_type.cpp
	test/test_framework/exception_tests.cpp
	test/test_framework/histogram.cpp
	test/test_framework/line_updater_stream_adapter.cpp
	test/test_framework/progress.cpp
	test/test_framework/shared_block_registry.cpp
	test/test_framework/test_allocators.cpp
	test/test_framework/test_objects.cpp
	test/test_framework/threading_extensions.cpp
	test/test_framework/allocator_stress_test.cpp
	test/test_settings.cpp
	test/main.cpp )

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
