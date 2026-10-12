cmake_minimum_required(VERSION 3.16)
project(cx_so2_numerical_tests LANGUAGES CXX)
enable_testing()
option(SO2_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
add_library(cx_so2 STATIC ../../cxgeom/src/CxGeoSO2Harmonic.cpp ../../cxgeom/src/CxGeoSO2ReferenceAsset.cpp)
target_sources(cx_so2 PRIVATE ../../cxgeom/src/CxGeoSO2StagedSearch.cpp)
target_include_directories(cx_so2 PUBLIC ../../cxgeom/include)
target_compile_features(cx_so2 PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cx_so2 PRIVATE -Wall -Wextra -Werror -pedantic)
  if(SO2_SANITIZERS)
    target_compile_options(cx_so2 PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(cx_so2 PUBLIC -fsanitize=address,undefined)
  endif()
endif()
add_executable(so2_native_probe native_probe.cpp)
target_link_libraries(so2_native_probe PRIVATE cx_so2)
add_executable(so2_native_guards native_guard_test.cpp)
target_link_libraries(so2_native_guards PRIVATE cx_so2)
add_test(NAME native_guards COMMAND so2_native_guards)
option(SO2_PYTHON_ORACLE "Enable optional Python numerical comparisons" OFF)
if(SO2_PYTHON_ORACLE)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME python_oracle COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR} -p test_so2_prototype.py -v)
add_test(NAME native_parity COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_native_parity.py --binary $<TARGET_FILE:so2_native_probe>)
endif()

add_executable(make_fastmatch_fixture make_fastmatch_fixture.cpp)

add_executable(so2_native_asset_guards native_asset_test.cpp)
target_link_libraries(so2_native_asset_guards PRIVATE cx_so2)
add_test(NAME native_asset_guards COMMAND so2_native_asset_guards)

add_executable(make_raster_fixtures make_raster_fixtures.cpp)

add_executable(make_complex_raster_fixtures make_complex_raster_fixtures.cpp)

target_compile_features(make_raster_fixtures PRIVATE cxx_std_17)
target_compile_features(make_complex_raster_fixtures PRIVATE cxx_std_17)

add_executable(make_topology_fixtures make_topology_fixtures.cpp)
target_compile_features(make_topology_fixtures PRIVATE cxx_std_17)

add_executable(so2_open_boundary_guards native_open_boundary_test.cpp)
target_link_libraries(so2_open_boundary_guards PRIVATE cx_so2)
add_test(NAME open_boundary_guards COMMAND so2_open_boundary_guards)

add_executable(harmonic_ui_parameter_guards native_ui_parameter_test.cpp)
target_compile_features(harmonic_ui_parameter_guards PRIVATE cxx_std_17)
add_test(NAME harmonic_ui_parameter_guards COMMAND harmonic_ui_parameter_guards)

add_executable(harmonic_feature_receipt_check native_feature_receipt_test.cpp)
target_compile_features(harmonic_feature_receipt_check PRIVATE cxx_std_17)
add_executable(so2_pose_guards native_pose_test.cpp)
target_link_libraries(so2_pose_guards PRIVATE cx_so2)
add_test(NAME pose_guards COMMAND so2_pose_guards)
add_executable(harmonic_pose_receipt_check native_pose_receipt_test.cpp)
target_compile_features(harmonic_pose_receipt_check PRIVATE cxx_std_17)
set(SO2_HEADLESS_APP "" CACHE FILEPATH "Optional real cximage headless executable")
set(SO2_HEADLESS_IMAGE "" CACHE FILEPATH "Local carrier image; geometry is generated, not image-derived")
if(SO2_HEADLESS_APP AND SO2_HEADLESS_IMAGE)
  add_test(NAME pose_script_generate COMMAND harmonic_pose_receipt_check
    generate ${CMAKE_CURRENT_BINARY_DIR}/pose_receipt_case.cxsc)
  set_tests_properties(pose_script_generate PROPERTIES FIXTURES_SETUP pose_script)
  add_test(NAME pose_headless COMMAND ${SO2_HEADLESS_APP} --cxscript-headless
    --image ${SO2_HEADLESS_IMAGE} --script ${CMAKE_CURRENT_BINARY_DIR}/pose_receipt_case.cxsc
    --out ${CMAKE_CURRENT_BINARY_DIR}/pose_receipt_case
    --unified-log ${CMAKE_CURRENT_BINARY_DIR}/pose_receipt_case.jsonl)
  set_tests_properties(pose_headless PROPERTIES FIXTURES_REQUIRED pose_script
    FIXTURES_SETUP pose_receipt PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60)
  add_test(NAME pose_receipt_check COMMAND harmonic_pose_receipt_check check
    ${CMAKE_CURRENT_BINARY_DIR}/pose_receipt_case/harmonic_audit_receipt.json)
  set_tests_properties(pose_receipt_check PROPERTIES FIXTURES_REQUIRED pose_receipt)
  set_tests_properties(pose_headless PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()
add_executable(harmonic_pose_presentation_guards native_pose_presentation_test.cpp)
target_compile_features(harmonic_pose_presentation_guards PRIVATE cxx_std_17)
add_test(NAME pose_presentation_guards COMMAND harmonic_pose_presentation_guards)
if(SO2_HEADLESS_APP AND SO2_HEADLESS_IMAGE)
  add_test(NAME pose_presentation_receipt COMMAND harmonic_pose_presentation_guards
    ${CMAKE_CURRENT_BINARY_DIR}/pose_receipt_case/harmonic_audit_receipt.json)
  set_tests_properties(pose_presentation_receipt PROPERTIES FIXTURES_REQUIRED pose_receipt)
endif()
add_executable(so2_staged_guards native_staged_test.cpp)
target_link_libraries(so2_staged_guards PRIVATE cx_so2)
add_test(NAME staged_guards COMMAND so2_staged_guards)
add_executable(harmonic_staged_contract_guards native_staged_contract_test.cpp)
target_link_libraries(harmonic_staged_contract_guards PRIVATE cx_so2)
add_test(NAME staged_contract_guards COMMAND harmonic_staged_contract_guards)
add_executable(harmonic_staged_receipt_check native_staged_receipt_test.cpp)
target_compile_features(harmonic_staged_receipt_check PRIVATE cxx_std_17)
if(SO2_HEADLESS_APP AND SO2_HEADLESS_IMAGE)
  add_test(NAME staged_script_generate COMMAND harmonic_staged_receipt_check
    generate ${CMAKE_CURRENT_BINARY_DIR}/staged_receipt_case.cxsc)
  set_tests_properties(staged_script_generate PROPERTIES FIXTURES_SETUP staged_script)
  add_test(NAME staged_headless COMMAND ${SO2_HEADLESS_APP} --cxscript-headless
    --image ${SO2_HEADLESS_IMAGE} --script ${CMAKE_CURRENT_BINARY_DIR}/staged_receipt_case.cxsc
    --out ${CMAKE_CURRENT_BINARY_DIR}/staged_receipt_case
    --unified-log ${CMAKE_CURRENT_BINARY_DIR}/staged_receipt_case.jsonl)
  set_tests_properties(staged_headless PROPERTIES FIXTURES_REQUIRED staged_script
    FIXTURES_SETUP staged_receipt PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  add_test(NAME staged_receipt_check COMMAND harmonic_staged_receipt_check check
    ${CMAKE_CURRENT_BINARY_DIR}/staged_receipt_case/harmonic_audit_receipt.json)
  set_tests_properties(staged_receipt_check PROPERTIES FIXTURES_REQUIRED staged_receipt)
endif()
add_executable(harmonic_staged_ui_guards native_staged_ui_test.cpp)
target_link_libraries(harmonic_staged_ui_guards PRIVATE cx_so2)
add_test(NAME staged_ui_guards COMMAND harmonic_staged_ui_guards)
if(SO2_HEADLESS_APP AND SO2_HEADLESS_IMAGE)
  add_test(NAME staged_ui_receipt COMMAND harmonic_staged_ui_guards
    ${CMAKE_CURRENT_BINARY_DIR}/staged_receipt_case/harmonic_audit_receipt.json)
  set_tests_properties(staged_ui_receipt PROPERTIES FIXTURES_REQUIRED staged_receipt)
endif()
add_executable(ui_font_guards native_ui_font_test.cpp
  ../../imgui/imgui.cpp ../../imgui/imgui_draw.cpp
  ../../imgui/imgui_tables.cpp ../../imgui/imgui_widgets.cpp)
target_compile_features(ui_font_guards PRIVATE cxx_std_17)
add_test(NAME ui_font_missing_fallback COMMAND ui_font_guards)
set(CXVISION_TEST_CJK_FONT "" CACHE FILEPATH "Local-only CJK font for native glyph test")
if(CXVISION_TEST_CJK_FONT)
  add_test(NAME ui_font_cjk_glyphs COMMAND ui_font_guards ${CXVISION_TEST_CJK_FONT})
endif()
add_executable(harmonic_independent_matrix native_independent_matrix_test.cpp)
target_link_libraries(harmonic_independent_matrix PRIVATE cx_so2)
add_test(NAME independent_contour_matrix COMMAND harmonic_independent_matrix
  ${CMAKE_CURRENT_BINARY_DIR}/independent_contour_matrix.csv)
add_executable(harmonic_circle_completion native_circle_completion_test.cpp)
target_link_libraries(harmonic_circle_completion PRIVATE cx_so2)
add_test(NAME circle_completion COMMAND harmonic_circle_completion
  ${CMAKE_CURRENT_BINARY_DIR}/circle_completion.csv)
add_subdirectory(../../cxgeom/polar_discrete_harmonic polar_encoding)
add_executable(harmonic_polar_encoding native_polar_encoding_test.cpp)
target_link_libraries(harmonic_polar_encoding PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME polar_encoding COMMAND harmonic_polar_encoding)
add_executable(harmonic_polar_matching native_polar_matching_test.cpp)
target_link_libraries(harmonic_polar_matching PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME polar_matching COMMAND harmonic_polar_matching)
add_executable(harmonic_polar_receipt native_polar_receipt_test.cpp)
target_link_libraries(harmonic_polar_receipt PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME polar_noise_receipt COMMAND harmonic_polar_receipt
  ${CMAKE_CURRENT_BINARY_DIR}/polar_match_receipt.json)
add_executable(harmonic_polar_boundary native_polar_boundary_test.cpp)
target_link_libraries(harmonic_polar_boundary PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME polar_boundary COMMAND harmonic_polar_boundary)
add_executable(harmonic_polar_partial_contract native_polar_partial_contract_test.cpp)
target_link_libraries(harmonic_polar_partial_contract PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME polar_partial_contract COMMAND harmonic_polar_partial_contract
  ${CMAKE_CURRENT_BINARY_DIR}/polar_partial_request_receipt.json)
add_executable(harmonic_polar_partial_assessment native_polar_partial_assessment_test.cpp)
target_link_libraries(harmonic_polar_partial_assessment PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME polar_partial_assessment COMMAND harmonic_polar_partial_assessment)
add_executable(harmonic_polar_partial_search native_polar_partial_search_test.cpp)
target_link_libraries(harmonic_polar_partial_search PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME polar_partial_search COMMAND harmonic_polar_partial_search)
add_executable(harmonic_polar_pipeline_matrix native_polar_pipeline_matrix_test.cpp)
target_link_libraries(harmonic_polar_pipeline_matrix PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME polar_pipeline_matrix COMMAND harmonic_polar_pipeline_matrix
  ${CMAKE_CURRENT_BINARY_DIR}/polar_pipeline_matrix.jsonl)
add_library(partial_script_parser STATIC
 ../../cxparser/muParser.cpp ../../cxparser/muParserBase.cpp
 ../../cxparser/muParserBytecode.cpp ../../cxparser/muParserCallback.cpp
 ../../cxparser/muParserClass.cpp ../../cxparser/muParserClassFunctionReader.cpp
 ../../cxparser/muparserClassReader.cpp ../../cxparser/muParserError.cpp
 ../../cxparser/muparsertokenreader.cpp ../../cxparser/muParserInt.cpp
 ../../cxparser/muParserRun.cpp)
target_include_directories(partial_script_parser PUBLIC ../../cxparser)
target_compile_features(partial_script_parser PUBLIC cxx_std_17)
add_executable(harmonic_partial_script native_partial_script_test.cpp)
target_link_libraries(harmonic_partial_script PRIVATE partial_script_parser cxgeom_polar_discrete_harmonic)
add_test(NAME polar_partial_cxscript COMMAND harmonic_partial_script
 ${CMAKE_CURRENT_BINARY_DIR}/partial_script)
set(PARTIAL_HEADLESS_APP "" CACHE FILEPATH "Isolated full application with PartialMatchAudit")
if(PARTIAL_HEADLESS_APP AND SO2_HEADLESS_IMAGE)
 set_tests_properties(polar_partial_cxscript PROPERTIES FIXTURES_SETUP partial_script_generated)
 foreach(mode success budget)
  if(mode STREQUAL "success")
   set(partial_script_name partial_case.cxsc)
  else()
   set(partial_script_name partial_budget_case.cxsc)
  endif()
  add_test(NAME partial_app_${mode} COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
   --image ${SO2_HEADLESS_IMAGE}
   --script ${CMAKE_CURRENT_BINARY_DIR}/partial_script/${partial_script_name}
   --out ${CMAKE_CURRENT_BINARY_DIR}/partial_headless/${mode}
   --unified-log ${CMAKE_CURRENT_BINARY_DIR}/partial_headless/${mode}.jsonl)
  set_tests_properties(partial_app_${mode} PROPERTIES
   FIXTURES_REQUIRED partial_script_generated FIXTURES_SETUP partial_app_${mode}_done
   PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
 endforeach()
 add_test(NAME partial_app_receipts COMMAND harmonic_partial_script check-headless
  ${CMAKE_CURRENT_BINARY_DIR}/partial_headless)
 set_tests_properties(partial_app_receipts PROPERTIES
  FIXTURES_REQUIRED "partial_app_success_done;partial_app_budget_done")
endif()
add_executable(make_partial_evidence make_partial_evidence.cpp)
target_link_libraries(make_partial_evidence PRIVATE partial_script_parser cxgeom_polar_discrete_harmonic)
add_test(NAME partial_evidence_generate COMMAND make_partial_evidence
 ${CMAKE_CURRENT_BINARY_DIR}/partial_evidence)
if(PARTIAL_HEADLESS_APP)
 set_tests_properties(partial_evidence_generate PROPERTIES FIXTURES_SETUP partial_evidence_ready)
 add_test(NAME partial_evidence_catalog COMMAND ${PARTIAL_HEADLESS_APP} --partial-evidence-catalog-smoke)
 set_tests_properties(partial_evidence_catalog PROPERTIES FIXTURES_REQUIRED partial_evidence_ready
  ENVIRONMENT "CXVISION_RUN_ROOT=${CMAKE_CURRENT_BINARY_DIR}/partial_evidence"
  PASS_REGULAR_EXPRESSION "PARTIAL_EVIDENCE_CATALOG_UI_PASS" TIMEOUT 60
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
 foreach(mode success budget)
  if(mode STREQUAL "success")
   set(case_id partial_match_missing_extra)
  else()
   set(case_id partial_match_budget_stop)
  endif()
  add_test(NAME partial_evidence_${mode} COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
   --image ${CMAKE_CURRENT_BINARY_DIR}/partial_evidence/${case_id}/source.ppm
   --script ${CMAKE_CURRENT_BINARY_DIR}/partial_evidence/${case_id}/run.cxsc
   --out ${CMAKE_CURRENT_BINARY_DIR}/partial_evidence_headless/${mode}
   --unified-log ${CMAKE_CURRENT_BINARY_DIR}/partial_evidence_headless/${mode}.jsonl)
  set_tests_properties(partial_evidence_${mode} PROPERTIES FIXTURES_REQUIRED partial_evidence_ready
   FIXTURES_SETUP partial_evidence_${mode}_done PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true"
   TIMEOUT 60 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
 endforeach()
 add_test(NAME partial_evidence_receipts COMMAND harmonic_partial_script check-headless
  ${CMAKE_CURRENT_BINARY_DIR}/partial_evidence_headless)
 set_tests_properties(partial_evidence_receipts PROPERTIES
  FIXTURES_REQUIRED "partial_evidence_success_done;partial_evidence_budget_done")
endif()
