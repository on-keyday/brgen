cmake_minimum_required(VERSION 3.25)

project(rebrgen)
enable_testing()   
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)

set(INSTALL_GTEST OFF)
include(FetchContent)  
FetchContent_Declare( 
    googletest
    DOWNLOAD_EXTRACT_TIMESTAMP true
    # Specify the commit you depend on and update it regularly.
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip 
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) 
FetchContent_MakeAvailable(googletest)

if("$ENV{BUILD_MODE}" STREQUAL "web")
add_compile_options(-fexperimental-library)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fexceptions -sENVIRONMENT=worker,node -sINVOKE_RUN=0 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXIT_RUNTIME=0 -sEXPORTED_RUNTIME_METHODS=[ccall,cwrap,FS] -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=16777216 -sSTACK_OVERFLOW_CHECK=1 --bind")
endif()

# Recent Emscripten builds SHARED libraries as relocatable side modules and
# rejects linking non-PIC objects against them (wasm-ld: R_WASM_TABLE_INDEX_SLEB
# ... recompile with -fPIC). The web distribution only ships standalone .wasm
# files and never dynamically links, so build these libraries statically there.
if("$ENV{BUILD_MODE}" STREQUAL "web")
set(REBRGEN_LIB_TYPE STATIC)
else()
set(REBRGEN_LIB_TYPE SHARED)
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -mfentry")

if(UNIX)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
elseif(WIN32)
add_link_options("/fuse-ld=lld")
endif()
endif()

include_directories("$ENV{BRGEN_DIR}/src")
include_directories("$ENV{FUTILS_DIR}/src/include")
if("$ENV{BUILD_MODE}" STREQUAL "native")
link_directories("$ENV{FUTILS_DIR}/built/shared/${CMAKE_BUILD_TYPE}/lib")
elseif("$ENV{BUILD_MODE}" STREQUAL "web")
link_directories("$ENV{FUTILS_DIR}/built/wasm-em/${CMAKE_BUILD_TYPE}/lib")
endif()
include_directories("src")



set(CMAKE_RUNTIME_OUTPUT_DIRECTORY tool)

if("$ENV{BUILD_OLD_BMGEN}" STREQUAL "1")
add_library(bm ${REBRGEN_LIB_TYPE} "src/old/bm/binary_module.cpp")

add_executable(bmgen
    "src/old/bmgen/main.cpp" 
    "src/old/bmgen/convert.cpp" 
    "src/old/bmgen/encode.cpp"
    "src/old/bmgen/load_json.cpp" 
    "src/old/bmgen/save_module.cpp"
    "src/old/bmgen/transform.cpp"
    "src/old/bmgen/transform/bind_coder.cpp"
    "src/old/bmgen/transform/bit_field_size.cpp"
    "src/old/bmgen/transform/control_flow_graph.cpp"
    "src/old/bmgen/transform/flatten.cpp"
    "src/old/bmgen/transform/merge_conditional.cpp"
    "src/old/bmgen/transform/property_accessor.cpp"
    "src/old/bmgen/transform/bit_accessor.cpp"
    "src/old/bmgen/transform/sort_formats.cpp"
    "src/old/bmgen/transform/sort_func_loc.cpp"
    "src/old/bmgen/transform/fallback/bit_op.cpp"
    "src/old/bmgen/transform/fallback/encode.cpp"
    "src/old/bmgen/transform/fallback/endian.cpp"
)

target_include_directories(bmgen PUBLIC "src/old")

add_executable(gen_template 
    "src/old/bm2/gen_template/main.cpp"
    "src/old/bm2/gen_template/accessor.cpp"
    "src/old/bm2/gen_template/eval.cpp"
    "src/old/bm2/gen_template/glue_code.cpp"
    "src/old/bm2/gen_template/inner_block.cpp"
    "src/old/bm2/gen_template/inner_function.cpp"
    "src/old/bm2/gen_template/parameter.cpp"
    "src/old/bm2/gen_template/type.cpp"
    "src/old/bm2/gen_template/hook_load.cpp"
)

target_include_directories(gen_template PUBLIC "src/old")
add_subdirectory("src/old")

target_link_libraries(bmgen futils bm)
target_link_libraries(gen_template futils)

if(UNIX)
set_target_properties(gen_template PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
#set_target_properties(gen_template2 PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
endif()
install(TARGETS bmgen bm DESTINATION tool)
install(TARGETS gen_template COMPONENT gen_template DESTINATION tool)

endif()

add_library(ebm ${REBRGEN_LIB_TYPE} "src/ebm/extended_binary_module.cpp")




if(NOT "$ENV{CODEGEN_ONLY}" STREQUAL "1")

add_executable(ebmgen
    "src/ebmgen/main.cpp"
)

add_library(ebm_mapping STATIC
    "src/ebmgen/mapping.cpp"
    "src/ebmgen/debug_printer.cpp"
    "src/ebmgen/json_printer.cpp"
    "src/ebmgen/interactive/debugger.cpp"
    "src/ebmcodegen/structs.cpp" # for debug info
)

add_library(ebmgen_lib STATIC
    "src/ebmgen/convert.cpp"
    "src/ebmgen/converter.cpp"
    "src/ebmgen/load_json.cpp"
    "src/ebmgen/json_conv.cpp"
    "src/ebmgen/convert/statement.cpp"
    "src/ebmgen/convert/expression.cpp"
    "src/ebmgen/convert/type.cpp"
    "src/ebmgen/convert/helper.cpp"
    "src/ebmgen/convert/encode.cpp"
    "src/ebmgen/convert/decode.cpp"
    "src/ebmgen/convert/union_property.cpp"
    "src/ebmgen/transform/transform.cpp"
    "src/ebmgen/transform/control_flow_graph.cpp"
    "src/ebmgen/transform/bit_manipulator.cpp"
    "src/ebmgen/transform/io_vectorized.cpp"
    "src/ebmgen/transform/remove_unused.cpp"
    "src/ebmgen/transform/bit_fields.cpp"
    "src/ebmgen/transform/bit_holder.cpp"
    "src/ebmgen/transform/derive_property.cpp"
    "src/ebmgen/transform/flatten_io_expression.cpp"
    "src/ebmgen/transform/add_cast_func.cpp"
    "src/ebmgen/transform/array_setter.cpp"
    "src/ebmgen/transform/derive_encode_decode_wrapper.cpp"
    "src/ebmgen/transform/propagate_io_input_desc.cpp"
    "src/ebmgen/transform/lower_runtime_state.cpp"
)

target_link_libraries(ebm_mapping ebm futils)
target_link_libraries(ebmgen_lib ebm futils ebm_mapping)
target_link_libraries(ebmgen ebmgen_lib)

target_link_libraries(ebmgen futils ebm)
install(TARGETS ebmgen DESTINATION tool)
target_compile_options(ebmgen PRIVATE -ftime-trace)


target_precompile_headers(ebmgen PRIVATE
    <format>
    <string>
    <vector>
    <variant>
    <optional>
    <ebm/extended_binary_module.hpp>
    <ebmgen/common.hpp>
    <ebmgen/convert/helper.hpp>
    <ebmgen/mapping.hpp>
    <ebmgen/visitor/visitor.hpp>
    <concepts>
    <strutil/append.h>
)

add_library(codegen_pch OBJECT "src/ebmcodegen/stub/dummy.cpp")
target_precompile_headers(codegen_pch PRIVATE
    <format>
    <expected>
    <string>
    <vector>
    <variant>
    <optional>
    <ebm/extended_binary_module.hpp>
    <ebmcodegen/stub/entry.hpp>
    <ebmcodegen/stub/util.hpp>
    <ebmgen/common.hpp>
    <ebmgen/convert/helper.hpp>
    <ebmgen/mapping.hpp>
    <ebmcodegen/stub/code_writer.hpp>
    <ebmcodegen/stub/context.hpp>
    <ebmcodegen/stub/dependency.hpp>
    <ebmcodegen/stub/make_visitor.hpp>
    <ebmcodegen/stub/output.hpp>
    <ebmcodegen/stub/writer_manager.hpp>
    <code/loc_writer.h>
    <concepts>
    <strutil/append.h>
)

endif()

add_library(ebmcodegen_lib STATIC
    "src/ebmcodegen/body_subset.cpp"
    "src/ebmcodegen/structs.cpp"
    "src/ebmcodegen/json_loader.cpp"
    "src/ebmcodegen/dsl/dsl.cpp"
    "src/ebmcodegen/class_based.cpp"
    "src/ebmcodegen/access.cpp"
)

add_executable(ebmcodegen
    "src/ebmcodegen/main.cpp"
)

target_link_libraries(ebmcodegen ebm futils ebmcodegen_lib)
#target_link_libraries(gen_template2 futils bm)
install(TARGETS ebm ebmcodegen DESTINATION tool)


#add_subdirectory("src/ebmgen/test")
if(NOT "$ENV{CODEGEN_ONLY}" STREQUAL "1")
add_subdirectory("src/ebmcg")
add_subdirectory("src/ebmip")
endif()





#install(TARGETS gen_template2 COMPONENT gen_template DESTINATION tool)
if("$ENV{BUILD_MODE}" STREQUAL "native")
# copy libfutils.dll to tool directory
if(NOT "$ENV{CODEGEN_ONLY}" STREQUAL "1")
    add_custom_command(
    TARGET ebmgen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$ENV{FUTILS_DIR}/built/shared/${CMAKE_BUILD_TYPE}/lib/libfutils${CMAKE_SHARED_LIBRARY_SUFFIX}" "${CMAKE_SOURCE_DIR}/tool/libfutils${CMAKE_SHARED_LIBRARY_SUFFIX}"
    )
    # if libs2j exists in BRGEN_DIR build output, copy it
    if(DEFINED ENV{BRGEN_DIR} AND EXISTS "$ENV{BRGEN_DIR}/built/native/${CMAKE_BUILD_TYPE}/tool/libs2j${CMAKE_SHARED_LIBRARY_SUFFIX}")
        add_custom_command(
        TARGET ebmgen POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$ENV{BRGEN_DIR}/built/native/${CMAKE_BUILD_TYPE}/tool/libs2j${CMAKE_SHARED_LIBRARY_SUFFIX}" "${CMAKE_SOURCE_DIR}/tool/libs2j${CMAKE_SHARED_LIBRARY_SUFFIX}"
        )
    endif()
else()
    add_custom_command(
    TARGET ebmcodegen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$ENV{FUTILS_DIR}/built/shared/${CMAKE_BUILD_TYPE}/lib/libfutils${CMAKE_SHARED_LIBRARY_SUFFIX}" "${CMAKE_SOURCE_DIR}/tool/libfutils${CMAKE_SHARED_LIBRARY_SUFFIX}"
    )
endif()

elseif("$ENV{BUILD_MODE}" STREQUAL "web")
if("$ENV{BUILD_OLD_BMGEN}" STREQUAL "1")
install(FILES "${CMAKE_BINARY_DIR}/tool/bmgen.wasm" DESTINATION tool)
endif()
install(FILES "${CMAKE_BINARY_DIR}/tool/ebmgen.wasm" DESTINATION tool)
endif()
