#!/bin/bash

cd bench
mkdir build
cd build
cmake ..
make
if [ "$BENCH_RUN" = "TRUE" ]; then
    sudo make install
    density_bench
fi
