@echo off
setlocal
set BASE_PATH=../..
set CTEST_OUTPUT_ON_FAILURE=1
ninja -C build
ninja -C build test
