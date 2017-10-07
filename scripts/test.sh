#!/bin/bash

cd test
mkdir build
cd build
cmake ..
echo "TEST_RUN = $TEST_RUN" 
make
if [ "$TEST_RUN" = "TRUE" ]; then
    sudo make install
    density_test
fi
