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

add_executable(seven_class_dataset_test dataset_test.cpp
  ../../cximage/CxSevenClassSupervision.cpp
  ../../cxgeom/src/CxGeoSO2ReferenceAsset.cpp
  ../../cxgeom/src/CxGeoSO2Harmonic.cpp)
target_link_libraries(seven_class_dataset_test PRIVATE cxparser_core)
target_include_directories(seven_class_dataset_test PRIVATE ../../cxgeom/include)
if(MSVC)
  target_compile_options(seven_class_dataset_test PRIVATE /utf-8)
endif()
add_test(NAME SevenClass_Dataset_Freeze_And_Runtime_Guards
  COMMAND seven_class_dataset_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../contracts/seven_class_supervision_rules.v1.json"
    "${CMAKE_CURRENT_BINARY_DIR}/dataset-evidence")

add_executable(seven_class_materializer materializer_main.cpp
  ../../cximage/CxSevenClassSupervision.cpp
  ../../cxgeom/src/CxGeoSO2ReferenceAsset.cpp
  ../../cxgeom/src/CxGeoSO2Harmonic.cpp)
target_include_directories(seven_class_materializer PRIVATE ../../cxgeom/include)
add_test(NAME SevenClass_Native_Synthetic_Materializer COMMAND seven_class_materializer
  "${CMAKE_CURRENT_SOURCE_DIR}/../../contracts/seven_class_supervision_rules.v1.json"
  "${CMAKE_CURRENT_BINARY_DIR}/materializer-evidence")
