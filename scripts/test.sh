#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

RUN=$1
BUILD_TYPE=$2
CPP_STD=$3
TEST_DATA_STACK=$4
GCOV=$5
PARAMS=$6
COMPILER_PARAMS=$7

echo "RUN=$RUN, BUILD_TYPE=$BUILD_TYPE, TEST_DATA_STACK=$TEST_DATA_STACK, GCOV=$GCOV, PARAMS=$PARAMS, COMPILER_PARAMS=$COMPILER_PARAMS"
echo "CPP_STD = $CPP_STD"

# config for thread sanitizer
export TSAN_OPTIONS="halt_on_error=1 history_size=4"

# GCC_OPTIONS
GCC_OPTIONS=""
if [ "$GCOV" == "gcov-7" ]; then
    GCC_OPTIONS+=" -fprofile-arcs -ftest-coverage"
fi
GCC_OPTIONS+=$COMPILER_PARAMS
echo "GCC_OPTIONS = $GCC_OPTIONS"

# MAKE_ARGS
MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=$BUILD_TYPE"
if [ "$TEST_DATA_STACK" = "TRUE" ]; then
    MAKE_ARGS+=" -DENABLE_TEST_DATA_STACK:BOOL=ON"
fi
if [ "$BUILD_TYPE" = "Debug" ]; then
    MAKE_ARGS+=" -DDENSITY_DEBUG:BOOL=ON"
fi
MAKE_ARGS+=" -DCMAKE_CXX_STANDARD=$CPP_STD"
MAKE_ARGS+=" -DCOMPILER_EXTRA:STRING=$GCC_OPTIONS"
echo "MAKE_ARGS = $MAKE_ARGS"

# build
cd test
mkdir build || true
cd build
cmake $MAKE_ARGS  ..
make

# run
if [ "$RUN" = "TRUE" ]; then
    $PWD/density_test $PARAMS
fi

# run coveralls
cd ../..
if [ "$GCOV" == "gcov-7" ]; then
    gcov-7 -version
    coveralls --gcov '/usr/bin/gcov-7' --gcov-options '\-lp' -b test/build/CMakeFiles/density_test.dir
fi
