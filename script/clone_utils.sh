#!/usr/bin/bash
git clone https://github.com/on-keyday/utils --depth 1
cd utils
. build shared Debug utils
if [ $1 = "wasm-em" ]; then
    . build shared Debug wasm-em
fi
cd ..
