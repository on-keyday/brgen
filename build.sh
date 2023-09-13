#!/bin/bash
BUILD_TYPE=Debug
UTILS_DIR=$(pwd)/utils cmake  -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_BUILD_TYPE=Debug -S . -B ./build
ninja -C ./build/
