#!/bin/bash
1:-build-iwyu}
cmake -S . -B "$BUILD_DIR" \
set -eu
 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
BUILD_DIR=${1:-build-iwyu}
 -DCMAKE_BUILD_TYPE=Debug \
cmake -S . -B "$BUILD_DIR" \
 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
 -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE="/usr/bin/iwyu_tool;-p;$BUILD_DIR;-Xiwyu;--mapping_file=/home/vatnar/dev/SD/iwyu.imp"
 -DCMAKE_BUILD_TYPE=Debug \
cmake --build "$BUILD_DIR" --target SD 2>&1 | tee iwyu-output.log
 -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE="/usr/bin/iwyu_tool;-p;$BUILD_DIR;-Xiwyu;--mapping_file=/home/vatnar/dev/SD/iwyu.imp"
cmake --build "$BUILD_DIR" --target SD 2>&1 | tee iwyu-output.log
