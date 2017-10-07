#!/bin/bash

cd bench
mkdir build
cd build
cmake ..
echo "$BENCH_RUN = $BENCH_RUN"
make
if [ "$BENCH_RUN" = "TRUE" ]; then
    sudo make install
    density_bench
fi
