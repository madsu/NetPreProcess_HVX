#!/bin/bash

rm -rf build_android
mkdir build_android
cd build_android

#generate idl
$HEXAGON_SDK_ROOT/tools/qaic/bin/qaic ../../inc/pre_process.idl

cmake -DANDROID_NDK=${ANDROID_ROOT_DIR} \
      -DCMAKE_TOOLCHAIN_FILE=$ANDROID_ROOT_DIR/build/cmake/android.toolchain.cmake \
      -DHEXAGON_SDK_ROOT=${HEXAGON_SDK_ROOT} \
      -DV=android_Release_aarch64 \
      -DCMAKE_BUILD_TYPE=Debug \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_STL=none \
      -DANDROID_NATIVE_API_LEVEL=21 \
      ../../

make -j $(nproc)

cp -rf DSPDemo ../../release