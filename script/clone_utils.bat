@echo off
setlocal
git clone https://github.com/on-keyday/utils --depth 1
cd utils
if "%1" = "wasm-em" (
    call build wasm-em Debug utils
) else (
    call build shared Debug utils
)
cd ..
