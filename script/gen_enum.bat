@echo off
setlocal
set INPUT_DIR=example/brgen_help
set OUTPUT_DIR=src/
set ENUM_GEN=go run ./src/tool/gen/enum_gen

tool\src2json example/brgen_help/ast_enum.bgn | %ENUM_GEN% > %OUTPUT_DIR%/core/ast/node/ast_enum.h
tool\src2json example/brgen_help/lexer_enum.bgn | %ENUM_GEN% > %OUTPUT_DIR%/core/lexer/lexer_enum.h
tool\src2json example/brgen_help/vm_enum.bgn | %ENUM_GEN% > %OUTPUT_DIR%/vm/vm_enum.h
