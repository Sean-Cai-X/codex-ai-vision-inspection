cmake_minimum_required(VERSION 3.10)

add_library(cx_torch_mainline INTERFACE)

target_include_directories(cx_torch_mainline INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cx_torch_mainline INTERFACE
    segmentation
    cx_libtorch_module_baseline
)

target_compile_features(cx_torch_mainline INTERFACE cxx_std_14)
