#!/usr/bin/bash
git clone https://github.com/on-keyday/utils --depth 1
cd utils
BUILD_TYPE=$2
if [ ! $BUILD_TYPE ]; then
    BUILD_TYPE=Debug
fi
if [ $1 = "wasm-em" ]; then
    . build wasm-em $BUILD_TYPE futils
else
    . build shared $BUILD_TYPE futils
fi
cd ..
