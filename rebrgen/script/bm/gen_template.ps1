# PowerShell equivalent of gen_template.bat
param (
    [string]$LANG_NAME
)

& python.exe script/gen_template.py $LANG_NAME
