#!/bin/bash
set -euo pipefail

SERVER_KIT=${1:-web/public/server-kit.tar.gz}


# if web/public/server-kit.tar.gz is not exists
# change to address https://on-keyday.github.io/brgen/server-kit.tar.gz
if [ ! -f "$SERVER_KIT" ]; then
    echo "No local server kit found at $SERVER_KIT, downloading from https://on-keyday.github.io/brgen/server-kit.tar.gz..."
    # if directory does not exist, create it
    mkdir -p "$(dirname "$SERVER_KIT")"
    curl -f -L -o "$SERVER_KIT" https://on-keyday.github.io/brgen/server-kit.tar.gz
else
    echo "Using local server kit at $SERVER_KIT"
fi

# second argument should be the directory to extract to, default to a temporary directory and remove after use
if [ -n "${2:-}" ]; then
    EXTRACTION_DIR="$(realpath "$2")"
    mkdir -p "$EXTRACTION_DIR"
else
    EXTRACTION_DIR="$(mktemp -d)"
    trap 'echo "Removing temporary directory..."; rm -rf "$EXTRACTION_DIR"; echo "Removed temporary directory."' EXIT
fi


echo "==> Extracting server kit..."
echo "    from: $SERVER_KIT to: $EXTRACTION_DIR"
tar -xzf "$SERVER_KIT" -C "$EXTRACTION_DIR"

echo "==> Installing dependencies..."
(
    cd "$EXTRACTION_DIR/web/dev"
    npm install
)

echo "==> Starting server..."
(
    cd "$EXTRACTION_DIR/web/dev"
    npm run serve
)