#!/bin/bash
set -eu
BUILD_DIR=${1:-build-iwyu}
cmake -S . -B "$BUILD_DIR" \
 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
 -DCMAKE_BUILD_TYPE=Debug \
 -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE="/usr/bin/iwyu_tool;-p;$BUILD_DIR;-Xiwyu;--mapping_file=/home/vatnar/dev/SD/iwyu.imp"
cmake --build "$BUILD_DIR" --target SD 2>&1 | tee iwyu-output.log
