#!/usr/bin/bash
git clone https://github.com/on-keyday/utils --depth 1
cd utils
cmake  -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_BUILD_TYPE=Debug -S . -B .
ninja utils
cd ..
