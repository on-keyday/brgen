#!/bin/bash
if [ ! -d utils ]; then
. ./script/clone_utils.sh
go mod download
fi
BUILD_MODE=$1

if [ ! BUILD_MODE ]; then
    BUILD_MODE=native
fi

BUILD_TYPE=Debug
UTILS_DIR=$(pwd)/utils
if [ $BUILD_MODE = "wasm-em" ];then
    emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=$BUILD_TYPE -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
else
    cmake  -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_BUILD_TYPE=$BUILD_TYPE -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
fi
ninja -C ./built/$BUILD_MODE/$BUILD_TYPE
ninja -C ./built/$BUILD_MODE/$BUILD_TYPE install
