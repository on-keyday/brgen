@echo off
setlocal
ninja clean
rmdir /Q /S %CD%\CMakeFiles
rmdir /Q /S %CD%\test
del /Q %CD%\.ninja_deps
del /Q %CD%\.ninja_log
del /Q %CD%\CMakeCache.txt
del /Q %CD%\cmake_install.cmake
del /Q %CD%\build.ninja
