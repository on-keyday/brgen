#license
cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")

add_compile_definitions(__TEST__)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoverage-mapping")

add_library(ast_test_component STATIC
"core/ast_test_component.cpp"
)

target_compile_options(ast_test_component PRIVATE "-fprofile-instr-generate=ast_test_component.profraw")

add_executable(lexer_test "core/lexer_test.cpp")

target_compile_options(lexer_test PRIVATE "-fprofile-instr-generate=lexer_test.profraw")


target_link_libraries(lexer_test gtest_main)
target_link_libraries(ast_test_component utils gtest)

add_executable(ast_test "core/ast_test.cpp")
target_link_libraries(ast_test ast_test_component)
target_compile_options(ast_test PRIVATE "-fprofile-instr-generate=ast_test.profraw")


add_executable(typing_test "core/typing_test.cpp")

target_link_libraries(typing_test ast_test_component)
target_compile_options(typing_test PRIVATE "-fprofile-instr-generate=typing_test.profraw")

add_executable(middle_test "core/middle_test.cpp")

target_link_libraries(middle_test ast_test_component)
target_compile_options(middle_test PRIVATE "-fprofile-instr-generate=middle_test.profraw")

add_executable(from_json_test "core/from_json_test.cpp")

target_link_libraries(from_json_test ast_test_component utils)
target_compile_options(from_json_test PRIVATE "-fprofile-instr-generate=from_json_test.profraw")

add_executable(section_writer_test "core/section_writer_test.cpp")

target_link_libraries(section_writer_test gtest_main)
target_compile_options(section_writer_test PRIVATE "-fprofile-instr-generate=section_writer_test.profraw")

add_executable(liner_eq_test "core/liner_eq_test.cpp")
target_link_libraries(liner_eq_test gtest_main)
target_compile_options(liner_eq_test PRIVATE "-fprofile-instr-generate=liner_eq_test.profraw")

add_executable(type_attribute "core/type_attribute_test.cpp")
target_link_libraries(type_attribute gtest_main)
target_compile_options(type_attribute PRIVATE "-fprofile-instr-generate=type_attribute.profraw")

add_test(NAME "lexer_test" COMMAND lexer_test)
add_test(NAME "ast_test" COMMAND ast_test)
add_test(NAME "typing_test" COMMAND typing_test)
add_test(NAME "middle_test" COMMAND middle_test)
add_test(NAME "from_json_test" COMMAND from_json_test)
add_test(NAME "section_writer_test" COMMAND section_writer_test)
add_test(NAME "liner_eq_test" COMMAND liner_eq_test)
add_test(NAME "type_attribute" COMMAND type_attribute)

if(WIN32)

add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.dll" "${CMAKE_BINARY_DIR}/test/libutils.dll"
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.pdb" "${CMAKE_BINARY_DIR}/test/libutils.pdb"
)
endif()
else()

add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.so" "${CMAKE_BINARY_DIR}/test/libutils.so"
)
endif()


if(APPLE)
# add -fprofile-instr-generate to linker flags
set_target_properties(lexer_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=lexer_test.profraw")
set_target_properties(ast_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=ast_test.profraw")
set_target_properties(typing_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=typing_test.profraw")
set_target_properties(middle_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=middle_test.profraw")
set_target_properties(from_json_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=from_json_test.profraw")
set_target_properties(section_writer_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=section_writer_test.profraw")
set_target_properties(liner_eq_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=liner_eq_test.profraw")
set_target_properties(type_attribute PROPERTIES LINK_FLAGS "-fprofile-instr-generate=type_attribute.profraw")
endif()

