#pragma once

#include "torch_nnmodule.h"

#include <algorithm>
#include <torch/script.h>
#include <torch/torch.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

class YoloV8SegBottleneckImpl : public torch::nn::Module
{
public:
    YoloV8SegBottleneckImpl(
        int64_t channels,
        bool shortcut)
        : shortcut_(shortcut)
    {
        cv1 = register_module(
            "cv1", ConvModule(channels, channels, 3, 1));
        cv2 = register_module(
            "cv2", ConvModule(channels, channels, 3, 1));
    }

    torch::Tensor forward(const torch::Tensor& input)
    {
        const torch::Tensor output = cv2->forward(cv1->forward(input));
        return shortcut_ ? input + output : output;
    }

private:
    ConvModule cv1{nullptr};
    ConvModule cv2{nullptr};
    bool shortcut_ = false;
};
TORCH_MODULE(YoloV8SegBottleneck);

class YoloV8SegC2fImpl : public torch::nn::Module
{
public:
    YoloV8SegC2fImpl(
        int64_t input_channels,
        int64_t output_channels,
        int64_t repeats,
        bool shortcut)
        : hidden_channels_(output_channels / 2)
    {
        cv1 = register_module(
            "cv1",
            ConvModule(input_channels, hidden_channels_ * 2, 1, 1));
        cv2 = register_module(
            "cv2",
            ConvModule(
                (2 + repeats) * hidden_channels_,
                output_channels,
                1,
                1));
        blocks = register_module("m", torch::nn::ModuleList());
        for (int64_t index = 0; index < repeats; ++index)
        {
            blocks->push_back(
                YoloV8SegBottleneck(hidden_channels_, shortcut));
        }
    }

    torch::Tensor forward(const torch::Tensor& input)
    {
        const auto chunks = cv1->forward(input).chunk(2, 1);
        std::vector<torch::Tensor> outputs{chunks[0], chunks[1]};
        torch::Tensor current = chunks[1];
        for (std::size_t index = 0; index < blocks->size(); ++index)
        {
            current = blocks[index]
                ->as<YoloV8SegBottleneckImpl>()
                ->forward(current);
            outputs.push_back(current);
        }
        return cv2->forward(torch::cat(outputs, 1));
    }

private:
    int64_t hidden_channels_ = 0;
    ConvModule cv1{nullptr};
    ConvModule cv2{nullptr};
    torch::nn::ModuleList blocks{nullptr};
};
TORCH_MODULE(YoloV8SegC2f);

class YoloV8SegUpsampleImpl : public torch::nn::Module
{
public:
    torch::Tensor forward(const torch::Tensor& input)
    {
        return torch::nn::functional::interpolate(
            input,
            torch::nn::functional::InterpolateFuncOptions()
                .scale_factor(std::vector<double>{2.0, 2.0})
                .mode(torch::kNearest));
    }
};
TORCH_MODULE(YoloV8SegUpsample);

class YoloV8SegConcatImpl : public torch::nn::Module
{
public:
    torch::Tensor forward(const std::vector<torch::Tensor>& inputs)
    {
        return torch::cat(inputs, 1);
    }
};
TORCH_MODULE(YoloV8SegConcat);

class YoloV8SegDFLImpl : public torch::nn::Module
{
public:
    explicit YoloV8SegDFLImpl(int64_t reg_max)
        : reg_max_(reg_max)
    {
        conv = register_module(
            "conv",
            torch::nn::Conv2d(
                torch::nn::Conv2dOptions(reg_max, 1, 1).bias(false)));
        torch::NoGradGuard no_grad;
        conv->weight.copy_(
            torch::arange(
                reg_max,
                torch::TensorOptions().dtype(torch::kFloat32))
                .view({1, reg_max, 1, 1}));
        conv->weight.set_requires_grad(false);
    }

    torch::Tensor expectation(const torch::Tensor& logits) const
    {
        const int64_t batch = logits.size(0);
        const int64_t anchors = logits.size(2);
        const torch::Tensor probabilities =
            logits.view({batch, 4, reg_max_, anchors}).softmax(2);
        const torch::Tensor projection = torch::arange(
            reg_max_,
            probabilities.options()).view({1, 1, reg_max_, 1});
        return (probabilities * projection).sum(2);
    }

private:
    int64_t reg_max_ = 16;
    torch::nn::Conv2d conv{nullptr};
};
TORCH_MODULE(YoloV8SegDFL);

class YoloV8SegProtoImpl : public torch::nn::Module
{
public:
    YoloV8SegProtoImpl(
        int64_t input_channels,
        int64_t hidden_channels,
        int64_t mask_channels)
    {
        cv1 = register_module(
            "cv1",
            ConvModule(input_channels, hidden_channels, 3, 1));
        upsample = register_module(
            "upsample",
            torch::nn::ConvTranspose2d(
                torch::nn::ConvTranspose2dOptions(
                    hidden_channels, hidden_channels, 2).stride(2)));
        cv2 = register_module(
            "cv2",
            ConvModule(hidden_channels, hidden_channels, 3, 1));
        cv3 = register_module(
            "cv3",
            ConvModule(hidden_channels, mask_channels, 1, 1));
    }

    torch::Tensor forward(const torch::Tensor& input)
    {
        return cv3->forward(
            cv2->forward(
                upsample->forward(
                    cv1->forward(input))));
    }

private:
    ConvModule cv1{nullptr};
    torch::nn::ConvTranspose2d upsample{nullptr};
    ConvModule cv2{nullptr};
    ConvModule cv3{nullptr};
};
TORCH_MODULE(YoloV8SegProto);

struct YoloV8SegRawOutput
{
    std::vector<torch::Tensor> box_logits;
    std::vector<torch::Tensor> class_logits;
    std::vector<torch::Tensor> mask_coefficients;
    torch::Tensor prototypes;
};

class YoloV8SegmentHeadImpl : public torch::nn::Module
{
public:
    YoloV8SegmentHeadImpl(
        int64_t num_classes,
        int64_t mask_channels,
        int64_t prototype_channels)
        : num_classes_(num_classes),
          mask_channels_(mask_channels)
    {
        const std::vector<int64_t> channels{64, 128, 256};
        cv2 = register_module("cv2", torch::nn::ModuleList());
        cv3 = register_module("cv3", torch::nn::ModuleList());
        cv4 = register_module("cv4", torch::nn::ModuleList());
        for (const int64_t channel : channels)
        {
            cv2->push_back(torch::nn::Sequential(
                ConvModule(channel, 64, 3),
                ConvModule(64, 64, 3),
                torch::nn::Conv2d(
                    torch::nn::Conv2dOptions(64, 64, 1))));
            cv3->push_back(torch::nn::Sequential(
                ConvModule(channel, 80, 3),
                ConvModule(80, 80, 3),
                torch::nn::Conv2d(
                    torch::nn::Conv2dOptions(80, num_classes, 1))));
            cv4->push_back(torch::nn::Sequential(
                ConvModule(channel, mask_channels, 3),
                ConvModule(mask_channels, mask_channels, 3),
                torch::nn::Conv2d(
                    torch::nn::Conv2dOptions(
                        mask_channels, mask_channels, 1))));
        }
        dfl = register_module("dfl", YoloV8SegDFL(16));
        proto = register_module(
            "proto",
            YoloV8SegProto(64, prototype_channels, mask_channels));
    }

    YoloV8SegRawOutput forward(
        const std::vector<torch::Tensor>& features)
    {
        TORCH_CHECK(features.size() == 3, "YOLOv8 Segment expects 3 scales");
        YoloV8SegRawOutput output;
        output.prototypes = proto->forward(features[0]);
        for (std::size_t index = 0; index < features.size(); ++index)
        {
            output.box_logits.push_back(
                cv2[index]->as<torch::nn::Sequential>()->forward(
                    features[index]));
            output.class_logits.push_back(
                cv3[index]->as<torch::nn::Sequential>()->forward(
                    features[index]));
            output.mask_coefficients.push_back(
                cv4[index]->as<torch::nn::Sequential>()->forward(
                    features[index]));
        }
        return output;
    }

    const YoloV8SegDFL& dfl_module() const
    {
        return dfl;
    }

private:
    int64_t num_classes_ = 80;
    int64_t mask_channels_ = 32;
    torch::nn::ModuleList cv2{nullptr};
    torch::nn::ModuleList cv3{nullptr};
    YoloV8SegDFL dfl{nullptr};
    YoloV8SegProto proto{nullptr};
    torch::nn::ModuleList cv4{nullptr};
};
TORCH_MODULE(YoloV8SegmentHead);

struct YoloV8SegWeightMappingReport
{
    int source_count = 0;
    int target_count = 0;
    int loaded_count = 0;
    // Classifier outputs may be deliberately reinitialized when a mature
    // parent is transferred to a different, explicitly declared ontology.
    // Keeping this separate from a shape mismatch makes the exception
    // auditable and prevents a permissive partial state load.
    std::vector<std::string> reinitialized_keys;
    std::vector<std::string> missing_keys;
    std::vector<std::string> unknown_keys;
    std::vector<std::string> shape_mismatches;

    bool complete() const
    {
        return source_count == target_count &&
               loaded_count == target_count &&
               missing_keys.empty() &&
               unknown_keys.empty() &&
               shape_mismatches.empty();
    }

    bool transfer_complete() const
    {
        return loaded_count + static_cast<int>(reinitialized_keys.size()) ==
                   target_count &&
               missing_keys.empty() && unknown_keys.empty() &&
               shape_mismatches.empty() && !reinitialized_keys.empty();
    }
};

class YoloV8SegmentImpl : public torch::nn::Module
{
public:
    explicit YoloV8SegmentImpl(int64_t num_classes = 80)
    {
        TORCH_CHECK(num_classes > 0, "YOLOv8-Seg num_classes must be positive");
        model = register_module("model", torch::nn::ModuleList());
        model->push_back(m0 = ConvModule(3, 16, 3, 2));
        model->push_back(m1 = ConvModule(16, 32, 3, 2));
        model->push_back(m2 = YoloV8SegC2f(32, 32, 1, true));
        model->push_back(m3 = ConvModule(32, 64, 3, 2));
        model->push_back(m4 = YoloV8SegC2f(64, 64, 2, true));
        model->push_back(m5 = ConvModule(64, 128, 3, 2));
        model->push_back(m6 = YoloV8SegC2f(128, 128, 2, true));
        model->push_back(m7 = ConvModule(128, 256, 3, 2));
        model->push_back(m8 = YoloV8SegC2f(256, 256, 1, true));
        model->push_back(m9 = SPPF(256, 256, 5));
        model->push_back(m10 = YoloV8SegUpsample());
        model->push_back(m11 = YoloV8SegConcat());
        model->push_back(m12 = YoloV8SegC2f(384, 128, 1, false));
        model->push_back(m13 = YoloV8SegUpsample());
        model->push_back(m14 = YoloV8SegConcat());
        model->push_back(m15 = YoloV8SegC2f(192, 64, 1, false));
        model->push_back(m16 = ConvModule(64, 64, 3, 2));
        model->push_back(m17 = YoloV8SegConcat());
        model->push_back(m18 = YoloV8SegC2f(192, 128, 1, false));
        model->push_back(m19 = ConvModule(128, 128, 3, 2));
        model->push_back(m20 = YoloV8SegConcat());
        model->push_back(m21 = YoloV8SegC2f(384, 256, 1, false));
        model->push_back(m22 = YoloV8SegmentHead(num_classes, 32, 64));
    }

    YoloV8SegRawOutput forward(const torch::Tensor& input)
    {
        const auto x0 = m0->forward(input);
        const auto x1 = m1->forward(x0);
        const auto x2 = m2->forward(x1);
        const auto x3 = m3->forward(x2);
        const auto x4 = m4->forward(x3);
        const auto x5 = m5->forward(x4);
        const auto x6 = m6->forward(x5);
        const auto x7 = m7->forward(x6);
        const auto x8 = m8->forward(x7);
        const auto x9 = m9->forward(x8);
        const auto x10 = m10->forward(x9);
        const auto x11 = m11->forward({x10, x6});
        const auto x12 = m12->forward(x11);
        const auto x13 = m13->forward(x12);
        const auto x14 = m14->forward({x13, x4});
        const auto x15 = m15->forward(x14);
        const auto x16 = m16->forward(x15);
        const auto x17 = m17->forward({x16, x12});
        const auto x18 = m18->forward(x17);
        const auto x19 = m19->forward(x18);
        const auto x20 = m20->forward({x19, x9});
        const auto x21 = m21->forward(x20);
        return m22->forward({x15, x18, x21});
    }

    YoloV8SegWeightMappingReport load_state_dict_strict(
        const std::string& path)
    {
        std::ifstream input(path, std::ios::binary);
        TORCH_CHECK(input.good(), "failed to open YOLOv8-Seg state dict");
        const std::vector<char> bytes(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        const torch::IValue payload = torch::pickle_load(bytes);
        TORCH_CHECK(
            payload.isGenericDict(),
            "YOLOv8-Seg asset must be a GenericDict state dict");

        std::map<std::string, torch::Tensor> source;
        for (const auto& item : payload.toGenericDict())
        {
            TORCH_CHECK(
                item.key().isString() && item.value().isTensor(),
                "YOLOv8-Seg state dict must contain string->tensor entries");
            source.emplace(
                item.key().toStringRef(),
                item.value().toTensor());
        }

        auto parameters = named_parameters(true);
        auto buffers = named_buffers(true);
        std::map<std::string, torch::Tensor*> targets;
        for (auto& item : parameters)
            targets.emplace(item.key(), &item.value());
        for (auto& item : buffers)
            targets.emplace(item.key(), &item.value());

        YoloV8SegWeightMappingReport report;
        report.source_count = static_cast<int>(source.size());
        report.target_count = static_cast<int>(targets.size());
        for (auto& target : targets)
        {
            const auto found = source.find(target.first);
            if (found == source.end())
            {
                report.missing_keys.push_back(target.first);
                continue;
            }
            if (target.second->sizes() != found->second.sizes())
            {
                std::ostringstream reason;
                reason << target.first << " target=" << target.second->sizes()
                       << " source=" << found->second.sizes();
                report.shape_mismatches.push_back(reason.str());
                continue;
            }
            ++report.loaded_count;
        }
        for (const auto& item : source)
        {
            if (targets.find(item.first) == targets.end())
                report.unknown_keys.push_back(item.first);
        }
        // Phase A only inspects keys and tensor metadata.  A failed strict
        // mapping must leave the V1 baseline untouched so callers can retain
        // a reproducible parent for parity and rollback.
        TORCH_CHECK(report.complete(),
                    "YOLOv8-Seg strict state-dict mapping is incomplete");

        // Phase B is entered only after the complete report is known.  This
        // makes the strict load atomic at the model-state level.
        torch::NoGradGuard no_grad;
        for (auto& target : targets)
        {
            const auto found = source.find(target.first);
            TORCH_CHECK(found != source.end(),
                        "strict mapping changed between inspection and copy");
            target.second->copy_(found->second);
        }
        return report;
    }

    // Controlled ontology transfer.  All parent tensors remain strict except
    // the last convolution of the class branches.  Those outputs are kept at
    // their freshly initialized target-ontology values; no parent class score
    // is silently reinterpreted as a business geometry class.
    YoloV8SegWeightMappingReport load_state_dict_transfer_classifier(
        const std::string& path)
    {
        std::ifstream input(path, std::ios::binary);
        TORCH_CHECK(input.good(), "failed to open YOLOv8-Seg state dict");
        const std::vector<char> bytes(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        const torch::IValue payload = torch::pickle_load(bytes);
        TORCH_CHECK(payload.isGenericDict(),
                    "YOLOv8-Seg asset must be a GenericDict state dict");

        std::map<std::string, torch::Tensor> source;
        for (const auto& item : payload.toGenericDict())
        {
            TORCH_CHECK(item.key().isString() && item.value().isTensor(),
                        "YOLOv8-Seg state dict must contain string->tensor entries");
            source.emplace(item.key().toStringRef(), item.value().toTensor());
        }
        auto parameters = named_parameters(true);
        auto buffers = named_buffers(true);
        std::map<std::string, torch::Tensor*> targets;
        for (auto& item : parameters)
            targets.emplace(item.key(), &item.value());
        for (auto& item : buffers)
            targets.emplace(item.key(), &item.value());

        auto is_classifier_output_transfer = [](
            const std::string& key,
            const torch::Tensor& target,
            const torch::Tensor& parent) {
            if (key.find(".cv3.") == std::string::npos ||
                target.dim() != parent.dim() || target.dim() < 1 ||
                target.size(0) == parent.size(0))
                return false;
            for (int64_t dim = 1; dim < target.dim(); ++dim)
            {
                if (target.size(dim) != parent.size(dim))
                    return false;
            }
            // Only a Conv2d output weight or its matching bias can differ.
            return target.dim() == 4 || target.dim() == 1;
        };

        YoloV8SegWeightMappingReport report;
        report.source_count = static_cast<int>(source.size());
        report.target_count = static_cast<int>(targets.size());
        for (auto& target : targets)
        {
            const auto found = source.find(target.first);
            if (found == source.end())
            {
                report.missing_keys.push_back(target.first);
                continue;
            }
            if (target.second->sizes() != found->second.sizes())
            {
                if (is_classifier_output_transfer(
                        target.first, *target.second, found->second))
                {
                    report.reinitialized_keys.push_back(target.first);
                    continue;
                }
                std::ostringstream reason;
                reason << target.first << " target=" << target.second->sizes()
                       << " source=" << found->second.sizes();
                report.shape_mismatches.push_back(reason.str());
                continue;
            }
            ++report.loaded_count;
        }
        for (const auto& item : source)
        {
            if (targets.find(item.first) == targets.end())
                report.unknown_keys.push_back(item.first);
        }
        // Phase A validates the complete transfer plan before copying any
        // parent feature.  The classifier remains an intentional fresh head.
        TORCH_CHECK(report.transfer_complete(),
                    "YOLOv8-Seg classifier transfer mapping is incomplete");
        torch::NoGradGuard no_grad;
        for (auto& target : targets)
        {
            if (std::find(report.reinitialized_keys.begin(),
                          report.reinitialized_keys.end(), target.first) !=
                report.reinitialized_keys.end())
                continue;
            const auto found = source.find(target.first);
            TORCH_CHECK(found != source.end(),
                        "transfer mapping changed between inspection and copy");
            target.second->copy_(found->second);
        }
        return report;
    }

    YoloV8SegmentHead& head()
    {
        return m22;
    }

    const YoloV8SegmentHead& head() const
    {
        return m22;
    }

private:
    torch::nn::ModuleList model{nullptr};
    ConvModule m0{nullptr};
    ConvModule m1{nullptr};
    YoloV8SegC2f m2{nullptr};
    ConvModule m3{nullptr};
    YoloV8SegC2f m4{nullptr};
    ConvModule m5{nullptr};
    YoloV8SegC2f m6{nullptr};
    ConvModule m7{nullptr};
    YoloV8SegC2f m8{nullptr};
    SPPF m9{nullptr};
    YoloV8SegUpsample m10{nullptr};
    YoloV8SegConcat m11{nullptr};
    YoloV8SegC2f m12{nullptr};
    YoloV8SegUpsample m13{nullptr};
    YoloV8SegConcat m14{nullptr};
    YoloV8SegC2f m15{nullptr};
    ConvModule m16{nullptr};
    YoloV8SegConcat m17{nullptr};
    YoloV8SegC2f m18{nullptr};
    ConvModule m19{nullptr};
    YoloV8SegConcat m20{nullptr};
    YoloV8SegC2f m21{nullptr};
    YoloV8SegmentHead m22{nullptr};
};
TORCH_MODULE(YoloV8Segment);
