@echo off
setlocal
git tag -d v0.0.0
git add .
git commit -m "release test %date% %time%"
git tag v0.0.0
git push
git push origin v0.0.0

