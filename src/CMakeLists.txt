#license
cmake_minimum_required(VERSION 3.22.1)

add_subdirectory(test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tool")
add_executable(src2json "tool/src2json.cpp")

target_link_libraries(src2json utils)

if(WIN32)
set(SUFFIX ".exe")
endif()

add_custom_target(brgen ALL COMMAND "go" "build" "-o" "${CMAKE_BINARY_DIR}/tool/brgen${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/brgen")

if(WIN32)

add_custom_command(
TARGET src2json POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.dll" "${CMAKE_BINARY_DIR}/tool/libutils.dll"
)
add_custom_command(
TARGET src2json POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.pdb" "${CMAKE_BINARY_DIR}/tool/libutils.pdb"
)
else()

add_custom_command(
TARGET src2json POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "$ENV{UTILS_DIR}/lib/libutils.so" "${CMAKE_BINARY_DIR}/tool/libutils.so"
)
endif()
