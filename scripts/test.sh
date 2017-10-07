#!/bin/bash

cd test
mkdir build
cd build
cmake ..
make
if [ "$TEST_RUN" = "TRUE" ]; then
    sudo make install
    density_test
fi
