cmake_minimum_required (VERSION 2.6)
project (density_test)

add_subdirectory(testity)

add_executable(density_test 
	benchmarks/bench_allocator.cpp
	benchmarks/bench_function_queue.cpp
	benchmarks/bench_lifo.cpp
	benchmarks/bench_queues.cpp
	functionality_cases/cases_any.cpp
	functionality_cases/cases_concurrent_function_queue.cpp
	functionality_cases/cases_concurrent_heterogeneous_queue.cpp
	functionality_cases/cases_heterogeneous_array.cpp
	functionality_cases/cases_lifo.cpp
	functionality_cases/cases_queue.cpp
	main.cpp )
target_include_directories(density_test PUBLIC ".." "testity")
add_dependencies(density_test testity)
target_link_libraries(density_test testity)