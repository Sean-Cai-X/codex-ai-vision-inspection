cmake_minimum_required(VERSION 3.21)
project(vision_ai_offline_business_contract LANGUAGES CXX)
add_executable(vision_ai_offline_business_contract_test
  OfflineBusinessServiceContractTest.cpp
  ../../contracts/OfflineBusinessService.cpp)
target_include_directories(vision_ai_offline_business_contract_test PRIVATE ../..)
target_compile_features(vision_ai_offline_business_contract_test PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(vision_ai_offline_business_contract_test PRIVATE /W4 /EHsc)
else()
  target_compile_options(vision_ai_offline_business_contract_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_test(NAME vision_ai_offline_business_contract_test COMMAND vision_ai_offline_business_contract_test)
