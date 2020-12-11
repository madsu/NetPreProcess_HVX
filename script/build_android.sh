#!/bin/bash

rm -rf build_android
mkdir build_android
cd build_android
cmake ../../ \
      -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI="arm64-v8a"

make -j $(nproc)

cp -rf libClDemo.so ../../build
cp -rf OpenCLDemo ../../build