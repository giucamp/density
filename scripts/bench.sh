#!/bin/bash
set -e

RUN=$1
echo "RUN = $RUN"

if [ "$RUN" = "TRUE" ]; then
    cd bench
    mkdir build || true
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..  
    make
    $PWD/density_bench
fi
