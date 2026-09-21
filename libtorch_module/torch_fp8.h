#ifndef TORCH_FP8_H
#define TORCH_FP8_H


#include <torch/torch.h>
#include <torch/nn/module.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <unordered_map>

#include "torch_util.h"

namespace torch_fp8 {

    enum class FP8Type {
        E4M3,
        E5M2
    };

    class FP8Tensor {
    public:
        FP8Tensor(const torch::Tensor& tensor, FP8Type type = FP8Type::E4M3,
            float scale = 1.0f, bool use_amax = true) {

            TORCH_CHECK(tensor.dtype() == torch::kFloat32 || tensor.dtype() == torch::kFloat16,
                "FP8Tensor input must be Float32 or Float16");

            type_ = type;
            device_ = tensor.device();
            scale_ = scale;

            if (use_amax) {
                auto amax = torch::max(torch::abs(tensor)).to(torch::kCPU).item<float>();
                scale_ = calculate_scale(amax, type);
            }

            data_ = convert_to_fp8(tensor, type, scale_);
            shape_ = tensor.sizes().vec();
            stride_ = tensor.strides().vec();
        }

        torch::Tensor to_fp32() const {
            return convert_from_fp8(data_, shape_, stride_, type_, scale_, device_);
        }

        torch::Tensor to_fp16() const {
            return to_fp32().to(torch::kFloat16);
        }

        float scale() const { return scale_; }

    private:
        float calculate_scale(float amax, FP8Type type) const {
            if (amax == 0.0f) return 1.0f;
            float fp8_max = (type == FP8Type::E4M3) ? 448.0f : 57344.0f;
            float scale = amax / fp8_max;
            return std::max(scale, 1e-8f);
        }

        torch::Tensor convert_to_fp8(const torch::Tensor& tensor, FP8Type type, float scale) const {
            auto scaled = tensor / scale;
            torch::Tensor fp8_data;

            if (type == FP8Type::E4M3) {
                fp8_data = torch::clamp(scaled, -448.0, 448.0).to(torch::kUInt8);
            }
            else {
                fp8_data = torch::clamp(scaled, -57344.0, 57344.0).to(torch::kUInt8);
            }
            return fp8_data.contiguous();
        }

        torch::Tensor convert_from_fp8(const torch::Tensor& fp8_data,
            const std::vector<int64_t>& shape,
            const std::vector<int64_t>& stride,
            FP8Type type, float scale,
            const torch::Device& device) const {

            auto fp32_data = fp8_data.to(torch::kFloat32).to(device);
            fp32_data = fp32_data * scale;
            return fp32_data.view(shape).as_strided(shape, stride);
        }

        torch::Tensor data_;
        FP8Type type_;
        float scale_;
        torch::Device device_ = torch::kCPU;
        std::vector<int64_t> shape_;
        std::vector<int64_t> stride_;
    };

    class FP8QuantizeImpl : public torch::nn::Module {
    public:
        FP8QuantizeImpl(FP8Type type = FP8Type::E4M3, bool per_channel = false,
            float scale = 1.0f, bool learnable_scale = false)
            : type_(type), per_channel_(per_channel), learnable_scale_(learnable_scale), scale_clip(nullptr)
        {
            if (learnable_scale) {
                scale_param_ = register_parameter("scale", torch::tensor({ scale }));

                scale_clip = register_module("scale_clip",
                    torch::nn::Hardtanh(torch::nn::HardtanhOptions().min_val(1e-8).max_val(1e4)));
            }
            else {
                scale_param_ = register_buffer("scale", torch::tensor({ scale }));
            }
        }

        torch::Tensor forward(torch::Tensor x) {
            TORCH_CHECK(x.dim() >= 2, "FP8Quantize: Input tensor dim must be >= 2");

            float scale_val = scale_param_.item<float>();

            if (learnable_scale_ && !scale_clip.is_empty()) {
                scale_val = scale_clip(scale_param_).item<float>();
            }

            if (per_channel_ && x.dim() > 1) {
                std::vector<int64_t> reduce_dims;
                for (int64_t i = 0; i < x.dim(); ++i) {
                    if (i != 1) reduce_dims.push_back(i);
                }

                auto amax = torch::amax(torch::abs(x), reduce_dims, true);

                scale_val = calculate_per_channel_scale(amax, type_);
            }

            FP8Tensor fp8_tensor(x, type_, scale_val);
            return fp8_tensor.to_fp32();
        }

    private:
        float calculate_per_channel_scale(const torch::Tensor& amax, FP8Type type) const {
            float fp8_max = (type == FP8Type::E4M3) ? 448.0f : 57344.0f;
            auto amax_cpu = amax.to(torch::kCPU);
            float avg_amax = torch::mean(amax_cpu).item<float>();
            return std::max(avg_amax / fp8_max, 1e-8f);
        }

        FP8Type type_;
        bool per_channel_;
        bool learnable_scale_;
        torch::Tensor scale_param_;
        torch::nn::Hardtanh scale_clip{ nullptr };
    };
    TORCH_MODULE(FP8Quantize);

    inline bool is_fp8_supported(const torch::Device& device) {
        if (device.is_cpu()) return false;
        if (device.is_cuda()) {
#ifdef USE_CUDA
            try {
                auto prop = torch::cuda::get_device_properties(device.index());
                return prop->major >= 8;
            }
            catch (...) { return false; }
#endif
            return false;
        }
        return false;
    }

    class FP8InferenceContext {
    public:
        FP8InferenceContext(torch::nn::Module& model, FP8Type type = FP8Type::E4M3,
            const torch::Device& device = torch::kCUDA)
            : model_(&model), type_(type), device_(device) {
            enable_fp8_inference();
        }

        ~FP8InferenceContext() {
            disable_fp8_inference();
        }

        template <typename... Args>
        torch::Tensor run(Args&&... args) {
            torch::NoGradGuard no_grad;
            std::vector<torch::Tensor> inputs = { std::forward<Args>(args)... };
            auto input = torch::cat(inputs, 0).to(device_);

            FP8Tensor fp8_input(input, type_);
            auto fp32_input = fp8_input.to_fp32();

            auto output = model_->forward(fp32_input).toTensor();

            FP8Tensor fp8_output(output, type_);
            return fp8_output.to_fp32();
        }

    private:
        void enable_fp8_inference() {
            for (auto module : model_->named_modules()) {
                if (auto conv = module.value()->as<torch::nn::Conv2dImpl>()) {
                    fp8_modules_[module.key()] = conv;
                }
                if (auto linear = module.value()->as<torch::nn::LinearImpl>()) {
                    fp8_modules_[module.key()] = linear;
                }
            }
        }

        void disable_fp8_inference() {
            fp8_modules_.clear();
        }

        torch::nn::Module* model_;
        FP8Type type_;
        torch::Device device_;
        std::unordered_map<std::string, torch::nn::Module*> fp8_modules_;
    };

    inline void convert_model_to_fp8(torch::nn::Module& model, FP8Type type = FP8Type::E4M3) {
        torch::NoGradGuard no_grad;

        torch::Device dev = torch_utils::get_module_device(model);

        bool supported = is_fp8_supported(dev);
        if (!supported && dev.is_cuda()) {
            std::cout << "[FP8] Warning: Device " << dev.str() << " may not fully support FP8." << std::endl;
        }

        for (auto& param : model.named_parameters()) {
            FP8Tensor fp8_param(param.value(), type);
            param.value().set_data(fp8_param.to_fp32());
        }
    }

    inline float calculate_fp8_quant_error(const torch::Tensor& fp32_tensor, FP8Type type = FP8Type::E4M3) {
        torch::NoGradGuard no_grad;
        FP8Tensor fp8_tensor(fp32_tensor, type);
        auto recon = fp8_tensor.to_fp32();

        auto mse = torch::mean(torch::pow(fp32_tensor - recon, 2.0)).item<float>();
        return std::sqrt(mse);
    }

}

#endif