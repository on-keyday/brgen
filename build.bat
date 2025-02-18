@echo off
setlocal
set S2J_LIB=1
set BRGEN_RUST_ENABLED=1
set BUILD_MODE=%1
set BUILD_TYPE=%2
set FUTILS_DIR=%3

if "%BUILD_MODE%" == "" (
    set BUILD_MODE=native
)
if "%FUTILS_DIR%" == "" (
    rem for developers
    set FUTILS_DIR=C:/workspace/utils_backup
)
if not exist %FUTILS_DIR% (
    call script\clone_utils.bat %BUILD_MODE% %BUILD_TYPE%
    set FUTILS_DIR=%CD%\utils
)

if "%BUILD_TYPE%" == "" (
    set BUILD_TYPE=Debug
)

if "%EMSDK_PATH%" == "" (
    set EMSDK_PATH=C:\workspace\emsdk\emsdk_env.bat 
)

set INSTALL_PREFIX=.
if "%BUILD_MODE%" == "native" (
cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -DCMAKE_INSTALL_PREFIX=%INSTALL_PREFIX% -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -S . -B ./built/%BUILD_MODE%/%BUILD_TYPE%
) else if "%BUILD_MODE%" == "wasm-em" (
call %EMSDK_PATH%
set GOOS=js
set GOARCH=wasm
call emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%INSTALL_PREFIX%/web/dev/src -S . -B ./built/%BUILD_MODE%/%BUILD_TYPE%
) else (
    echo "Invalid build mode: %BUILD_MODE%"
    exit 1
)
rem ninja -C ./built/%BUILD_MODE%/%BUILD_TYPE%
ninja -C ./built/%BUILD_MODE%/%BUILD_TYPE% install

if "%BUILD_MODE%" == "wasm-em" (
rem    cd ./src/tool/json2rust
rem    wasm-pack build --target web
rem    copy ..\..\..\LICENSE .\pkg\LICENSE
rem    cd ../../../
    python script/copy_bmgen_stub.py
    cd ./web/dev
    call tsc
    call webpack
    cd ../../
    python script/copy_example.py
)
