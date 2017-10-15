#!/bin/bash

RUN=$1
DEBUG=$2
TEST_DATA_STACK=$3
GCOV=$4
PARAMS=$5

echo "RUN=$RUN, DEBUG=$DEBUG, TEST_DATA_STACK=$TEST_DATA_STACK, GCOV=$GCOV, PARAMS=$PARAMS"

if [ "$DEBUG" = "TRUE" ]; then
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Debug"
fi

if [ "$TEST_DATA_STACK" = "TRUE" ]; then
    MAKE_ARGS+=" -DENABLE_TEST_DATA_STACK:BOOL=ON"
fi

if [ "$GCOV" != "" ]; then
    GCC_OPTIONS+=" -fprofile-arcs -ftest-coverage"
fi

echo "MAKE_ARGS = $MAKE_ARGS"

cd test
mkdir build
cd build
cmake $MAKE_ARGS -DCOMPILER_EXTRA:STRING="$GCC_OPTIONS" ..
make
if [ "$RUN" = "TRUE" ]; then
    sudo make install
    density_test $PARAMS
fi

cd ../..
if [ "$GCOV" == "gcov-7" ]; then
    gcov-7 -version
    coveralls --verbose --gcov '/usr/bin/gcov-7' --gcov-options '\-lp' -b test/build/CMakeFiles/density_test.dir
fi
