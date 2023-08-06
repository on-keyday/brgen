cmake_minimum_required(VERSION 3.26.4)

project(binarugen)

set(CMAKE_CXX_STANDARD 20)
include_directories("$ENV{UTILS_DIR}/src/include")
link_directories("$ENV{UTILS_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "test")
add_executable(lexer_test "src/lexer/test.cpp")
add_executable(ast_test "src/ast/test.cpp"
"src/ast/test_component.cpp"
"src/ast/ast.cpp")
add_executable(vm_test "src/vm/test.cpp"
"src/ast/test_component.cpp"
"src/ast/ast.cpp")
target_compile_options(lexer_test PRIVATE "-fconstexpr-steps=2147483647")
target_link_libraries(ast_test utils)
target_link_libraries(vm_test utils)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")

add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.dll" "test/libutils.dll"
)
