#ifndef TORCH_PROTOTYPE_INDEX_H
#define TORCH_PROTOTYPE_INDEX_H


#include <torch/torch.h>

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct PrototypeEntry {
    std::string prototype_id;
    std::string class_name;
    std::string subtype_name;
    std::string modality_tag;
    torch::Tensor semantic_vec;
    torch::Tensor geometry_vec;
    torch::Tensor texture_vec;
    torch::Tensor shape_vec;
    float confidence = 1.0f;
    int sample_count = 1;
};

struct PrototypeSearchResult {
    std::string prototype_id;
    std::string class_name;
    std::string subtype_name;
    float fused_score = -std::numeric_limits<float>::infinity();
    float semantic_score = 0.0f;
    float geometry_score = 0.0f;
    float texture_score = 0.0f;
    float shape_score = 0.0f;
};

struct PrototypeSearchQuery {
    torch::Tensor semantic_vec;
    torch::Tensor geometry_vec;
    torch::Tensor texture_vec;
    torch::Tensor shape_vec;
};

struct PrototypeFusionWeights {
    float semantic = 1.0f;
    float geometry = 1.0f;
    float texture = 1.0f;
    float shape = 0.5f;
};

class PrototypeIndex {
public:
    void add_or_update(const PrototypeEntry & entry) {
        _validate_entry(entry);
        auto it = entries_.find(entry.prototype_id);
        if (it == entries_.end()) {
            entries_[entry.prototype_id] = entry;
            class_to_ids_[entry.class_name].push_back(entry.prototype_id);
            return;
        }

        auto & dst = it->second;
        dst.class_name = entry.class_name;
        dst.subtype_name = entry.subtype_name;
        dst.modality_tag = entry.modality_tag;
        dst.semantic_vec = _merge_tensor(dst.semantic_vec, entry.semantic_vec, dst.sample_count, entry.sample_count);
        dst.geometry_vec = _merge_tensor(dst.geometry_vec, entry.geometry_vec, dst.sample_count, entry.sample_count);
        dst.texture_vec = _merge_tensor(dst.texture_vec, entry.texture_vec, dst.sample_count, entry.sample_count);
        dst.shape_vec = _merge_tensor(dst.shape_vec, entry.shape_vec, dst.sample_count, entry.sample_count);
        dst.confidence = std::max(dst.confidence, entry.confidence);
        dst.sample_count += entry.sample_count;
    }

    std::vector<PrototypeSearchResult> search_topk(
        const PrototypeSearchQuery & query,
        int topk = 5,
        PrototypeFusionWeights weights = {}) const {
        TORCH_CHECK(topk > 0, "PrototypeIndex.search_topk topk must be positive");
        _validate_query(query);
        _validate_weights(weights);

        std::vector<PrototypeSearchResult> results;
        results.reserve(entries_.size());

        for (const auto & kv : entries_) {
            const auto & entry = kv.second;
            PrototypeSearchResult item;
            item.prototype_id = entry.prototype_id;
            item.class_name = entry.class_name;
            item.subtype_name = entry.subtype_name;
            item.semantic_score = cosine(query.semantic_vec, entry.semantic_vec);
            item.geometry_score = cosine(query.geometry_vec, entry.geometry_vec);
            item.texture_score = cosine(query.texture_vec, entry.texture_vec);
            item.shape_score = cosine(query.shape_vec, entry.shape_vec);
            item.fused_score =
                weights.semantic * item.semantic_score +
                weights.geometry * item.geometry_score +
                weights.texture  * item.texture_score +
                weights.shape    * item.shape_score;
            results.push_back(item);
        }

        std::sort(results.begin(), results.end(), [](const auto & a, const auto & b) {
            return a.fused_score > b.fused_score;
        });

        if (static_cast<int>(results.size()) > topk) {
            results.resize(topk);
        }
        return results;
    }

    std::vector<PrototypeEntry> get_class_entries(const std::string & class_name) const {
        std::vector<PrototypeEntry> result;
        auto it = class_to_ids_.find(class_name);
        if (it == class_to_ids_.end()) {
            return result;
        }

        for (const auto & id : it->second) {
            auto eit = entries_.find(id);
            if (eit != entries_.end()) {
                result.push_back(eit->second);
            }
        }
        return result;
    }

    bool empty() const {
        return entries_.empty();
    }

    size_t size() const {
        return entries_.size();
    }

private:
    static void _validate_flat_tensor(const torch::Tensor & tensor, const char * name) {
        TORCH_CHECK(tensor.defined(), "Prototype tensor ", name, " must be defined");
        TORCH_CHECK(
            tensor.dim() == 1 || tensor.dim() == 2,
            "Prototype tensor ",
            name,
            " must be rank-1 or rank-2, got ",
            tensor.sizes());
        if (tensor.dim() == 2) {
            TORCH_CHECK(
                tensor.size(0) == 1,
                "Prototype tensor ",
                name,
                " rank-2 form must have batch size 1, got ",
                tensor.sizes());
        }
        TORCH_CHECK(tensor.numel() > 0, "Prototype tensor ", name, " must not be empty");
    }

    static int64_t _flattened_width(const torch::Tensor & tensor) {
        return tensor.flatten().size(0);
    }

    static void _validate_entry(const PrototypeEntry & entry) {
        TORCH_CHECK(!entry.prototype_id.empty(), "PrototypeEntry.prototype_id must not be empty");
        TORCH_CHECK(!entry.class_name.empty(), "PrototypeEntry.class_name must not be empty");
        TORCH_CHECK(entry.confidence >= 0.0f, "PrototypeEntry.confidence must be non-negative");
        TORCH_CHECK(entry.sample_count > 0, "PrototypeEntry.sample_count must be positive");

        _validate_flat_tensor(entry.semantic_vec, "semantic_vec");
        _validate_flat_tensor(entry.geometry_vec, "geometry_vec");
        _validate_flat_tensor(entry.texture_vec, "texture_vec");
        _validate_flat_tensor(entry.shape_vec, "shape_vec");
    }

    static void _validate_query(const PrototypeSearchQuery & query) {
        _validate_flat_tensor(query.semantic_vec, "semantic_vec");
        _validate_flat_tensor(query.geometry_vec, "geometry_vec");
        _validate_flat_tensor(query.texture_vec, "texture_vec");
        _validate_flat_tensor(query.shape_vec, "shape_vec");
    }

    static void _validate_weights(const PrototypeFusionWeights & weights) {
        TORCH_CHECK(weights.semantic >= 0.0f, "PrototypeFusionWeights.semantic must be non-negative");
        TORCH_CHECK(weights.geometry >= 0.0f, "PrototypeFusionWeights.geometry must be non-negative");
        TORCH_CHECK(weights.texture >= 0.0f, "PrototypeFusionWeights.texture must be non-negative");
        TORCH_CHECK(weights.shape >= 0.0f, "PrototypeFusionWeights.shape must be non-negative");
    }

    static torch::Tensor _merge_tensor(
        const torch::Tensor & lhs,
        const torch::Tensor & rhs,
        int lhs_count,
        int rhs_count) {
        if (!lhs.defined()) return rhs.detach().clone();
        if (!rhs.defined()) return lhs.detach().clone();
        TORCH_CHECK(
            _flattened_width(lhs) == _flattened_width(rhs),
            "Prototype tensor width mismatch during merge. lhs=",
            lhs.sizes(),
            " rhs=",
            rhs.sizes());
        const auto total = static_cast<float>(lhs_count + rhs_count);
        return ((lhs * lhs_count) + (rhs * rhs_count)) / total;
    }

    static float cosine(const torch::Tensor & a, const torch::Tensor & b) {
        if (!a.defined() || !b.defined()) {
            return 0.0f;
        }

        auto a1 = a.flatten().to(torch::kFloat32).cpu();
        auto b1 = b.flatten().to(torch::kFloat32).cpu();
        TORCH_CHECK(
            a1.numel() == b1.numel(),
            "Prototype cosine width mismatch. lhs=",
            a.sizes(),
            " rhs=",
            b.sizes());
        auto dot = (a1 * b1).sum().item<float>();
        auto na = torch::norm(a1).item<float>();
        auto nb = torch::norm(b1).item<float>();
        if (na <= 0.0f || nb <= 0.0f) {
            return 0.0f;
        }
        return dot / (na * nb);
    }

    std::unordered_map<std::string, PrototypeEntry> entries_;
    std::unordered_map<std::string, std::vector<std::string>> class_to_ids_;
};

#endif
