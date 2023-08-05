cmake_minimum_required(VERSION 3.26.4)

project(binarugen)

set(CMAKE_CXX_STANDARD 20)
include_directories("C:/workspace/utils_backup/src/include")
link_directories("C:/workspace/utils_backup/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "test")
add_executable(lexer_test "src/lexer/test.cpp")
add_executable(ast_test "src/ast/test.cpp")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")
