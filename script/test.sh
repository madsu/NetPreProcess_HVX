#!/bin/bash
./build_android.sh
./build_dsp.sh

adb push ../release/libpre_process_skel.so /vendor/lib/rfsa/dsp/sdk
adb push ../release/DSPDemo /data/local/tmp/HVX_test
adb push ../release/libpre_process_stub.so /data/local/tmp/HVX_test