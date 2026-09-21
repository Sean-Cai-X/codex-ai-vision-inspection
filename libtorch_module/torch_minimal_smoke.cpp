#include <torch/torch.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "torch_backbone.h"
#include "torch_DeepLabV3.h"
#include "torch_deeplabv3_plus.h"
#include "torch_feature_head.h"
#include "torch_fusion_head.h"
#include "torch_incremental_pipeline.h"
#include "torch_modelconfig.h"
#include "torch_mobilevit_mainline_bridge.h"
#include "torch_mobilevitv2.h"
#include "torch_nnmodule.h"
#include "torch_pan.h"
#include "torch_occ_bridge.h"
#include "torch_resnet18.h"
#include "torch_resnet50.h"
#include "torch_segmentation_mainline_bridge.h"
#include "torch_taskalignedassigner.h"
#include "torch_tuning_profiles.h"
#include "torch_v8loss.h"
#include "torch_yolo_head.h"

namespace {


torch::Device get_test_device() {
    if (const char* use_cuda = std::getenv("LIBTORCH_MODULE_USE_CUDA")) {
        if (std::string(use_cuda) == "0") {
            return torch::Device(torch::kCPU);
        }
        if (std::string(use_cuda) == "1" && torch::cuda::is_available()) {
            return torch::Device(torch::kCUDA);
        }
    }
    if (const char* force_cpu = std::getenv("LIBTORCH_MODULE_FORCE_CPU")) {
        if (std::string(force_cpu) == "1") {
            return torch::Device(torch::kCPU);
        }
    }
    return torch::cuda::is_available() ? torch::Device(torch::kCUDA) : torch::Device(torch::kCPU);
}

void expect(bool cond, const std::string& message) {
    if (!cond) {
        throw std::runtime_error(message);
    }
}

int run_test(const std::string& name, const std::function<void()>& fn) {
    try {
        std::cout << "[RUN ] " << name << std::endl;
        fn();
        std::cout << "[ OK ] " << name << std::endl;
        return 0;
    } catch (const c10::Error& e) {
        std::cerr << "[FAIL] " << name << " :: " << e.what_without_backtrace() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << name << " :: " << e.what() << std::endl;
        return 1;
    }
}

void test_config_and_utils() {
    auto cfg = ModelConfig::get_config("nano", 3);
    expect(std::abs(cfg.width_multiple - 0.25f) < 1e-6f, "nano width_multiple mismatch");
    expect(std::abs(cfg.depth_multiple - 0.33f) < 1e-6f, "nano depth_multiple mismatch");
    expect(cfg.num_classes == 3, "num_classes override mismatch");

    auto scaled = scale_channels({64, 128, 256}, 0.25f);
    expect(scaled.size() == 3, "scaled channel count mismatch");
    expect(scaled[0] == 16 && scaled[1] == 32 && scaled[2] == 64, "scaled channels mismatch");
    expect(scale_blocks(9, 0.33f) == 3, "scaled block count mismatch");
}

void test_nnmodule_shapes() {
    auto device = get_test_device();
    Stem stem(3, 32);
    C2f c2f(32, 64, 2, true);
    SPPF sppf(64, 64, 5);

    stem->to(device);
    c2f->to(device);
    sppf->to(device);

    auto x = torch::randn({1, 3, 640, 640}, device);
    auto stem_out = stem->forward(x);
    expect(stem_out.sizes() == torch::IntArrayRef({1, 32, 320, 320}), "stem output shape mismatch");

    auto c2f_out = c2f->forward(stem_out);
    expect(c2f_out.sizes() == torch::IntArrayRef({1, 64, 320, 320}), "c2f output shape mismatch");

    auto sppf_out = sppf->forward(c2f_out);
    expect(sppf_out.sizes() == torch::IntArrayRef({1, 64, 320, 320}), "sppf output shape mismatch");
}

void test_backbone_pan_path() {
    auto device = get_test_device();
    auto base_channels = std::vector<int64_t>{64, 128, 256, 512, 1024};
    auto strides = std::vector<int64_t>{8, 16, 32};

    YOLOv8Backbone backbone(base_channels, 0.33f, 0.25f);
    backbone->to(device);
    auto x = torch::randn({1, 3, 640, 640}, device);

    auto feats = backbone->forward(x);
    expect(feats.size() == 3, "backbone must return 3 feature maps");
    expect(feats[0].size(2) == 80 && feats[0].size(3) == 80, "P3 shape mismatch");
    expect(feats[1].size(2) == 40 && feats[1].size(3) == 40, "P4 shape mismatch");
    expect(feats[2].size(2) == 20 && feats[2].size(3) == 20, "P5 shape mismatch");

    PAN pan(backbone->get_out_channels(), 0.33f, 0.25f);
    pan->to(device);
    auto pan_out = pan->forward(feats);
    expect(pan_out.size() == 3, "PAN must return 3 outputs");
    expect(pan_out[0].size(2) == 80 && pan_out[1].size(2) == 40 && pan_out[2].size(2) == 20,
        "PAN output spatial sizes mismatch");
}

void test_mobilevitv2_contract() {
    auto device = get_test_device();

    MobileViTv2 model(9);
    model->to(device);
    model->eval();

    auto x = torch::randn({2, 3, 256, 256}, device);
    auto out = model->forward(x);
    expect(out.sizes() == torch::IntArrayRef({2, 9}), "MobileViTv2 output shape mismatch");
    expect(torch::isfinite(out).all().item<bool>(), "MobileViTv2 output contains non-finite values");

    model->reset_head(4);
    auto out_reset = model->forward(x);
    expect(out_reset.sizes() == torch::IntArrayRef({2, 4}), "MobileViTv2 reset_head output shape mismatch");
}

void test_mobilevit_mainline_bridge_contract() {
    auto device = get_test_device();

    auto runner = make_mobilevit_mainline_runner_config(6, 256, 2);
    runner.device_policy = device.is_cuda() ? MobileViTDevicePolicy::ForceCUDA : MobileViTDevicePolicy::ForceCPU;
    runner.validate();

    MobileViTv2 model(6);
    model->to(device);

    auto train_imgs = torch::randn({2, 3, 256, 256}, device);
    auto train_targets = torch::randint(0, 6, {2}, torch::TensorOptions().dtype(torch::kLong).device(device));
    auto eval_imgs = torch::randn({2, 3, 256, 256}, device);
    auto eval_targets = torch::randint(0, 6, {2}, torch::TensorOptions().dtype(torch::kLong).device(device));

    auto session = run_mobilevit_mainline_session(
        model, train_imgs, train_targets, eval_imgs, eval_targets, runner);
    session.validate();

    expect(session.passed, "MobileViT mainline session should pass");
    expect(session.flat_run.track_name == "mobilevit_mainline", "MobileViT flat run track mismatch");
    expect(session.flat_run.outcomes.size() == 2, "MobileViT flat run should contain smoke/eval outcomes");
    expect(session.flat_run.outcomes[0].stage_name == "mobilevit_train_smoke_stage",
        "MobileViT flat run should start with smoke stage");
    expect(session.flat_run.outcomes[1].stage_name == "mobilevit_eval_stage",
        "MobileViT flat run should include eval stage");

    auto trainer = run_mobilevit_trainer_session(
        model, train_imgs, train_targets, eval_imgs, eval_targets, runner);
    trainer.validate();

    auto analysis = build_mobilevit_trainer_analysis(trainer);
    analysis.validate();

    expect(analysis.timeline.stages.size() == 4, "MobileViT trainer timeline size mismatch");
    expect(analysis.timeline.stages.front().stage_name == "mobilevit_logging",
        "MobileViT trainer timeline should start with logging");
    expect(analysis.timeline.stages[1].stage_name == "mobilevit_smoke_train",
        "MobileViT trainer timeline should include smoke stage");
    expect(analysis.comparison_rows.size() == 2, "MobileViT trainer comparison row count mismatch");
    expect(analysis.comparison_rows[0].stage_name == "mobilevit_trainer_smoke_stage",
        "MobileViT trainer comparison should start with smoke stage");
    expect(analysis.recommendation.track_name == "mobilevit_mainline",
        "MobileViT trainer recommendation track mismatch");
    expect(analysis.lifecycle_summary.all_passed, "MobileViT trainer lifecycle should pass");
    expect(analysis.flat_run.run_name == "mobilevit_trainer_session_run",
        "MobileViT trainer flat run name mismatch");

    auto unified = build_mobilevit_unified_mainline_bundle(session, analysis);
    unified.validate();
    auto unified_summary = build_mobilevit_unified_mainline_summary(unified);
    unified_summary.validate();

    expect(unified.flat_run.run_name == "mobilevit_unified_mainline_run",
        "MobileViT unified flat run name mismatch");
    expect(unified.flat_run.outcomes.size() == 4,
        "MobileViT unified flat run should aggregate mainline and trainer outcomes");
    expect(unified.recommendation.track_name == "mobilevit_mainline",
        "MobileViT unified recommendation track mismatch");
    expect(unified_summary.total_outcomes == 4, "MobileViT unified summary outcomes mismatch");
    expect(unified_summary.total_comparisons == 2, "MobileViT unified summary comparisons mismatch");
}

void test_mobilenetv3_backbone_contract() {
    auto device = get_test_device();

    MobileNetV3 model("large", 16);
    model->to(device);
    auto x = torch::randn({1, 3, 224, 224}, device);
    auto feats = model->forward_features(x);

    expect(feats.first.defined(), "MobileNetV3 low-level feature must be defined");
    expect(feats.second.defined(), "MobileNetV3 high-level feature must be defined");
    expect(feats.first.sizes() == torch::IntArrayRef({1, 40, 28, 28}),
        "MobileNetV3 low-level feature shape mismatch");
    expect(feats.second.sizes() == torch::IntArrayRef({1, 960, 14, 14}),
        "MobileNetV3 high-level feature shape mismatch");
    expect(model->get_low_level_channels() == 40, "MobileNetV3 low-level channel contract mismatch");
    expect(model->get_out_channels() == 960, "MobileNetV3 out channel contract mismatch");
}

void test_resnet18_contract() {
    auto device = get_test_device();

    ResNet18 classifier(7, true);
    classifier->to(device);
    classifier->eval();

    auto cls_input = torch::randn({2, 3, 224, 224}, device);
    auto cls_out = classifier->forward(cls_input);
    expect(cls_out.sizes() == torch::IntArrayRef({2, 7}), "ResNet18 classifier output shape mismatch");

    ResNet18 backbone_only(1000, false);
    backbone_only->to(device);
    backbone_only->eval();

    auto feat_input = torch::randn({1, 3, 224, 224}, device);
    auto feats = backbone_only->forward_features(feat_input);
    expect(feats.size() == 3, "ResNet18 forward_features must return 3 feature maps");
    expect(feats[0].sizes() == torch::IntArrayRef({1, 128, 28, 28}), "ResNet18 P3 shape mismatch");
    expect(feats[1].sizes() == torch::IntArrayRef({1, 256, 14, 14}), "ResNet18 P4 shape mismatch");
    expect(feats[2].sizes() == torch::IntArrayRef({1, 512, 7, 7}), "ResNet18 P5 shape mismatch");
}

void test_resnet50_contract() {
    auto device = get_test_device();

    ResNet50 classifier(11, true);
    classifier->to(device);
    classifier->eval();

    auto cls_input = torch::randn({2, 3, 224, 224}, device);
    auto cls_out = classifier->forward(cls_input);
    expect(cls_out.sizes() == torch::IntArrayRef({2, 11}), "ResNet50 classifier output shape mismatch");

    ResNet50 backbone_only(1000, false);
    backbone_only->to(device);
    backbone_only->eval();

    auto feat_input = torch::randn({1, 3, 224, 224}, device);
    auto feats = backbone_only->forward_features(feat_input);
    expect(feats.size() == 3, "ResNet50 forward_features must return 3 feature maps");
    expect(feats[0].sizes() == torch::IntArrayRef({1, 512, 28, 28}), "ResNet50 P3 shape mismatch");
    expect(feats[1].sizes() == torch::IntArrayRef({1, 1024, 14, 14}), "ResNet50 P4 shape mismatch");
    expect(feats[2].sizes() == torch::IntArrayRef({1, 2048, 7, 7}), "ResNet50 P5 shape mismatch");
}

void test_deeplabv3_contract() {
    auto device = get_test_device();

    DeepLabV3 model(5);
    model->to(device);
    model->eval();

    auto x = torch::randn({1, 3, 128, 128}, device);
    auto out = model->forward(x);
    expect(out.sizes() == torch::IntArrayRef({1, 5, 128, 128}), "DeepLabV3 output shape mismatch");
    expect(torch::isfinite(out).all().item<bool>(), "DeepLabV3 output contains non-finite values");
}

void test_deeplabv3_plus_contract() {
    auto device = get_test_device();

    {
        DeepLabV3Plus model("resnet18", 4);
        model->to(device);
        model->eval();

        auto x = torch::randn({1, 3, 128, 128}, device);
        auto out = model->forward(x);
        expect(out.count("out") == 1, "DeepLabV3Plus(resnet18) missing out tensor");
        expect(out.at("out").sizes() == torch::IntArrayRef({1, 4, 128, 128}),
            "DeepLabV3Plus(resnet18) output shape mismatch");
    }

    {
        DeepLabV3Plus model("mobilenet_v3_large", 6);
        model->to(device);
        model->eval();

        auto x = torch::randn({1, 3, 128, 128}, device);
        auto out = model->forward(x);
        expect(out.count("out") == 1, "DeepLabV3Plus(mobilenet_v3_large) missing out tensor");
        expect(out.at("out").sizes() == torch::IntArrayRef({1, 6, 128, 128}),
            "DeepLabV3Plus(mobilenet_v3_large) output shape mismatch");
    }
}

void test_segmentation_mainline_bridge_contract() {
    auto device = get_test_device();
    auto runner = make_segmentation_mainline_runner_config("deeplabv3plus", "mobilenet_v3_large", 3, 128, 2);
    runner.device_policy = device.is_cuda() ? SegmentationDevicePolicy::ForceCUDA : SegmentationDevicePolicy::ForceCPU;

    auto train_imgs = torch::randn({2, 3, 128, 128}, device);
    auto train_masks = torch::randint(0, 3, {2, 128, 128},
        torch::TensorOptions().dtype(torch::kLong).device(device));
    auto eval_imgs = torch::randn({2, 3, 128, 128}, device);
    auto eval_masks = torch::randint(0, 3, {2, 128, 128},
        torch::TensorOptions().dtype(torch::kLong).device(device));

    auto session = run_segmentation_mainline_session(
        train_imgs, train_masks, eval_imgs, eval_masks, runner);
    session.validate();

    expect(session.passed, "Segmentation mainline session should pass");
    expect(session.flat_run.outcomes.size() == 2,
        "Segmentation mainline session should expose smoke/eval outcomes");
    expect(session.eval.sample_count == 2, "Segmentation eval sample count mismatch");
    expect(session.eval.foreground_iou >= 0.0 && session.eval.foreground_iou <= 1.0,
        "Segmentation eval foreground IoU must stay in range");

    auto trainer = run_segmentation_trainer_session(
        train_imgs, train_masks, eval_imgs, eval_masks, runner);
    trainer.validate();
    auto analysis = build_segmentation_trainer_analysis(trainer);
    analysis.validate();
    auto unified = build_segmentation_unified_mainline_bundle(session, analysis);
    unified.validate();
    auto unified_summary = build_segmentation_unified_mainline_summary(unified);
    unified_summary.validate();

    expect(analysis.timeline.stages.size() == 4,
        "Segmentation trainer timeline should expose four lifecycle stages");
    expect(analysis.comparison_rows.size() == 2,
        "Segmentation trainer analysis should expose smoke/eval comparison rows");
    expect(unified.flat_run.outcomes.size() == 4,
        "Segmentation unified bundle should aggregate four outcomes");
}

void test_assigner_and_loss() {
    auto device = get_test_device();
    const int64_t num_classes = 3;
    const int64_t batch = 1;
    const int64_t max_gt = 4;

    TaskAlignedAssigner assigner(5, num_classes, 1.0f, 6.0f);
    assigner->to(device);

    auto pd_scores = torch::rand({batch, 1, 32, num_classes}, device);
    auto pd_bboxes = torch::rand({batch, 1, 32, 4}, device);
    auto gt_labels = torch::full({batch, max_gt, 1}, -1, torch::TensorOptions().dtype(torch::kLong).device(device));
    auto gt_bboxes = torch::zeros({batch, max_gt, 4}, device);
    auto mask_gt = torch::zeros({batch, max_gt, 1}, device);

    gt_labels[0][0][0] = 1;
    gt_bboxes[0][0] = torch::tensor({10.0f, 10.0f, 20.0f, 20.0f}, device);
    mask_gt[0][0][0] = 1.0f;

    auto assigned = assigner->forward(pd_scores, pd_bboxes, gt_labels, gt_bboxes, mask_gt);
    expect(assigned.sizes() == torch::IntArrayRef({batch, 32}), "assigner output shape mismatch");

    YOLOv8Loss loss_fn(num_classes);
    loss_fn->to(device);

    std::vector<torch::Tensor> preds = {
        torch::randn({batch, 80 * 80, 4 + num_classes}, device),
        torch::randn({batch, 40 * 40, 4 + num_classes}, device),
        torch::randn({batch, 20 * 20, 4 + num_classes}, device),
    };

    auto targets = torch::full({batch, max_gt, 6}, -1.0f, device);
    targets[0][0] = torch::tensor({0.0f, 1.0f, 20.0f, 20.0f, 16.0f, 16.0f}, device);

    auto loss_out = loss_fn->forward(preds, targets);
    auto total_loss = std::get<0>(loss_out);
    expect(torch::isfinite(total_loss).item<bool>(), "loss is not finite");

    YoloLossConfig dfl_cfg;
    dfl_cfg.enable_dfl = true;
    dfl_cfg.reg_max = 8;
    YOLOv8Loss dfl_loss_fn(num_classes, dfl_cfg);
    dfl_loss_fn->to(device);

    std::vector<torch::Tensor> dfl_preds = {
        torch::randn({batch, 80 * 80, dfl_cfg.box_channels() + num_classes}, device),
        torch::randn({batch, 40 * 40, dfl_cfg.box_channels() + num_classes}, device),
        torch::randn({batch, 20 * 20, dfl_cfg.box_channels() + num_classes}, device),
    };

    auto dfl_loss_out = dfl_loss_fn->forward(dfl_preds, targets);
    auto dfl_total_loss = std::get<0>(dfl_loss_out);
    const auto& dfl_items = std::get<1>(dfl_loss_out);
    expect(torch::isfinite(dfl_total_loss).item<bool>(), "DFL loss is not finite");
    expect(dfl_items.at("dfl_loss") > 0.0f, "DFL loss component must be positive when DFL is enabled");
}

void test_yolov8_detect_head_contract() {
    auto device = get_test_device();
    auto base_channels = std::vector<int64_t>{64, 128, 256, 512, 1024};
    auto strides = std::vector<int64_t>{8, 16, 32};

    YOLOv8Backbone backbone(base_channels, 0.33f, 0.25f);
    backbone->to(device);
    PAN pan(backbone->get_out_channels(), 0.33f, 0.25f);
    pan->to(device);

    YoloDetectHeadConfig default_cfg;
    YOLOv8Detect default_head(3, backbone->get_out_channels(), strides, default_cfg);
    default_head->to(device);

    auto x = torch::randn({1, 3, 640, 640}, device);
    auto feats = pan->forward(backbone->forward(x));
    auto pred = default_head->forward(feats);

    expect(default_head->box_output_channels() == 4, "default head box channels mismatch");
    expect(default_head->class_output_channels() == 3, "default head class channels mismatch");
    expect(pred.sizes() == torch::IntArrayRef({1, 8400, 7}), "default head output shape mismatch");

    YoloDetectHeadConfig dfl_cfg;
    dfl_cfg.use_dfl = true;
    dfl_cfg.reg_max = 8;
    YOLOv8Detect dfl_head(3, backbone->get_out_channels(), strides, dfl_cfg);
    dfl_head->to(device);
    auto dfl_pred = dfl_head->forward(feats);

    expect(dfl_head->box_output_channels() == 32, "DFL head box channels mismatch");
    expect(dfl_pred.sizes() == torch::IntArrayRef({1, 8400, 35}), "DFL head output shape mismatch");
}

void test_yolo_postprocess_contract() {
    YoloPostProcessConfig base_cfg;
    base_cfg.conf_threshold = 0.5f;
    base_cfg.iou_threshold = 0.5f;
    base_cfg.num_classes = 3;

    auto base_pred = torch::zeros({1, 2, 7}, torch::kFloat32);
    base_pred[0][0] = torch::tensor({10.0f, 10.0f, 20.0f, 20.0f, 0.9f, 0.1f, 0.0f});
    base_pred[0][1] = torch::tensor({12.0f, 12.0f, 22.0f, 22.0f, 0.2f, 0.8f, 0.0f});

    auto base_boxes = post_process(base_pred, base_cfg);
    expect(base_boxes.size() == 2, "Base post_process should keep two class-separated boxes");
    expect(base_boxes[0].score >= 0.8f, "Base post_process score mismatch");

    YoloPostProcessConfig dfl_cfg;
    dfl_cfg.conf_threshold = 0.5f;
    dfl_cfg.iou_threshold = 0.5f;
    dfl_cfg.num_classes = 3;
    dfl_cfg.use_dfl = true;
    dfl_cfg.reg_max = 4;

    auto dfl_pred = torch::zeros({1, 1, dfl_cfg.box_channels() + dfl_cfg.num_classes}, torch::kFloat32);
    dfl_pred[0][0][1] = 6.0f;
    dfl_pred[0][0][5] = 6.0f;
    dfl_pred[0][0][10] = 6.0f;
    dfl_pred[0][0][15] = 6.0f;
    dfl_pred[0][0][16] = 0.9f;

    auto dfl_boxes = post_process(dfl_pred, dfl_cfg);
    expect(dfl_boxes.size() == 1, "DFL post_process should produce one detection");
    expect(dfl_boxes[0].cls == 0, "DFL post_process class mismatch");
    expect(dfl_boxes[0].x1 < dfl_boxes[0].x2, "DFL post_process x ordering mismatch");
    expect(dfl_boxes[0].y1 < dfl_boxes[0].y2, "DFL post_process y ordering mismatch");
}

void test_yolo_eval_match_summary_contract() {
    std::vector<std::vector<BBox>> detections = {{
        {0.0f, 0.0f, 10.0f, 10.0f, 0.95f, 1},
        {20.0f, 20.0f, 30.0f, 30.0f, 0.60f, 0},
    }};
    std::vector<std::vector<BBox>> targets = {{
        {1.0f, 1.0f, 9.0f, 9.0f, 1.0f, 1},
        {40.0f, 40.0f, 50.0f, 50.0f, 1.0f, 0},
    }};

    auto summary = summarize_yolo_matches(detections, targets, 0.5f);
    expect(summary.images == 1, "Eval match summary image count mismatch");
    expect(summary.predicted_boxes == 2, "Eval match summary predicted_boxes mismatch");
    expect(summary.target_boxes == 2, "Eval match summary target_boxes mismatch");
    expect(summary.true_positives == 1, "Eval match summary TP mismatch");
    expect(summary.false_positives == 1, "Eval match summary FP mismatch");
    expect(summary.false_negatives == 1, "Eval match summary FN mismatch");
    expect(summary.per_class.size() == 2, "Eval match summary per-class size mismatch");
    expect(summary.per_class.at(1).true_positives == 1, "Eval match summary class-1 TP mismatch");
    expect(summary.per_class.at(1).target_boxes == 1, "Eval match summary class-1 target mismatch");
    expect(std::abs(summary.per_class.at(1).precision - 1.0f) < 1e-6f, "Eval match summary class-1 precision mismatch");
    expect(std::abs(summary.per_class.at(1).recall - 1.0f) < 1e-6f, "Eval match summary class-1 recall mismatch");
    expect(summary.per_class.at(0).false_positives == 1, "Eval match summary class-0 FP mismatch");
    expect(summary.per_class.at(0).false_negatives == 1, "Eval match summary class-0 FN mismatch");
    expect(std::abs(summary.per_class.at(0).precision - 0.0f) < 1e-6f, "Eval match summary class-0 precision mismatch");
    expect(std::abs(summary.per_class.at(0).recall - 0.0f) < 1e-6f, "Eval match summary class-0 recall mismatch");
    expect(std::abs(summary.precision - 0.5f) < 1e-6f, "Eval match summary precision mismatch");
    expect(std::abs(summary.recall - 0.5f) < 1e-6f, "Eval match summary recall mismatch");
    expect(summary.f1 > 0.49f && summary.f1 < 0.51f, "Eval match summary f1 mismatch");
    expect(summary.matched_iou > 0.6f, "Eval match summary matched IoU must reflect the matched box");
}

void test_tuning_profiles_contract() {
    auto yolo_smoke = YoloTuningProfile::smoke();
    yolo_smoke.validate();
    expect(yolo_smoke.model_type == "nano", "YOLO smoke profile model_type mismatch");
    expect(yolo_smoke.optimizer.epochs == 1, "YOLO smoke profile epochs mismatch");

    auto yolo_baseline = YoloTuningProfile::baseline();
    yolo_baseline.validate();
    expect(yolo_baseline.model_type == "small", "YOLO baseline profile model_type mismatch");
    expect(yolo_baseline.augmentation.input_size == 640, "YOLO baseline input size mismatch");

    auto mobilevit_profile = ClassificationTuningProfile::mobilevitv2_baseline();
    mobilevit_profile.validate();
    expect(mobilevit_profile.backbone == "mobilevitv2", "MobileViTv2 profile backbone mismatch");
    expect(mobilevit_profile.input_size == 256, "MobileViTv2 profile input size mismatch");

    auto resnet18_profile = ClassificationTuningProfile::resnet18_baseline();
    resnet18_profile.validate();
    expect(resnet18_profile.embedding_dim == 512, "ResNet18 profile embedding mismatch");

    auto resnet50_profile = ClassificationTuningProfile::resnet50_baseline();
    resnet50_profile.validate();
    expect(resnet50_profile.embedding_dim == 2048, "ResNet50 profile embedding mismatch");

    auto mobile_seg = SegmentationTuningProfile::mobilenetv3_deeplabv3plus();
    mobile_seg.validate();
    expect(mobile_seg.backbone == "mobilenet_v3_large", "MobileNetV3 segmentation backbone mismatch");
    expect(mobile_seg.low_level_channels == 40, "MobileNetV3 segmentation low-level channels mismatch");

    auto res18_seg = SegmentationTuningProfile::resnet18_deeplabv3plus();
    res18_seg.validate();
    expect(res18_seg.backbone == "resnet18", "ResNet18 segmentation backbone mismatch");
    expect(res18_seg.low_level_channels == 128, "ResNet18 segmentation low-level channels mismatch");

    auto res50_seg = SegmentationTuningProfile::resnet50_deeplabv3();
    res50_seg.validate();
    expect(res50_seg.backbone == "resnet50", "ResNet50 segmentation backbone mismatch");
    expect(res50_seg.decoder == "deeplabv3", "ResNet50 segmentation decoder mismatch");

    auto plan = build_mainline_tuning_plan();
    expect(plan.size() >= 8, "Expected staged tuning plan entries");
    for (const auto& entry : plan) {
        entry.validate();
    }
    expect(plan.front().track == TuningTrack::YoloMainline, "First tuning track must be YOLO mainline");
    expect(plan.front().phase == TuningPhase::Smoke, "First tuning phase must be smoke");
    expect(plan.front().priority == 1, "First tuning priority mismatch");
    expect(plan[1].requires_dataset, "Baseline YOLO plan should require dataset");
    expect(plan[2].track == TuningTrack::MobileViTMainline, "MobileViT baseline plan missing");
    expect(plan.back().track == TuningTrack::SegmentationAnalysis, "Final plan should be segmentation analysis");

    auto yolo_plan = build_yolo_mainline_plan();
    expect(yolo_plan.size() == 3, "YOLO mainline plan size mismatch");
    expect(yolo_plan[0].phase == TuningPhase::Smoke, "YOLO mainline must start with smoke");
    expect(yolo_plan[2].phase == TuningPhase::Stabilize, "YOLO mainline must end with stabilize");

    auto mobilevit_plan = build_mobilevit_mainline_plan();
    expect(mobilevit_plan.size() == 2, "MobileViT mainline plan size mismatch");
    expect(mobilevit_plan[0].track == TuningTrack::MobileViTMainline, "MobileViT mainline track mismatch");

    auto support_plan = build_segmentation_support_plan();
    expect(support_plan.size() == 3, "Segmentation support plan size mismatch");
    expect(support_plan.front().track == TuningTrack::LightweightSegmentation,
        "Segmentation support must start with lightweight segmentation");

    TuningReadiness smoke_only;
    auto ready_smoke = filter_ready_plan(plan, smoke_only);
    expect(ready_smoke.size() == 1, "Smoke-only readiness should expose one tuning step");
    expect(ready_smoke.front().name == "yolov8_smoke_gate", "Unexpected first ready tuning step");

    TuningReadiness dataset_ready;
    dataset_ready.dataset_available = true;
    dataset_ready.labels_available = true;
    auto ready_dataset = filter_ready_plan(plan, dataset_ready);
    expect(ready_dataset.size() == 7, "Dataset readiness should expose seven tuning steps");
    expect(ready_dataset.back().name == "deeplab_analysis_compare",
        "Dataset readiness should end with DeepLab analysis compare");

    TuningReadiness full_ready;
    full_ready.dataset_available = true;
    full_ready.labels_available = true;
    full_ready.external_weights_available = true;
    auto ready_full = filter_ready_plan(plan, full_ready);
    expect(ready_full.size() == plan.size(), "Full readiness should expose the entire tuning plan");

    auto yolo_specs = build_yolo_mainline_specs();
    expect(yolo_specs.size() == 3, "YOLO tuning specs size mismatch");
    for (const auto& spec : yolo_specs) {
        spec.validate();
    }
    expect(yolo_specs.front().step_name == "yolo_smoke_gate", "YOLO specs must start with smoke gate");
    expect(to_string(yolo_specs[1].metrics[1].kind) == "map50", "YOLO baseline metric mismatch");

    auto mobilevit_specs = build_mobilevit_mainline_specs();
    expect(mobilevit_specs.size() == 2, "MobileViT tuning specs size mismatch");
    for (const auto& spec : mobilevit_specs) {
        spec.validate();
    }
    expect(to_string(mobilevit_specs.front().metrics[1].kind) == "top1", "MobileViT top1 metric missing");

    auto segmentation_specs = build_segmentation_support_specs();
    expect(segmentation_specs.size() == 3, "Segmentation support specs size mismatch");
    for (const auto& spec : segmentation_specs) {
        spec.validate();
    }
    expect(to_string(segmentation_specs.front().metrics[0].kind) == "foreground_iou",
        "Segmentation support should track foreground IoU");

    auto exec_smoke = build_current_priority_execution_plan(smoke_only);
    expect(exec_smoke.size() == 1, "Smoke execution plan size mismatch");
    exec_smoke.front().validate();
    expect(exec_smoke.front().plan.name == "yolov8_smoke_gate", "Smoke execution must start with YOLO gate");
    expect(exec_smoke.front().spec.step_name == "yolo_smoke_gate", "Smoke execution spec mismatch");

    auto exec_dataset = build_current_priority_execution_plan(dataset_ready);
    expect(exec_dataset.size() == 7, "Dataset execution plan size mismatch");
    for (const auto& item : exec_dataset) {
        item.validate();
    }
    expect(exec_dataset[0].plan.track == TuningTrack::YoloMainline, "Dataset execution must start with YOLO");
    expect(exec_dataset[2].plan.track == TuningTrack::MobileViTMainline,
        "Dataset execution must include MobileViT baseline in priority order");
    expect(exec_dataset.back().plan.track == TuningTrack::SegmentationAnalysis,
        "Dataset execution must end with segmentation analysis");

    auto exec_full = build_current_priority_execution_plan(full_ready);
    expect(exec_full.size() == plan.size(), "Full execution plan size mismatch");
    expect(exec_full[6].plan.name == "resnet_backbone_compare", "Full execution backbone compare missing");

    auto smoke_batches = build_execution_batches(smoke_only);
    expect(smoke_batches.size() == 1, "Smoke batches size mismatch");
    smoke_batches.front().validate();
    expect(smoke_batches.front().name == "yolo_mainline_batch", "Smoke batch must stay on YOLO mainline");

    auto dataset_batches = build_execution_batches(dataset_ready);
    expect(dataset_batches.size() == 3, "Dataset batches size mismatch");
    for (const auto& batch : dataset_batches) {
        batch.validate();
    }
    expect(dataset_batches[0].name == "yolo_mainline_batch", "First dataset batch must be YOLO");
    expect(dataset_batches[1].name == "mobilevit_mainline_batch", "Second dataset batch must be MobileViT");
    expect(dataset_batches[2].name == "segmentation_support_batch",
        "Final dataset batch must be segmentation support");
    expect(dataset_batches[2].items.back().plan.track == TuningTrack::SegmentationAnalysis,
        "Segmentation batch must end with analysis track");

    auto smoke_sweeps = build_execution_batch_sweeps(smoke_only);
    expect(smoke_sweeps.size() == 1, "Smoke sweeps size mismatch");
    smoke_sweeps.front().validate();
    expect(smoke_sweeps.front().batch_name == "yolo_mainline_batch", "Smoke sweeps must target YOLO batch");

    auto dataset_sweeps = build_execution_batch_sweeps(dataset_ready);
    expect(dataset_sweeps.size() == 3, "Dataset sweeps size mismatch");
    for (const auto& sweep : dataset_sweeps) {
        sweep.validate();
    }
    expect(dataset_sweeps[0].sweeps[0].knob_name == "input_size", "YOLO batch sweep should start with input size");
    expect(dataset_sweeps[1].sweeps[1].knob_name == "patch_size",
        "MobileViT batch sweep should include patch size");
    expect(dataset_sweeps[2].sweeps.back().knob_name == "backbone",
        "Segmentation batch sweep should include backbone comparison");

    auto dataset_stages = build_execution_sweep_stages(dataset_ready);
    expect(dataset_stages.size() == 8, "Dataset sweep stages size mismatch");
    for (const auto& stage : dataset_stages) {
        stage.validate();
    }
    expect(dataset_stages[0].stage_name == "yolo_shape_stage", "YOLO stages must start with shape stage");
    expect(dataset_stages[1].stage_name == "yolo_schedule_stage", "YOLO schedule stage ordering mismatch");
    expect(dataset_stages[2].stage_name == "yolo_loss_stage", "YOLO loss stage ordering mismatch");
    expect(dataset_stages[3].stage_name == "mobilevit_input_stage",
        "MobileViT stages must begin after YOLO stages");
    expect(dataset_stages[5].stage_name == "mobilevit_capacity_stage",
        "MobileViT capacity stage ordering mismatch");
    expect(dataset_stages.back().stage_name == "segmentation_backbone_stage",
        "Segmentation stages must end with backbone compare");

    auto dataset_gates = build_execution_stage_gates(dataset_ready);
    expect(dataset_gates.size() == dataset_stages.size(), "Stage gates size mismatch");
    for (const auto& gate : dataset_gates) {
        gate.validate();
    }
    expect(dataset_gates[0].stage_name == "yolo_shape_stage", "First stage gate must match YOLO shape stage");
    expect(to_string(dataset_gates[1].pass_metrics[0].kind) == "map50",
        "YOLO schedule gate should track map50");
    expect(to_string(dataset_gates[3].pass_metrics[0].kind) == "top1",
        "MobileViT input gate should track top1");
    expect(to_string(dataset_gates.back().pass_metrics[0].kind) == "foreground_iou",
        "Final segmentation gate should track foreground IoU");
    expect(dataset_gates.back().stop_conditions.size() >= 2,
        "Final segmentation gate should include explicit stop conditions");

    auto dataset_outcomes = build_example_stage_outcomes(dataset_ready);
    expect(dataset_outcomes.size() == dataset_stages.size(), "Stage outcomes size mismatch");
    for (const auto& outcome : dataset_outcomes) {
        outcome.validate();
    }
    expect(dataset_outcomes[0].stage_name == "yolo_shape_stage", "First stage outcome must match YOLO shape stage");
    expect(dataset_outcomes[0].selected_knobs[0].first == "input_size",
        "YOLO shape outcome must record input size selection");
    expect(to_string(dataset_outcomes[1].metrics[0].kind) == "map50",
        "YOLO schedule outcome should record map50");
    expect(dataset_outcomes[3].selected_knobs[0].first == "input_size",
        "MobileViT input outcome should record ROI input size");
    expect(dataset_outcomes.back().selected_knobs[0].second == "mobilenet_v3_large",
        "Segmentation backbone outcome should keep the lightweight backbone example");

    auto run_reports = build_example_run_reports(dataset_ready);
    expect(run_reports.size() == 3, "Run reports size mismatch");
    for (const auto& report : run_reports) {
        report.validate();
    }
    expect(run_reports[0].track_name == "yolo_mainline", "First run report must summarize YOLO mainline");
    expect(run_reports[0].outcomes.size() == 3, "YOLO run report should contain three stage outcomes");
    expect(run_reports[1].track_name == "mobilevit_mainline",
        "Second run report must summarize MobileViT mainline");
    expect(run_reports[2].track_name == "segmentation_support",
        "Third run report must summarize segmentation support");
    expect(run_reports[2].outcomes.back().stage_name == "segmentation_backbone_stage",
        "Segmentation report must end with the backbone stage");

    auto checklist_smoke = build_mainline_execution_checklist(smoke_only);
    expect(checklist_smoke.size() == 1, "Smoke checklist should only expose the first YOLO step");
    checklist_smoke.front().validate();
    expect(checklist_smoke.front().track_name == "yolo_mainline", "Smoke checklist must stay on YOLO mainline");

    auto checklist_dataset = build_mainline_execution_checklist(dataset_ready);
    expect(checklist_dataset.size() == 6, "Dataset checklist size mismatch");
    for (const auto& item : checklist_dataset) {
        item.validate();
    }
    expect(checklist_dataset[0].stage_name == "yolo_shape_stage", "Checklist must begin with YOLO shape stage");
    expect(checklist_dataset[2].stage_name == "yolo_loss_stage", "YOLO checklist must include loss stage");
    expect(checklist_dataset[3].track_name == "mobilevit_mainline",
        "MobileViT checklist must begin after YOLO stages");
    expect(checklist_dataset.back().stage_name == "mobilevit_capacity_stage",
        "Checklist must end with MobileViT capacity stage");
    expect(checklist_dataset[1].requires_dataset, "YOLO schedule checklist step should require dataset");

    auto first_pass_matrix = build_first_pass_mainline_matrix(dataset_ready);
    expect(first_pass_matrix.size() == 13, "First-pass experiment matrix size mismatch");
    for (const auto& row : first_pass_matrix) {
        row.validate();
    }
    expect(first_pass_matrix.front().experiment_id == "yolo_shape_320",
        "First experiment should begin with the YOLO shape sweep");
    expect(first_pass_matrix[3].stage_name == "yolo_schedule_stage",
        "YOLO schedule experiments should follow the shape sweep");
    expect(first_pass_matrix[7].track_name == "mobilevit_mainline",
        "MobileViT experiments should follow YOLO experiments");
    expect(first_pass_matrix.back().experiment_id == "mobilevit_cap_768_b16",
        "Final first-pass experiment should end with MobileViT capacity");

    auto first_pass_results = build_example_first_pass_results(dataset_ready);
    expect(first_pass_results.size() == first_pass_matrix.size(), "First-pass experiment results size mismatch");
    for (const auto& result : first_pass_results) {
        result.validate();
    }
    expect(first_pass_results.front().experiment_id == "yolo_shape_320",
        "First experiment result should align with the first matrix row");
    expect(to_string(first_pass_results[3].metrics[0].kind) == "map50",
        "YOLO schedule results should record map50");
    expect(first_pass_results.back().experiment_id == "mobilevit_cap_768_b16",
        "Last experiment result should align with the final MobileViT capacity row");

    auto comparison_table = build_first_pass_comparison_table(dataset_ready);
    expect(comparison_table.size() == 6, "First-pass comparison table size mismatch");
    for (const auto& row : comparison_table) {
        row.validate();
    }
    expect(comparison_table.front().best_experiment_id == "yolo_shape_512",
        "YOLO shape comparison should select the 512 input run");
    expect(comparison_table[1].stage_name == "yolo_schedule_stage",
        "YOLO schedule comparison row missing");
    expect(comparison_table[3].best_experiment_id == "mobilevit_input_256",
        "MobileViT input comparison should select the 256 ROI run");
    expect(comparison_table.back().best_experiment_id == "mobilevit_cap_640_b32",
        "MobileViT capacity comparison should keep the 640 embedding baseline");

    auto smoke_report = build_yolo_smoke_run_report(320, 2, 2, 1.25, true);
    smoke_report.validate();
    expect(smoke_report.track_name == "yolo_mainline", "YOLO smoke run report track mismatch");
    expect(smoke_report.outcomes.size() == 1, "YOLO smoke run report should contain one outcome");
    expect(smoke_report.outcomes.front().stage_name == "yolo_train_smoke_stage",
        "YOLO smoke run report stage mismatch");
    expect(smoke_report.outcomes.front().selected_knobs[2].first == "max_train_batches",
        "YOLO smoke run report should record the smoke batch cap");
    expect(smoke_report.all_passed, "YOLO smoke run report should pass for finite loss and gradients");

    std::vector<TuningClassSummary> yolo_eval_classes = {
        {1, 2, 2, 1, 1, 1, 0.5, 0.5, 0.5},
        {3, 1, 1, 1, 0, 0, 1.0, 1.0, 1.0}
    };
    auto eval_report = build_yolo_eval_run_report(
        10.5, 0.66, 0.66, 0.66, 0.72, 2, 1, 1, 3, 3, yolo_eval_classes);
    eval_report.validate();
    expect(eval_report.run.run_name == "yolo_eval_run", "YOLO eval run report name mismatch");
    expect(eval_report.run.outcomes.front().stage_name == "yolo_eval_stage",
        "YOLO eval run report stage mismatch");
    expect(eval_report.per_class.size() == 2, "YOLO eval run report per-class size mismatch");
    expect(eval_report.per_class.front().class_id == 1, "YOLO eval run report first class mismatch");
    expect(eval_report.run.summary.find("TP=2") != std::string::npos,
        "YOLO eval run report summary should include TP count");

    auto round_report = build_yolo_mainline_round_report(smoke_report, eval_report);
    round_report.validate();
    expect(round_report.round_passed, "YOLO mainline round report should pass for valid smoke/eval reports");
    expect(round_report.summary.find("mainline round passed") != std::string::npos,
        "YOLO mainline round report summary mismatch");

    auto flat_round = flatten_yolo_mainline_round_report(round_report);
    flat_round.validate();
    expect(flat_round.run_name == "yolo_mainline_round_run", "Flattened YOLO round report name mismatch");
    expect(flat_round.outcomes.size() == 2, "Flattened YOLO round report should contain smoke and eval outcomes");
    expect(flat_round.outcomes[0].stage_name == "yolo_train_smoke_stage",
        "Flattened YOLO round report should start with smoke stage");
    expect(flat_round.outcomes[1].stage_name == "yolo_eval_stage",
        "Flattened YOLO round report should include eval stage");

    auto yolo_bundle = build_yolo_mainline_bundle(smoke_report, eval_report);
    yolo_bundle.validate();
    expect(yolo_bundle.round_report.round_passed, "YOLO mainline bundle should remain passed");
    expect(yolo_bundle.flat_run.run_name == "yolo_mainline_round_run",
        "YOLO mainline bundle flat run name mismatch");
    expect(yolo_bundle.comparison_rows.size() == 2, "YOLO mainline bundle comparison row count mismatch");
    expect(yolo_bundle.recommendation.track_name == "yolo_mainline",
        "YOLO mainline bundle recommendation track mismatch");

    auto round_rows = build_yolo_mainline_round_comparison_rows(round_report);
    expect(round_rows.size() == 2, "YOLO round comparison rows size mismatch");
    for (const auto& row : round_rows) {
        row.validate();
    }
    expect(round_rows[0].stage_name == "yolo_train_smoke_stage",
        "YOLO round comparison should start with smoke stage");
    expect(round_rows[1].best_experiment_id == "yolo_eval_current_round",
        "YOLO round comparison should expose the current eval experiment id");

    auto recommendation = build_yolo_mainline_recommendation(round_rows);
    recommendation.validate();
    expect(recommendation.track_name == "yolo_mainline", "YOLO recommendation track mismatch");
    expect(recommendation.selected_experiments.size() == 2, "YOLO recommendation size mismatch");
    expect(recommendation.selected_experiments[0].second == "yolo_smoke_current_round",
        "YOLO recommendation should select current smoke round");

}

void test_occ_tensor_bridge_minimal() {
    OccTensorBridge bridge(4);
    std::vector<float> vec = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tensor = bridge.to_tensor(vec);
    expect(tensor.sizes() == torch::IntArrayRef({1, 4}), "OCC bridge minimal tensor shape mismatch");
    expect(std::fabs(tensor[0][2].item<float>() - 3.0f) < 1e-6f, "OCC bridge minimal tensor value mismatch");
}

void test_feature_head_external_descriptors() {
    FeatureHeadConfig cfg;
    cfg.in_channels = 4;
    cfg.pooled_dim = 4;
    cfg.hidden_dim = 16;
    cfg.semantic_dim = 8;
    cfg.geometry_dim = 8;
    cfg.texture_dim = 4;
    cfg.shape_dim = 6;
    cfg.external_geometry_dim = 5;
    cfg.external_shape_dim = 3;
    cfg.use_external_geometry = true;
    cfg.use_external_shape = true;
    cfg.dropout = 0.0f;

    MultiBranchFeatureHead head(cfg);
    auto device = get_test_device();
    head->to(device);
    head->eval();

    MultiBranchFeatureInput input;
    input.feature_map = torch::randn({2, 4, 3, 3}, device);
    input.external_geometry = torch::ones({2, 5}, device);
    input.external_shape = torch::zeros({2, 3}, device);

    auto embedding = head->forward(input);
    expect(embedding.semantic.sizes() == torch::IntArrayRef({2, 8}), "semantic branch output shape mismatch");
    expect(embedding.geometry.sizes() == torch::IntArrayRef({2, 8}), "geometry branch output shape mismatch");
    expect(embedding.texture.sizes() == torch::IntArrayRef({2, 4}), "texture branch output shape mismatch");
    expect(embedding.shape.sizes() == torch::IntArrayRef({2, 6}), "shape branch output shape mismatch");
}

void test_incremental_pipeline_external_descriptors() {
    FeatureHeadConfig feature_cfg;
    feature_cfg.in_channels = 4;
    feature_cfg.pooled_dim = 4;
    feature_cfg.hidden_dim = 16;
    feature_cfg.semantic_dim = 8;
    feature_cfg.geometry_dim = 8;
    feature_cfg.texture_dim = 4;
    feature_cfg.shape_dim = 6;
    feature_cfg.external_geometry_dim = 5;
    feature_cfg.external_shape_dim = 3;
    feature_cfg.use_external_geometry = true;
    feature_cfg.use_external_shape = true;
    feature_cfg.dropout = 0.0f;

    FusionHeadConfig fusion_cfg;
    fusion_cfg.semantic_dim = feature_cfg.semantic_dim;
    fusion_cfg.geometry_dim = feature_cfg.geometry_dim;
    fusion_cfg.texture_dim = feature_cfg.texture_dim;
    fusion_cfg.shape_dim = feature_cfg.shape_dim;
    fusion_cfg.hidden_dim = 12;
    fusion_cfg.fused_dim = 10;
    fusion_cfg.num_classes = 3;
    fusion_cfg.dropout = 0.0f;

    auto device = get_test_device();
    MultiBranchFeatureHead feature_head(feature_cfg);
    MultiFeatureFusionHead fusion_head(fusion_cfg);
    feature_head->to(device);
    fusion_head->to(device);

    IncrementalFeaturePipeline pipeline{
        feature_head,
        fusion_head};

    RoiSample sample;
    sample.sample_id = "sample_0";
    sample.image_path = "synthetic.png";
    sample.class_name = "widget";
    sample.subtype_name = "line";
    sample.modality_tag = "synthetic";
    sample.feature_map = torch::randn({1, 4, 2, 2}, device);
    sample.external_geometry_descriptor = torch::ones({1, 5}, device);
    sample.external_shape_descriptor = torch::zeros({1, 3}, device);

    PipelinePrediction pred_before = pipeline.infer(sample, 3);
    expect(pred_before.fusion.class_logits.sizes() == torch::IntArrayRef({1, 3}), "pipeline class logits shape mismatch");
    expect(pred_before.topk.empty(), "prototype index should be empty before update");

    pipeline.incremental_update(sample, "proto_0", 0.9f);
    expect(pipeline.prototype_index().size() == 1, "prototype index size mismatch after update");

    PipelinePrediction pred_after = pipeline.infer(sample, 3);
    expect(!pred_after.topk.empty(), "prototype retrieval should return results after update");
    expect(pred_after.topk.front().prototype_id == "proto_0", "top retrieved prototype mismatch");
}

}

int main() {
    std::cout << "========================================\n";
    std::cout << " libtorch_module minimal validation loop\n";
    std::cout << " pure LibTorch only, no OpenCV/OCC\n";
    std::cout << "========================================\n";
    auto device = get_test_device();
    std::cout << "Device: " << (device.is_cuda() ? "CUDA" : "CPU") << std::endl;

    int failures = 0;
    failures += run_test("config_and_utils", test_config_and_utils);
    failures += run_test("occ_tensor_bridge_minimal", test_occ_tensor_bridge_minimal);
    failures += run_test("feature_head_external_descriptors", test_feature_head_external_descriptors);
    failures += run_test("incremental_pipeline_external_descriptors", test_incremental_pipeline_external_descriptors);
    failures += run_test("nnmodule_shapes", test_nnmodule_shapes);
    failures += run_test("backbone_pan_path", test_backbone_pan_path);
    failures += run_test("mobilevitv2_contract", test_mobilevitv2_contract);
    failures += run_test("mobilevit_mainline_bridge_contract", test_mobilevit_mainline_bridge_contract);
    failures += run_test("mobilenetv3_backbone_contract", test_mobilenetv3_backbone_contract);
    failures += run_test("resnet18_contract", test_resnet18_contract);
    failures += run_test("resnet50_contract", test_resnet50_contract);
    failures += run_test("deeplabv3_contract", test_deeplabv3_contract);
    failures += run_test("deeplabv3_plus_contract", test_deeplabv3_plus_contract);
    failures += run_test("segmentation_mainline_bridge_contract", test_segmentation_mainline_bridge_contract);
    failures += run_test("yolov8_detect_head_contract", test_yolov8_detect_head_contract);
    failures += run_test("yolo_postprocess_contract", test_yolo_postprocess_contract);
    failures += run_test("yolo_eval_match_summary_contract", test_yolo_eval_match_summary_contract);
    failures += run_test("assigner_and_loss", test_assigner_and_loss);
    failures += run_test("tuning_profiles_contract", test_tuning_profiles_contract);

    if (failures == 0) {
        std::cout << "\nMINIMAL VALIDATION PASSED" << std::endl;
    } else {
        std::cerr << "\nMINIMAL VALIDATION FAILURES: " << failures << std::endl;
    }

    return failures;
}
