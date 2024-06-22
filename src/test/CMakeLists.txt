#license
cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")

add_compile_definitions(__TEST__)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoverage-mapping")
endif()

add_library(parse_core STATIC 
"../core/ast/parse.cpp"
"../core/middle/typing.cpp"
"../core/middle/type_attribute.cpp"
"../tool/src2json/test.cpp"
)

add_library(ast_test_component STATIC
"core/ast_test_component.cpp"
)
target_link_libraries(ast_test_component futils gtest parse_core)

add_executable(lexer_test "core/lexer_test.cpp")

target_link_libraries(lexer_test gtest_main)


add_executable(ast_test "core/ast_test.cpp")
target_link_libraries(ast_test ast_test_component)


add_executable(typing_test "core/typing_test.cpp")

target_link_libraries(typing_test ast_test_component)


add_executable(middle_test "core/middle_test.cpp")

target_link_libraries(middle_test ast_test_component)




add_executable(from_json_test "core/from_json_test.cpp")

target_link_libraries(from_json_test ast_test_component futils)


add_executable(section_writer_test 
  "core/section_writer_test.cpp"
)

target_link_libraries(section_writer_test gtest_main)




add_executable(type_attribute "core/type_attribute_test.cpp")
target_link_libraries(type_attribute gtest_main parse_core)

add_executable(derive_test "core/derive_test.cpp")
target_link_libraries(derive_test gtest_main)

add_executable(ctype_test "tool/json2c/ctype_test.cpp")
target_link_libraries(ctype_test gtest_main parse_core)

add_executable(deep_copy_test "core/deep_copy_test.cpp")
target_link_libraries(deep_copy_test gtest_main parse_core futils)

add_test(NAME "lexer_test" COMMAND lexer_test)
add_test(NAME "ast_test" COMMAND ast_test)
add_test(NAME "typing_test" COMMAND typing_test)
add_test(NAME "middle_test" COMMAND middle_test)
add_test(NAME "from_json_test" COMMAND from_json_test)
add_test(NAME "section_writer_test" COMMAND section_writer_test)
add_test(NAME "type_attribute" COMMAND type_attribute)
add_test(NAME "derive_test" COMMAND derive_test)
add_test(NAME "ctype_test" COMMAND ctype_test)
add_test(NAME "deep_copy_test" COMMAND deep_copy_test)

if(WIN32)

add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{FUTILS_DIR}/lib/libfutils.dll" "${CMAKE_BINARY_DIR}/test/libfutils.dll"
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{FUTILS_DIR}/lib/libfutils.pdb" "${CMAKE_BINARY_DIR}/test/libfutils.pdb"
)
endif()
elseif(APPLE)
add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{FUTILS_DIR}/lib/libfutils.dylib" "${CMAKE_BINARY_DIR}/test/libfutils.dylib"
)
else()

add_custom_command(
TARGET ast_test POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{FUTILS_DIR}/lib/libfutils.so" "${CMAKE_BINARY_DIR}/test/libfutils.so"
)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
target_compile_options(ast_test_component PRIVATE "-fprofile-instr-generate=ast_test_component.profraw")
target_compile_options(parse_core PRIVATE "-fprofile-instr-generate=parse_core.profraw")
target_compile_options(lexer_test PRIVATE "-fprofile-instr-generate=lexer_test.profraw")
target_compile_options(ast_test PRIVATE "-fprofile-instr-generate=ast_test.profraw")
target_compile_options(typing_test PRIVATE "-fprofile-instr-generate=typing_test.profraw")
target_compile_options(middle_test PRIVATE "-fprofile-instr-generate=middle_test.profraw")
target_compile_options(from_json_test PRIVATE "-fprofile-instr-generate=from_json_test.profraw")
target_compile_options(section_writer_test PRIVATE "-fprofile-instr-generate=section_writer_test.profraw")
target_compile_options(type_attribute PRIVATE "-fprofile-instr-generate=type_attribute.profraw")
target_compile_options(derive_test PRIVATE "-fprofile-instr-generate=derive_test.profraw")
target_compile_options(ctype_test PRIVATE "-fprofile-instr-generate=ctype_test.profraw")
target_compile_options(deep_copy_test PRIVATE "-fprofile-instr-generate=deep_copy_test.profraw")
endif()


if(APPLE)
# add -fprofile-instr-generate to linker flags
set_target_properties(lexer_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=lexer_test.profraw")
set_target_properties(ast_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=ast_test.profraw")
set_target_properties(typing_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=typing_test.profraw")
set_target_properties(middle_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=middle_test.profraw")
set_target_properties(from_json_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=from_json_test.profraw")
set_target_properties(section_writer_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=section_writer_test.profraw")
set_target_properties(type_attribute PROPERTIES LINK_FLAGS "-fprofile-instr-generate=type_attribute.profraw")
set_target_properties(derive_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=derive_test.profraw")
set_target_properties(ctype_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=ctype_test.profraw")
set_target_properties(deep_copy_test PROPERTIES LINK_FLAGS "-fprofile-instr-generate=deep_copy_test.profraw")
endif()

