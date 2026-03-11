@echo off
setlocal

set LANG_NAME=%1
if "%LANG_NAME%" == "" (
    echo Usage: %0 LANG_NAME
    exit /b 1
)

python script/gen_template.py %LANG_NAME%