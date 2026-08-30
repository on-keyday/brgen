#license
# nast を単体で建てる。brgen 本体の CMake からは独立していて、ここだけを
# configure できる。本体は C++20 で組んであるが nast は C++23 を要る。
#
#   cmake -S src/core/nast -B src/core/nast/build/cmake -G Ninja
#   cmake --build src/core/nast/build/cmake
#
# 生成 (nodes.h / nast_wire.bgn / nast_wire_conv.hpp) は build 時に走る。
# nast_wire.hpp だけは brgen 本体の src2json / json2cpp2 を要るので、
# 建っていれば作り直し、無ければ既存のものを使う。
#
# ソースが増えたらここに足す。glob は使わない — ファイルを足したときに
# configure し直す必要があるかどうかが、書いてある内容から判らなくなるため。
cmake_minimum_required(VERSION 3.26)
project(nast CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# 並列数を絞る。既定 (全コア) だとこのマシンは熱で周波数が落ち、画面も
# 固まる。上げたいときは -DNAST_BUILD_PARALLEL=<n> か、
# cmake --build ... -j <n> で明示する。
set(NAST_BUILD_PARALLEL "4" CACHE STRING "parallel compile jobs")
if(NAST_BUILD_PARALLEL)
    set_property(GLOBAL PROPERTY JOB_POOLS nast_compile=${NAST_BUILD_PARALLEL})
    set(CMAKE_JOB_POOL_COMPILE nast_compile)
    set(CMAKE_JOB_POOL_LINK nast_compile)
endif()

# デバッグ情報は既定で出さない。Debug の既定 (-g -gcodeview) だと
# 実行ファイル 1 本につき 45-60MB の pdb を作り、そのリンクだけで 20 秒近く
# かかる。ライブラリを 1 つ触ると全ツールがリンクし直されるので、
# 書いて測る回転がそのぶん止まる。デバッガに乗せたいときだけ
# -DNAST_DEBUG_INFO=ON。
option(NAST_DEBUG_INFO "emit debug info (large pdb, slow link)" OFF)
if(NAST_DEBUG_INFO)
    # clang++ をドライバにすると、CMake はコンパイルに -g -gcodeview を
    # 入れるだけでリンカには何も渡さない。lld-link / link.exe は /DEBUG が
    # 無いと pdb を作らず、実行ファイルからも参照しないので、デバッガが
    # シンボルを見つけられない (ブレークポイントが刺さらない)。
    if(WIN32)
        add_link_options("-Xlinker" "/DEBUG")
    endif()
endif()
if(NOT NAST_DEBUG_INFO)
    # Windows の clang 向けに CMake が入れる -g -gcodeview を、構成ごとの
    # フラグ変数から抜く。後ろに -g0 を足すやり方でも消えるが、コマンドに
    # 矛盾したフラグが並ぶので取り除くほうにする。
    foreach(cfg DEBUG RELWITHDEBINFO RELEASE MINSIZEREL)
        set(_v "CMAKE_CXX_FLAGS_${cfg}")
        if(DEFINED ${_v})
            string(REGEX REPLACE "(^| )-g(codeview)?( |$)" " " ${_v} "${${_v}}")
            string(REGEX REPLACE "(^| )/(Zi|ZI|Z7)( |$)" " " ${_v} "${${_v}}")
            string(STRIP "${${_v}}" ${_v})
        endif()
    endforeach()
    # Windows では -g -gcodeview がフラグ変数ではなく
    # CMAKE_MSVC_DEBUG_INFORMATION_FORMAT (既定 = Embedded/ProgramDatabase)
    # から来る。空にすると出なくなる。
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "")
endif()

set(NAST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(REPO_ROOT "${NAST_DIR}/../../.." ABSOLUTE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# ---- futils --------------------------------------------------------------
# build.py と同じ順で探す: FUTILS_DIR (cache か環境変数) -> <repo>/utils ->
# rebrgen/build_config.json。ヘッダの在処で判定する。
function(nast_probe_futils root out_var)
    if(root AND EXISTS "${root}/src/include/json/stringer.h")
        set(${out_var} "${root}" PARENT_SCOPE)
    endif()
endfunction()

if(NOT FUTILS_DIR)
    nast_probe_futils("$ENV{FUTILS_DIR}" FUTILS_DIR)
endif()
if(NOT FUTILS_DIR)
    nast_probe_futils("${REPO_ROOT}/utils" FUTILS_DIR)
endif()
if(NOT FUTILS_DIR AND EXISTS "${REPO_ROOT}/rebrgen/build_config.json")
    file(READ "${REPO_ROOT}/rebrgen/build_config.json" _cfg)
    string(JSON _cfg_futils ERROR_VARIABLE _ignored GET "${_cfg}" FUTILS_DIR)
    nast_probe_futils("${_cfg_futils}" FUTILS_DIR)
endif()
if(NOT FUTILS_DIR)
    message(FATAL_ERROR "futils not found. set -DFUTILS_DIR=<dir> or FUTILS_DIR env")
endif()
message(STATUS "nast: futils at ${FUTILS_DIR}")

# ライブラリ本体は built/<mode>/<type>/lib にある。mode は futils 側の
# FUTILS_BUILD_MODE で、shared 以外 (freestanding-static / kernel / gcc /
# wasm-em) はそれぞれ別用途の成果物なので混ぜない。ここは shared だけを見る。
# 別の場所を使いたいときは -DFUTILS_LIB=<path> で直に渡す。
set(FUTILS_BUILD_TYPE "Debug" CACHE STRING "futils build type to link against")
find_library(FUTILS_LIB
    NAMES futils libfutils
    PATHS "${FUTILS_DIR}/built/shared/${FUTILS_BUILD_TYPE}/lib"
    NO_DEFAULT_PATH REQUIRED)
message(STATUS "nast: futils lib ${FUTILS_LIB}")

add_library(futils_imported INTERFACE)
target_include_directories(futils_imported INTERFACE "${FUTILS_DIR}/src/include")
target_link_libraries(futils_imported INTERFACE "${FUTILS_LIB}")
if(MSVC OR (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    # shared の futils は dll のデバッグ実行時と組んである。合わせないと
    # 実行時ライブラリが二重になってリンクが割れる。
    target_compile_options(futils_imported INTERFACE "-fms-runtime-lib=dll_dbg")
endif()

# ---- 生成 ---------------------------------------------------------------
# nodes.json -> node/nodes.h と lsp の nast_nodes.ts
set(NODES_JSON "${NAST_DIR}/node/nodes.json")
set(NODES_H "${NAST_DIR}/node/nodes.h")
set(NAST_NODES_TS "${REPO_ROOT}/lsp/server/src/nast_nodes.ts")
file(GLOB NODEGEN_SRC "${NAST_DIR}/gen/*.py")
add_custom_command(
    OUTPUT "${NODES_H}" "${NAST_NODES_TS}"
    COMMAND Python3::Interpreter "${NAST_DIR}/nodegen.py"
    DEPENDS "${NODES_JSON}" "${NAST_DIR}/nodegen.py" ${NODEGEN_SRC}
    COMMENT "nodegen: node/nodes.h + lsp nast_nodes.ts"
    VERBATIM)

# nodes.json -> wire/nast_wire.bgn と wire/nast_wire_conv.hpp
set(WIRE_BGN "${NAST_DIR}/wire/nast_wire.bgn")
set(WIRE_CONV "${NAST_DIR}/wire/nast_wire_conv.hpp")
add_custom_command(
    OUTPUT "${WIRE_BGN}" "${WIRE_CONV}"
    COMMAND Python3::Interpreter "${NAST_DIR}/wiregen.py"
    DEPENDS "${NODES_JSON}" "${NAST_DIR}/wiregen.py"
    COMMENT "wiregen: wire/nast_wire.bgn + nast_wire_conv.hpp"
    VERBATIM)

# nast_wire.bgn -> nast_wire.hpp。brgen 本体のツールを要るので、
# 建っていなければ既存のものを残す (build.py と同じ扱い)。
set(WIRE_HPP "${NAST_DIR}/wire/nast_wire.hpp")
if(WIN32)
    set(_exe ".exe")
else()
    set(_exe "")
endif()
set(SRC2JSON "${REPO_ROOT}/tool/src2json${_exe}")
set(JSON2CPP2 "${REPO_ROOT}/tool/json2cpp2${_exe}")
if(EXISTS "${SRC2JSON}" AND EXISTS "${JSON2CPP2}")
    set(WIRE_JSON "${REPO_ROOT}/ignore/nast/nast_wire.json")
    add_custom_command(
        OUTPUT "${WIRE_HPP}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REPO_ROOT}/ignore/nast"
        COMMAND ${CMAKE_COMMAND} -E env "${SRC2JSON}" "${WIRE_BGN}" > "${WIRE_JSON}"
        COMMAND ${CMAKE_COMMAND} -E env "${JSON2CPP2}" -f "${WIRE_JSON}" --use-error > "${WIRE_HPP}"
        DEPENDS "${WIRE_BGN}" "${SRC2JSON}" "${JSON2CPP2}"
        COMMENT "json2cpp2: wire/nast_wire.hpp"
        VERBATIM)
else()
    message(STATUS "nast: src2json/json2cpp2 not built; keeping wire/nast_wire.hpp as is")
endif()

add_custom_target(nast_generate DEPENDS "${NODES_H}" "${WIRE_CONV}" "${WIRE_HPP}")

# ---- ライブラリ ----------------------------------------------------------
# 木を組む段と解析の段。ツールはこれをリンクする。
add_library(nast_core STATIC
    "parse/parse.cpp"
    "parse/stream.cpp"
    "parse/unparse.cpp"
    "node/compare.cpp"
    "lowering/conditional.cpp"
    "lowering/field_io.cpp"
    "lowering/int_bytes.cpp"
    "lowering/match_to_if.cpp"
    "lowering/predicate.cpp"
    "lowering/stream_io.cpp"
    "bind/endian_scope.cpp"
    "bind/evaluator.cpp"
    "bind/import_resolver.cpp"
    "bind/pipeline.cpp"
    "bind/requires.cpp"
    "bind/scope_resolver.cpp"
    "bind/type_size.cpp"
    "bind/typer.cpp"
    "bind/union_layout.cpp"
)
add_dependencies(nast_core nast_generate)
target_include_directories(nast_core PUBLIC "${NAST_DIR}" "${REPO_ROOT}/src")
target_link_libraries(nast_core PUBLIC futils_imported)

# ---- ツール --------------------------------------------------------------
# nast_test はヘッダだけで足りるが、compare.cpp の実体は要る。
add_executable(nast_test "tool/test.cpp" "node/compare.cpp")
target_include_directories(nast_test PRIVATE "${NAST_DIR}" "${REPO_ROOT}/src")
target_link_libraries(nast_test PRIVATE futils_imported)
target_compile_definitions(nast_test PRIVATE NAST_TEST_WITH_JSON NAST_TEST_WITH_JSON_PARSE)
add_dependencies(nast_test nast_generate)

foreach(tool corpus dump unparse_test wire_test backend)
    add_executable(nast_${tool} "tool/${tool}.cpp")
    target_link_libraries(nast_${tool} PRIVATE nast_core)
endforeach()

# ---- バックエンド ----
# newbackend.py が足す。ここに並んだものが nast2<lang> として建つ。

# ---- 計測 ----------------------------------------------------------------
foreach(b coverage fn_fallible lex_split parse_split probe token_cost)
    add_executable(nast_${b} "bench/${b}.cpp")
    target_link_libraries(nast_${b} PRIVATE nast_core)
endforeach()

# ---- テスト --------------------------------------------------------------
enable_testing()
add_test(NAME nast_unit COMMAND nast_test)

# example/ の実フォーマットに加えて、testdata/ の確認用も往復に乗せる。
# あちらは「この構文がこう解けること」を目で見るために書いたものだが、
# 目で見た後は往復が守ってくれる側に置く。
file(GLOB_RECURSE NAST_CORPUS "${REPO_ROOT}/example/*.bgn" "${NAST_DIR}/testdata/*.bgn")
if(NAST_CORPUS)
    # 構文エラーを含む入力が 3 つあるので、corpus は戻り値を見ない
    # (数え上げが目的で、落ちないことだけ確かめる)。
    add_test(NAME nast_wire_roundtrip COMMAND nast_wire_test ${NAST_CORPUS})
    add_test(NAME nast_unparse_roundtrip COMMAND nast_unparse_test ${NAST_CORPUS})
endif()

# Windows では futils が dll なので、実行できるように隣へ置く。
# Windows では futils が dll なので、実行できるように隣へ置く。
#
# どのツールを建てても要るものなので、nast_core に紐づける。以前は
# nast_test の POST_BUILD にしていたが、それだと `-t nast_backend` だけを
# 建てたツリーに dll が来ず、起動が 0xC0000135 (DLL not found) で落ちた。
if(WIN32)
    get_filename_component(_futils_lib_dir "${FUTILS_LIB}" DIRECTORY)
    add_custom_target(nast_runtime_dll ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_futils_lib_dir}/libfutils.dll"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfutils.dll"
        COMMENT "copying libfutils.dll next to the tools"
        VERBATIM)
    # どのツールも起動に要るので、実行ファイル全部から依存させる。
    foreach(t nast_test nast_corpus nast_dump nast_unparse_test nast_wire_test nast_backend
              nast_coverage nast_fn_fallible nast_lex_split nast_parse_split nast_token_cost)
        if(TARGET ${t})
            add_dependencies(${t} nast_runtime_dll)
        endif()
    endforeach()
endif()
