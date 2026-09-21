#include <torch/torch.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "torch_occ_bridge.h"
#include "torch_task_routing.h"
#include "torch_feature_head.h"
#include "torch_fusion_head.h"
#include "torch_incremental_pipeline.h"
#include "torch_prototype_index.h"

namespace {

torch::Device get_test_device() {
    if (const char* use_cuda = std::getenv("LIBTORCH_MODULE_USE_CUDA")) {
        if (std::string(use_cuda) == "1" && torch::cuda::is_available()) {
            return torch::Device(torch::kCUDA);
        }
    }
    return torch::Device(torch::kCPU);
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

FeatureHeadConfig make_feature_cfg() {
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
    cfg.l2_normalize = true;
    return cfg;
}

FusionHeadConfig make_fusion_cfg() {
    FusionHeadConfig cfg;
    cfg.semantic_dim = 8;
    cfg.geometry_dim = 8;
    cfg.texture_dim = 4;
    cfg.shape_dim = 6;
    cfg.hidden_dim = 12;
    cfg.fused_dim = 10;
    cfg.num_classes = 3;
    cfg.dropout = 0.0f;
    return cfg;
}

void test_occ_tensor_bridge_contract() {
    OccTensorBridge bridge(5);

    std::vector<float> vec = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    auto tensor = bridge.to_tensor(vec);

    expect(tensor.sizes() == torch::IntArrayRef({1, 5}), "bridge tensor shape mismatch");
    expect(std::fabs(tensor[0][4].item<float>() - 0.5f) < 1e-6f, "bridge tensor value mismatch");

    std::vector<float> empty_vec;
    auto zero_tensor = bridge.to_tensor(empty_vec);
    expect(zero_tensor.sizes() == torch::IntArrayRef({1, 5}), "bridge zero fallback shape mismatch");
    expect(zero_tensor.sum().item<float>() == 0.0f, "bridge zero fallback should sum to zero");
}

void test_feature_head_base_path() {
    auto device = get_test_device();
    MultiBranchFeatureHead head(make_feature_cfg());
    head->to(device);
    head->eval();

    auto feature_map = torch::randn({2, 4, 3, 3}, device);
    auto embedding = head->forward(feature_map);

    expect(embedding.semantic.sizes() == torch::IntArrayRef({2, 8}), "semantic branch output shape mismatch");
    expect(embedding.geometry.sizes() == torch::IntArrayRef({2, 8}), "geometry branch output shape mismatch");
    expect(embedding.texture.sizes() == torch::IntArrayRef({2, 4}), "texture branch output shape mismatch");
    expect(embedding.shape.sizes() == torch::IntArrayRef({2, 6}), "shape branch output shape mismatch");
}

void test_feature_head_external_descriptors() {
    auto device = get_test_device();
    MultiBranchFeatureHead head(make_feature_cfg());
    head->to(device);
    head->eval();

    MultiBranchFeatureInput input;
    input.feature_map = torch::randn({2, 4, 3, 3}, device);
    input.external_geometry = torch::ones({2, 5}, device);
    input.external_shape = torch::zeros({2, 3}, device);

    auto embedding = head->forward(input);
    expect(embedding.geometry.sizes() == torch::IntArrayRef({2, 8}), "geometry branch output shape mismatch with external descriptor");
    expect(embedding.shape.sizes() == torch::IntArrayRef({2, 6}), "shape branch output shape mismatch with external descriptor");

    auto geometry_norm = torch::norm(embedding.geometry, 2, 1);
    auto shape_norm = torch::norm(embedding.shape, 2, 1);
    expect(torch::allclose(geometry_norm, torch::ones_like(geometry_norm), 1e-4, 1e-4), "geometry branch should remain L2 normalized");
    expect(torch::allclose(shape_norm, torch::ones_like(shape_norm), 1e-4, 1e-4), "shape branch should remain L2 normalized");
}

void test_feature_head_invalid_config_rejected() {
    FeatureHeadConfig cfg = make_feature_cfg();
    cfg.use_external_geometry = true;
    cfg.external_geometry_dim = 0;

    bool threw = false;
    try {
        MultiBranchFeatureHead head(cfg);
        (void)head;
    } catch (const c10::Error&) {
        threw = true;
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw, "invalid feature-head config should be rejected");
}

MultiBranchEmbedding make_test_embedding(const torch::Device& device) {
    MultiBranchEmbedding embedding;
    embedding.semantic = torch::randn({1, 8}, device);
    embedding.geometry = torch::randn({1, 8}, device);
    embedding.texture = torch::randn({1, 4}, device);
    embedding.shape = torch::randn({1, 6}, device);
    return embedding;
}

void test_fusion_head_contract() {
    auto device = get_test_device();
    MultiFeatureFusionHead fusion(make_fusion_cfg());
    fusion->to(device);
    fusion->eval();

    auto out = fusion->forward(make_test_embedding(device));
    expect(out.fused_embedding.sizes() == torch::IntArrayRef({1, 10}), "fusion embedding shape mismatch");
    expect(out.class_logits.sizes() == torch::IntArrayRef({1, 3}), "fusion logits shape mismatch");
}

void test_fusion_head_invalid_embedding_rejected() {
    auto device = get_test_device();
    MultiFeatureFusionHead fusion(make_fusion_cfg());
    fusion->to(device);
    fusion->eval();

    auto bad = make_test_embedding(device);
    bad.shape = torch::randn({1, 5}, device);

    bool threw = false;
    try {
        (void)fusion->forward(bad);
    } catch (const c10::Error&) {
        threw = true;
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw, "invalid fusion embedding should be rejected");
}

void test_prototype_index_contract() {
    PrototypeIndex index;

    PrototypeEntry entry;
    entry.prototype_id = "proto_0";
    entry.class_name = "widget";
    entry.subtype_name = "line";
    entry.modality_tag = "contract";
    entry.semantic_vec = torch::tensor({1.0f, 0.0f});
    entry.geometry_vec = torch::tensor({0.0f, 1.0f});
    entry.texture_vec = torch::tensor({1.0f, 1.0f});
    entry.shape_vec = torch::tensor({0.5f, 0.5f});
    index.add_or_update(entry);

    PrototypeSearchQuery query;
    query.semantic_vec = torch::tensor({1.0f, 0.0f});
    query.geometry_vec = torch::tensor({0.0f, 1.0f});
    query.texture_vec = torch::tensor({1.0f, 1.0f});
    query.shape_vec = torch::tensor({0.5f, 0.5f});

    auto topk = index.search_topk(query, 1);
    expect(topk.size() == 1, "prototype search should return one result");
    expect(topk.front().prototype_id == "proto_0", "prototype search returned unexpected id");
}

void test_prototype_index_invalid_entry_rejected() {
    PrototypeIndex index;

    PrototypeEntry bad_entry;
    bad_entry.prototype_id = "";
    bad_entry.class_name = "widget";
    bad_entry.semantic_vec = torch::tensor({1.0f, 0.0f});
    bad_entry.geometry_vec = torch::tensor({0.0f, 1.0f});
    bad_entry.texture_vec = torch::tensor({1.0f, 1.0f});
    bad_entry.shape_vec = torch::tensor({0.5f, 0.5f});

    bool threw = false;
    try {
        index.add_or_update(bad_entry);
    } catch (const c10::Error&) {
        threw = true;
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw, "invalid prototype entry should be rejected");
}

void test_incremental_pipeline_contract() {
    auto device = get_test_device();
    auto feature_head = MultiBranchFeatureHead(make_feature_cfg());
    auto fusion_head = MultiFeatureFusionHead(make_fusion_cfg());
    feature_head->to(device);
    fusion_head->to(device);

    IncrementalFeaturePipeline pipeline{
        feature_head,
        fusion_head
    };

    RoiSample sample;
    sample.sample_id = "sample_0";
    sample.image_path = "synthetic.png";
    sample.class_name = "widget";
    sample.subtype_name = "line";
    sample.modality_tag = "contract_smoke";
    sample.feature_map = torch::randn({1, 4, 2, 2}, device);
    sample.external_geometry_descriptor = torch::ones({1, 5}, device);
    sample.external_shape_descriptor = torch::zeros({1, 3}, device);

    auto pred_before = pipeline.infer(sample, 3);
    expect(pred_before.fusion.class_logits.sizes() == torch::IntArrayRef({1, 3}), "pipeline class-logit shape mismatch");
    expect(pred_before.topk.empty(), "prototype index should be empty before update");

    pipeline.incremental_update(sample, "proto_0", 0.9f);
    expect(pipeline.prototype_index().size() == 1, "prototype index should contain one entry after update");

    auto pred_after = pipeline.infer(sample, 3);
    expect(!pred_after.topk.empty(), "prototype retrieval should return results after update");
    expect(pred_after.topk.front().prototype_id == "proto_0", "unexpected top retrieved prototype id");
}

void test_incremental_pipeline_invalid_sample_rejected() {
    auto device = get_test_device();
    auto feature_head = MultiBranchFeatureHead(make_feature_cfg());
    auto fusion_head = MultiFeatureFusionHead(make_fusion_cfg());
    feature_head->to(device);
    fusion_head->to(device);

    IncrementalFeaturePipeline pipeline{
        feature_head,
        fusion_head
    };

    RoiSample bad_sample;
    bad_sample.feature_map = torch::randn({1, 4, 2}, device);

    bool threw = false;
    try {
        (void)pipeline.infer(bad_sample, 3);
    } catch (const c10::Error&) {
        threw = true;
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw, "invalid pipeline sample should be rejected");
}

void test_backend_route_prefers_mlpack_for_geometry_tasks() {
    TorchTaskBoundary boundary;
    boundary.kind = TorchTaskKind::GeometryClustering;
    boundary.geometry_or_descriptor_only = true;
    boundary.batch_size = 16;
    boundary.feature_dim = 32;

    const auto decision = route_torch_task(boundary);
    expect(decision.route == TorchBackendRoute::Mlpack, "geometry-only clustering should route to mlpack");
}

void test_backend_route_prefers_torch_for_visual_learning_tasks() {
    TorchTaskBoundary boundary;
    boundary.kind = TorchTaskKind::VisualDetection;
    boundary.has_image_tensor = true;
    boundary.has_dense_feature_tensor = true;
    boundary.requires_gradient_training = true;
    boundary.requires_end_to_end_learning = true;
    boundary.batch_size = 4;
    boundary.num_classes = 3;

    const auto decision = route_torch_task(boundary);
    expect(decision.route == TorchBackendRoute::Torch, "visual detection should route to torch");
}

void test_backend_route_requires_explicit_review_for_mixed_tasks() {
    TorchTaskBoundary boundary;
    boundary.kind = TorchTaskKind::GeometryMatching;
    boundary.has_dense_feature_tensor = true;
    boundary.geometry_or_descriptor_only = true;
    boundary.requires_gradient_training = true;

    const auto decision = route_torch_task(boundary);
    expect(decision.route == TorchBackendRoute::ManualReview, "mixed geometry and trainable tensor task should require review");
}

void test_minimal_train_loop_io_contract() {
    auto device = get_test_device();
    torch::nn::Linear head(6, 3);
    head->to(device);

    torch::optim::SGD optimizer(head->parameters(), torch::optim::SGDOptions(0.05));

    TorchTrainIO io;
    io.features = torch::randn({4, 6}, device);
    io.labels = torch::tensor({0, 1, 2, 1}, torch::TensorOptions().dtype(torch::kLong).device(device));

    const auto result = run_minimal_torch_train_step(head, optimizer, io);
    expect(result.batch_size == 4, "train loop batch size mismatch");
    expect(result.num_classes == 3, "train loop class count mismatch");
    expect(result.logits.sizes() == torch::IntArrayRef({4, 3}), "train loop logits shape mismatch");
    expect(result.loss.defined(), "train loop loss should be defined");
    expect(torch::isfinite(result.loss).item<bool>(), "train loop loss should be finite");
}

void test_minimal_infer_loop_io_contract() {
    auto device = get_test_device();
    torch::nn::Linear head(6, 4);
    head->to(device);

    TorchInferIO io;
    io.features = torch::randn({2, 6}, device);
    io.topk = 2;

    const auto result = run_minimal_torch_infer(head, io);
    expect(result.logits.sizes() == torch::IntArrayRef({2, 4}), "infer loop logits shape mismatch");
    expect(result.scores.sizes() == torch::IntArrayRef({2, 2}), "infer loop score shape mismatch");
    expect(result.labels.sizes() == torch::IntArrayRef({2, 2}), "infer loop label shape mismatch");
    expect(torch::all(result.scores >= 0.0f).item<bool>(), "infer scores must be non-negative");
    expect(torch::all(result.scores <= 1.0f).item<bool>(), "infer scores must be bounded");
}

}

int main() {
    auto device = get_test_device();

    std::cout << "========================================\n";
    std::cout << " libtorch_module contract smoke\n";
    std::cout << " feature head + pipeline + backend routing\n";
    std::cout << " torch handles learned tensor tasks, mlpack handles structured geometry/numeric tasks\n";
    std::cout << " OpenCV/OCC deferred to next stage\n";
    std::cout << "========================================\n";
    std::cout << "Device: " << (device.is_cuda() ? "CUDA" : "CPU") << std::endl;

    int failures = 0;
    failures += run_test("occ_tensor_bridge_contract", test_occ_tensor_bridge_contract);
    failures += run_test("feature_head_base_path", test_feature_head_base_path);
    failures += run_test("feature_head_external_descriptors", test_feature_head_external_descriptors);
    failures += run_test("feature_head_invalid_config_rejected", test_feature_head_invalid_config_rejected);
    failures += run_test("fusion_head_contract", test_fusion_head_contract);
    failures += run_test("fusion_head_invalid_embedding_rejected", test_fusion_head_invalid_embedding_rejected);
    failures += run_test("prototype_index_contract", test_prototype_index_contract);
    failures += run_test("prototype_index_invalid_entry_rejected", test_prototype_index_invalid_entry_rejected);
    failures += run_test("incremental_pipeline_contract", test_incremental_pipeline_contract);
    failures += run_test("incremental_pipeline_invalid_sample_rejected", test_incremental_pipeline_invalid_sample_rejected);
    failures += run_test("backend_route_prefers_mlpack_for_geometry_tasks", test_backend_route_prefers_mlpack_for_geometry_tasks);
    failures += run_test("backend_route_prefers_torch_for_visual_learning_tasks", test_backend_route_prefers_torch_for_visual_learning_tasks);
    failures += run_test("backend_route_requires_explicit_review_for_mixed_tasks", test_backend_route_requires_explicit_review_for_mixed_tasks);
    failures += run_test("minimal_train_loop_io_contract", test_minimal_train_loop_io_contract);
    failures += run_test("minimal_infer_loop_io_contract", test_minimal_infer_loop_io_contract);

    if (failures == 0) {
        std::cout << "\nCONTRACT SMOKE PASSED" << std::endl;
    } else {
        std::cerr << "\nCONTRACT SMOKE FAILURES: " << failures << std::endl;
    }

    return failures;
}
