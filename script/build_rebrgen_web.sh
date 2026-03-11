#!/bin/bash
# Build rebrgen native + WASM and generate web glue code.
# Run from the repository root (brgen/).
# Requires: cmake, ninja, clang++, emsdk (EMSDK env var set)
set -e

REBRGEN_DIR="./rebrgen"

if [ ! -d "$REBRGEN_DIR" ]; then
    echo "Error: $REBRGEN_DIR directory not found. Run from the brgen repository root."
    exit 1
fi

cd "$REBRGEN_DIR"

# Setup futils
if [ -d "utils" ]; then
    echo "Pulling existing utils..."
    cd utils
    git pull
    rm -rf ./built
    . build shared Release futils
    cd ..
else
    echo "Cloning utils..."
    git clone https://github.com/on-keyday/utils.git
    cd utils
    rm -rf ./built
    . build shared Release futils
    cd ..
fi

# Build native (needed for gen_template + ebm2* tools used by glue generation)
echo "Building rebrgen native..."
BUILD_OLD_BMGEN=1 FUTILS_DIR=$(pwd)/utils python script/build.py native Release

# Rebuild futils for WASM
echo "Rebuilding futils for WASM..."
cd utils
rm -rf ./built
. build wasm-em Release futils
cd ..

# Build WASM
echo "Building rebrgen WASM..."
rm -rf ./built/web
BUILD_OLD_BMGEN=1 EMSDK_DIR=$EMSDK/ FUTILS_DIR=./utils python script/build.py web Release
cp README.md web/ 2>/dev/null || true
cp LICENSE web/ 2>/dev/null || true

# Generate web glue code
echo "Generating web glue code..."
chmod -R +x ./tool
python script/bm/generate_web_glue.py
python script/ebmwebgen.py

echo "rebrgen web build complete. Output in rebrgen/web/"
