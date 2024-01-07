@echo off
setlocal

for /f %%a in (./script/tag.txt) do set RELEASE_TAG=%%a
if "%RELEASE_TAG%"=="" (
    echo "tag.txt" not found
    exit /b 1
)
rem test
echo %RELEASE_TAG%
git tag -d %RELEASE_TAG%
git push origin --delete %RELEASE_TAG%
git add .
git commit -m "release %RELEASE_TAG% %date% %time%"
git tag %RELEASE_TAG%
git push
git push origin %RELEASE_TAG%
