# Architecture

tool/brgen - code generator driver <- input is brgen.json
|
|--tool/src2json - parse source code then convert to json ast, dump type information of ast node, lexer etc...
|
|--tool/json2cpp - C++ code generator
|
|--tool/json2go - Go code generator
