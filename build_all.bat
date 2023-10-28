@echo off
setlocal
set EMSDK_PATH="C:\workspace\emsdk\emsdk_env.bat"
call build.bat
if not %errorlevel% == 0 goto :error
call %EMSDK_PATH%
call build.bat wasm-em
if not %errorlevel% == 0 goto :error
python script/generate.py
if not %errorlevel% == 0 goto :error
cd web/dev
call webpack.cmd
set LEV=%errorlevel%
cd ../..
rem if not %LEV% == 0 goto :error
call install_lsp.bat
:error
