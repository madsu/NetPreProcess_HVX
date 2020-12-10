#!/bin/bash

rm -rf build_linux
mkdir build_linux
cd build_linux
cmake ../../

make -j $(nproc)

cp -rf libClDemo.so ../../build
cp -rf OpenCLDemo ../../build