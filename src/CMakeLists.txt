#license
cmake_minimum_required(VERSION 3.22.1)

# find_package(Clang )

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tool")


add_executable(src2json "tool/src2json/src2json.cpp")
target_link_libraries(src2json utils)

add_executable(json2cpp "tool/json2cpp/main.cpp")
target_link_libraries(json2cpp utils)

add_executable(json2cpp2 "tool/json2cpp2/main.cpp")
target_link_libraries(json2cpp2 utils)

if(WIN32)
set(SUFFIX ".exe")
elseif("$ENV{BUILD_MODE}" STREQUAL "wasm-em")
set(SUFFIX ".wasm")
endif()

add_custom_target(json2go ALL COMMAND "${GO_COMPILER}" "build" "${GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/json2go${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/json2go" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/json2go" "${CMAKE_SOURCE_DIR}/ast2go")


if("$ENV{BUILD_MODE}" STREQUAL "wasm-em")
install(TARGETS src2json DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/src2json.wasm" DESTINATION "lib" OPTIONAL)
install(TARGETS json2cpp DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/json2cpp.wasm" DESTINATION "lib" OPTIONAL)
install(TARGETS json2cpp2 DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/json2cpp2.wasm" DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/json2go.wasm" DESTINATION "lib" OPTIONAL)
file(TO_CMAKE_PATH "$ENV{WASMEXEC_FILE}" WASMEXEC)
install(FILES "${WASMEXEC}" RENAME "go_wasm_exec.js" DESTINATION "lib" OPTIONAL)
install(CODE "file(APPEND \"${CMAKE_INSTALL_PREFIX}/lib/go_wasm_exec.js\" \"const Go = globalThis.Go;\n\")")
install(CODE "file(APPEND \"${CMAKE_INSTALL_PREFIX}/lib/go_wasm_exec.js\" \"export {Go}\n\")")
install(CODE "file(COPY_FILE \"${CMAKE_INSTALL_PREFIX}/s2j/go_wasm_exec.d.ts.txt\" \"${CMAKE_INSTALL_PREFIX}/lib/go_wasm_exec.d.ts\")")
return()
endif()

add_executable(ctobgn "tool/ctobgn/main.cpp")
target_link_libraries(ctobgn utils)



add_custom_target(brgen ALL COMMAND "${GO_COMPILER}" "build" "${GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/brgen${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/brgen" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/brgen" )

add_custom_target(gen_ast2go ALL COMMAND "${GO_COMPILER}" "build" "${GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2go${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2go" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2go" )
add_custom_target(gen_ast2ts ALL COMMAND "${GO_COMPILER}" "build" "${GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2ts${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2ts" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2ts" )
add_custom_target(gen_ast2rust ALL COMMAND "${GO_COMPILER}" "build" "${GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2rust${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2rust" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2rust" )
add_custom_target(gen_ast2py ALL COMMAND "${GO_COMPILER}" "build" "${GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2py${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2py" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2py" )
add_custom_target(gen_ast2c ALL COMMAND "${GO_COMPILER}" "build" "${GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2c${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2c" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2c" )
add_custom_target(gen_ast2csharp ALL COMMAND "${GO_COMPILER}" "build" "${GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2csharp${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2csharp" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2csharp" )


if(WIN32)

    add_custom_command(
    TARGET src2json POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${UTILS_LIB_DIR}/libutils.dll" "${CMAKE_BINARY_DIR}/tool/libutils.dll"
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(
    TARGET src2json POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${UTILS_LIB_DIR}/libutils.pdb" "${CMAKE_BINARY_DIR}/tool/libutils.pdb"
    )
    install(FILES "${CMAKE_BINARY_DIR}/tool/libutils.pdb" DESTINATION tool OPTIONAL)
    endif()
    install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libutils.dll" DESTINATION tool OPTIONAL)
    
else()

    add_custom_command(
    TARGET src2json POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${UTILS_LIB_DIR}/libutils.so" "${CMAKE_BINARY_DIR}/tool/libutils.so"
    )
    install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libutils.so" DESTINATION tool OPTIONAL)
endif()


add_subdirectory(test)
if(UNIX)
set_target_properties(src2json PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(json2cpp PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(json2cpp2 PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(ctobgn PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
endif()
install(TARGETS src2json json2cpp json2cpp2 ctobgn DESTINATION tool OPTIONAL)

install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/brgen${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/json2go${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2go${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2ts${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2rust${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2py${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2c${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2csharp${SUFFIX}" DESTINATION tool OPTIONAL)
