cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)

project(GLoops_demo)

# demo files
file(GLOB GLOOPS_DEMO_SRC "*.cpp")
file(GLOB GLOOPS_DEMO_SHADERS "resources/shaders/*.*")
message("GLOOPS_DEMO_SHADERS : " ${GLOOPS_DEMO_SHADERS})
source_group("Source Files\\shaders" FILES ${GLOOPS_DEMO_SHADERS})
add_executable(GLoops_demo ${GLOOPS_DEMO_SRC} ${GLOOPS_DEMO_SHADERS})

# GLOOPS
set(GLOOPS_DEMO_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(GLOOPS_PATH "${GLOOPS_DEMO_PATH}/../")
add_subdirectory(${GLOOPS_PATH} "${CMAKE_CURRENT_BINARY_DIR}/gloops/") 
target_link_libraries(GLoops_demo GLoops)
target_compile_definitions(GLoops_demo PUBLIC 
	GLOOPS_DEMO_RESOURCES_PATH="${GLOOPS_DEMO_PATH}/resources/")