@echo off
setlocal
set BUILD_TYPE=Debug
set UTILS_DIR=C:/workspace/utils_backup
set LLVM_DIR=C:/workspace/llvm-project/llvm/build/lib/cmake/llvm
set Clang_DIR=C:/workspace/llvm-project/clang/build/cmake/modules/CMakeFiles
cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -S . -B ./build
ninja -C ./build/

