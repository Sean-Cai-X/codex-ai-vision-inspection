cmake_minimum_required(VERSION 3.21)

project(cxvision_dev_analysis_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(CXVISION_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(GLFW_ROOT "D:/glfw-3.3.10" CACHE PATH "Path to the GLFW source tree")
set(GLAD_ROOT
    "${CXVISION_REPO_ROOT}/../../analysis_workspace/imGuIZMO.quat/libs/glad"
    CACHE PATH "Path to glad sources")
set(OCCT_ROOT
    "D:/OpenCASCADE-7.7.0-vc14-64/opencascade-7.7.0"
    CACHE PATH "Path to the OpenCASCADE root")
set(OpenCV_DIR "D:/opencv/build" CACHE PATH "Path to OpenCVConfig.cmake")

if(NOT EXISTS "${GLFW_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "GLFW_ROOT is invalid: ${GLFW_ROOT}")
endif()
if(NOT EXISTS "${GLAD_ROOT}/glad.cpp")
  message(FATAL_ERROR "GLAD_ROOT is invalid: ${GLAD_ROOT}")
endif()
if(NOT EXISTS "${OCCT_ROOT}/inc/Standard.hxx")
  message(FATAL_ERROR "OCCT_ROOT is invalid: ${OCCT_ROOT}")
endif()
if(NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
  message(FATAL_ERROR "OpenCV_DIR is invalid: ${OpenCV_DIR}")
endif()

find_package(OpenCV REQUIRED CONFIG)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${GLFW_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/glfw" EXCLUDE_FROM_ALL)

add_executable(cxvision_dev_analysis_gui
  main.cpp
  DevAnalysisGuiApp.cpp
  DevAnalysisGuiState.cpp
  adapters/DevAnalysisGeomAttachmentDemo.cpp
  panels/DevAnalysisGuiPanels.cpp
  "${CXVISION_REPO_ROOT}/cxgeom/src/CxGeomAnnotationBody.cpp"
  "${CXVISION_REPO_ROOT}/cxgeom/src/CxGeomElementBody.cpp"
  "${CXVISION_REPO_ROOT}/cxgeom/src/CxShapeHandle.cpp"
  "${GLAD_ROOT}/glad.cpp"
  "${CXVISION_REPO_ROOT}/imgui/imgui.cpp"
  "${CXVISION_REPO_ROOT}/imgui/imgui_draw.cpp"
  "${CXVISION_REPO_ROOT}/imgui/imgui_tables.cpp"
  "${CXVISION_REPO_ROOT}/imgui/imgui_widgets.cpp"
  "${CXVISION_REPO_ROOT}/imgui/imgui_impl_glfw.cpp"
  "${CXVISION_REPO_ROOT}/imgui/imgui_impl_opengl3.cpp"
)

target_include_directories(cxvision_dev_analysis_gui PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/panels"
  "${GLAD_ROOT}/.."
  "${CXVISION_REPO_ROOT}/imgui"
  "${CXVISION_REPO_ROOT}/cxgeom/include"
  "${OpenCV_INCLUDE_DIRS}"
  "${OCCT_ROOT}/inc"
)

target_link_directories(cxvision_dev_analysis_gui PRIVATE
  "${OCCT_ROOT}/win64/vc14/lib"
)

target_compile_definitions(cxvision_dev_analysis_gui PRIVATE
  _CRT_SECURE_NO_WARNINGS
  ImTextureID=ImU64
)

target_link_libraries(cxvision_dev_analysis_gui PRIVATE
  glfw
  opengl32
  TKernel
  TKMath
  TKG2d
  TKG3d
  TKGeomBase
  TKBRep
  TKGeomAlgo
  TKTopAlgo
  TKPrim
  ${OpenCV_LIBS}
)

if(MSVC)
  target_compile_options(cxvision_dev_analysis_gui PRIVATE /W4 /EHsc)
endif()

set_target_properties(cxvision_dev_analysis_gui PROPERTIES
  OUTPUT_NAME "cxvision_dev_analysis_gui"
  FOLDER "development/analysis"
)

install(TARGETS cxvision_dev_analysis_gui
  RUNTIME DESTINATION bin
)
