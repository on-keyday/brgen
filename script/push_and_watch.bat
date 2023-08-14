@echo off
setlocal
git push
ping -n 4 127.0.0.1>nul
gh run watch -i1
