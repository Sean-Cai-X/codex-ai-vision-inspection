cmake_minimum_required(VERSION 3.16)
project(cx_so2_numerical_tests LANGUAGES CXX)
enable_testing()
add_executable(landmark_identity_uncertainty_guards native_landmark_identity_uncertainty_test.cpp)
target_compile_features(landmark_identity_uncertainty_guards PRIVATE cxx_std_17)
add_test(NAME landmark_identity_uncertainty COMMAND landmark_identity_uncertainty_guards)
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
if(PARTIAL_MASK_OPENCV_TEST)
 find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
 add_executable(gray_full_app native_gray_full_app.cpp)
 target_include_directories(gray_full_app PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(gray_full_app PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
endif()
add_executable(boundary_chunk_guards native_boundary_chunk_test.cpp)
target_link_libraries(boundary_chunk_guards PRIVATE cx_so2)
add_test(NAME boundary_chunk_plan COMMAND boundary_chunk_guards)
add_executable(common_templates_guards native_common_templates_test.cpp)
target_link_libraries(common_templates_guards PRIVATE cx_so2)
add_test(NAME common_templates COMMAND common_templates_guards ${CMAKE_CURRENT_BINARY_DIR}/common_templates)
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
add_executable(independent_image_score native_independent_image_score.cpp)
target_compile_features(independent_image_score PRIVATE cxx_std_17)

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

add_executable(observed_boundary_input_guards native_observed_boundary_test.cpp)
target_link_libraries(observed_boundary_input_guards PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME observed_boundary_input COMMAND observed_boundary_input_guards)

add_executable(boundary_bundle_guards native_boundary_bundle_test.cpp)
target_link_libraries(boundary_bundle_guards PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME boundary_bundle COMMAND boundary_bundle_guards)

add_executable(boundary_order_guards native_boundary_order_test.cpp)
target_link_libraries(boundary_order_guards PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME boundary_order COMMAND boundary_order_guards)

option(PARTIAL_MASK_OPENCV_TEST "Enable decoded binary-mask bridge tests" OFF)
if(PARTIAL_MASK_OPENCV_TEST)
 find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
 add_executable(mask_boundary_guards native_mask_boundary_test.cpp)
 target_include_directories(mask_boundary_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(mask_boundary_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME mask_boundary COMMAND mask_boundary_guards ${CMAKE_CURRENT_BINARY_DIR}/mask_boundary)
 add_executable(mask_matrix_guards native_mask_matrix_test.cpp)
 target_include_directories(mask_matrix_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(mask_matrix_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME mask_matrix COMMAND mask_matrix_guards ${CMAKE_CURRENT_BINARY_DIR}/mask_matrix_order_gate)
 add_executable(boundary_endpoint_guards native_boundary_endpoint_test.cpp)
 target_include_directories(boundary_endpoint_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(boundary_endpoint_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME boundary_endpoint COMMAND boundary_endpoint_guards)
 add_test(NAME mask_endpoint_matrix COMMAND mask_matrix_guards ${CMAKE_CURRENT_BINARY_DIR}/mask_arc_matrix endpoints)
 add_executable(arc_resolution_guards native_arc_resolution_test.cpp)
 target_include_directories(arc_resolution_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(arc_resolution_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME arc_resolution_matrix COMMAND arc_resolution_guards ${CMAKE_CURRENT_BINARY_DIR}/arc_resolution_matrix)
 add_executable(gray_boundary_guards native_gray_boundary_test.cpp)
 target_include_directories(gray_boundary_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(gray_boundary_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME gray_boundary_refinement COMMAND gray_boundary_guards ${CMAKE_CURRENT_BINARY_DIR}/gray_boundary_refinement)
 add_executable(observed_gray_guards native_observed_gray_test.cpp)
 target_include_directories(observed_gray_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(observed_gray_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME observed_gray_normals COMMAND observed_gray_guards ${CMAKE_CURRENT_BINARY_DIR}/observed_gray_normals)
 add_executable(refined_pair_ui_guards native_refined_pair_ui_test.cpp)
 target_include_directories(refined_pair_ui_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(refined_pair_ui_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME refined_pair_ui_parameters COMMAND refined_pair_ui_guards)
 add_executable(make_refined_arc_pairs make_refined_arc_pairs.cpp)
 target_include_directories(make_refined_arc_pairs PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(make_refined_arc_pairs PRIVATE partial_script_parser cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 set(PAIR_ROOT ${CMAKE_CURRENT_BINARY_DIR}/refined_arc_pairs)
 add_test(NAME independent_asymmetric_gray_pairs COMMAND make_refined_arc_pairs ${PAIR_ROOT})
 set_tests_properties(independent_asymmetric_gray_pairs PROPERTIES FIXTURES_SETUP pair_assets_ready)
 if(PARTIAL_HEADLESS_APP)
  foreach(id curve_combined s_curve_missing landmark_sha_rejected)
   add_test(NAME paired_${id}_headless COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
    --image ${PAIR_ROOT}/curve_identity/reference.pgm --script ${PAIR_ROOT}/${id}/run.cxsc
    --out ${PAIR_ROOT}/headless/${id} --unified-log ${PAIR_ROOT}/headless/${id}.jsonl)
   set_tests_properties(paired_${id}_headless PROPERTIES FIXTURES_REQUIRED pair_assets_ready
    FIXTURES_SETUP pair_${id}_done ENVIRONMENT "CXVISION_RUN_ROOT=${PAIR_ROOT}" PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  endforeach()
  add_test(NAME paired_evidence_catalog COMMAND ${PARTIAL_HEADLESS_APP} --paired-evidence-catalog-smoke)
  set_tests_properties(paired_evidence_catalog PROPERTIES FIXTURES_REQUIRED pair_assets_ready
   ENVIRONMENT "CXVISION_RUN_ROOT=${PAIR_ROOT}" PASS_REGULAR_EXPRESSION "PAIRED_EVIDENCE_CATALOG_PASS"
   TIMEOUT 60 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  add_test(NAME paired_headless_parity COMMAND make_refined_arc_pairs ${PAIR_ROOT} verify-headless)
  set_tests_properties(paired_headless_parity PROPERTIES FIXTURES_REQUIRED "pair_curve_combined_done;pair_s_curve_missing_done;pair_landmark_sha_rejected_done")
 endif()
 add_executable(refined_arc_script_guards native_refined_arc_script_test.cpp)
 target_include_directories(refined_arc_script_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(refined_arc_script_guards PRIVATE partial_script_parser cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 set(REFINED_ARC_ROOT ${CMAKE_CURRENT_BINARY_DIR}/refined_arc_script)
 add_test(NAME refined_arc_cxscript COMMAND refined_arc_script_guards ${REFINED_ARC_ROOT})
 set_tests_properties(refined_arc_cxscript PROPERTIES FIXTURES_SETUP refined_arc_ready)
 if(PARTIAL_HEADLESS_APP)
  add_test(NAME refined_arc_headless COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
   --image ${REFINED_ARC_ROOT}/source.pgm --script ${REFINED_ARC_ROOT}/run.cxsc
   --out ${REFINED_ARC_ROOT}/headless --unified-log ${REFINED_ARC_ROOT}/headless.jsonl)
  set_tests_properties(refined_arc_headless PROPERTIES FIXTURES_REQUIRED refined_arc_ready
   FIXTURES_SETUP refined_arc_executed PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  add_test(NAME refined_arc_headless_parity COMMAND refined_arc_script_guards ${REFINED_ARC_ROOT} verify-headless)
  set_tests_properties(refined_arc_headless_parity PROPERTIES FIXTURES_REQUIRED refined_arc_executed)
 endif()
 foreach(mode common common-reject endpoint-projection endpoint-projection-reject)
  set(COMMON_ARC_ROOT ${CMAKE_CURRENT_BINARY_DIR}/refined_arc_${mode})
  add_test(NAME refined_arc_${mode}_cxscript COMMAND refined_arc_script_guards ${COMMON_ARC_ROOT} ${mode})
  set_tests_properties(refined_arc_${mode}_cxscript PROPERTIES FIXTURES_SETUP refined_arc_${mode}_ready)
  if(PARTIAL_HEADLESS_APP)
   add_test(NAME refined_arc_${mode}_headless COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
    --image ${COMMON_ARC_ROOT}/source.pgm --script ${COMMON_ARC_ROOT}/run.cxsc
    --out ${COMMON_ARC_ROOT}/headless --unified-log ${COMMON_ARC_ROOT}/headless.jsonl)
   set_tests_properties(refined_arc_${mode}_headless PROPERTIES FIXTURES_REQUIRED refined_arc_${mode}_ready
    FIXTURES_SETUP refined_arc_${mode}_executed PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
   add_test(NAME refined_arc_${mode}_parity COMMAND refined_arc_script_guards ${COMMON_ARC_ROOT} verify-headless)
   set_tests_properties(refined_arc_${mode}_parity PROPERTIES FIXTURES_REQUIRED refined_arc_${mode}_executed)
  endif()
 endforeach()
 add_executable(refined_pose_diagnostic native_refined_pose_diagnostic_test.cpp)
 target_include_directories(refined_pose_diagnostic PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(refined_pose_diagnostic PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 set(POSE_DIAGNOSTIC_ROOT ${CMAKE_CURRENT_BINARY_DIR}/refined_pose_diagnostic_evidence)
 add_test(NAME refined_pose_error_diagnostic COMMAND refined_pose_diagnostic ${POSE_DIAGNOSTIC_ROOT})
 set_tests_properties(refined_pose_error_diagnostic PROPERTIES FIXTURES_SETUP pose_diagnostic_ready)
 if(PARTIAL_HEADLESS_APP)
  foreach(id r37_s075 r37_s150 rm60_s075 rm60_s150)
   add_test(NAME refined_pose_${id}_headless COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
    --image ${POSE_DIAGNOSTIC_ROOT}/${id}/reference.pgm --script ${POSE_DIAGNOSTIC_ROOT}/${id}/run.cxsc
    --out ${POSE_DIAGNOSTIC_ROOT}/${id}/headless --unified-log ${POSE_DIAGNOSTIC_ROOT}/${id}/headless.jsonl)
   set_tests_properties(refined_pose_${id}_headless PROPERTIES FIXTURES_REQUIRED pose_diagnostic_ready
    FIXTURES_SETUP pose_${id}_executed PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  endforeach()
  add_test(NAME refined_pose_headless_parity COMMAND refined_pose_diagnostic ${POSE_DIAGNOSTIC_ROOT} verify-headless)
  set_tests_properties(refined_pose_headless_parity PROPERTIES
   FIXTURES_REQUIRED "pose_r37_s075_executed;pose_r37_s150_executed;pose_rm60_s075_executed;pose_rm60_s150_executed")
 endif()
 add_executable(boundary_resolution_study native_resolution_study.cpp)
 target_include_directories(boundary_resolution_study PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(boundary_resolution_study PRIVATE partial_script_parser cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_executable(gray_full_boundary native_gray_full_boundary.cpp)
 target_include_directories(gray_full_boundary PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(gray_full_boundary PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_executable(gray_polarity_replay native_gray_polarity_replay.cpp)
 target_include_directories(gray_polarity_replay PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(gray_polarity_replay PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_executable(gray_phase_review native_gray_phase_review.cpp)
 target_include_directories(gray_phase_review PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(gray_phase_review PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_executable(gray_phase_study native_gray_phase_study.cpp)
 target_include_directories(gray_phase_study PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(gray_phase_study PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_executable(pixel_cell_study native_pixel_cell_study.cpp)
 target_include_directories(pixel_cell_study PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(pixel_cell_study PRIVATE cx_so2 ${OpenCV_LIBS})
 add_executable(observed_boundary_score native_observed_boundary_score.cpp)
 target_include_directories(observed_boundary_score PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(observed_boundary_score PRIVATE ${OpenCV_LIBS})
 target_compile_features(observed_boundary_score PRIVATE cxx_std_17)
 add_executable(boundary_stress_guards native_boundary_stress_test.cpp)
 target_include_directories(boundary_stress_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(boundary_stress_guards PRIVATE partial_script_parser cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_executable(identity_raster_guards native_identity_raster_test.cpp)
 target_include_directories(identity_raster_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(identity_raster_guards PRIVATE partial_script_parser cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 set(IDENTITY_RASTER_ROOT ${CMAKE_CURRENT_BINARY_DIR}/identity_raster)
 add_test(NAME identity_raster_guards COMMAND identity_raster_guards ${IDENTITY_RASTER_ROOT})
 set_tests_properties(identity_raster_guards PROPERTIES FIXTURES_SETUP identity_raster_ready)
 if(PARTIAL_HEADLESS_APP)
  foreach(id asymmetric_combined repeated_boundary uncertainty_rejected)
   add_test(NAME identity_raster_app_${id} COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
    --image ${IDENTITY_RASTER_ROOT}/${id}/reference.pgm --script ${IDENTITY_RASTER_ROOT}/${id}/run.cxsc
    --out ${IDENTITY_RASTER_ROOT}/headless/${id} --unified-log ${IDENTITY_RASTER_ROOT}/headless/${id}.jsonl)
   set_tests_properties(identity_raster_app_${id} PROPERTIES FIXTURES_REQUIRED identity_raster_ready
    FIXTURES_SETUP identity_raster_${id}_done ENVIRONMENT "CXVISION_RUN_ROOT=${IDENTITY_RASTER_ROOT}"
    PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  endforeach()
  add_test(NAME identity_raster_headless_parity COMMAND identity_raster_guards ${IDENTITY_RASTER_ROOT} verify-headless)
  set_tests_properties(identity_raster_headless_parity PROPERTIES FIXTURES_REQUIRED "identity_raster_asymmetric_combined_done;identity_raster_repeated_boundary_done;identity_raster_uncertainty_rejected_done")
  add_test(NAME identity_raster_gui_catalog COMMAND ${PARTIAL_HEADLESS_APP} --paired-evidence-catalog-smoke)
  set_tests_properties(identity_raster_gui_catalog PROPERTIES FIXTURES_REQUIRED identity_raster_ready
   ENVIRONMENT "CXVISION_RUN_ROOT=${IDENTITY_RASTER_ROOT}" PASS_REGULAR_EXPRESSION "PAIRED_EVIDENCE_CATALOG_PASS"
   TIMEOUT 60 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
 endif()

 set(BOUNDARY_STRESS_ROOT ${CMAKE_CURRENT_BINARY_DIR}/boundary_stress)
 add_test(NAME boundary_stress_guards COMMAND boundary_stress_guards ${IDENTITY_RASTER_ROOT} ${BOUNDARY_STRESS_ROOT})
 set_tests_properties(boundary_stress_guards PROPERTIES FIXTURES_REQUIRED identity_raster_ready FIXTURES_SETUP boundary_stress_ready)
 if(PARTIAL_HEADLESS_APP)
  foreach(id clean_control blur_3p2 blur_8 low_contrast missing_boundary boundary_gap local_contrast_dropout noise_4 sharpen_0p8 roi_snap_ambiguous)
   add_test(NAME boundary_stress_app_${id} COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
    --image ${BOUNDARY_STRESS_ROOT}/${id}/reference.pgm --script ${BOUNDARY_STRESS_ROOT}/${id}/run.cxsc
    --out ${BOUNDARY_STRESS_ROOT}/headless/${id} --unified-log ${BOUNDARY_STRESS_ROOT}/headless/${id}.jsonl)
   set_tests_properties(boundary_stress_app_${id} PROPERTIES FIXTURES_REQUIRED boundary_stress_ready
    FIXTURES_SETUP boundary_stress_${id}_done ENVIRONMENT "CXVISION_RUN_ROOT=${BOUNDARY_STRESS_ROOT}"
    PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  endforeach()
  add_test(NAME boundary_stress_headless_parity COMMAND boundary_stress_guards ${IDENTITY_RASTER_ROOT} ${BOUNDARY_STRESS_ROOT} verify-headless)
  set_tests_properties(boundary_stress_headless_parity PROPERTIES FIXTURES_REQUIRED "boundary_stress_clean_control_done;boundary_stress_blur_3p2_done;boundary_stress_blur_8_done;boundary_stress_low_contrast_done;boundary_stress_missing_boundary_done;boundary_stress_boundary_gap_done;boundary_stress_local_contrast_dropout_done;boundary_stress_noise_4_done;boundary_stress_sharpen_0p8_done;boundary_stress_roi_snap_ambiguous_done")
  add_test(NAME boundary_stress_gui_catalog COMMAND ${PARTIAL_HEADLESS_APP} --paired-evidence-catalog-smoke)
  set_tests_properties(boundary_stress_gui_catalog PROPERTIES FIXTURES_REQUIRED boundary_stress_ready
   ENVIRONMENT "CXVISION_RUN_ROOT=${BOUNDARY_STRESS_ROOT}" PASS_REGULAR_EXPRESSION "PAIRED_EVIDENCE_CATALOG_PASS"
   TIMEOUT 60 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
 endif()
 add_executable(landmark_arc_guards native_landmark_arc_test.cpp)
 target_include_directories(landmark_arc_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(landmark_arc_guards PRIVATE partial_script_parser cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME landmark_arc_coordinates COMMAND landmark_arc_guards ${PAIR_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/landmark_arc_comparison)
 set_tests_properties(landmark_arc_coordinates PROPERTIES FIXTURES_REQUIRED pair_assets_ready FIXTURES_SETUP landmark_arc_assets_ready)
 if(PARTIAL_HEADLESS_APP)
  set(LANDMARK_ARC_ROOT ${CMAKE_CURRENT_BINARY_DIR}/landmark_arc_comparison)
  foreach(id curve_combined curve_missing s_curve_extra)
   add_test(NAME landmark_arc_app_${id} COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
    --image ${LANDMARK_ARC_ROOT}/${id}/reference.pgm --script ${LANDMARK_ARC_ROOT}/${id}/run.cxsc
    --out ${LANDMARK_ARC_ROOT}/headless/${id} --unified-log ${LANDMARK_ARC_ROOT}/headless/${id}.jsonl)
   set_tests_properties(landmark_arc_app_${id} PROPERTIES FIXTURES_REQUIRED landmark_arc_assets_ready
    FIXTURES_SETUP landmark_arc_${id}_done ENVIRONMENT "CXVISION_RUN_ROOT=${LANDMARK_ARC_ROOT}"
    PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  endforeach()
  add_test(NAME landmark_arc_gui_catalog COMMAND ${PARTIAL_HEADLESS_APP} --paired-evidence-catalog-smoke)
  set_tests_properties(landmark_arc_gui_catalog PROPERTIES FIXTURES_REQUIRED landmark_arc_assets_ready
   ENVIRONMENT "CXVISION_RUN_ROOT=${LANDMARK_ARC_ROOT}" PASS_REGULAR_EXPRESSION "PAIRED_EVIDENCE_CATALOG_PASS"
   TIMEOUT 60 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  add_test(NAME landmark_arc_headless_parity COMMAND landmark_arc_guards ${PAIR_ROOT} ${LANDMARK_ARC_ROOT} verify-headless)
  set_tests_properties(landmark_arc_headless_parity PROPERTIES FIXTURES_REQUIRED "landmark_arc_curve_combined_done;landmark_arc_curve_missing_done;landmark_arc_s_curve_extra_done")
 endif()
 add_executable(refined_arc_guards native_refined_arc_test.cpp)
 target_include_directories(refined_arc_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(refined_arc_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME refined_boundary_arc COMMAND refined_arc_guards ${CMAKE_CURRENT_BINARY_DIR}/refined_boundary_arc)
 add_executable(bound_gray_guards native_bound_gray_test.cpp)
 target_include_directories(bound_gray_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(bound_gray_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME bound_gray_frame COMMAND bound_gray_guards ${CMAKE_CURRENT_BINARY_DIR}/bound_gray_frame)
endif()

add_executable(endpoint_projection_guards native_endpoint_projection_test.cpp)
target_link_libraries(endpoint_projection_guards PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME endpoint_projection_guards COMMAND endpoint_projection_guards)
add_executable(common_arc_guards native_common_arc_test.cpp)
target_link_libraries(common_arc_guards PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME common_arc_support COMMAND common_arc_guards)

add_executable(boundary_arc_guards native_boundary_arc_test.cpp)
target_link_libraries(boundary_arc_guards PRIVATE cxgeom_polar_discrete_harmonic)
add_test(NAME boundary_arc COMMAND boundary_arc_guards)
if(PARTIAL_MASK_OPENCV_TEST)
 add_executable(make_gray_boundary_evidence make_gray_boundary_evidence.cpp)
 target_include_directories(make_gray_boundary_evidence PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(make_gray_boundary_evidence PRIVATE partial_script_parser cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 set(GRAY_EVIDENCE_ROOT ${CMAKE_CURRENT_BINARY_DIR}/gray_boundary_evidence)
 add_test(NAME gray_cxscript_evidence COMMAND make_gray_boundary_evidence ${GRAY_EVIDENCE_ROOT})
 add_executable(gray_ui_guards native_gray_ui_test.cpp)
 target_include_directories(gray_ui_guards PRIVATE ${OpenCV_INCLUDE_DIRS})
 target_link_libraries(gray_ui_guards PRIVATE cx_so2 cxgeom_polar_discrete_harmonic ${OpenCV_LIBS})
 add_test(NAME gray_ui_parameters COMMAND gray_ui_guards ${GRAY_EVIDENCE_ROOT}/straight/source.pgm)
 set_tests_properties(gray_ui_parameters PROPERTIES FIXTURES_REQUIRED gray_evidence_ready)
 set_tests_properties(gray_cxscript_evidence PROPERTIES FIXTURES_SETUP gray_evidence_ready)
 if(PARTIAL_HEADLESS_APP)
  foreach(mode straight corner no_refined)
   add_test(NAME gray_app_${mode} COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
    --image ${GRAY_EVIDENCE_ROOT}/${mode}/source.pgm --script ${GRAY_EVIDENCE_ROOT}/${mode}/run.cxsc
    --out ${GRAY_EVIDENCE_ROOT}/headless/${mode} --unified-log ${GRAY_EVIDENCE_ROOT}/headless/${mode}.jsonl)
   set_tests_properties(gray_app_${mode} PROPERTIES FIXTURES_REQUIRED gray_evidence_ready
    FIXTURES_SETUP gray_app_${mode}_done ENVIRONMENT "CXVISION_RUN_ROOT=${GRAY_EVIDENCE_ROOT}"
    PASS_REGULAR_EXPRESSION "cxscript_headless_ok=true" TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  endforeach()
  add_test(NAME gray_app_receipts COMMAND make_gray_boundary_evidence ${GRAY_EVIDENCE_ROOT} verify-headless)
  set_tests_properties(gray_app_receipts PROPERTIES FIXTURES_REQUIRED "gray_app_straight_done;gray_app_corner_done;gray_app_no_refined_done")
  add_test(NAME gray_app_wrong_image COMMAND ${PARTIAL_HEADLESS_APP} --cxscript-headless
   --image ${GRAY_EVIDENCE_ROOT}/corner/source.pgm --script ${GRAY_EVIDENCE_ROOT}/straight/run.cxsc
   --out ${GRAY_EVIDENCE_ROOT}/headless/wrong_image --unified-log ${GRAY_EVIDENCE_ROOT}/headless/wrong_image.jsonl)
  set_tests_properties(gray_app_wrong_image PROPERTIES FIXTURES_REQUIRED gray_evidence_ready
   ENVIRONMENT "CXVISION_RUN_ROOT=${GRAY_EVIDENCE_ROOT}" WILL_FAIL TRUE TIMEOUT 60
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
 endif()
endif()
if(PARTIAL_MASK_OPENCV_TEST AND PARTIAL_HEADLESS_APP)
 add_test(NAME gray_evidence_catalog COMMAND ${PARTIAL_HEADLESS_APP} --gray-evidence-catalog-smoke)
 set_tests_properties(gray_evidence_catalog PROPERTIES FIXTURES_REQUIRED gray_evidence_ready
  ENVIRONMENT "CXVISION_RUN_ROOT=${GRAY_EVIDENCE_ROOT}"
  PASS_REGULAR_EXPRESSION "GRAY_EVIDENCE_CATALOG_PASS" TIMEOUT 60
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()
