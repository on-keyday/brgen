#license
cmake_minimum_required(VERSION 3.26)
enable_testing()
project(brgen VERSION 0.0.2)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(NOT "$ENV{FUTILS_TARGET_TRIPLE}" STREQUAL "")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -target=$ENV{FUTILS_TARGET_TRIPLE}")
  endif()
  set(CMAKE_CXX_FLAGS "-fuse-ld=ld.lld")
endif()

if(UNIX)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
endif()
endif()




set(CMAKE_CXX_STANDARD 20)


if("$ENV{BUILD_MODE}" STREQUAL "native")
link_directories("$ENV{FUTILS_DIR}/built/shared/${CMAKE_BUILD_TYPE}/lib")
set(FUTILS_LIB_DIR "$ENV{FUTILS_DIR}/built/shared/${CMAKE_BUILD_TYPE}/lib")
elseif("$ENV{BUILD_MODE}" STREQUAL "wasm-em")
link_directories("$ENV{FUTILS_DIR}/built/wasm-em/${CMAKE_BUILD_TYPE}/lib")
set(FUTILS_LIB_DIR "$ENV{FUTILS_DIR}/built/wasm-em/${CMAKE_BUILD_TYPE}/lib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -sENVIRONMENT=worker -sINVOKE_RUN=0 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXIT_RUNTIME=0 -sEXPORTED_RUNTIME_METHODS=[ccall,cwrap,FS]")
endif()

if(NOT GO_COMPILER)
if("$ENV{GO_COMPILER}" STREQUAL "")
message("GO_COMPILER not set, using default")
set(GO_COMPILER "go")
else()
message("GO_COMPILER set to $ENV{GO_COMPILER}")
set(GO_COMPILER "$ENV{GO_COMPILER}")
endif()
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
set(CMAKE_GO_FLAGS "-ldflags=-s -w")
else()
set(CMAKE_GO_FLAGS "")
endif()

if("$ENV{BUILD_MODE}" STREQUAL "native")
set(INSTALL_GTEST OFF)
include(FetchContent)
FetchContent_Declare(
  googletest
  DOWNLOAD_EXTRACT_TIMESTAMP true
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
endif()


include_directories("$ENV{FUTILS_DIR}/src/include")
include_directories("./src")
include_directories("./built/$ENV{BUILD_MODE}/${CMAKE_BUILD_TYPE}/_deps/googletest-src/googletest/include")


if("$ENV{S2J_USE_NETWORK}" STREQUAL "1")
set(S2J_USE_NETWORK 1)
endif()

if("$ENV{S2J_LIB}" STREQUAL "1")  
set(S2J_LIB 1)
endif()

add_subdirectory(src)






