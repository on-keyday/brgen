@echo off
setlocal
set BUILD_MODE=%1
set EMSDK_PATH="C:\workspace\emsdk\emsdk_env.bat"
call build.bat native %BUILD_MODE%
if not %errorlevel% == 0 goto :error
call %EMSDK_PATH%
call build.bat wasm-em %BUILD_MODE%
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
