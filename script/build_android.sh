#!/bin/bash

rm -rf build_android
mkdir build_android
cd build_android
cmake -DANDROID_NDK=${ANDROID_ROOT_DIR} \
      -DCMAKE_TOOLCHAIN_FILE=$ANDROID_ROOT_DIR/build/cmake/android.toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_STL=none \
      -DANDROID_NATIVE_API_LEVEL=21 \
      ../../

make -j $(nproc)

cp -rf DSPDemo ../../release