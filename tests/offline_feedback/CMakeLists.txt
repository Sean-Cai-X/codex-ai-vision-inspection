cmake_minimum_required(VERSION 3.20)
project(vision_ai_offline_feedback_tests LANGUAGES CXX)
enable_testing()
add_subdirectory(../../contracts contracts)
add_executable(offline_feedback_test native_test.cpp
  ../../cxgeom/src/CxGeoSO2ReferenceAsset.cpp ../../cxgeom/src/CxGeoSO2Harmonic.cpp)
target_link_libraries(offline_feedback_test PRIVATE VisionAI::OfflineFeedbackValidation)
target_include_directories(offline_feedback_test PRIVATE ../../cxgeom/include)
add_test(NAME OfflineFeedback_Strict_Privacy_And_Link_Guards COMMAND offline_feedback_test
  "${CMAKE_CURRENT_SOURCE_DIR}/../../contracts/business_offline_feedback_contract.v1.json"
  "${CMAKE_CURRENT_BINARY_DIR}/synthetic-protocol-fixtures")
set_tests_properties(OfflineFeedback_Strict_Privacy_And_Link_Guards PROPERTIES FIXTURES_SETUP feedback_fixture)
add_executable(offline_feedback_validate validate_main.cpp
  ../../cxgeom/src/CxGeoSO2ReferenceAsset.cpp ../../cxgeom/src/CxGeoSO2Harmonic.cpp)
target_link_libraries(offline_feedback_validate PRIVATE VisionAI::OfflineFeedbackValidation)
target_include_directories(offline_feedback_validate PRIVATE ../../cxgeom/include)
add_test(NAME OfflineFeedback_CLI_Original_Byte_Digest_Links COMMAND offline_feedback_validate
  "${CMAKE_CURRENT_SOURCE_DIR}/../../contracts/business_offline_feedback_contract.v1.json"
  "${CMAKE_CURRENT_BINARY_DIR}/synthetic-protocol-fixtures/issue.json"
  "${CMAKE_CURRENT_BINARY_DIR}/synthetic-protocol-fixtures/disposition.json"
  "${CMAKE_CURRENT_BINARY_DIR}/synthetic-protocol-fixtures/repair.json")
add_test(NAME OfflineFeedback_CLI_Rejects_Wrong_Digest COMMAND offline_feedback_validate
  "${CMAKE_CURRENT_SOURCE_DIR}/../../contracts/business_offline_feedback_contract.v1.json"
  "${CMAKE_CURRENT_BINARY_DIR}/synthetic-protocol-fixtures/issue.json"
  "${CMAKE_CURRENT_BINARY_DIR}/synthetic-protocol-fixtures/wrong-disposition.json")
set_tests_properties(OfflineFeedback_CLI_Original_Byte_Digest_Links OfflineFeedback_CLI_Rejects_Wrong_Digest
  PROPERTIES FIXTURES_REQUIRED feedback_fixture)
set_tests_properties(OfflineFeedback_CLI_Rejects_Wrong_Digest PROPERTIES WILL_FAIL TRUE)
set(RUNTIME_IMPORT_LIBRARY "" CACHE FILEPATH "Optional actual native runtime import library for manual R&D inference verification")
if(EXISTS "${RUNTIME_IMPORT_LIBRARY}")
  add_executable(offline_feedback_runtime_probe runtime_probe.cpp
    ../../cximage/CxSevenClassSupervision.cpp
    ../../cxgeom/src/CxGeoSO2ReferenceAsset.cpp ../../cxgeom/src/CxGeoSO2Harmonic.cpp)
  target_link_libraries(offline_feedback_runtime_probe PRIVATE VisionAI::OfflineFeedbackValidation "${RUNTIME_IMPORT_LIBRARY}")
  target_include_directories(offline_feedback_runtime_probe PRIVATE ../../cxgeom/include)
  if(MSVC)
    target_compile_options(offline_feedback_runtime_probe PRIVATE /W4 /utf-8 /EHsc)
  endif()
endif()
if(MSVC)
  target_compile_options(offline_feedback_test PRIVATE /W4 /utf-8 /EHsc)
  target_compile_options(offline_feedback_validate PRIVATE /W4 /utf-8 /EHsc)
endif()
