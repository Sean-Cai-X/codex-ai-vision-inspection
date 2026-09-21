#ifndef TORCH_LLAMA_BRIDGE_H
#define TORCH_LLAMA_BRIDGE_H


#include <sstream>
#include <string>
#include <vector>

#include "torch_prototype_index.h"

struct LlamaBridgeRequest {
    std::string image_path;
    std::string class_hint;
    std::string task_prompt;
    std::vector<PrototypeSearchResult> candidates;
};

struct LlamaBridgeResponse {
    std::string final_label;
    std::string subtype_label;
    std::string explanation;
    bool is_potential_new_class = false;
};

class LlamaBridge {
public:
    static std::string build_candidate_summary(const std::vector<PrototypeSearchResult> & candidates) {
        std::ostringstream oss;
        for (size_t i = 0; i < candidates.size(); ++i) {
            const auto & c = candidates[i];
            oss << i + 1 << ". class=" << c.class_name
                << ", subtype=" << c.subtype_name
                << ", fused=" << c.fused_score
                << ", semantic=" << c.semantic_score
                << ", geometry=" << c.geometry_score
                << ", texture=" << c.texture_score
                << ", shape=" << c.shape_score
                << "\n";
        }
        return oss.str();
    }

    static std::string build_prompt(const LlamaBridgeRequest & req) {
        std::ostringstream oss;
        oss
            << "You are assisting a visual feature classification pipeline.\n"
            << "Task: rerank the candidate classes for the ROI and explain the decision.\n"
            << "Image path: " << req.image_path << "\n";

        if (!req.class_hint.empty()) {
            oss << "Class hint: " << req.class_hint << "\n";
        }

        if (!req.task_prompt.empty()) {
            oss << "Additional task prompt: " << req.task_prompt << "\n";
        }

        oss
            << "Candidate prototypes:\n"
            << build_candidate_summary(req.candidates)
            << "Return: final_label, subtype_label, explanation, is_potential_new_class.\n";
        return oss.str();
    }
};

#endif