#license
cmake_minimum_required(VERSION 3.22.1)
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
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


include_directories("$ENV{UTILS_DIR}/src/include")
include_directories("./src")
include_directories("./build/_deps/googletest-src/googletest/include")
link_directories("$ENV{UTILS_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")

add_subdirectory(src)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")





