#pragma once

#include <array>
#include <string>

namespace cx_torch_mainline {

struct CxParserPublicContract {
    std::string public_runtime_entry;
    std::array<std::string, 3> invocation_shapes;
    std::array<std::string, 4> unified_record_types;
    std::array<std::string, 4> element_chains;
};

inline CxParserPublicContract MakeCxParserPublicContract() {
    return CxParserPublicContract{
        "cxparser_ext_cxscript_cli",
        { "--script", "--script-dir", "--kind --layer --module --case" },
        {
            "UnifiedImageReviewRecord",
            "UnifiedTaskReviewBundle",
            "UnifiedCompareSlice",
            "UnifiedAnomalyFocusBundle",
        },
        { "bbox", "roi_crop", "template_alignment", "roi_diff" },
    };
}

}  // namespace cx_torch_mainline

