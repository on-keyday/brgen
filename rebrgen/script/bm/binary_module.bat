@echo off
setlocal
set TOOL_PATH=C:\workspace\shbrgen\brgen\tool
%TOOL_PATH%\src2json src/old/bm/binary_module.bgn > save/bm.json
%TOOL_PATH%\json2cpp2 -f save/bm.json --mode header_file --enum-stringer --use-error --dll-export > src/old/bm/binary_module.hpp
%TOOL_PATH%\json2cpp2 -f save/bm.json --mode source_file --enum-stringer --use-error --dll-export > src/old/bm/binary_module.cpp
rem %TOOL_PATH%\json2cpp2 -f save/bm.json --mode header_only > save/binary_module2.hpp
