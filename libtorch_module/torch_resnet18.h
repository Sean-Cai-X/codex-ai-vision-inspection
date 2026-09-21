#ifndef TORCH_RESNET18_H
#define TORCH_RESNET18_H


#include <torch/torch.h>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <ATen/autocast_mode.h>
#include <c10/core/AutogradState.h>

#include <torch/csrc/autograd/grad_mode.h>
#include <torch/data/transforms/tensor.h>
#include <torch/data/dataloader.h>

#include "torch_nnmodule.h"
#include "torch_mermaid.h"


class BasicBlockImpl : public torch::nn::Module {
public:
    BasicBlockImpl(int64_t in_channels, int64_t out_channels, int64_t stride = 1) {
        cv1 = register_module("cv1",
            ConvModule(in_channels, out_channels, 3, stride, 1, true, true, "relu"));

        cv2 = register_module("cv2",
            ConvModule(out_channels, out_channels, 3, 1, 1, true, false, "none"));

        if (stride != 1 || in_channels != out_channels) {
            downsample = register_module("downsample",
                torch::nn::Sequential(
                    torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels, out_channels, 1).stride(stride).bias(false)),
                    torch::nn::BatchNorm2d(out_channels)
                ));
        }

        act = register_module("act", torch::nn::ReLU());
    }

    torch::Tensor forward(torch::Tensor x) {
        auto identity = x;

        auto out = cv1->forward(x);
        out = cv2->forward(out);

        if (!downsample.is_empty()) {
            identity = downsample->forward(x);
        }

        out += identity;
        return act->forward(out);
    }

private:
    ConvModule cv1{ nullptr }, cv2{ nullptr };
    torch::nn::Sequential downsample{ nullptr };
    torch::nn::ReLU act{ nullptr };
};
TORCH_MODULE(BasicBlock);

class ResNet18Impl : public torch::nn::Module {
public:
    ResNet18Impl(int64_t num_classes = 1000, bool include_top = true)
        : include_top_(include_top), num_classes_(num_classes) {

        stem = register_module("stem",
            ConvModule(3, 64, 7, 2, 3, true, true, "relu"));

        pool = register_module("pool",
            torch::nn::MaxPool2d(torch::nn::MaxPool2dOptions(3).stride(2).padding(1)));

        layer1 = make_layer(64, 64, 2, 1);
        register_module("layer1", layer1);

        layer2 = make_layer(64, 128, 2, 2);
        register_module("layer2", layer2);

        layer3 = make_layer(128, 256, 2, 2);
        register_module("layer3", layer3);

        layer4 = make_layer(256, 512, 2, 2);
        register_module("layer4", layer4);

        if (include_top_) {
            avgpool = register_module("avgpool", torch::nn::AdaptiveAvgPool2d(torch::nn::AdaptiveAvgPool2dOptions({ 1, 1 })));
            fc = register_module("fc", torch::nn::Linear(512, num_classes));
        }
    }


    void load_pretrained_and_reset_head(const std::string& path, int64_t new_num_classes) {
        load_weights(path);

        if (include_top_ && new_num_classes != num_classes_) {
            std::cout << "[Transfer] Resetting FC layer: " << num_classes_ << " -> " << new_num_classes << std::endl;

            unregister_module("fc");
            fc = register_module("fc", torch::nn::Linear(512, new_num_classes));
            fc->to(this->parameters()[0].device());
            num_classes_ = new_num_classes;

            this->to(this->parameters()[0].device());
        }
    }

    void freeze_backbone(bool freeze = true) {
        std::cout << "[Transfer] " << (freeze ? "Freezing" : "Unfreezing") << " backbone..." << std::endl;

        for (auto& pair : this->named_parameters()) {
            if (pair.key().find("fc") == std::string::npos) {
                pair.value().set_requires_grad(!freeze);
            }
            else {
                pair.value().set_requires_grad(true);
            }
        }
    }

    float train_step_amp_o(torch::optim::Optimizer& optimizer,
        ManualGradScaler& scaler,
        torch::Tensor imgs, torch::Tensor targets) {

        this->train();
        optimizer.zero_grad();

        torch::Tensor loss;

        {
            bool prev_autocast = at::autocast::is_enabled();
            at::autocast::set_enabled(true);

            auto output = this->forward(imgs);
            loss = torch::nn::functional::cross_entropy(output, targets);

            at::autocast::set_enabled(prev_autocast);
        }

        scaler.scale(loss).backward();
        scaler.step(optimizer,this);
        scaler.update();

        return loss.item<float>();
    }

    template <typename ModelType, typename OptimizerType>
    float train_step_amp_o(
        ModelType& model,
        OptimizerType& optimizer,
        ManualGradScaler& scaler,
        torch::Tensor imgs,
        torch::Tensor targets,
        torch::Device device = torch::kCUDA
    ) {
        model->train();

        c10::AutoGradMode enable_guard(true);
        torch::Tensor output = model->forward(imgs);

        torch::Tensor loss = torch::nn::functional::cross_entropy(output, targets);

        optimizer.zero_grad();

        scaler.scale(loss).backward();

        scaler.step(optimizer, model.get());

        scaler.update();

        return loss.item<float>();
    }

    template <typename ModelType, typename OptimizerType>
    float train_step_amp(
        ModelType& model,
        OptimizerType& optimizer,
        ManualGradScaler& scaler,
        torch::Tensor imgs,
        torch::Tensor targets,
        torch::Device device = torch::kCUDA
    ) {
        model->train();

        bool prev_amp = at::autocast::is_enabled();
        at::autocast::set_enabled(true);

        torch::Tensor output;
        torch::Tensor loss;

        try {
            output = model->forward(imgs);

            if (targets.scalar_type() != torch::kLong) targets = targets.to(torch::kLong);

            loss = torch::nn::functional::cross_entropy(output.to(torch::kFloat32), targets);
        }
        catch (...) {
            at::autocast::set_enabled(prev_amp);
            throw;
        }

        at::autocast::set_enabled(prev_amp);

        optimizer.zero_grad();

        scaler.scale(loss).backward();

        scaler.step(optimizer, model.get());

        scaler.update();

        return loss.item<float>();
    }

    

    float evaluate(std::function<torch::data::Example<torch::Tensor, torch::Tensor>()> data_loader,
        torch::Device device) {
        this->eval();
        torch::NoGradGuard no_grad;

        int correct = 0;
        int total = 0;
        torch::data::Example<torch::Tensor, torch::Tensor> batch;

        while ((batch = data_loader()).data.defined()) {
            auto data = batch.data.to(device);
            auto targets = batch.target.to(device);

            auto output = this->forward(data);
            auto pred = output.argmax(1);

            correct += pred.eq(targets).sum().item<int>();
            total += data.size(0);
        }

        return total > 0 ? static_cast<float>(correct) / total : 0.0f;
    }

    int predict(torch::Tensor img) {
        this->eval();
        torch::NoGradGuard no_grad;
        auto output = this->forward(img);
        return output.argmax(1).item<int>();
    }

    torch::Tensor forward(torch::Tensor x) {
        x = stem->forward(x);
        x = pool->forward(x);

        x = layer1->forward(x);
        auto p2 = x;

        x = layer2->forward(x);
        auto p3 = x;

        x = layer3->forward(x);
        auto p4 = x;

        x = layer4->forward(x);
        auto p5 = x;

        if (include_top_) {
            x = avgpool->forward(x);
            x = x.flatten(1);
            x = fc->forward(x);
            return x;
        }
        else {
            return x;
        }
    }

    std::vector<torch::Tensor> forward_features(torch::Tensor x) {
        x = stem->forward(x);
        x = pool->forward(x);

        auto p2 = layer1->forward(x);
        auto p3 = layer2->forward(p2);
        auto p4 = layer3->forward(p3);
        auto p5 = layer4->forward(p4);

        return { p3, p4, p5 };
    }

    std::vector<int64_t> get_out_channels() const {
        return { 128, 256, 512 };
    }



    void load_weights(const std::string& path) {
        std::cout << "Loading ResNet18 weights from " << path << "..." << std::endl;

        std::vector<char> f;
        try {
            std::ifstream input(path, std::ios::binary);
            f.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            input.close();
        }
        catch (...) {
            std::cerr << "Failed to handle file: " << path << std::endl; return;
        }

        torch::IValue data = torch::pickle_load(f);
        auto dict = data.toGenericDict();
        auto params = this->named_parameters(true);
        auto buffers = this->named_buffers(true);

        torch::NoGradGuard no_grad;

        for (auto& item : dict) {
            std::string py_key = item.key().toStringRef();
            torch::Tensor val = item.value().toTensor();

            std::string cpp_key = py_key;

            if (py_key.find("conv1.") == 0) {
                cpp_key.replace(0, 5, "stem.conv");
            }
            else if (py_key.find("bn1.") == 0) {
                cpp_key.replace(0, 3, "stem.bn");
            }
            else if (py_key.find("layer") == 0) {
                size_t c1_pos = cpp_key.find(".conv1.");
                if (c1_pos != std::string::npos) {
                    cpp_key.replace(c1_pos, 7, ".cv1.conv.");
                }

                size_t c2_pos = cpp_key.find(".conv2.");
                if (c2_pos != std::string::npos) {
                    cpp_key.replace(c2_pos, 7, ".cv2.conv.");
                }

                size_t b1_pos = cpp_key.find(".bn1.");
                if (b1_pos != std::string::npos) {
                    cpp_key.replace(b1_pos, 5, ".cv1.bn.");
                }

                size_t b2_pos = cpp_key.find(".bn2.");
                if (b2_pos != std::string::npos) {
                    cpp_key.replace(b2_pos, 5, ".cv2.bn.");
                }

            }

            if (params.contains(cpp_key)) {
                params[cpp_key].copy_(val);
            }
            else if (buffers.contains(cpp_key)) {
                buffers[cpp_key].copy_(val);
            }
            else {
            }
        }
        std::cout << "Weights loaded." << std::endl;
    }

    void export_structure(const std::string& path = "resnet18.mmd") {
        torch_utils::MermaidGenerator::generate(*this, path, "ResNet-18");

    }
private:
    bool include_top_;
    ConvModule stem{ nullptr };
    torch::nn::MaxPool2d pool{ nullptr };

    torch::nn::Sequential layer1{ nullptr };
    torch::nn::Sequential layer2{ nullptr };
    torch::nn::Sequential layer3{ nullptr };
    torch::nn::Sequential layer4{ nullptr };

    torch::nn::AdaptiveAvgPool2d avgpool{ nullptr };
    torch::nn::Linear fc{ nullptr };

    int64_t num_classes_;
    torch::nn::Sequential make_layer(int64_t in_ch, int64_t out_ch, int64_t blocks, int64_t stride) {
        torch::nn::Sequential layers;

        layers->push_back(BasicBlock(in_ch, out_ch, stride));

        for (int64_t i = 1; i < blocks; i++) {
            layers->push_back(BasicBlock(out_ch, out_ch, 1));
        }

        return layers;
    }
};
TORCH_MODULE(ResNet18);

#endif
