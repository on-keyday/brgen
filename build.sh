#!/bin/bash
if [ ! -d utils ]; then
. ./script/clone_utils.sh
go mod download
fi
BUILD_MODE=native
BUILD_TYPE=Debug
UTILS_DIR=$(pwd)/utils cmake  -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_BUILD_TYPE=$BUILD_TYPE -S . -B ./build
ninja -C ./built/
