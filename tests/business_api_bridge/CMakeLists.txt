cmake_minimum_required(VERSION 3.20)

project(vision_ai_business_case_runner LANGUAGES CXX)

set(CXVISION_REPO_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    CACHE PATH "Path to the cxvision source tree that owns json.hpp")

find_package(VisionAIBusiness CONFIG REQUIRED)

if(NOT EXISTS "${CXVISION_REPO_ROOT}/libtorchsegmentation/src/utils/json.hpp")
  message(FATAL_ERROR "cxvision nlohmann json.hpp is missing")
endif()

add_executable(vision_ai_business_case_runner BusinessApiCaseRunner.cpp)

target_include_directories(vision_ai_business_case_runner PRIVATE
  "${CXVISION_REPO_ROOT}")

target_link_libraries(vision_ai_business_case_runner PRIVATE
  VisionAI::BusinessCore)

target_compile_features(vision_ai_business_case_runner PRIVATE cxx_std_17)

if(MSVC)
  target_compile_options(vision_ai_business_case_runner PRIVATE /W4 /EHsc /utf-8)
else()
  target_compile_options(vision_ai_business_case_runner PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS vision_ai_business_case_runner
  RUNTIME DESTINATION bin
)
