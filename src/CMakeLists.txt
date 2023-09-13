#license
cmake_minimum_required(VERSION 3.22.1)

add_subdirectory(test)

add_executable(src2json "tool/src2json.cpp")

target_link_libraries(src2json utils)
