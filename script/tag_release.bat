@echo off
setlocal
set TAG=v0.0.2
git tag -d %TAG%
git push origin --delete %TAG%
git add .
git commit -m "release %TAG% %date% %time%"
git tag %TAG%
git push
git push origin %TAG%
