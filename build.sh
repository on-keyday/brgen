#!/bin/bash
if [ ! -d utils ]; then
. ./script/clone_utils.sh
go mod download
fi
BUILD_MODE=$1

if [ ! $BUILD_MODE ]; then
    BUILD_MODE=native
fi

BUILD_TYPE=Debug

export UTILS_DIR=$(pwd)/utils
export BUILD_MODE=$BUILD_MODE
INSTALL_PREFIX=.

if [ $BUILD_MODE = "wasm-em" ];then
   emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=$BUILD_TYPE -D CMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
else
   cmake  -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_INSTALL_PREFIX=$INSTALL_PREFIX/web/dev -D CMAKE_BUILD_TYPE=$BUILD_TYPE -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
fi
ninja -C ./built/$BUILD_MODE/$BUILD_TYPE
ninja -C ./built/$BUILD_MODE/$BUILD_TYPE install

if [ $BUILD_MODE = "wasm-em" ]; then
cd ./web/dev
webpack
cd ../..
fi

unset UTILS_DIR
unset BUILD_MODE