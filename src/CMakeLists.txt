#license
cmake_minimum_required(VERSION 3.26.4)


add_executable(lexer_test "test/lexer_test.cpp")

add_library(ast_test_component STATIC
"test/ast_test_component.cpp"
)

target_link_libraries(lexer_test gtest_main)
target_link_libraries(ast_test_component utils gtest)

add_executable(ast_test "test/ast_test.cpp")
target_link_libraries(ast_test ast_test_component)


add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.dll" "${CMAKE_BINARY_DIR}/test/libutils.dll"
)

#[[

add_executable(typing_test "core/typing/test.cpp"
"core/typing/typing.cpp")
add_executable(treeopt_test "core/treeopt/test.cpp")

# languages
add_executable(c_lang_test "langs/c/test.cpp"
"langs/c/c_lang.cpp")

target_compile_options(lexer_test PRIVATE "-fconstexpr-steps=2147483647")
target_link_libraries(ast_test_component utils)
target_link_libraries(ast_test ast_test_component)
target_link_libraries(typing_test ast_test_component)
target_link_libraries(treeopt_test ast_test_component)
target_link_libraries(c_lang_test ast_test_component)



]]

