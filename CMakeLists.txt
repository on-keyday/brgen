#license
cmake_minimum_required(VERSION 3.26)
enable_testing()
project(brgen)
if(UNIX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
endif()

set(CMAKE_CXX_STANDARD 23)
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


include_directories("$ENV{UTILS_DIR}/src/include")
include_directories("./src")
include_directories("./build/_deps/googletest-src/googletest/include")

if("$ENV{BUILD_MODE}" STREQUAL "native")
link_directories("$ENV{UTILS_DIR}/built/shared/${CMAKE_BUILD_TYPE}/lib")
set(UTILS_LIB_DIR "$ENV{UTILS_DIR}/built/shared/${CMAKE_BUILD_TYPE}/lib")
elseif("$ENV{BUILD_MODE}" STREQUAL "wasm-em")
link_directories("$ENV{UTILS_DIR}/built/wasm-em/${CMAKE_BUILD_TYPE}/lib")
set(UTILS_LIB_DIR "$ENV{UTILS_DIR}/built/wasm-em/${CMAKE_BUILD_TYPE}/lib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
endif()


add_subdirectory(src)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")





