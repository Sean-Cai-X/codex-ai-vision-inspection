add_library(vision_ai_offline_business_contract STATIC
  OfflineBusinessService.cpp)

target_include_directories(vision_ai_offline_business_contract PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/..")
target_compile_features(vision_ai_offline_business_contract PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(vision_ai_offline_business_contract PRIVATE /W4 /EHsc)
else()
  target_compile_options(vision_ai_offline_business_contract PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(VisionAI::OfflineBusinessContract ALIAS vision_ai_offline_business_contract)

add_library(vision_ai_offline_feedback_validation STATIC OfflineFeedbackValidation.cpp)
target_include_directories(vision_ai_offline_feedback_validation PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_compile_features(vision_ai_offline_feedback_validation PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(vision_ai_offline_feedback_validation PRIVATE /W4 /utf-8 /EHsc)
endif()
add_library(VisionAI::OfflineFeedbackValidation ALIAS vision_ai_offline_feedback_validation)
