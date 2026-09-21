#ifndef TORCH_UTIL_H
#define TORCH_UTIL_H


#include <torch/torch.h>
#include <torch/script.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <chrono>

#ifdef USE_CUDA
#include <c10/cuda/CUDACachingAllocator.h>
#endif

namespace torch_utils {

    inline torch::Device get_module_device(const torch::nn::Module& module) {
        auto params = module.parameters();
        if (params.size() > 0) {
            return params[0].device();
        }
        auto buffers = module.buffers();
        if (buffers.size() > 0) {
            return buffers[0].device();
        }
        return torch::kCPU;
    }

}

#endif