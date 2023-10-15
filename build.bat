@echo off
setlocal
set BUILD_MODE=%1

if "%BUILD_MODE%" == "" (
    set BUILD_MODE=native
)


set BUILD_TYPE=Debug
set UTILS_DIR=C:/workspace/utils_backup
set LLVM_DIR=C:/workspace/llvm-project/llvm/build/lib/cmake/llvm
set Clang_DIR=C:/workspace/llvm-project/clang/build/cmake/modules/CMakeFiles
if "%BUILD_MODE%" == "native" (
cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -S . -B ./built/%BUILD_MODE%/%BUILD_TYPE%
) else if "%BUILD_MODE%" == "wasm-em" (
call emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -S . -B ./built/%BUILD_MODE%/%BUILD_TYPE%
) else (
    echo "Invalid build mode: %BUILD_MODE%"
    exit 1
)
ninja -C ./built/%BUILD_MODE%/%BUILD_TYPE%

