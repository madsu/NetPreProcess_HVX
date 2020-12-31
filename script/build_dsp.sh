#!/bin/bash

HEXAGON_CMAKE_ROOT=${HEXAGON_SDK_ROOT}/build/cmake
_src=${PWD}

V="hexagon_Release_dynamic_toolv83_v66"
_build=${_src}/${V}.cmake

rm -rf ${_build} && mkdir -p ${_build}

cd ${_build} && \
    cmake \
    -DCMAKE_TOOLCHAIN_FILE=${HEXAGON_CMAKE_ROOT}/Hexagon_Toolchain.cmake \
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${_build} \
    -DBUILD_DSP_SHARED=ON \
    -DV=${V}  \
    -DHEXAGON_CMAKE_ROOT=${HEXAGON_CMAKE_ROOT}\
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    ../../

make -j $(nproc)

cp *.so ../../release