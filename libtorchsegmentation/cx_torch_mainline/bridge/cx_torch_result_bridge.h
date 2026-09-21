#pragma once

#include <string>
#include <vector>

namespace cx_torch_mainline {

struct CxTorchElementRef {
    std::string id;
    std::string kind;
    std::string source_ref;
    std::string status;
    std::string finding;
};

struct CxTorchChainRef {
    std::string name;
    std::string status;
    std::vector<std::string> element_ids;
    std::string summary;
};

struct CxTorchUnifiedReviewDraft {
    std::string source_thread;
    std::string case_name;
    std::string image_id;
    std::string stage;
    std::string input_image_ref;
    std::string primary_visual_ref;
    std::string status;
    std::string element_summary;
    std::string element_chain_summary;
    std::vector<std::string> visualization_refs;
    std::vector<CxTorchElementRef> elements;
    std::vector<CxTorchChainRef> element_chains;
};

inline CxTorchElementRef MakeElementRef(
    const std::string& id,
    const std::string& kind,
    const std::string& source_ref,
    const std::string& status,
    const std::string& finding) {
    return CxTorchElementRef{id, kind, source_ref, status, finding};
}

inline CxTorchChainRef MakeChainRef(
    const std::string& name,
    const std::string& status,
    const std::vector<std::string>& element_ids,
    const std::string& summary) {
    return CxTorchChainRef{name, status, element_ids, summary};
}

}  // namespace cx_torch_mainline
