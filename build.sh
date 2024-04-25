#!/bin/bash
# TODO(on-keyday): on macos, S2J_LIB=1 is not work well, so not set S2J_LIB

export S2J_LIB=1


export BRGEN_RUST_ENABLED=1

BUILD_MODE=$1
BUILD_TYPE=$2

if [ ! $BUILD_MODE ]; then
   BUILD_MODE=native
fi

if [ ! $BUILD_TYPE ]; then
   BUILD_TYPE=Debug
fi

if [ ! -d utils ]; then
. ./script/clone_utils.sh $BUILD_MODE $BUILD_TYPE
fi
go mod download
if [ $BUILD_MODE = "wasm-em" ]; then
   cd web/dev
   npm install
   cd ../..
fi


export FUTILS_DIR=$(pwd)/utils
export BUILD_MODE=$BUILD_MODE
INSTALL_PREFIX=.

CXX_COMPILER=clang++
C_COMPILER=clang

# override compiler if specified
if [ $FUTILS_CXX_COMPILER ]; then
   CXX_COMPILER=$FUTILS_CXX_COMPILER
fi
if [ $FUTILS_C_COMPILER ]; then
   C_COMPILER=$FUTILS_C_COMPILER
fi

if [ $BUILD_MODE = "wasm-em" ];then
   export GOOS=js
   export GOARCH=wasm
   emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=$BUILD_TYPE -D CMAKE_INSTALL_PREFIX=$INSTALL_PREFIX/web/dev/src -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
else
   cmake  -D CMAKE_CXX_COMPILER=$CXX_COMPILER -D CMAKE_C_COMPILER=$C_COMPILER -G Ninja -D CMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -D CMAKE_BUILD_TYPE=$BUILD_TYPE -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
fi
#ninja -C ./built/$BUILD_MODE/$BUILD_TYPE
ninja -C ./built/$BUILD_MODE/$BUILD_TYPE install

if [ $BUILD_MODE = "wasm-em" ]; then
unset GOOS
unset GOARCH
#cd ./src/tool/json2rust
#wasm-pack build --target web
#cp ../../../LICENSE ./pkg
#cd ../../..
cd ./web/dev
tsc
webpack
cd ../..
python script/copy_example.py
fi

unset FUTILS_DIR
unset BUILD_MODE
unset S2J_LIB
unset BRGEN_RUST_ENABLED