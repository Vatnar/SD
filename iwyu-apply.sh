#!/bin/bash
will compile and run IWYU on each source file)'
# IWYU convenience script
cmake --build $BUILD_DIR --target SD 2>&1 | tee iwyu-output.log
# Usage: ./iwyu-apply.sh [build-dir]


echo ''
BUILD_DIR=${1:-build-iwyu}
echo 'IWYU output saved to: iwyu-output.log'

echo 'Configuring with IWYU enabled...'
cmake -B $BUILD_DIR -DCMAKE_EXPORT_COMPILE_COMMANDS=ON\
    -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE="/usr/bin/iwyu_tool;-p;$BUILD_DIR;-Xiwyu;--mapping_file=\${CMAKE_SOURCE_DIR}/iwyu.imp"\
    -DCMAKE_BUILD_TYPE=Debug

echo ''
echo 'Building with IWYU analysis...'
echo '(This will compile and run IWYU on each source file)'
cmake --build $BUILD_DIR --target SD 2>&1 | tee iwyu-output.log

echo ''
echo 'IWYU output saved to: iwyu-output.log'
