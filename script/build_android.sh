#!/bin/bash

HEXAGON_CMAKE_ROOT=${HEXAGON_SDK_ROOT}/build/cmake
_src=${PWD}

V="android_Release_aarch64"
_build=${_src}/${V}.cmake${_dir_surfix}

rm -rf ${_build} && mkdir -p ${_build}

cd ${_build} && \
    cmake \
    -DCMAKE_SYSTEM_NAME="Android"  \
    -DANDROID_NDK=${ANDROID_ROOT_DIR} \
    -DCMAKE_TOOLCHAIN_FILE=${ANDROID_ROOT_DIR}/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a -DANDROID_STL=c++_shared\
    -DANDROID_NATIVE_API_LEVEL=21 \
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${_build} \
    -DCMAKE_BUILD_TYPE=Debug  \
    -DV=${V}  \
    -DHEXAGON_CMAKE_ROOT=${HEXAGON_CMAKE_ROOT}\
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    ../../

make -j $(nproc)

cp *.so ../../release
cp DSPDemo ../../release