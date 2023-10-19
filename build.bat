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
set INSTALL_PREFIX=.
if "%BUILD_MODE%" == "native" (
cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -DCMAKE_INSTALL_PREFIX=%INSTALL_PREFIX% -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -S . -B ./built/%BUILD_MODE%/%BUILD_TYPE%
) else if "%BUILD_MODE%" == "wasm-em" (
call emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%INSTALL_PREFIX%/web/dev/src -S . -B ./built/%BUILD_MODE%/%BUILD_TYPE%
) else (
    echo "Invalid build mode: %BUILD_MODE%"
    exit 1
)
rem ninja -C ./built/%BUILD_MODE%/%BUILD_TYPE%
ninja -C ./built/%BUILD_MODE%/%BUILD_TYPE% install

if "%BUILD_MODE%" == "wasm-em" (
    cd ./web/dev
    call webpack
    cd ../../
)
