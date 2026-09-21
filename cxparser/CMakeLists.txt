# File: CMakeLists.txt (MINIMAL VERSION)
# Role: Builds cxcore_full_core and cxparser_ext_cxscript_cli targets.

cmake_minimum_required(VERSION 3.20)

project(cxparser_minimal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Helper functions and flags kept for dependency chain stability ---

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

function(cxparser_harden_standard_entry target_name)
  if(MSVC)
    # Minimal hardening for the core/entry points
    target_compile_options(${target_name} PRIVATE /utf-8 /FS /Z7)
    # set_property(TARGET ${target_name} PROPERTY VS_GLOBAL_TrackFileAccess false) # Removed for minimal code
  endif()
endfunction()

# --- 1. cxparser_core Library (Required by cxparser_ext_cxscript_cli) ---

add_library(cxparser_core STATIC
  muParser.cpp
  muParserBase.cpp
  muParserBytecode.cpp
  muParserCallback.cpp
  muParserClass.cpp
  muParserClassFunctionReader.cpp
  muparserClassReader.cpp
  muParserError.cpp
  muParserInt.cpp
  muParserStrClassMap.cpp
  muParserTreeNode.cpp
  muparsertokenreader.cpp
)

target_include_directories(cxparser_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
  target_compile_definitions(cxparser_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
cxparser_harden_standard_entry(cxparser_core)


# --- 2. cxcore_full_core (Optional/Conditional Target) ---

option(CXPARSER_ENABLE_CXCORE_REAL_BRIDGE "Enable cxcore full-core bridge for real numeric classical tests" OFF)

if(CXPARSER_ENABLE_CXCORE_REAL_BRIDGE)
  # Note: Full normalization/setup logic was complex and is omitted here for brevity,
  # assuming necessary environment variables/flags (like CUDA_NVCC_FLAGS) are set elsewhere.

  # Setting required flags for cxcore_full_core
  set(CXCORE_BUILD_OCCT_SMOKE OFF CACHE BOOL "" FORCE)
  set(CXCORE_BUILD_OPENCV_SMOKE OFF CACHE BOOL "" FORCE)
  set(CXCORE_BUILD_3D_DETAIL_CORE OFF CACHE BOOL "" FORCE)
  set(CXCORE_BUILD_FULL_CORE ON CACHE BOOL "" FORCE)

  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cxcore ${CMAKE_BINARY_DIR}/cxcore_from_cxparser)

  if(TARGET cxcore_full_core)
    # Adding external geometric dependencies
    target_sources(cxcore_full_core PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/../cxgeom/src/CxShapeHandle.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../cxgeom/src/CxCurveBuilder.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../cxgeom/src/CxSetCircleBuild.cpp
    )
    target_include_directories(cxcore_full_core PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/../cxgeom/include
    )
  endif()
endif()


# --- 3. cxparser_ext_cxscript_cli (The main application target) ---

add_executable(cxparser_ext_cxscript_cli
  ../cxparser_ext/debug/cxscript_debug_embedded_runner.cpp
  ../cxparser_ext/debug/cxscript_debug_log_builder.cpp
  ../cxparser_ext/scenarios/parser_cxscript_cli.cpp
)

target_include_directories(cxparser_ext_cxscript_cli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../cxparser_ext/debug
)

target_link_libraries(cxparser_ext_cxscript_cli PRIVATE cxparser_core)

# Conditional linking for CXCORE_REAL_BRIDGE (if enabled)
if(CXPARSER_ENABLE_CXCORE_REAL_BRIDGE)
  target_compile_definitions(cxparser_ext_cxscript_cli PRIVATE CXPARSER_ENABLE_CXCORE_REAL_BRIDGE=1)
  target_include_directories(cxparser_ext_cxscript_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../cxcore/core
    # Assuming OpenCV_DIR and OCCT_ROOT are set externally for this minimal example
    # ${OpenCV_DIR}/include
    # ${OCCT_ROOT}/inc
  )
  # Linking against cxcore targets
  if(TARGET cxcore_full_core)
      target_link_libraries(cxparser_ext_cxscript_cli PRIVATE cxcore_full_core)
  endif()
  # Minimal placeholder links for OCCT/OpenCV if required
  # target_link_libraries(cxparser_ext_cxscript_cli PRIVATE cxcore_occt_runtime ${OpenCV_LIBS})
endif()

if(MSVC)
  target_compile_definitions(cxparser_ext_cxscript_cli PRIVATE
    CXPARSER_WORKSPACE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.."
  )
  target_compile_options(cxparser_ext_cxscript_cli PRIVATE /utf-8)
endif()

cxparser_harden_standard_entry(cxparser_ext_cxscript_cli)
