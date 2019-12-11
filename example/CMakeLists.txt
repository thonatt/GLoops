cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)

project(GLoops_example)
set(CURRENT_TARGET GLoops_example)

file(GLOB AppFiles "*.cpp")

add_executable(GLoops_example ${AppFiles})

# GLOOPS
set(GLOOP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../")
message("GLOOP_PATH" ${GLOOP_PATH})

FIND_PATH(GLOOPS_DIR CMakeLists.txt "${CMAKE_CURRENT_SOURCE_DIR}/../")
include("${GLOOPS_DIR}/CMakeLists.txt")