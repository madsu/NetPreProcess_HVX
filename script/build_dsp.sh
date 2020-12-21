#!/bin/bash

rm -rf build_dsp
mkdir build_dsp
cd build_dsp

#generate idl
$HEXAGON_SDK_ROOT/tools/qaic/bin/qaic ../../inc/pre_process.idl

cmake -DCMAKE_TOOLCHAIN_FILE=$HEXAGON_SDK_ROOT/build/cmake/Hexagon_Toolchain.cmake \
      -DHEXAGON_SDK_ROOT=${HEXAGON_SDK_ROOT} \
      -DENABLE_HVX=ON \
      ../../

make -j $(nproc)

cp -rf DSPDemo ../../release