cmake_minimum_required(VERSION 3.20)
project(cx_seven_class_supervision_native LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(../../cxparser parser)
add_executable(seven_class_supervision_native_test
  native_test.cpp
  ../../cximage/CxSevenClassSupervision.cpp
  ../../cxgeom/src/CxGeoSO2ReferenceAsset.cpp
  ../../cxgeom/src/CxGeoSO2Harmonic.cpp)
target_link_libraries(seven_class_supervision_native_test PRIVATE cxparser_core)
target_include_directories(seven_class_supervision_native_test PRIVATE ../../cxgeom/include)
if(MSVC)
  target_compile_options(seven_class_supervision_native_test PRIVATE /utf-8 /W4)
endif()
add_test(NAME SevenClass_Native_CxScript_Conversion
  COMMAND seven_class_supervision_native_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../contracts/seven_class_supervision_rules.v1.json"
    "${CMAKE_CURRENT_BINARY_DIR}/native-evidence")
