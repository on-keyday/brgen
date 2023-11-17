#!/bin/bash
BUILD_MODE=$1
BUILD_TYPE=$2

if [ ! $BUILD_MODE ]; then
   BUILD_MODE=native
fi

if [ ! $BUILD_TYPE ]; then
   BUILD_TYPE=Debug
fi

if [ ! -d utils ]; then
. ./script/clone_utils.sh $BUILD_MODE
go mod download
if [ $BUILD_MODE = "wasm-em" ]; then
   cd web/dev
   npm install
   cd ../..
fi
fi




export UTILS_DIR=$(pwd)/utils
export BUILD_MODE=$BUILD_MODE
INSTALL_PREFIX=.

if [ $BUILD_MODE = "wasm-em" ];then
   emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=$BUILD_TYPE -D CMAKE_INSTALL_PREFIX=$INSTALL_PREFIX/web/dev/src -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
else
   cmake  -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -D CMAKE_BUILD_TYPE=$BUILD_TYPE -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
fi
ninja -C ./built/$BUILD_MODE/$BUILD_TYPE
ninja -C ./built/$BUILD_MODE/$BUILD_TYPE install

if [ $BUILD_MODE = "wasm-em" ]; then
cd ./web/dev
tsc
webpack
cd ../..
fi

unset UTILS_DIR
unset BUILD_MODE