#pragma once

#include <cstdint>
#include <string>

class ParserDebugBridge;

struct CxFastMatchRuntimeCapture
{
    bool object_found = false;
    bool object_unique = false;

    std::uintptr_t object_address = 0;
    std::uintptr_t local_image_address = 0;
    std::uintptr_t global_image_address = 0;
    std::uintptr_t last_learn_argument = 0;

    int image_rows = 0;
    int image_cols = 0;
    int image_type = -1;
    bool image_empty = true;

    int object_model_point_count = 0;
    int object_learn_a_count = 0;
    int object_learn_b_count = 0;
    int object_learn_a2_count = 0;
    int object_learn_b2_count = 0;
    int object_learn_status_code = 0;
    int object_normal_trace_candidate_count = 0;
    int object_normal_trace_deduplicated_count = 0;
    int object_normal_trace_point_count = 0;
    int object_normal_trace_pair_count = 0;

    bool global_model_found = false;
    bool global_learn_a_found = false;
    bool global_learn_b_found = false;
    bool global_learn_status_found = false;

    int global_model_point_count = 0;
    int global_learn_a_count = 0;
    int global_learn_b_count = 0;
    int global_learn_status_code = 0;

    std::string object_aliases;
    std::string failure_stage;
    std::string reason;
};

bool CaptureFastMatchRuntime(
    ParserDebugBridge& bridge,
    const std::string& object_name,
    CxFastMatchRuntimeCapture& capture);
