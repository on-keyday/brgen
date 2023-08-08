#license
cmake_minimum_required(VERSION 3.26.4)

project(brgen)

set(CMAKE_CXX_STANDARD 20)
include_directories("$ENV{UTILS_DIR}/src/include")
include_directories("./")
link_directories("$ENV{UTILS_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "test")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "test")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "test")
add_executable(lexer_test "core/lexer/test.cpp")
add_library(ast_test_component SHARED 
"core/ast/test_component.cpp"
"core/ast/ast.cpp")

add_executable(ast_test "core/ast/test.cpp")
add_executable(treeopt_test "core/treeopt/test.cpp")

# languages
add_executable(c_lang_test "langs/c/test.cpp"
"langs/c/c_lang.cpp")

target_compile_options(lexer_test PRIVATE "-fconstexpr-steps=2147483647")
target_link_libraries(ast_test_component utils)
target_link_libraries(ast_test ast_test_component)
target_link_libraries(treeopt_test ast_test_component)
target_link_libraries(c_lang_test ast_test_component)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")

add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.dll" "test/libutils.dll"
)
