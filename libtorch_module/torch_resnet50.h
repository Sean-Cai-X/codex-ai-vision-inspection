#ifndef TORCH_RESNET50_H
#define TORCH_RESNET50_H


#include <torch/torch.h>
#include <vector>
#include <string>
#include <fstream>
#include "torch_nnmodule.h"


class ResNetBottleneckImpl : public torch::nn::Module {
public:
    ResNetBottleneckImpl(int64_t in_channels, int64_t mid_channels, int64_t stride = 1, int64_t expansion = 4) {

        int64_t out_channels = mid_channels * expansion;

        cv1 = register_module("cv1",
            ConvModule(in_channels, mid_channels, 1, 1, 0, true, true, "relu"));

        cv2 = register_module("cv2",
            ConvModule(mid_channels, mid_channels, 3, stride, 1, true, true, "relu"));

        cv3 = register_module("cv3",
            ConvModule(mid_channels, out_channels, 1, 1, 0, true, false, "none"));

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
        out = cv3->forward(out);

        if (!downsample.is_empty()) {
            identity = downsample->forward(x);
        }

        out += identity;
        return act->forward(out);
    }

private:
    ConvModule cv1{ nullptr }, cv2{ nullptr }, cv3{ nullptr };
    torch::nn::Sequential downsample{ nullptr };
    torch::nn::ReLU act{ nullptr };
};
TORCH_MODULE(ResNetBottleneck);

class ResNet50Impl : public torch::nn::Module {
public:
    ResNet50Impl(int64_t num_classes = 1000, bool include_top = true)
        : include_top_(include_top), num_classes_(num_classes) {

        stem = register_module("stem",
            ConvModule(3, 64, 7, 2, 3, true, true, "relu"));

        pool = register_module("pool",
            torch::nn::MaxPool2d(torch::nn::MaxPool2dOptions(3).stride(2).padding(1)));

        layer1 = make_layer(64, 64, 3, 1);
        register_module("layer1", layer1);

        layer2 = make_layer(256, 128, 4, 2);
        register_module("layer2", layer2);

        layer3 = make_layer(512, 256, 6, 2);
        register_module("layer3", layer3);

        layer4 = make_layer(1024, 512, 3, 2);
        register_module("layer4", layer4);

        if (include_top_) {
            avgpool = register_module("avgpool", torch::nn::AdaptiveAvgPool2d(torch::nn::AdaptiveAvgPool2dOptions({ 1, 1 })));
            fc = register_module("fc", torch::nn::Linear(2048, num_classes));
        }
    }
    void export_structure(const std::string& path = "resnet50.mmd") {
        torch_utils::MermaidGenerator::generate(*this, path, "ResNet-50");

    }

    void load_pretrained_and_reset_head(const std::string& path, int64_t new_num_classes) {
        load_weights(path);

        if (include_top_ && new_num_classes != num_classes_) {
            std::cout << "[Transfer] Resetting FC layer: " << num_classes_ << " -> " << new_num_classes << std::endl;

            unregister_module("fc");
            fc = register_module("fc", torch::nn::Linear(2048, new_num_classes));
            num_classes_ = new_num_classes;

            fc->to(this->parameters()[0].device());

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

    torch::Tensor forward(torch::Tensor x) {
        x = stem->forward(x);
        x = pool->forward(x);

        x = layer1->forward(x);
        x = layer2->forward(x);
        x = layer3->forward(x);
        x = layer4->forward(x);

        if (include_top_) {
            x = avgpool->forward(x);
            x = x.flatten(1);
            x = fc->forward(x);
        }
        return x;
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
        return { 512, 1024, 2048 };
    }

    float train_step_amp(torch::optim::Optimizer& optimizer,
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
        scaler.step(optimizer, this);
        scaler.update();

        return loss.item<float>();
    }

    /*
    float evaluate(torch::data::DataLoader<torch::data::datasets::MapDataset<torch::data::datasets::ImageFolder, torch::data::transforms::Stack<>>>& loader,
        torch::Device device) {
        this->eval();
        torch::NoGradGuard no_grad;

        int correct = 0;
        int total = 0;

        for (auto& batch : loader) {
            auto data = batch.data.to(device);
            auto targets = batch.target.to(device);

            auto output = this->forward(data);
            auto pred = output.argmax(1);

            correct += pred.eq(targets).sum().item<int>();
            total += data.size(0);
        }

        return static_cast<float>(correct) / total;
    }
    */

    float evaluate(std::function<torch::data::Example<torch::Tensor, torch::Tensor>()> data_loader,
        torch::Device device) {
        // KEY: iterator-style evaluation helper for custom loaders.
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
        // KEY: single-image classification helper.
        this->eval();
        torch::NoGradGuard no_grad;
        // img: [1, 3, H, W]
        auto output = this->forward(img);
        return output.argmax(1).item<int>();
    }

    /*
    import torch
    import torchvision.models as models

    # Example export helper used to create plain ResNet weight files.
    # 1. Build the torchvision model.
    # 2. Read the state_dict.
    # 3. Move tensors to CPU and save a plain .pt file.

    def export_resnet(name):
        if name == 'resnet18':
            model = models.resnet18(pretrained=True)
        elif name == 'resnet50':
            model = models.resnet50(pretrained=True)
        else:
            raise ValueError(f"Unsupported model: {name}")

        sd = model.state_dict()
        cpu_sd = {str(k): v.cpu() for k, v in sd.items()}
        save_name = f"{name}_weights.pt"
        torch.save(cpu_sd, save_name)
        print(f"Exported {name} to {save_name}")

    if __name__ == '__main__':
        export_resnet('resnet18')
        export_resnet('resnet50')
    */
    void load_weights(const std::string& path) {
        // CHECK: this loader remaps torchvision-style keys into the local module naming.
        std::cout << "Loading ResNet50 weights from " << path << "..." << std::endl;

        std::ifstream input(path, std::ios::binary);
        std::vector<char> f((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        torch::IValue data = torch::pickle_load(f);
        auto dict = data.toGenericDict();
        auto params = this->named_parameters(true);
        auto buffers = this->named_buffers(true);

        torch::NoGradGuard no_grad;

        for (auto& item : dict) {
            std::string py_key = item.key().toStringRef();
            torch::Tensor val = item.value().toTensor();
            std::string cpp_key = py_key;

            // KEY: remap torchvision stem keys into local ConvModule naming.
            if (py_key.find("conv1.") == 0) cpp_key.replace(0, 5, "stem.conv");
            else if (py_key.find("bn1.") == 0) cpp_key.replace(0, 3, "stem.bn");

            // KEY: remap bottleneck conv/bn keys into cv1/cv2/cv3 naming.
            else if (py_key.find("layer") == 0) {

                size_t p;
                if ((p = cpp_key.find(".conv1.")) != std::string::npos) cpp_key.replace(p, 7, ".cv1.conv.");
                else if ((p = cpp_key.find(".bn1.")) != std::string::npos) cpp_key.replace(p, 5, ".cv1.bn.");

                else if ((p = cpp_key.find(".conv2.")) != std::string::npos) cpp_key.replace(p, 7, ".cv2.conv.");
                else if ((p = cpp_key.find(".bn2.")) != std::string::npos) cpp_key.replace(p, 5, ".cv2.bn.");

                else if ((p = cpp_key.find(".conv3.")) != std::string::npos) cpp_key.replace(p, 7, ".cv3.conv.");
                else if ((p = cpp_key.find(".bn3.")) != std::string::npos) cpp_key.replace(p, 5, ".cv3.bn.");
            }

                if (params.contains(cpp_key)) {
                if (params[cpp_key].sizes() == val.sizes()) {
                    params[cpp_key].copy_(val);
                }
                else {
                    // RISK: shape mismatch is logged but not fatal.
                    std::cerr << "Shape mismatch: " << cpp_key << std::endl;
                }
            }
            else if (buffers.contains(cpp_key)) {
                buffers[cpp_key].copy_(val);
            }
        }
        std::cout << "Weights loaded successfully." << std::endl;
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
    torch::nn::Sequential make_layer(int64_t in_ch, int64_t mid_ch, int64_t blocks, int64_t stride) {
        // KEY: build a standard Bottleneck stage.
        torch::nn::Sequential layers;

        layers->push_back(ResNetBottleneck(in_ch, mid_ch, stride));

        int64_t out_ch = mid_ch * 4;
        for (int64_t i = 1; i < blocks; i++) {
            layers->push_back(ResNetBottleneck(out_ch, mid_ch, 1));
        }

        return layers;
    }
};
TORCH_MODULE(ResNet50);

#endif // TORCH_RESNET50_H
