#license
cmake_minimum_required(VERSION 3.22.1)

# find_package(Clang )

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tool")
add_executable(src2json "tool/src2json.cpp")

target_link_libraries(src2json utils)

add_executable(json2cpp "tool/json2cpp/main.cpp")

target_link_libraries(json2cpp utils)

if(WIN32)
set(SUFFIX ".exe")
endif()

add_custom_target(brgen ALL COMMAND "go" "build" "-o" "${CMAKE_BINARY_DIR}/tool/brgen${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/brgen" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/brgen" )

add_custom_target(json2go ALL COMMAND "go" "build" "-o" "${CMAKE_BINARY_DIR}/tool/json2go${SUFFIX}" "${CMAKE_SOURCE_DIR}/src/tool/json2go" DEPENDS "${CMAKE_SOURCE_DIR}/src/tool/json2go" "${CMAKE_SOURCE_DIR}/ast2go")


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

add_subdirectory(test)