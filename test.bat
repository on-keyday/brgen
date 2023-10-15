@echo off
setlocal
set BASE_PATH=%CD%
set CTEST_OUTPUT_ON_FAILURE=1
call build.bat
ninja -C built/native/Debug test
