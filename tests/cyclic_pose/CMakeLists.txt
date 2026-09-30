cmake_minimum_required(VERSION 3.21)
project(cxvision_cyclic_pose_contract LANGUAGES CXX)
enable_testing()
# The portable GN runtime ships LibTorch binaries but not nvcc. This opt-in
# mirrors its existing dynamic link contract; no CUDA kernels are compiled here.
set(CXVISION_PREBUILT_TORCH_ROOT "" CACHE PATH "Existing portable LibTorch root")
set(CXVISION_TORCH_CXX11_ABI "" CACHE STRING "Explicit ABI for portable Linux package")
if(CXVISION_PREBUILT_TORCH_ROOT)
    if(NOT CXVISION_TORCH_CXX11_ABI MATCHES "^[01]$")
        message(FATAL_ERROR "Portable package requires explicit CXVISION_TORCH_CXX11_ABI=0|1")
    endif()
    set(TORCH_INCLUDE_DIRS
        "${CXVISION_PREBUILT_TORCH_ROOT}/include"
        "${CXVISION_PREBUILT_TORCH_ROOT}/include/torch/csrc/api/include")
    foreach(component torch torch_cpu c10)
        find_library(CYCLIC_${component} NAMES ${component}
            PATHS "${CXVISION_PREBUILT_TORCH_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
        list(APPEND TORCH_LIBRARIES "${CYCLIC_${component}}")
    endforeach()
    set(TORCH_CXX_FLAGS "-D_GLIBCXX_USE_CXX11_ABI=${CXVISION_TORCH_CXX11_ABI}")
else()
    find_package(Torch REQUIRED)
endif()
add_executable(cyclic_pose_contract
    contract_test.cpp
    ../../libtorch_module/torch_cyclic_group_ops.cpp)
target_compile_features(cyclic_pose_contract PRIVATE cxx_std_17)
target_include_directories(cyclic_pose_contract PRIVATE ../../libtorch_module ${TORCH_INCLUDE_DIRS})
target_link_libraries(cyclic_pose_contract PRIVATE "${TORCH_LIBRARIES}")
separate_arguments(TORCH_REQUIRED_FLAGS NATIVE_COMMAND "${TORCH_CXX_FLAGS}")
target_compile_options(cyclic_pose_contract PRIVATE ${TORCH_REQUIRED_FLAGS})
add_test(NAME cyclic_pose_contract COMMAND cyclic_pose_contract)
add_executable(cyclic_interpolation_contract
    interpolation_test.cpp
    ../../libtorch_module/torch_cyclic_group_ops.cpp
    ../../libtorch_module/torch_cyclic_analytic_lift.cpp)
target_compile_features(cyclic_interpolation_contract PRIVATE cxx_std_17)
target_include_directories(cyclic_interpolation_contract PRIVATE ../../libtorch_module ${TORCH_INCLUDE_DIRS})
target_link_libraries(cyclic_interpolation_contract PRIVATE "${TORCH_LIBRARIES}")
target_compile_options(cyclic_interpolation_contract PRIVATE ${TORCH_REQUIRED_FLAGS})
add_test(NAME cyclic_interpolation_contract COMMAND cyclic_interpolation_contract)
set_tests_properties(cyclic_pose_contract cyclic_interpolation_contract PROPERTIES TIMEOUT 60)
