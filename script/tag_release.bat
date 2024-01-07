@echo off
setlocal

for /f "tokens=*" %%a in (tag.txt) do set TAG=%%a
if "%TAG%"=="" (
    echo "tag.txt" not found
    exit /b 1
)
git tag -d %TAG%
git push origin --delete %TAG%
git add .
git commit -m "release %TAG% %date% %time%"
git tag %TAG%
git push
git push origin %TAG%
