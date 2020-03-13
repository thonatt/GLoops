cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)

project(GLoops_demo)

# demo files
file(GLOB GLOOPS_DEMO_SRC "*.cpp")

add_executable(GLoops_demo ${GLOOPS_DEMO_SRC})

# GLOOPS
set(GLOOP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../")
add_subdirectory(${GLOOP_PATH} "${CMAKE_CURRENT_BINARY_DIR}/gloops/") 
target_link_libraries(GLoops_demo GLoops)