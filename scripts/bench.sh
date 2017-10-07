#!/bin/bash

RUN=$1

cd bench
mkdir build
cd build
cmake ..
echo "RUN = $RUN"
make
if [ "$RUN" = "TRUE" ]; then
    sudo make install
    density_bench
fi
