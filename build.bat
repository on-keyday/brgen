@echo off
set BUILD_TYPE=Debug
cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -S . -B .
ninja

