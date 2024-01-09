@echo off
cd lsp
call vsce package
call code --install-extension ./brgen-lsp-0.0.1.vsix
cd ../
