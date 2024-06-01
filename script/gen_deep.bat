@echo off
setlocal
set OUTPUT_DIR=src/
set DEEP_GEN=go run ./src/tool/gen/cpp_deep_copy

tool\src2json --dump-types | %DEEP_GEN% /dev/stdout | clang-format > %OUTPUT_DIR%/core/ast/node/deep_copy.h
