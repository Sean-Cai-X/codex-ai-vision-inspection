#include "OfflineFeedbackValidation.h"
#include <cmath>
#include <regex>
#include <set>
#include <stdexcept>
#include <vector>

namespace vision_ai::offline_feedback {
namespace {
void Require(bool value, const char* code) {
    if (!value) throw std::invalid_argument(code);
}
bool Type(const Json& j, const std::string& t) {
    if (t == "object") return j.is_object();
    if (t == "array") return j.is_array();
    if (t == "string") return j.is_string();
    if (t == "boolean") return j.is_boolean();
    if (t == "null") return j.is_null();
    if (t == "integer") return j.is_number_integer();
    if (t == "number") return j.is_number();
    throw std::invalid_argument("FEEDBACK_SCHEMA_UNSUPPORTED_TYPE");
}
void Shape(const Json& j, const Json& s, const Json& contract, int depth = 0) {
    Require(depth <= 32, "FEEDBACK_DEPTH_LIMIT");
    if (s.contains("$ref")) {
        const auto ref = s.at("$ref").get<std::string>();
        Require(ref.rfind("#/$defs/", 0) == 0, "FEEDBACK_EXTERNAL_SCHEMA_REF");
        Shape(j, contract.at("$defs").at(ref.substr(8)), contract, depth + 1);
        return;
    }
    if (s.contains("type")) {
        bool valid = false;
        if (s.at("type").is_array()) {
            for (const auto& t : s.at("type")) valid = valid || Type(j, t.get<std::string>());
        } else valid = Type(j, s.at("type").get<std::string>());
        Require(valid, "FEEDBACK_TYPE_INVALID");
    }
    if (s.contains("const")) Require(j == s.at("const"), "FEEDBACK_CONST_INVALID");
    if (s.contains("enum")) {
        bool valid = false;
        for (const auto& v : s.at("enum")) valid = valid || v == j;
        Require(valid, "FEEDBACK_ENUM_INVALID");
    }
    if (j.is_object()) {
        for (const auto& key : s.value("required", Json::array()))
            Require(j.contains(key.get<std::string>()), "FEEDBACK_FIELD_MISSING");
        const auto props = s.value("properties", Json::object());
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (props.contains(it.key())) Shape(it.value(), props.at(it.key()), contract, depth + 1);
            else Require(s.value("additionalProperties", true), "FEEDBACK_FIELD_NOT_ALLOWED");
        }
    } else if (j.is_array()) {
        if (s.contains("minItems")) Require(j.size() >= s.at("minItems").get<std::size_t>(), "FEEDBACK_ARRAY_SIZE");
        if (s.contains("maxItems")) Require(j.size() <= s.at("maxItems").get<std::size_t>(), "FEEDBACK_ARRAY_SIZE");
        if (s.contains("items")) for (const auto& v : j) Shape(v, s.at("items"), contract, depth + 1);
    } else if (j.is_string()) {
        const auto text = j.get<std::string>();
        if (s.contains("minLength")) Require(text.size() >= s.at("minLength").get<std::size_t>(), "FEEDBACK_TEXT_LENGTH");
        if (s.contains("maxLength")) Require(text.size() <= s.at("maxLength").get<std::size_t>(), "FEEDBACK_TEXT_LENGTH");
        if (s.contains("pattern")) Require(std::regex_match(text, std::regex(s.at("pattern").get<std::string>())), "FEEDBACK_TEXT_FORMAT");
    } else if (j.is_number()) {
        const auto value = j.get<double>();
        Require(std::isfinite(value), "FEEDBACK_NONFINITE");
        if (s.contains("minimum")) Require(value >= s.at("minimum").get<double>(), "FEEDBACK_NUMBER_RANGE");
        if (s.contains("maximum")) Require(value <= s.at("maximum").get<double>(), "FEEDBACK_NUMBER_RANGE");
    }
}
void Privacy(const Json& j) {
    if (j.is_string()) {
        // Conservative guard, not a complete DLP classifier. Human export preview remains required.
        const auto text = j.get<std::string>();
        static const std::regex path("([A-Za-z]:[\\\\/]|\\\\\\\\|(^|[[:space:]])/|[A-Za-z][A-Za-z0-9+.-]*://|data:|base64)", std::regex::icase);
        Require(!std::regex_search(text, path), "FEEDBACK_PRIVATE_TEXT");
    } else if (j.is_structured()) for (const auto& v : j) Privacy(v);
}
bool Known(const Json& j, const char* field) { return !j.at(field).is_null(); }
void Counts(const Json& d, const Json& parameters, const Json& classCounts, const Json& selectedCandidates) {
    Require(d.at("detection_cap") == parameters.at("max_detections"), "FEEDBACK_CAP_PARAMETER_MISMATCH");
    const auto equation = [&](const char* total, const char* a, const char* b) {
        if (Known(d,total) && Known(d,a) && Known(d,b))
            Require(d.at(total).get<long long>() == d.at(a).get<long long>() + d.at(b).get<long long>(), "FEEDBACK_COUNT_INCONSISTENT");
    };
    equation("above_threshold_count","examined_count","unexamined_count");
    equation("examined_count","suppressed_count","selected_before_mask_count");
    equation("selected_before_mask_count","returned_count","mask_rejected_count");
    if (Known(d,"selected_before_mask_count")) Require(d.at("selected_before_mask_count") <= d.at("detection_cap"), "FEEDBACK_COUNT_INCONSISTENT");
    if ((Known(d,"unexamined_count") && d.at("unexamined_count").get<long long>() > 0) || (Known(d,"cap_hit") && d.at("cap_hit") == true))
        Require(!Known(d,"full_post_nms_count"), "FEEDBACK_TRUNCATION_HAS_FULL_COUNT");
    if (Known(d,"cap_hit") && d.at("cap_hit") == true && Known(d,"selected_before_mask_count"))
        Require(d.at("selected_before_mask_count") == d.at("detection_cap"), "FEEDBACK_CAP_HIT_INCONSISTENT");
    if (Known(d,"full_post_nms_count") && Known(d,"selected_before_mask_count"))
        Require(d.at("full_post_nms_count") == d.at("selected_before_mask_count"), "FEEDBACK_FULL_COUNT_INCONSISTENT");
    if (!classCounts.is_null()) {
        long long total = 0;
        for (const auto& c : classCounts) total += c.get<long long>();
        if (Known(d,"returned_count")) Require(total == d.at("returned_count").get<long long>(), "FEEDBACK_CLASS_COUNT_MISMATCH");
        std::vector<long long> selected(7,0);
        for (const auto& c : selectedCandidates) ++selected[c.at("class_id").get<std::size_t>()];
        for (std::size_t i = 0; i < 7; ++i) Require(selected[i] <= classCounts.at(i).get<long long>(), "FEEDBACK_SELECTED_COUNT_MISMATCH");
    }
}
void Issue(const Json& j) {
    const auto& ann = j.at("annotation");
    const bool confirmed = ann.at("state") == "confirmed";
    Require(!confirmed || (Known(ann, "sha256") && Known(ann, "version")), "FEEDBACK_ANNOTATION_BINDING_REQUIRED");
    const bool geometryTruth = confirmed && j.at("business_target").at("presence") == "present";
    for (const auto* field : {"best_target_iou", "best_any_class_iou", "center_error_pixels", "boundary_distance_pixels"})
        Require(geometryTruth || j.at("metrics").at(field).is_null(), "FEEDBACK_GT_METRIC_WITHOUT_TRUTH");
    for (const auto* field : {"unmatched_gt_count", "unmatched_prediction_count"})
        Require((confirmed && j.at("business_target").at("presence") != "undecidable") || j.at("metrics").at(field).is_null(), "FEEDBACK_GT_METRIC_WITHOUT_TRUTH");
    const auto& exec = j.at("execution");
    if (exec.at("status") == "SUCCEEDED")
        Require(!Known(exec, "error_code") && !Known(exec, "failure_stage"), "FEEDBACK_SUCCESS_HAS_FAILURE");
    else {
        Require(Known(exec, "error_code") && Known(exec, "failure_stage"), "FEEDBACK_FAILURE_DIAGNOSTIC_REQUIRED");
        Require(j.at("issue_type") == "execution_failure", "FEEDBACK_FAILED_EXECUTION_NOT_QUALITY");
        Require(j.at("raw_class_counts").is_null() && j.at("selected_candidates").empty(), "FEEDBACK_FAILED_EXECUTION_HAS_RESULTS");
        for (const auto& v : j.at("metrics").items())
            if (v.key() != "metric_version") Require(v.value().is_null(), "FEEDBACK_FAILED_EXECUTION_HAS_RESULTS");
    }
    std::set<std::string> ids;
    for (const auto& c : j.at("selected_candidates")) {
        Require(ids.insert(c.at("candidate_id").get<std::string>()).second, "FEEDBACK_DUPLICATE_CANDIDATE_ID");
        const auto& b = c.at("bounds");
        Require(b.at(2).get<double>() >= b.at(0).get<double>() && b.at(3).get<double>() >= b.at(1).get<double>(), "FEEDBACK_BOUNDS_INVALID");
    }
    std::set<std::string> matches;
    const auto& assoc = j.at("association");
    for (const auto& id : assoc.at("candidate_ids"))
        Require(ids.count(id.get<std::string>()) && matches.insert(id.get<std::string>()).second, "FEEDBACK_ASSOCIATION_CANDIDATE_INVALID");
    if (assoc.at("source") == "automatic_suggestion")
        Require(Known(assoc, "rule") && Known(assoc, "threshold"), "FEEDBACK_SUGGESTION_RULE_REQUIRED");
    if (assoc.at("source") == "none")
        Require(assoc.at("candidate_ids").empty() && !Known(assoc, "annotation_ref") && !Known(assoc, "rule") && !Known(assoc, "threshold"), "FEEDBACK_NONE_HAS_ASSOCIATION");
    if (Known(assoc, "annotation_ref"))
        Require(confirmed, "FEEDBACK_ASSOCIATION_WITHOUT_ANNOTATION");
    Counts(j.at("diagnostics"),j.at("parameters"),j.at("raw_class_counts"),j.at("selected_candidates"));
}
}
Json ParseStrict(const std::string& bytes) {
    Require(bytes.size() <= 1024 * 1024, "FEEDBACK_FILE_LIMIT");
    std::vector<std::set<std::string>> keys;
    try { return Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& parsed) {
        Require(depth <= 32, "FEEDBACK_DEPTH_LIMIT");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        if (event == Json::parse_event_t::key)
            Require(!keys.empty() && keys.back().insert(parsed.get<std::string>()).second, "FEEDBACK_DUPLICATE_JSON_KEY");
        if (event == Json::parse_event_t::object_end) keys.pop_back();
        return true;
    }); } catch (const Json::exception&) {
        // nlohmann parse errors can echo private input snippets. Return a code only.
        throw std::invalid_argument("FEEDBACK_JSON_INVALID");
    }
}
void ValidateRecord(const Json& record, const Json& contract, const std::string& kind) {
    try {
    Require(kind == "issue" || kind == "disposition" || kind == "repair" || kind == "inference_diagnostics", "FEEDBACK_KIND_INVALID");
    Require(contract.at("$id") == "visionai.business.offline_feedback_contract.v1", "FEEDBACK_CONTRACT_VERSION_INVALID");
    Shape(record, contract.at("$defs").at(kind), contract);
    Privacy(record);
    if (kind == "issue") Issue(record);
    if (kind == "inference_diagnostics") {
        Counts(record.at("counts"),record.at("parameters"),record.at("raw_class_counts"),Json::array());
        Require(!record.at("raw_class_counts").is_null(), "FEEDBACK_RUNTIME_CLASS_COUNTS_REQUIRED");
        for (const auto& field : record.at("counts").items())
            if (field.key() != "full_post_nms_count") Require(!field.value().is_null(), "FEEDBACK_RUNTIME_COUNT_REQUIRED");
        Require(record.at("mask_rejection_reasons").at("contour_fewer_than_three_points") == record.at("counts").at("mask_rejected_count"), "FEEDBACK_MASK_REASON_COUNT_MISMATCH");
        const auto& t = record.at("transform"); const auto& input = record.at("input");
        Require(t.at("resized_width").get<long long>() + 2*t.at("pad_x").get<long long>() <= input.at("width").get<long long>() &&
            t.at("resized_height").get<long long>() + 2*t.at("pad_y").get<long long>() <= input.at("height").get<long long>(), "FEEDBACK_TRANSFORM_INVALID");
    }
    if (kind == "disposition") {
        if (record.at("status") == "needs_diagnostics")
            Require(!record.at("requested_diagnostics").empty(), "FEEDBACK_DIAGNOSTICS_REQUIRED");
        else Require(record.at("requested_diagnostics").empty(), "FEEDBACK_UNEXPECTED_DIAGNOSTICS");
        if (record.at("status") == "reproduced")
            Require(record.at("evidence_kind") == "rnd_synthetic_reproduction" && Known(record, "evidence_sha256"), "FEEDBACK_REPRODUCTION_EVIDENCE_REQUIRED");
        if (record.at("status") == "fix_pending_retest")
            Require(Known(record, "repair_lock_sha256") && Known(record, "evidence_sha256") && record.at("evidence_kind") == "release_validation", "FEEDBACK_REPAIR_EVIDENCE_REQUIRED");
        else Require(!Known(record, "repair_lock_sha256"), "FEEDBACK_UNEXPECTED_REPAIR");
    }
    if (kind == "repair") {
        Require(record.at("training_supported") == false || Known(record, "training_acceptance_sha256"), "FEEDBACK_TRAINING_EVIDENCE_REQUIRED");
        std::set<std::string> issues;
        for (const auto& binding : record.at("issue_bindings"))
            Require(issues.insert(binding.at("issue_id").get<std::string>()).second, "FEEDBACK_DUPLICATE_ISSUE_BINDING");
    }
    } catch (const Json::exception&) {
        throw std::invalid_argument("FEEDBACK_CONTRACT_INVALID");
    } catch (const std::regex_error&) {
        throw std::invalid_argument("FEEDBACK_CONTRACT_INVALID");
    }
}
Json ExtractInferenceDiagnostics(const std::string& bytes, const Json& contract) {
    const auto root = ParseStrict(bytes);
    Require(root.is_object(), "FEEDBACK_RUNTIME_RESULT_INVALID");
    if (!root.contains("diagnostics")) return nullptr;
    const auto diagnostic = root.at("diagnostics");
    ValidateRecord(diagnostic,contract,"inference_diagnostics");
    return diagnostic;
}
void ValidateLinks(const Json& issue, const std::string& issueSha, const Json& disposition, const Json* repair) {
    Require(disposition.at("issue_id") == issue.at("issue_id") && disposition.at("issue_sha256") == issueSha, "FEEDBACK_ISSUE_BINDING_MISMATCH");
    if (disposition.at("status") == "fix_pending_retest") {
        Require(repair != nullptr, "FEEDBACK_REPAIR_BINDING_REQUIRED");
        Require(disposition.at("repair_lock_sha256") == repair->at("handoff_lock_sha256"), "FEEDBACK_REPAIR_LOCK_MISMATCH");
        bool found = false;
        for (const auto& b : repair->at("issue_bindings"))
            found = found || (b.at("issue_id") == issue.at("issue_id") && b.at("issue_sha256") == issueSha);
        Require(found, "FEEDBACK_REPAIR_ISSUE_MISMATCH");
    } else Require(repair == nullptr, "FEEDBACK_UNEXPECTED_REPAIR");
}
}
