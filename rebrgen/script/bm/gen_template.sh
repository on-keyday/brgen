#!/bin/bash
set -e

LANG_NAME=$1
if [ -z "$LANG_NAME" ]; then
    echo "Usage: $0 <lang_name>"
    exit 1
fi
python script/gen_template.py "$LANG_NAME"
