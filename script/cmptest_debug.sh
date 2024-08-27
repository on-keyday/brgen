#!/bin/bash
FUTILS_DIR=/c/workspace/utils_backup 
export FUTILS_DIR=$FUTILS_DIR
./build.sh
tool/brgen -config test-brgen.json
tool/cmptest -f ./ignore/example/test_info.json -c ./testkit/cmptest.json --print-fail-command --save-tmp-dir --as-vscode --clean-tmp

