#!/bin/bash
# pack_server_kit.sh
# Packages the brgen API server into a self-contained zip that can be
# run with: unzip server-kit.zip && cd web/dev && npm install && npm run serve
#
# Must be run from the repository root after all WASM / build artifacts
# have been produced (i.e. after the "Build Wasm" CI step).
#
# The zip preserves the repo-root-relative directory layout so that the
# file: protocol dependencies in web/dev/package.json resolve correctly:
#   "ast2ts":   "file:../../astlib/ast2ts/out"
#   "json2rust": "file:../../src/tool/json2rust/pkg"

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "$STAGING_DIR"' EXIT

OUTPUT_DIR="${1:-web/public}"
OUTPUT_FILE="$OUTPUT_DIR/server-kit.zip"

echo "==> Packing server kit..."

# ---- 1. web/dev: package.json + all source (including lib/ artifacts) ----
mkdir -p "$STAGING_DIR/web/dev"
cp web/dev/package.json "$STAGING_DIR/web/dev/"

# Copy src/ (TypeScript sources + lib/ build artifacts).
# Exclude node_modules if somehow present inside src/.
rsync -a --exclude='node_modules' web/dev/src/ "$STAGING_DIR/web/dev/src/"

# ---- 2. ast2ts package (file: dependency) ----
if [ -d "astlib/ast2ts/out" ]; then
    mkdir -p "$STAGING_DIR/astlib/ast2ts"
    cp -r astlib/ast2ts/out "$STAGING_DIR/astlib/ast2ts/out"
    # ast2ts also needs its own package.json for npm to resolve it
    if [ -f "astlib/ast2ts/out/package.json" ]; then
        : # already copied inside out/
    elif [ -f "astlib/ast2ts/package.json" ]; then
        cp astlib/ast2ts/package.json "$STAGING_DIR/astlib/ast2ts/"
    fi
else
    echo "WARNING: astlib/ast2ts/out/ not found -- ast2ts dependency will be missing"
fi

# ---- 3. json2rust wasm-pack package (file: dependency) ----
if [ -d "src/tool/json2rust/pkg" ]; then
    mkdir -p "$STAGING_DIR/src/tool/json2rust"
    cp -r src/tool/json2rust/pkg "$STAGING_DIR/src/tool/json2rust/pkg"
else
    echo "WARNING: src/tool/json2rust/pkg/ not found -- json2rust dependency will be missing"
fi

# ---- 4. Create the zip ----
mkdir -p "$OUTPUT_DIR"
(cd "$STAGING_DIR" && zip -r -q - .) > "$OUTPUT_FILE"

SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)
echo "==> Server kit written to $OUTPUT_FILE ($SIZE)"
echo "    Usage: unzip server-kit.zip && cd web/dev && npm install && npm run serve"
