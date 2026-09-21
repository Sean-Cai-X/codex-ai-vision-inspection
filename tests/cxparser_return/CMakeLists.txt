cmake_minimum_required(VERSION 3.21)

project(cxvision_cxparser_return_regression LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CXVISION_CXPARSER_TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muParser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muParserBase.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muParserBytecode.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muParserCallback.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muParserClass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muParserClassFunctionReader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muparserClassReader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muParserError.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muparsertokenreader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muParserInt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muParserRun.cpp
)

add_library(cxvision_cxparser_test_core STATIC
  ${CXVISION_CXPARSER_TEST_SOURCES}
)

target_include_directories(cxvision_cxparser_test_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser
)

target_compile_definitions(cxvision_cxparser_test_core PRIVATE
  _CRT_SECURE_NO_WARNINGS
)

add_executable(cxvision_cxparser_return_regression
  return_keyword_regression.cpp
)

target_link_libraries(cxvision_cxparser_return_regression PRIVATE
  cxvision_cxparser_test_core
)

add_library(cxvision_cxparser_native_test_support STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser/muparsertest.cpp
)

target_link_libraries(cxvision_cxparser_native_test_support PUBLIC
  cxvision_cxparser_test_core
)

target_include_directories(cxvision_cxparser_native_test_support PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cxparser
)

add_executable(cxvision_cxparser_basic_regression
  native_basic_regression_main.cpp
)

add_executable(cxvision_cxparser_control_flow_regression
  native_control_flow_regression_main.cpp
)

add_executable(cxvision_cxparser_full_regression
  native_full_regression_main.cpp
)

target_link_libraries(cxvision_cxparser_basic_regression PRIVATE
  cxvision_cxparser_native_test_support
)

target_link_libraries(cxvision_cxparser_control_flow_regression PRIVATE
  cxvision_cxparser_native_test_support
)

target_link_libraries(cxvision_cxparser_full_regression PRIVATE
  cxvision_cxparser_native_test_support
)