cmake_minimum_required(VERSION 3.0)
enable_testing()

project(GeneratedCodeTest CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories("${FUTILS_DIR}/src/include")
link_directories("${FUTILS_DIR}/lib")

