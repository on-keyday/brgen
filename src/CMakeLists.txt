#license
cmake_minimum_required(VERSION 3.26.4)


add_executable(lexer_test "test/lexer_test.cpp")

add_library(ast_test_component STATIC
"test/ast_test_component.cpp"
"core/typing/typing.cpp"
)

target_link_libraries(lexer_test gtest_main)
target_link_libraries(ast_test_component utils gtest)

add_executable(ast_test "test/ast_test.cpp")
target_link_libraries(ast_test ast_test_component)


add_executable(typing_test "test/typing_test.cpp"
)

target_link_libraries(typing_test ast_test_component)

add_executable(middle_test "test/middle_test.cpp")

target_link_libraries(middle_test ast_test_component)

add_executable(section_writer_test "test/section_writer_test.cpp")

target_link_libraries(section_writer_test gtest_main)

add_executable(cpp_test "test/langs/cpp_test.cpp"
"langs/cpp/cpp_lang.cpp")



target_link_libraries(cpp_test ast_test_component)

add_executable(go_test "test/langs/go_test.cpp"
"langs/go/go_lang.cpp")

target_link_libraries(go_test ast_test_component)

add_test(NAME "lexer_test" COMMAND lexer_test)
add_test(NAME "ast_test" COMMAND ast_test)
add_test(NAME "typing_test" COMMAND typing_test)
add_test(NAME "middle_test" COMMAND middle_test)
add_test(NAME "section_writer_test" COMMAND section_writer_test)
add_test(NAME "cpp_test" COMMAND cpp_test)
add_test(NAME "go_test" COMMAND go_test)
add_test(NAME "cpp_compile" 
COMMAND go run "${CMAKE_SOURCE_DIR}/src/test_tool" "cpp" "${CMAKE_BINARY_DIR}/test/cpp_test_result.json")
add_test(NAME "go_compile" 
COMMAND go run "${CMAKE_SOURCE_DIR}/src/test_tool" "go" "${CMAKE_BINARY_DIR}/test/go_test_result.json")


if(WIN32)

add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.dll" "${CMAKE_BINARY_DIR}/test/libutils.dll"
)
else()

add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.so" "${CMAKE_BINARY_DIR}/test/libutils.so"
)
endif()


#[[


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

