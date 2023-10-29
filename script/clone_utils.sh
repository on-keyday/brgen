#!/usr/bin/bash
git clone https://github.com/on-keyday/utils --depth 1
cd utils
if [ $1 = "wasm-em" ]; then
    . build wasm-em Debug utils
else
    . build shared Debug utils
fi
cd ..
