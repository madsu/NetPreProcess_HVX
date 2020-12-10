#!/bin/bash

mkdir build_android
cd build_android
cmake ../../ \
      -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake

make -j $(nproc)