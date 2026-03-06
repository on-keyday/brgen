#!/bin/bash
set -euo pipefail

SERVER_KIT=${1:-}

CLEAN_UP_SERVER_KIT=false
CLEAN_UP_EXTRACTION_DIR=false

function clean_up {
    if [ "$CLEAN_UP_SERVER_KIT" = true ] && [ -f "$SERVER_KIT" ]; then
        echo "Cleaning up downloaded server kit at $SERVER_KIT..."
        rm -f "$SERVER_KIT"
        echo "Cleaned up downloaded server kit."
    fi
    if [ "$CLEAN_UP_EXTRACTION_DIR" = true ] && [ -d "$EXTRACTION_DIR" ]; then
        echo "Cleaning up extracted server kit directory at $EXTRACTION_DIR..."
        rm -rf "$EXTRACTION_DIR"
        echo "Cleaned up extracted server kit directory."
    fi
}

# if no server kit path is provided, or the file does not exist, download from the official URL
if [ -z "$SERVER_KIT" ] || [ ! -f "$SERVER_KIT" ]; then
    # if empty, use temporary directory and remove after use
    if [ -z "$SERVER_KIT" ]; then
        SERVER_KIT="$(mktemp)"
        CLEAN_UP_SERVER_KIT=true
        echo "Downloading server kit from https://on-keyday.github.io/brgen/server-kit.tar.gz to temporary file $SERVER_KIT..."
    else
        echo "No local server kit found at $SERVER_KIT, downloading from https://on-keyday.github.io/brgen/server-kit.tar.gz..."   
    fi
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
    CLEAN_UP_EXTRACTION_DIR=true
    echo "No extraction directory provided, using temporary directory $EXTRACTION_DIR for server kit extraction..."
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