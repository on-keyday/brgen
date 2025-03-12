@echo off
setlocal
set BUILD_MODE_BASE=%1
call build.bat native %BUILD_MODE_BASE%
if not %errorlevel% == 0 goto :error
go env > script/go_dump_env.bat
call script\go_dump_env.bat
set WASMEXEC_FILE=%GOROOT%\lib\wasm\wasm_exec.js  
call build.bat wasm-em %BUILD_MODE_BASE%
if not %errorlevel% == 0 goto :error
python script/generate.py
if not %errorlevel% == 0 goto :error
cd web/dev
call webpack.cmd
set LEV=%errorlevel%
cd ../..
rem if not %LEV% == 0 goto :error
call script\install_lsp.bat
:error
