#include "OfflineFeedbackValidation.h"
#include "CxGeoSO2ReferenceAsset.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace f = vision_ai::offline_feedback;
using J = f::Json;
namespace {
int checks = 0;
const std::string sha = "sha256:" + std::string(64, 'a');
J Issue() {
    return {
        {"schema","visionai.business.no_image_issue.v1"},{"issue_id","synthetic-issue-1"},
        {"project_ref","opaque-project-1"},{"case_ref","opaque-case-1"},{"sample_ref","opaque-sample-1"},
        {"image_sha256",sha},{"source_split","Valid"},
        {"business_target",{{"version","target-1"},{"description","inner ellipse boundary"},{"presence","present"}}},
        {"annotation",{{"version","ann-1"},{"sha256",sha},{"state","confirmed"}}},
        {"dataset",{{"revision","frozen-1"},{"sha256",sha}}},
        {"model",{{"model_id","synthetic-model"},{"manifest_sha256",sha},{"checkpoint_sha256",sha},{"origin","RND_SYNTHETIC"}}},
        {"runtime",{{"sha256",sha},{"abi_version","abi-1"},{"preprocessing_version","letterbox-rgb-1"},{"postprocessing_version","box-nms-mask-1"}}},
        {"execution",{{"task_id","task-1"},{"request_id","req-1"},{"status","SUCCEEDED"},{"elapsed_ms",12.5},{"error_code",nullptr},{"failure_stage",nullptr}}},
        {"parameters",{{"configuration_sha256",sha},{"confidence_threshold",0.25},{"nms_iou_threshold",0.45},{"mask_threshold",0.5},{"max_detections",20}}},
        {"class_catalog",{"arc","circle","ellipse","line","open_curve","polygon","closed_curve"}},
        {"raw_class_counts",{0,0,1,0,0,0,0}},
        {"selected_candidates",J::array({{{"candidate_id","candidate-1"},{"class_id",2},{"score",0.7},{"bounds",{10,20,40,50}}}})},
        {"association",{{"source","human_confirmed"},{"annotation_ref","annotation-object-1"},{"candidate_ids",{"candidate-1"}},{"rule",nullptr},{"threshold",nullptr}}},
        {"metrics",{{"metric_version","single_gt_class_agnostic_v1"},{"best_target_iou",0.0},{"best_any_class_iou",0.0},{"center_error_pixels",15.0},{"boundary_distance_pixels",nullptr},{"unmatched_gt_count",1},{"unmatched_prediction_count",1},{"duplicate_pair_count",0}}},
        {"diagnostics",{{"above_threshold_count",30},{"examined_count",25},{"suppressed_count",5},{"unexamined_count",5},{"selected_before_mask_count",20},{"mask_rejected_count",19},{"returned_count",1},{"detection_cap",20},{"cap_hit",true},{"full_post_nms_count",nullptr}}},
        {"issue_type","boundary_error"},{"expected","One inner ellipse matches the confirmed annotation"},
        {"actual","Candidate 1 is on the outer boundary"},{"operation_steps",{"Select imported model","Run current registered image","Confirm candidate association"}},
        {"human_confirmed",true},{"production_allowed",false}
    };
}
J Disposition() {
    return {{"schema","visionai.rd.issue_disposition.v1"},{"issue_id","synthetic-issue-1"},{"issue_sha256",sha},
        {"status","needs_diagnostics"},{"summary","Need pre and post NMS counts"},
        {"requested_diagnostics",{"nms_counts"}},{"evidence_kind","structured_review"},
        {"evidence_sha256",nullptr},{"repair_lock_sha256",nullptr},{"production_allowed",false}};
}
J Repair() {
    return {{"schema","visionai.rd.issue_repair_binding.v1"},{"package_id","synthetic-protocol-drill"},{"handoff_lock_sha256",sha},
        {"issue_bindings",J::array({{{"issue_id","synthetic-issue-1"},{"issue_sha256",sha}}})},
        {"change_scope","DIAGNOSTIC_ONLY"},{"change_summary","Protocol drill only - not a real repair"},
        {"candidate_model_id","synthetic-model"},{"candidate_manifest_sha256",sha},{"candidate_checkpoint_sha256",sha},
        {"runtime_sha256",sha},{"abi_version","abi-1"},{"inference_supported",true},{"training_supported",false},
        {"training_acceptance_sha256",nullptr},{"requires_restart",true},{"quality_status","NOT_ACCEPTED"},
        {"production_allowed",false},{"auto_activate",false}};
}
void Pass(const std::function<void()>& action) { action(); ++checks; }
void Reject(const std::function<void()>& action, const std::string& code) {
    try { action(); } catch (const std::exception& e) {
        if (e.what() == code) { ++checks; return; }
        throw std::runtime_error("wrong rejection: " + std::string(e.what()) + " expected " + code);
    }
    throw std::runtime_error("rejection missing: " + code);
}
}
int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 3) throw std::runtime_error("schema [synthetic-fixture-output] arguments required");
        std::ifstream in(argv[1]); std::string bytes{std::istreambuf_iterator<char>(in), {}};
        const auto schema = f::ParseStrict(bytes);
        const auto check = [&](const J& j, const std::string& kind = "issue") { f::ValidateRecord(j,schema,kind); };
        auto issue = Issue(); auto disposition = Disposition(); auto repair = Repair();
        Pass([&]{ check(issue); check(disposition,"disposition"); check(repair,"repair"); f::ValidateLinks(issue,sha,disposition); });
        auto wrongSchema = schema; wrongSchema["$id"] = "unsupported-version";
        Reject([&]{ f::ValidateRecord(issue,wrongSchema,"issue"); },"FEEDBACK_CONTRACT_VERSION_INVALID");
        Pass([&]{ f::ParseStrict("{\"a\":{\"id\":1},\"b\":{\"id\":2}}"); });
        const auto bad = [&](const std::function<void(J&)>& edit, const std::string& code) {
            auto j = issue; edit(j); Reject([&]{ check(j); },code);
        };
        for (const auto* key : {"image_path","mask","overlay","pixels","contour","thumbnail"})
            bad([&](J& j){ j[key] = "private"; },"FEEDBACK_FIELD_NOT_ALLOWED");
        bad([](J& j){ j["model"]["source_path"] = "private"; },"FEEDBACK_FIELD_NOT_ALLOWED");
        bad([](J& j){ j.erase("dataset"); },"FEEDBACK_FIELD_MISSING");
        bad([](J& j){ j["image_sha256"] = "sha256:BAD"; },"FEEDBACK_TEXT_FORMAT");
        bad([](J& j){ j["source_split"] = "TEST"; },"FEEDBACK_ENUM_INVALID");
        bad([](J& j){ j["class_catalog"][1] = "ellipse"; },"FEEDBACK_CONST_INVALID");
        bad([](J& j){ j["production_allowed"] = true; },"FEEDBACK_CONST_INVALID");
        for (const auto& text : {"D:\\private\\test.png","file:///private/test","https://host/test","\\\\server\\share","/private/test","data:image/png;base64,aaa"})
            bad([&](J& j){ j["expected"] = text; },"FEEDBACK_PRIVATE_TEXT");
        bad([](J& j){ j["parameters"]["confidence_threshold"] = 2; },"FEEDBACK_NUMBER_RANGE");
        bad([](J& j){ j["parameters"]["max_detections"] = 1.5; },"FEEDBACK_TYPE_INVALID");
        bad([](J& j){ j["execution"]["elapsed_ms"] = std::numeric_limits<double>::infinity(); },"FEEDBACK_NONFINITE");
        bad([](J& j){ j["annotation"]["sha256"] = nullptr; },"FEEDBACK_ANNOTATION_BINDING_REQUIRED");
        bad([](J& j){ j["annotation"]["state"] = "missing"; },"FEEDBACK_GT_METRIC_WITHOUT_TRUTH");
        bad([](J& j){ j["business_target"]["presence"] = "absent"; },"FEEDBACK_GT_METRIC_WITHOUT_TRUTH");
        bad([](J& j){ j["selected_candidates"].push_back(j["selected_candidates"][0]); },"FEEDBACK_DUPLICATE_CANDIDATE_ID");
        bad([](J& j){ j["selected_candidates"][0]["bounds"] = {50,20,10,40}; },"FEEDBACK_BOUNDS_INVALID");
        bad([](J& j){ j["association"]["candidate_ids"] = {"missing"}; },"FEEDBACK_ASSOCIATION_CANDIDATE_INVALID");
        bad([](J& j){ j["association"]["source"] = "automatic_suggestion"; },"FEEDBACK_SUGGESTION_RULE_REQUIRED");
        bad([](J& j){ j["association"]["source"] = "none"; },"FEEDBACK_NONE_HAS_ASSOCIATION");
        bad([](J& j){ j["diagnostics"]["above_threshold_count"] = 31; },"FEEDBACK_COUNT_INCONSISTENT");
        bad([](J& j){ j["diagnostics"]["full_post_nms_count"] = 20; },"FEEDBACK_TRUNCATION_HAS_FULL_COUNT");
        bad([](J& j){ j["diagnostics"]["detection_cap"] = 30; },"FEEDBACK_CAP_PARAMETER_MISMATCH");
        bad([](J& j){ j["raw_class_counts"][2] = 2; },"FEEDBACK_CLASS_COUNT_MISMATCH");
        bad([](J& j){ j["raw_class_counts"] = {0,1,0,0,0,0,0}; },"FEEDBACK_SELECTED_COUNT_MISMATCH");
        bad([](J& j){ j["execution"]["error_code"] = "FAIL"; },"FEEDBACK_SUCCESS_HAS_FAILURE");
        bad([](J& j){ j["execution"]["status"] = "FAILED"; },"FEEDBACK_FAILURE_DIAGNOSTIC_REQUIRED");
        auto verify = issue;
        verify["source_split"] = "VERIFY"; verify["annotation"] = {{"version",nullptr},{"sha256",nullptr},{"state","missing"}};
        for (auto& v : verify["metrics"].items()) if (v.key() != "metric_version") v.value() = nullptr;
        verify["business_target"]["presence"] = "undecidable";
        verify["association"] = {{"source","none"},{"annotation_ref",nullptr},{"candidate_ids",J::array()},{"rule",nullptr},{"threshold",nullptr}};
        Pass([&]{ check(verify); });
        auto failed = verify; failed["execution"]["status"] = "FAILED";
        failed["execution"]["error_code"] = "DEPENDENCY_MISSING"; failed["execution"]["failure_stage"] = "runtime_load";
        failed["issue_type"] = "execution_failure"; failed["raw_class_counts"] = nullptr; failed["selected_candidates"] = J::array();
        for (auto& v : failed["diagnostics"].items()) if (v.key() != "detection_cap") v.value() = nullptr;
        Pass([&]{ check(failed); });
        failed["raw_class_counts"] = {0,0,0,0,0,0,0};
        Reject([&]{ check(failed); },"FEEDBACK_FAILED_EXECUTION_HAS_RESULTS");
        Reject([&]{ f::ParseStrict("{\"a\":1,\"a\":2}"); },"FEEDBACK_DUPLICATE_JSON_KEY");
        Reject([&]{ f::ParseStrict("{\"outer\":{\"a\":1,\"a\":2}}"); },"FEEDBACK_DUPLICATE_JSON_KEY");
        Reject([&]{ f::ParseStrict(std::string(1024*1024+1,' ')); },"FEEDBACK_FILE_LIMIT");
        Reject([&]{ f::ParseStrict("{private invalid json}"); },"FEEDBACK_JSON_INVALID");
        Reject([&]{ f::ParseStrict(std::string(34,'[') + "0" + std::string(34,']')); },"FEEDBACK_DEPTH_LIMIT");
        auto d = disposition; d["requested_diagnostics"] = J::array();
        Reject([&]{ check(d,"disposition"); },"FEEDBACK_DIAGNOSTICS_REQUIRED");
        d = disposition; d["status"] = "resolved";
        Reject([&]{ check(d,"disposition"); },"FEEDBACK_ENUM_INVALID");
        d = disposition; d["status"] = "reproduced"; d["requested_diagnostics"] = J::array();
        Reject([&]{ check(d,"disposition"); },"FEEDBACK_REPRODUCTION_EVIDENCE_REQUIRED");
        d["evidence_kind"] = "rnd_synthetic_reproduction"; d["evidence_sha256"] = sha;
        Pass([&]{ check(d,"disposition"); });
        d["status"] = "fix_pending_retest";
        Reject([&]{ check(d,"disposition"); },"FEEDBACK_REPAIR_EVIDENCE_REQUIRED");
        d["evidence_kind"] = "release_validation"; d["repair_lock_sha256"] = sha;
        Pass([&]{ check(d,"disposition"); f::ValidateLinks(issue,sha,d,&repair); });
        Reject([&]{ f::ValidateLinks(issue,sha,d); },"FEEDBACK_REPAIR_BINDING_REQUIRED");
        Reject([&]{ f::ValidateLinks(issue,sha,disposition,&repair); },"FEEDBACK_UNEXPECTED_REPAIR");
        auto wrong = repair; wrong["handoff_lock_sha256"] = "sha256:" + std::string(64,'b');
        Reject([&]{ f::ValidateLinks(issue,sha,d,&wrong); },"FEEDBACK_REPAIR_LOCK_MISMATCH");
        wrong = repair; wrong["issue_bindings"][0]["issue_id"] = "other-issue";
        Reject([&]{ f::ValidateLinks(issue,sha,d,&wrong); },"FEEDBACK_REPAIR_ISSUE_MISMATCH");
        Reject([&]{ f::ValidateLinks(issue,"sha256:" + std::string(64,'b'),d,&repair); },"FEEDBACK_ISSUE_BINDING_MISMATCH");
        wrong = repair; wrong["training_supported"] = true;
        Reject([&]{ check(wrong,"repair"); },"FEEDBACK_TRAINING_EVIDENCE_REQUIRED");
        wrong = repair; wrong["issue_bindings"].push_back(wrong["issue_bindings"][0]);
        Reject([&]{ check(wrong,"repair"); },"FEEDBACK_DUPLICATE_ISSUE_BINDING");
        if (argc == 3) {
            const std::filesystem::path root(argv[2]);
            std::filesystem::create_directories(root);
            const auto write = [&](const char* name, const std::string& content) {
                const auto path = root / name;
                if (std::filesystem::exists(path)) {
                    std::ifstream file(path,std::ios::binary);
                    const std::string old{std::istreambuf_iterator<char>(file),{}};
                    if (old != content) throw std::runtime_error("SYNTHETIC_FIXTURE_OVERWRITE_FORBIDDEN");
                    return;
                }
                std::ofstream file(path,std::ios::binary); file << content;
                file.close(); if (!file) throw std::runtime_error("SYNTHETIC_FIXTURE_WRITE_FAILED");
            };
            const auto issueBytes = issue.dump(2) + '\n';
            const auto fileSha = "sha256:" + cxgeom::so2::Sha256Bytes(issueBytes);
            d["issue_sha256"] = fileSha;
            repair["issue_bindings"][0]["issue_sha256"] = fileSha;
            Pass([&]{ f::ValidateLinks(issue,fileSha,d,&repair); });
            write("issue.json",issueBytes);
            write("disposition.json",d.dump(2) + '\n');
            write("repair.json",repair.dump(2) + '\n');
            d["issue_sha256"] = "sha256:" + std::string(64,'b');
            write("wrong-disposition.json",d.dump(2) + '\n');
        }
        std::cout << "PASS: " << checks << " native contract checks; SYNTHETIC_PROTOCOL_DRILL_ONLY; no business issue closed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
