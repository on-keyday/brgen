#license
cmake_minimum_required(VERSION 3.26.4)

project(brgen)

set(CMAKE_CXX_STANDARD 20)
include_directories("$ENV{UTILS_DIR}/src/include")
include_directories("./src")
link_directories("$ENV{UTILS_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "test")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "test")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "test")
add_subdirectory(src)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")




include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Now simply link against gtest or gtest_main as needed. Eg
# add_executable(example example.cpp)
# target_link_libraries(example gtest_main)
add_test(NAME example_test COMMAND example)
