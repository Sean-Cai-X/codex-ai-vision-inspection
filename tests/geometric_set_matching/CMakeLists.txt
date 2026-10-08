cmake_minimum_required(VERSION 3.16)
project(cxgeom_gsm0 LANGUAGES CXX)
enable_testing()
option(GSM_SANITIZERS "Enable native address/undefined sanitizers" OFF)
add_subdirectory(../../cxgeom/geometric_set_matching gsm-core)
add_executable(geometric_set_contract_test contract_test.cpp)
target_link_libraries(geometric_set_contract_test PRIVATE cxgeom_geometric_set_matching)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cxgeom_geometric_set_matching PRIVATE -Wall -Wextra -Werror -pedantic)
  target_compile_options(geometric_set_contract_test PRIVATE -Wall -Wextra -Werror -pedantic)
  if(GSM_SANITIZERS)
    target_compile_options(cxgeom_geometric_set_matching PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(cxgeom_geometric_set_matching PUBLIC -fsanitize=address,undefined)
  endif()
endif()
add_test(NAME GSM0_contract_topology COMMAND geometric_set_contract_test
  "${CMAKE_CURRENT_SOURCE_DIR}/../../contracts/geometric_set_match.schema.json")

add_executable(geometric_set_matching_test matching_test.cpp)
target_link_libraries(geometric_set_matching_test PRIVATE cxgeom_geometric_set_matching)
add_test(NAME GSM1_full_set COMMAND geometric_set_matching_test)


add_executable(geometric_set_adapter_test adapter_test.cpp
  ../../cximage/FindSetMatch.cpp
  ../../cxgeom/src/CxGeoSO2Harmonic.cpp
  ../../cxgeom/src/CxGeoSO2ReferenceAsset.cpp)
target_include_directories(geometric_set_adapter_test PRIVATE ../../cxgeom/include)
target_link_libraries(geometric_set_adapter_test PRIVATE cxgeom_geometric_set_matching)
add_test(NAME GSM1_adapter COMMAND "${CMAKE_COMMAND}"
  "-DPROBE=$<TARGET_FILE:geometric_set_adapter_test>"
  "-DSCHEMA=${CMAKE_CURRENT_SOURCE_DIR}/../../contracts/geometric_set_match.schema.json"
  "-DOUTPUT_BASE=${CMAKE_CURRENT_BINARY_DIR}/adapter-runs"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/run_adapter_test.cmake")
