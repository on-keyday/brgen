# PowerShell equivalent of script/binary_module.bat
$TOOL_PATH = "C:\workspace\shbrgen\brgen\tool"
& $TOOL_PATH\src2json src/old/bm/binary_module.bgn | Out-File save/bm.json -Encoding utf8
& $TOOL_PATH\json2cpp2 -f save/bm.json --mode header_file --enum-stringer --use-error --dll-export | Out-File src/old/bm/binary_module.hpp -Encoding utf8
& $TOOL_PATH\json2cpp2 -f save/bm.json --mode source_file --enum-stringer --use-error --dll-export | Out-File src/old/bm/binary_module.cpp -Encoding utf8
#& $TOOL_PATH\json2cpp2 -f save/bm.json --mode header_only | Out-File save/binary_module2.hpp -Encoding utf8
