#!/bin/bash

rm -rf build_dsp
mkdir build_dsp
cd build_dsp
cmake -DCMAKE_TOOLCHAIN_FILE=$HEXAGON_SDK_ROOT/build/cmake/Hexagon_Toolchain.cmake -DENABLE_HVX=ON ../../

make -j $(nproc)

cp -rf DSPDemo ../../release