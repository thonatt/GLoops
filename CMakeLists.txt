#requires CURRENT_TARGET and GLOOPS_DIR to be set
#defines gloopsFiles gloopsShaders gloopsImguiFiles

set(GLOOPS_EXTLIBS_PATH "${GLOOPS_DIR}/extlibs/")
set(GLOOPS_SRC_PATH "${GLOOPS_DIR}/gloops/")

include_directories( ${GLOOPS_DIR} ${GLOOPS_EXTLIBS_PATH} )

# GLOOPS CORE
file(GLOB gloopsFiles "${GLOOPS_SRC_PATH}*.hpp" "${GLOOPS_SRC_PATH}*.cpp")

# SHADERS
set(GLOOPS_SHADERS_PATH "${GLOOPS_SRC_PATH}/shaders/")
file(GLOB gloopsShaders 
	"${GLOOPS_SHADERS_PATH}*.vert" 
	"${GLOOPS_SHADERS_PATH}*.frag" 
	"${GLOOPS_SHADERS_PATH}*.geom")

# IMGUI
set(GLOOPS_IMGUI_PATH "${GLOOPS_EXTLIBS_PATH}/imgui-master/")
set(GLOOPS_IMGUI_OPENGL "${GLOOPS_IMGUI_PATH}examples/imgui_impl_glfw")
set(GLOOPS_IMGUI_GLFW "${GLOOPS_IMGUI_PATH}examples/imgui_impl_opengl3")
file(GLOB gloopsImguiFiles "${GLOOPS_IMGUI_PATH}*.h" "${GLOOPS_IMGUI_PATH}*.cpp" 
	"${GLOOPS_IMGUI_OPENGL}.h" "${GLOOPS_IMGUI_OPENGL}.cpp" 
	"${GLOOPS_IMGUI_GLFW}.h" "${GLOOPS_IMGUI_GLFW}.cpp" )

include_directories(${GLOOPS_IMGUI_PATH})

# GLOOPS SETUP

source_group("gloops\\" FILES ${gloopsFiles})
source_group("gloops\\shaders" FILES ${gloopsShaders})
source_group("gloops\\imgui" FILES ${gloopsImguiFiles})

target_sources(${CURRENT_TARGET} PUBLIC ${gloopsFiles} ${gloopsShaders} ${gloopsImguiFiles})

# GLEW
find_package(OpenGL REQUIRED)

set(GLEW_INCLUDE_DIR "${GLOOPS_EXTLIBS_PATH}/glew-2.1.0/include/")
set(GLEW_SHARED_LIBRARY_RELEASE "${GLOOPS_EXTLIBS_PATH}/glew-2.1.0/lib/Release/x64/glew32.lib")
find_package(GLEW REQUIRED)
target_link_libraries(${CURRENT_TARGET} ${OPENGL_gl_LIBRARY} GLEW::GLEW)

# GLFW
find_package(GLFW3 3.3 REQUIRED HINTS "${GLOOPS_EXTLIBS_PATH}/glfw/lib/cmake/glfw3/")
target_link_libraries(${CURRENT_TARGET} glfw ${GLFW_LIBRARIES})

# EIGEN
find_package (Eigen3 3.3 REQUIRED NO_MODULE HINTS "${GLOOPS_EXTLIBS_PATH}/Eigen3.3.7/share/eigen3/cmake/")
target_link_libraries (${CURRENT_TARGET} Eigen3::Eigen)

# ASSIMP
find_package(assimp 5.0 REQUIRED HINTS "${GLOOPS_EXTLIBS_PATH}/Assimp/lib/cmake/assimp-5.0/")
target_link_libraries(${CURRENT_TARGET} assimp::assimp)

# EMBREE
find_package(embree 3.0 REQUIRED HINTS "${GLOOPS_EXTLIBS_PATH}/Embree/")
target_link_libraries(${CURRENT_TARGET} ${EMBREE_LIBRARIES})
include_directories(${EMBREE_INCLUDE_DIRS})