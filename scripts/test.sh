#!/bin/bash

RUN=$1
DEBUG=$2
GCOV=$3

echo "RUN = $RUN, DEBUG = $DEBUG, GCOV = $GCOV"

if [ "$DEBUG" = "TRUE" ]; then
    MAKE_ARGS+=" -DCMAKE_BUILD_TYPE=Debug"
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
if [ "$COVERAGE" = "TRUE" ]; then
    $GCOV -version
    coveralls --verbose --gcov '$GCOV' --gcov-options '\-lp' -b test/build/CMakeFiles/density_test.dir
fi
