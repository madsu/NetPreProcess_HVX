#!/bin/bash

rm -rf build_dsp
mkdir build_dsp
cd build_dsp
cmake -DCMAKE_TOOLCHAIN_FILE=../../toolchains/hexagon.toolchain.cmake -DENABLE_HVX=ON ../../

make -j $(nproc)

cp -rf DSPDemo ../../release