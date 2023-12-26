@echo off
setlocal
git clone https://github.com/on-keyday/utils --depth 1
cd utils
if "%2" == "" (
    set BUILD_TYPE=Debug
) else (
    set BUILD_TYPE=%2
)
if "%1" = "wasm-em" (
    call build wasm-em %BUILD_TYPE% utils
) else (
    call build shared %BUILD_TYPE% utils
)
cd ..
