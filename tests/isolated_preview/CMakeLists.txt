cmake_minimum_required(VERSION 3.20)
project(rd_isolated_preview LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(VISION_AI_SOURCE "" CACHE PATH "Read-only Vision-AI checkout containing the real typed adapters")
if(NOT EXISTS "${VISION_AI_SOURCE}/adapters/LocalBusinessAssetBroker.cpp")
  message(FATAL_ERROR "Provide VISION_AI_SOURCE; no fake transport fallback")
endif()
add_executable(rd_isolated_preview
  main.cpp
  ../../cximage/CxSevenClassSupervision.cpp
  ../../cxgeom/src/CxGeoSO2ReferenceAsset.cpp
  ../../cxgeom/src/CxGeoSO2Harmonic.cpp
  "${VISION_AI_SOURCE}/adapters/OfflineBusinessServiceBridge.cpp"
  "${VISION_AI_SOURCE}/adapters/LocalOfflineBusinessRuntimeTransport.cpp"
  "${VISION_AI_SOURCE}/adapters/LocalBusinessAssetBroker.cpp"
  "${VISION_AI_SOURCE}/materializer/GeometryTrainingDatasetMaterializer.cpp"
  "${VISION_AI_SOURCE}/contracts/TorchTrainingProviderApi.cpp"
  "${VISION_AI_SOURCE}/contracts/ProviderReceiptSecurity.cpp")
target_include_directories(rd_isolated_preview PRIVATE "${VISION_AI_SOURCE}" ../../cxgeom/include)
target_compile_definitions(rd_isolated_preview PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(rd_isolated_preview PRIVATE /utf-8 /EHsc)
target_link_libraries(rd_isolated_preview PRIVATE bcrypt psapi)
