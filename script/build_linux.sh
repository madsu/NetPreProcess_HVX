#!/bin/bash

rm -rf build_linux
mkdir build_linux
cd build_linux
cmake ../../ -DCMAKE_BUILD_TYPE=Debug

make -j $(nproc)

cp -rf DSPDemo ../../release