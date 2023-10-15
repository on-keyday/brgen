#license
cmake_minimum_required(VERSION 3.22.1)

# find_package(Clang )

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tool")


add_executable(src2json "tool/src2json.cpp")
target_link_libraries(src2json utils)

if("$ENV{BUILD_MODE}" STREQUAL "wasm-em")
return()
endif()

add_executable(json2cpp "tool/json2cpp/main.cpp")
target_link_libraries(json2cpp utils)

if(WIN32)
set(SUFFIX ".exe")
endif()

add_custom_target(brgen ALL COMMAND "go" "build" "-o" "${CMAKE_BINARY_DIR}/tool/brgen${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/brgen" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/brgen" )

add_custom_target(json2go ALL COMMAND "go" "build" "-o" "${CMAKE_BINARY_DIR}/tool/json2go${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/json2go" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/json2go" "${CMAKE_SOURCE_DIR}/ast2go")

add_custom_target(gen_ast2go ALL COMMAND "go" "build" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2go${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2go" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2go" )
add_custom_target(gen_ast2ts ALL COMMAND "go" "build" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2ts${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2ts" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2ts" )
add_custom_target(gen_ast2rust ALL COMMAND "go" "build" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2rust${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2rust" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2rust" )


if(WIN32)

    add_custom_command(
    TARGET src2json POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${UTILS_LIB_DIR}/libutils.dll" "${CMAKE_BINARY_DIR}/tool/libutils.dll"
    )
    add_custom_command(
    TARGET src2json POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${UTILS_LIB_DIR}/libutils.pdb" "${CMAKE_BINARY_DIR}/tool/libutils.pdb"
    )

else()

    add_custom_command(
    TARGET src2json POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${UTILS_LIB_DIR}/libutils.so" "${CMAKE_BINARY_DIR}/tool/libutils.so"
    )
endif()


add_subdirectory(test)
