#!/bin/bash
./build_android.sh
./build_dsp.sh

adb shell rm -rf /data/local/tmp/HVX_test/*
adb shell rm -rf vendor/lib/rfsa/dsp/sdk/libpre_process_skel.so

adb push ../release/libpre_process_skel.so /vendor/lib/rfsa/dsp/sdk
adb push ../release/0.nv21 /data/local/tmp/HVX_test
adb push ../release/DSPDemo /data/local/tmp/HVX_test
adb push ../release/libpre_process_stub.so /data/local/tmp/HVX_test
adb push ../release/libopencv_java4.so /data/local/tmp/HVX_test
adb push ../release/libdspCV_skel.so /data/local/tmp/HVX_test
adb push ../release/libc++_shared.so /data/local/tmp/HVX_test

adb shell export LD_LIBRARY_PATH=/data/local/tmp/HVX_test:/vendor/lib64/:$LD_LIBRARY_PATH ADSP_LIBRARY_PATH="/vendor/lib/rfsa/dsp/sdk\;/vendor/lib/rfsa/dsp/testsig;" /data/local/tmp/HVX_test/DSPDemo /data/local/tmp/HVX_test/0.nv21

rm -rf ./HVX_test
adb pull /data/local/tmp/HVX_test .