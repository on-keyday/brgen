@echo off
setlocal
set BUILD_TYPE=Debug
set UTILS_DIR=C:/workspace/utils_backup
cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -S . -B .
ninja

