#!/bin/bash

RUN=$1
DEBUG=$2
TEST_DATA_STACK=$3
GCOV=$4

echo "RUN = $RUN, DEBUG = $DEBUG, GCOV = $GCOV"

if [ "$DEBUG" = "TRUE" ]; then
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Debug"
fi

if [ "$TEST_DATA_STACK" = "TRUE" ]; then
    MAKE_ARGS+=" -DENABLE_TEST_DATA_STACK=ON"
fi

if [ "$GCOV" != "" ]; then
    GCC_OPTIONS+=" -fprofile-arcs -ftest-coverage"
fi

cd test
mkdir build
cd build
cmake "$MAKE_ARGS" -DCOMPILER_EXTRA:STRING="$GCC_OPTIONS" ..
make
if [ "$RUN" = "TRUE" ]; then
    sudo make install
    density_test
fi

cd ../..
if [ "$GCOV" != "" ]; then
    $GCOV -version
    coveralls --verbose --gcov '$GCOV' --gcov-options '\-lp' -b test/build/CMakeFiles/density_test.dir
fi
