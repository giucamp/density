#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

if [ "$1" == "Gcc49" ]; then
    # CXX=g++-4.9 && CPP_STD=11 && TEST_BUILD=Release && TEST_RUN=TRUE && TEST_DATA_STACK=FALSE && GCOV=no && BENCH_RUN=TRUE && TEST_PARAMS=-queue_tests_cardinality:25000
    export CXX=g++-4.9
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Release"
    PARAMS="-queue_tests_cardinality:25000"
    TEST_RUN=TRUE
    BENCH_RUN=TRUE
elif [ "$1" == "Gcc7_Debug_gcov" ]; then
    #CXX=g++-7 && CPP_STD=11 && TEST_BUILD=Debug && TEST_RUN=TRUE && TEST_DATA_STACK=TRUE && GCOV=gcov-7 && BENCH_RUN=TRUE && TEST_PARAMS=-queue_tests_cardinality:15000
    export CXX=g++-7
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Debug"
    MAKE_ARGS+=" -DDENSITY_DEBUG:BOOL=ON"
    MAKE_ARGS+=" -DTEST_DATA_STACK:BOOL=ON"
    MAKE_ARGS+=" -DCODE_COVERAGE:BOOL=ON"
    PARAMS="-queue_tests_cardinality:15000"
    TEST_RUN=TRUE
    BENCH_RUN=FALSE
elif [ "$1" == "clang4" ]; then
    #CXX=clang++-4.0 && CPP_STD=11 && TEST_BUILD=Release && TEST_RUN=TRUE && TEST_DATA_STACK=TRUE && BENCH_RUN=TRUE && GCOV=no && TEST_PARAMS=
    export CXX=clang++-4.0
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Release"
    PARAMS="-queue_tests_cardinality:25000"
    TEST_RUN=TRUE
    BENCH_RUN=TRUE
elif [ "$1" == "clang5_Debug" ]; then
    #CXX=clang++-5.0 && CPP_STD=11 && TEST_BUILD=Debug && TEST_RUN=TRUE && TEST_DATA_STACK=FALSE && GCOV=no && BENCH_RUN=FALSE && TEST_PARAMS=-queue_tests_cardinality:10000
    export CXX=clang++-5.0
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Debug"
    MAKE_ARGS+=" -DDENSITY_DEBUG:BOOL=ON"
    MAKE_ARGS+=" -DTEST_DATA_STACK:BOOL=ON"
    PARAMS="-queue_tests_cardinality:10000"
    TEST_RUN=TRUE
    BENCH_RUN=FALSE
elif [ "$1" == "clang5_thread_sanitizer" ]; then
    # CXX=clang++-5.0 && CPP_STD=11 && TEST_BUILD=Release && TEST_RUN=TRUE && TEST_DATA_STACK=FALSE && BENCH_RUN=FALSE && GCOV=no && TEST_PARAMS=\"\" && COMPILER_PARAMS=-fsanitize=thread
    export CXX=clang++-5.0
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Release"
    MAKE_ARGS+=" -DTEST_DATA_STACK:BOOL=OFF"
    MAKE_ARGS+=" -DTHREAD_SANITIZER:BOOL=ON"
    MAKE_ARGS+=" -DCODE_COVERAGE:BOOL=OFF"
    PARAMS="-test_allocators:0 -queue_tests_cardinality:20000 -exclude:lifo,queue,conc_queue,page_def"
    TEST_RUN=TRUE
    BENCH_RUN=FALSE
elif [ "$1" == "Gcc7_thread_sanitizer" ]; then
    #CXX=g++-7 && CPP_STD=11 && TEST_BUILD=Release && TEST_RUN=TRUE && TEST_DATA_STACK=FALSE && BENCH_RUN=FALSE && GCOV=no && TEST_PARAMS=\"-test_allocators:0 -queue_tests_cardinality:10000 -exclude:lifo,queue,conc_queue,page_def
    export CXX=g++-7
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Release"
    MAKE_ARGS+=" -DTHREAD_SANITIZER:BOOL=ON"
    PARAMS="-test_allocators:0 -queue_tests_cardinality:10000 -exclude:lifo,queue,conc_queue,page_def"
    TEST_RUN=TRUE
    BENCH_RUN=FALSE
elif [ "$1" == "Gcc7_Debug_cpp14" ]; then
    export CXX=g++-7
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Debug"
    MAKE_ARGS+=" -DCMAKE_CXX_STANDARD=14"
    TEST_RUN=FALSE
    BENCH_RUN=FALSE
elif [ "$1" == "Gcc7_Debug_cpp17" ]; then
    export CXX=g++-7
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Debug"
    MAKE_ARGS+=" -DCMAKE_CXX_STANDARD=17"
    TEST_RUN=FALSE
    BENCH_RUN=FALSE
fi

echo "CXX=$CXX"
echo "MAKE_ARGS=$MAKE_ARGS"
echo "TEST_RUN=$TEST_RUN"
echo "PARAMS=$PARAMS"
echo "BENCH_RUN=$BENCH_RUN"

# config for thread sanitizer
export TSAN_OPTIONS="halt_on_error=1 history_size=4"

# build
cd test
mkdir build || true
cd build
cmake $MAKE_ARGS  ..
#make VERBOSE=1
make

# run test
if [ "$TEST_RUN" = "TRUE" ]; then
    $PWD/density_test "$PARAMS"
fi

# run coveralls
cd ../..
if [ "$GCOV" == "gcov-7" ]; then
    gcov-7 -version
    coveralls --gcov '/usr/bin/gcov-7' --gcov-options '\-lp' -b test/build/CMakeFiles/density_test.dir
fi

# run bench
if [ "$BENCH_RUN" = "TRUE" ]; then

    DIR=$(dirname "$0")
    cd "$DIR"
    cd ../bench
    mkdir build || true
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..  
    make
    $PWD/density_bench

fi

