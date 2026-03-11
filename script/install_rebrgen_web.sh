#!/bin/bash
# Install rebrgen web artifacts into brgen's web/dev directory.
# Run from the repository root (brgen/).
#
# Usage:
#   ./script/install_rebrgen_web.sh [REBRGEN_WEB_DIR]
#
# REBRGEN_WEB_DIR: Path to the rebrgen web output directory.
#   Defaults to ./rebrgen/web (for local builds).
#   In CI, pass the artifact download path (e.g., ./rebrgen-web).
set -e

REBRGEN_WEB_DIR="${1:-./rebrgen/web}"
BMGEN_DIR="./web/dev/src/lib/bmgen"

if [ ! -d "$REBRGEN_WEB_DIR/tool" ]; then
    echo "Error: $REBRGEN_WEB_DIR/tool not found."
    echo "Usage: $0 [REBRGEN_WEB_DIR]"
    exit 1
fi

echo "Installing rebrgen web artifacts from $REBRGEN_WEB_DIR..."

# Copy WASM artifacts + generated glue code to bmgen directory
mkdir -p "$BMGEN_DIR"
cp -r "$REBRGEN_WEB_DIR/tool/"* "$BMGEN_DIR/"

# Build wasmCopy.js by combining brgen's base + rebrgen's BM + EBM copy lists
echo "Generating wasmCopy.js..."
cp ./web/dev/wasmCopy.js.txt ./web/dev/wasmCopy.js
cat "$BMGEN_DIR/wasmCopy.js.txt" "$BMGEN_DIR/ebmWasmCopy.js.txt" >> ./web/dev/wasmCopy.js

echo "rebrgen web artifacts installed to $BMGEN_DIR"
ls -al "$BMGEN_DIR"
