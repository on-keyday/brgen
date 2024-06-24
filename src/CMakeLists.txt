#license
cmake_minimum_required(VERSION 3.22.1)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tool")

# Setting C++ targets
add_executable(src2json 
    "tool/src2json/src2json.cpp" 
    "tool/src2json/entry.cpp"
    "core/ast/parse.cpp"
    "core/middle/typing.cpp"
    "core/middle/type_attribute.cpp"
    "core/middle/analyze_block_trait.cpp"
)
target_link_libraries(src2json futils)
add_compile_definitions(BRGEN_VERSION="${PROJECT_VERSION}")


add_executable(json2cpp2 "tool/json2cpp2/main.cpp")
target_link_libraries(json2cpp2 futils)

add_executable(json2c "tool/json2c/main.cpp")
target_link_libraries(json2c futils)

add_executable(json2vm "tool/json2vm/main.cpp" "vm/compile.cpp" "vm/vm.cpp")
target_link_libraries(json2vm futils)

add_executable(json2ts "tool/json2ts/main.cpp" "tool/json2ts/generate.cpp")
target_link_libraries(json2ts futils)


# Setting Go targets
if(WIN32)
set(SUFFIX ".exe")
elseif("$ENV{BUILD_MODE}" STREQUAL "wasm-em")
set(SUFFIX ".wasm")
endif()

add_custom_target(json2go ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/json2go${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/json2go" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/json2go" "${CMAKE_SOURCE_DIR}/astlib/ast2go")
add_custom_target(json2kaitai ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/json2kaitai${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/json2kaitai" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/json2kaitai" "${CMAKE_SOURCE_DIR}/astlib/ast2go")
add_custom_target(json2mermaid ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/json2mermaid${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/json2mermaid" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/json2mermaid" "${CMAKE_SOURCE_DIR}/astlib/ast2go")





# Setting Wasm

if("$ENV{BUILD_MODE}" STREQUAL "wasm-em")
# Setting C++ to Wasm targets
install(TARGETS src2json DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/src2json.wasm" DESTINATION "lib" OPTIONAL)
install(TARGETS json2cpp2 DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/json2cpp2.wasm" DESTINATION "lib" OPTIONAL)
install(TARGETS json2c DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/json2c.wasm" DESTINATION "lib" OPTIONAL)
install(TARGETS json2vm DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/json2vm.wasm" DESTINATION "lib" OPTIONAL)
install(TARGETS json2ts DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/json2ts.wasm" DESTINATION "lib" OPTIONAL)

# Setting Go to Wasm targets
install(FILES "${CMAKE_BINARY_DIR}/tool/json2go.wasm" DESTINATION "lib" OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/tool/json2kaitai.wasm" DESTINATION "lib" OPTIONAL)
file(TO_CMAKE_PATH "$ENV{WASMEXEC_FILE}" WASMEXEC)
install(FILES "${WASMEXEC}" RENAME "go_wasm_exec.js" DESTINATION "lib" OPTIONAL)
install(CODE "file(APPEND \"${CMAKE_INSTALL_PREFIX}/lib/go_wasm_exec.js\" \"const Go = globalThis.Go;\n\")" OPTIONAL)
install(CODE "file(APPEND \"${CMAKE_INSTALL_PREFIX}/lib/go_wasm_exec.js\" \"export {Go}\n\")" OPTIONAL)


# Setting Rust to Wasm targets
if("$ENV{BRGEN_RUST_ENABLED}" STREQUAL "1")
    add_custom_target(
        json2rust ALL
        COMMAND "wasm-pack" "build" "--target" "web"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/tool/json2rust
    )
    add_custom_command(
        TARGET json2rust POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/src/tool/json2rust/pkg"
    )
endif()

return() # Stop here when building for wasm
endif()


# Setting Rust targets for native

if("$ENV{BRGEN_RUST_ENABLED}" STREQUAL "1")

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(RUST_RELEASE_OPTION "")
    else()
    set(RUST_RELEASE_OPTION "--release")
    endif()

    add_custom_target(
        json2rsut ALL
        COMMAND "cargo" "build" "--bins" "${RUST_RELEASE_OPTION}" "--target-dir" "${CMAKE_BINARY_DIR}/tool/json2rust/target"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/tool/json2rust
    )

    add_custom_target(
        cmptest ALL
        COMMAND "cargo" "build" "--bins" "${RUST_RELEASE_OPTION}" "--target-dir" "${CMAKE_BINARY_DIR}/tool/cmptest/target"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/tool/cmptest
    )

endif()

# Libraries

if(WIN32)
  set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

# Setting shared library version src2json
if(S2J_LIB)
add_library(s2j SHARED 
    "tool/src2json/src2json.cpp" 
    "tool/src2json/entry.cpp"
    "core/ast/parse.cpp"
    "core/middle/typing.cpp"
    "core/middle/type_attribute.cpp"
    "core/middle/analyze_block_trait.cpp"
)
target_compile_definitions(s2j PRIVATE SRC2JSON_DLL)
target_link_libraries(s2j futils)
install(TARGETS s2j DESTINATION "tool" OPTIONAL)
endif()

# Setting network extension for src2json
if(S2J_USE_NETWORK)
target_sources(src2json PRIVATE "tool/src2json/network.cpp")
target_link_libraries(src2json fnet fnetserv)
add_compile_definitions(S2J_USE_NETWORK)
if(S2J_LIB)
target_sources(s2j PRIVATE "tool/src2json/network.cpp")
target_link_libraries(s2j fnet fnetserv)
add_compile_definitions(S2J_USE_NETWORK)
endif()
endif()


# Setting C++ tools
add_executable(ctobgn "tool/ctobgn/main.cpp")
target_link_libraries(ctobgn futils)

add_executable(rfc2bgn "tool/rfc2bgn/main.cpp")
target_link_libraries(rfc2bgn futils)

add_executable(hex2bin "tool/hex2bin/main.cpp")
target_link_libraries(hex2bin futils)

# Setting Go tools

add_custom_target(brgen ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/brgen${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/brgen" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/brgen" )

add_custom_target(gen_ast2go ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2go${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2go" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2go" )
add_custom_target(gen_ast2ts ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2ts${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2ts" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2ts" )
add_custom_target(gen_ast2rust ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2rust${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2rust" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2rust" )
add_custom_target(gen_ast2py ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2py${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2py" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2py" )
add_custom_target(gen_ast2c ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2c${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2c" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2c" )
add_custom_target(gen_ast2csharp ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2csharp${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2csharp" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2csharp" )
add_custom_target(gen_ast2mermaid ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2mermaid${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2mermaid" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2mermaid" )
add_custom_target(gen_ast2dart ALL COMMAND "${GO_COMPILER}" "build" "${CMAKE_GO_FLAGS}" "-o" "${CMAKE_BINARY_DIR}/tool/gen_ast2dart${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2dart" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/gen/ast2dart" )

# Copy dependency libraries (futils,fnet,fnetserv)

if(WIN32)

    add_custom_command(
    TARGET src2json POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfutils.dll" "${CMAKE_BINARY_DIR}/tool/libfutils.dll"
    )
    install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfutils.dll" DESTINATION tool OPTIONAL)
    
    if(S2J_USE_NETWORK)
        add_custom_command(
        TARGET src2json POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfnet.dll" "${CMAKE_BINARY_DIR}/tool/libfnet.dll"
        )
        install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfnet.dll" DESTINATION tool OPTIONAL)
        add_custom_command(
        TARGET src2json POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfnetserv.dll" "${CMAKE_BINARY_DIR}/tool/libfnetserv.dll"
        )
        install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfnetserv.dll" DESTINATION tool OPTIONAL)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command( 
        TARGET src2json POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfutils.pdb" "${CMAKE_BINARY_DIR}/tool/libfutils.pdb"
        )
        install(FILES "${CMAKE_BINARY_DIR}/tool/libfutils.pdb" DESTINATION tool OPTIONAL)
        if(S2J_USE_NETWORK)
            add_custom_command( 
            TARGET src2json POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfnet.pdb" "${CMAKE_BINARY_DIR}/tool/libfnet.pdb"
            )
            install(FILES "${CMAKE_BINARY_DIR}/tool/libfnet.pdb" DESTINATION tool OPTIONAL)
            add_custom_command( 
            TARGET src2json POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfnetserv.pdb" "${CMAKE_BINARY_DIR}/tool/libfnetserv.pdb"
            )
            install(FILES "${CMAKE_BINARY_DIR}/tool/libfnetserv.pdb" DESTINATION tool OPTIONAL)
        endif()
    endif()


    
elseif(APPLE)
    add_custom_command(
    TARGET src2json POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfutils.dylib" "${CMAKE_BINARY_DIR}/tool/libfutils.dylib"
    )
    install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfutils.dylib" DESTINATION tool OPTIONAL)

    if(S2J_USE_NETWORK)
        add_custom_command(
        TARGET src2json POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfnet.dylib" "${CMAKE_BINARY_DIR}/tool/libfnet.dylib"
        )
        install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfnet.dylib" DESTINATION tool OPTIONAL)
        add_custom_command(
        TARGET src2json POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfnetserv.dylib" "${CMAKE_BINARY_DIR}/tool/libfnetserv.dylib"
        )
        install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfnetserv.dylib" DESTINATION tool OPTIONAL)
    endif()

else()

    add_custom_command(
    TARGET src2json POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfutils.so" "${CMAKE_BINARY_DIR}/tool/libfutils.so"
    )
    install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfutils.so" DESTINATION tool OPTIONAL)

    if(S2J_USE_NETWORK)
        add_custom_command(
        TARGET src2json POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfnet.so" "${CMAKE_BINARY_DIR}/tool/libfnet.so"
        )
        install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfnet.so" DESTINATION tool OPTIONAL)
        add_custom_command(
        TARGET src2json POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_LIB_DIR}/libfnetserv.so" "${CMAKE_BINARY_DIR}/tool/libfnetserv.so"
        )
        install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfnetserv.so" DESTINATION tool OPTIONAL)
    endif()
endif()

# Add test directory
add_subdirectory(test)

# Set RPATH for Unix-like systems
if(UNIX)
set_target_properties(src2json PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(json2cpp2 PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(json2c PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(json2vm PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(json2ts PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(ctobgn PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(rfc2bgn PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(hex2bin PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
set_target_properties(brgen PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
if(S2J_LIB)
set_target_properties(s2j PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
endif()
endif()

# Set Install path for C++ targets

install(TARGETS src2json json2cpp2 json2c json2vm json2ts ctobgn rfc2bgn hex2bin DESTINATION tool OPTIONAL)


# Set Install path for Go targets
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/brgen${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/json2go${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/json2kaitai${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/json2mermaid${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2go${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2ts${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2rust${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2py${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2c${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2csharp${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2mermaid${SUFFIX}" DESTINATION tool OPTIONAL)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/gen_ast2dart${SUFFIX}" DESTINATION tool OPTIONAL)

# Set Install path for Rust targets
if("$ENV{BRGEN_RUST_ENABLED}" STREQUAL "1")
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(RUST_TARGET_DIR "debug")
    else()
        set(RUST_TARGET_DIR "release")
    endif()
    install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/json2rust/target/${RUST_TARGET_DIR}/json2rust${SUFFIX}" DESTINATION tool OPTIONAL)
    install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/cmptest/target/${RUST_TARGET_DIR}/cmptest${SUFFIX}" DESTINATION tool OPTIONAL)
endif()
