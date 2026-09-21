#ifndef CXPARSER_EXT_PARSER_MULTIMODAL_SLICE_TYPES_H
#define CXPARSER_EXT_PARSER_MULTIMODAL_SLICE_TYPES_H

#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace cxparser_ext
{
struct MultimodalSliceObject
{
  std::string object_id;
  std::string object_kind;
  std::string geometry_ref;
  std::string semantic_label;
  std::string summary;
  double confidence = 0.0;
};

struct MultimodalSliceRelation
{
  std::string relation_kind;
  std::string source_object_id;
  std::string target_object_id;
  std::string summary;
};

struct OperationAtom
{
  std::string atom_id;
  std::string stage;
  std::string action_kind;
  std::string input_ref;
  std::string output_ref;
  std::string status;
  std::string summary;
};

struct MultimodalSlice
{
  std::string slice_id;
  std::string source_ref;
  std::string source_hash;
  std::string modality;
  std::string analysis_kind;
  std::string result_ref;
  std::string evidence_ref;
  std::string log_path;
  std::string model_ref;
  double confidence = 0.0;
  std::string next_action;
  std::vector<MultimodalSliceObject> objects;
  std::vector<MultimodalSliceRelation> relations;
  std::vector<std::string> tags;
};

struct UnifiedReviewMetric
{
  std::string metric_name;
  std::string metric_value;
  std::string metric_unit;
  std::string expected_value;
  std::string expected_range;
  std::string baseline_value;
  std::string deviation_level;
  std::string metric_status;
};

struct TaskContext
{
  std::string source_thread;
  std::string task_id;
  std::string batch_id;
  std::string case_name;
  std::string stage;
  std::string task_entry_name;
  std::string task_family;
  std::string pipeline_family;
  std::string model_family;
  std::string scenario_family;
  std::string requested_device;
  std::string actual_device;
  std::string device_evidence;
  std::string sequence_family;
  std::string sequence_stage;
  std::string sequence_index;
  std::string upstream_ref;
  std::string downstream_ref;
};

struct GeometrySemanticType
{
  std::string primary_geometry_semantic;
  std::vector<std::string> auxiliary_geometry_semantics;
  std::vector<std::string> supported_geometry_semantics;
  std::string semantic_status;
};

struct GeometryTemplateSpec
{
  std::string primary_geometry_semantic;
  std::vector<std::string> auxiliary_geometry_semantics;
  std::string template_provenance;
  std::string template_identity;
  std::string review_priority;
};

struct ImageAcquisitionSpec
{
  std::string scope_type;
  std::string execution_mode;
  std::string source_image_ref;
  std::string crop_identity;
  std::string provenance;
};

struct TrainingInput
{
  TaskContext task_context;
  GeometryTemplateSpec geometry_template_spec;
  ImageAcquisitionSpec image_acquisition_spec;
  std::string dataset_ref;
  std::string model_route;
};

struct RunInput
{
  TaskContext task_context;
  GeometryTemplateSpec geometry_template_spec;
  ImageAcquisitionSpec image_acquisition_spec;
  std::string input_image_ref;
  std::string model_route;
};

struct ReviewDecision
{
  TaskContext task_context;
  std::string target_object_ref;
  std::string review_status;
  std::string review_reason;
  std::string reviewer_action;
};

struct FlowbackAction
{
  TaskContext task_context;
  std::string target_object_ref;
  std::string flowback_type;
  std::string trigger_reason;
  std::string next_target;
};

struct UnifiedDetectionElement
{
  std::string element_id;
  std::string element_type;
  std::string semantic_role;
  std::string geometry_payload;
  std::string source_ref;
  std::string primary_overlay_ref;
  std::vector<std::string> overlay_refs;
  std::string confidence;
  std::string provenance;
  std::string consistency_status;
  std::string candidate_status;
  std::string match_status;
  std::string manual_review_signal;
  std::string linked_template_element_id;
  std::string template_relation;
  std::string drift_summary;
  std::string element_group_id;
  std::string element_group_label;
  std::string focus_region_ref;
  std::string local_delta_ref;
  std::string element_status_summary;
  std::string element_findings;
  std::string element_level_focus;
};

struct UnifiedElementChain
{
  std::string chain_id;
  std::string chain_type;
  std::string chain_status;
  std::string source_ref;
  std::string target_ref;
  std::vector<std::string> element_ids;
  std::string template_relation;
  std::string chain_summary;
  std::string chain_findings;
  std::string chain_focus;
};

struct SequenceLinkTrace
{
  std::string sequence_family;
  std::string from_stage;
  std::string to_stage;
  std::string upstream_artifact_ref;
  std::string intermediate_artifact_ref;
  std::string downstream_artifact_ref;
  std::string attach_back_ref;
  std::string transition_status;
};
struct UnifiedImageReviewRecord
{
  std::string source_thread;
  std::string task_id;
  std::string batch_id;
  std::string case_name;
  std::string image_id;
  std::string stage;
  std::string input_image_ref;
  std::string primary_visual_ref;
  std::string status;
  std::vector<UnifiedReviewMetric> metrics;
  std::vector<std::string> anomaly_flags;
  std::string notes;
  std::vector<std::string> output_image_refs;
  std::vector<std::string> visualization_refs;
  std::string metric_summary_text;
  std::vector<std::string> baseline_refs;
  std::vector<std::string> compare_tags;
  std::vector<std::string> artifact_refs;
  std::vector<std::string> contract_evidence;
  std::vector<std::string> phenomenon_evidence;
  std::vector<std::string> interaction_evidence;
  std::string observation_personality;
  std::string default_open_chain;
  std::string evidence_focus_summary;
  std::string thread_handoff;
  std::string task_entry_name;
  std::string task_family;
  std::string pipeline_family;
  std::string model_family;
  std::string scenario_family;
  std::string visual_evidence_set;
  std::string device_evidence;
  std::string sequence_family;
  std::string sequence_stage;
  std::string sequence_index;
  std::string sequence_trace_ref;
  std::string sequence_records;
  std::string sequence_summary;
  std::string sequence_status_summary;
  std::string stage_refs;
  std::string script_refs;
  std::string image_refs;
  std::string conclusion_refs;
  std::string issue_refs;
  std::string lifecycle_summary;
  std::string lifecycle_zone_refs;
  std::string init_stage_refs;
  std::string repeatable_stage_refs;
  std::string debug_stage_refs;
  std::string replay_stage_refs;
  std::string reset_stage_refs;
  std::string lifecycle_risk_summary;
  std::string test_image_ref;
  std::string visual_evidence_ref_set;
  std::string single_image_conclusion_ref;
  std::string element_conclusion_ref_set;
  std::string task_conclusion_ref;
  std::string anomaly_conclusion_ref;
  std::string next_action_ref;
  std::string element_ref_set;
  std::string element_type;
  std::string element_source;
  std::string element_visual_anchor;
  std::string template_relation;
  std::string consistency_status;
  std::string chain_ref_set;
  std::string chain_key;
  std::string chain_status;
  std::string chain_focus_ref;
  std::string chain_issue_ref;
  std::string stage_ref_set;
  std::string current_stage;
  std::string upstream_ref;
  std::string downstream_ref;
  std::string stage_status;
  std::string issue_entry_ref_set;
  std::string recommended_image_ref;
  std::string recommended_element_ref;
  std::string recommended_chain_ref;
  std::string recommended_stage_ref;
  std::string issue_kind_hint;
  std::string raw_image_ref;
  std::string edge_image_ref;
  std::string element_relation_image_ref;
  std::string candidate_image_ref;
  std::string match_image_ref;
  std::string geometry_stage;
  std::string candidate_stage;
  std::string match_stage;
  std::string problem_focus_image_ref;
  std::string problem_focus_element_id;
  std::string problem_focus_chain_id;
  std::string problem_issue_type;
  std::string single_image_geometry_conclusion;
  std::string element_conclusion;
  std::string match_conclusion;
  std::string task_conclusion;
  std::string next_step_suggestion;
  std::string gui_chain_summary;
  std::string business_eval_fields;
  std::string pipeline_link_trace;
  std::string refresh_mode;
  std::vector<std::string> changed_fields;
  std::vector<std::string> changed_element_ids;
  std::vector<std::string> changed_chain_keys;
  std::string refresh_priority;
  std::vector<UnifiedDetectionElement> detection_elements;
  std::vector<UnifiedElementChain> element_chains;
  std::string primary_detection_semantic;
  std::string template_alignment_status;
  std::string element_group_summary;
  std::string element_status_summary;
  std::string candidate_status_summary;
  std::string match_status_summary;
  std::string manual_review_signal_summary;
  std::string element_findings;
  std::string element_level_focus;
  std::string focus_refresh_targets;
  std::string local_delta_targets;
  std::string grouped_element_preview;
  std::string focus_element_preview;
  std::string delta_element_preview;
  int missing_element_count = 0;
  int abnormal_element_count = 0;
  int drifted_element_count = 0;
  int candidate_element_count = 0;
};

struct UnifiedTaskReviewBundle
{
  std::string source_thread;
  std::string task_id;
  std::string batch_id;
  std::string task_type;
  std::string case_group;
  std::string primary_visual_ref;
  int total_images = 0;
  int abnormal_images = 0;
  std::vector<std::string> focus_image_ids;
  std::string metric_summary;
  std::string stage_summary;
  std::string current_conclusion;
  std::string next_attention_points;
  std::string status_distribution;
  std::string anomaly_type_distribution;
  std::string baseline_compare_summary;
  int review_required_count = 0;
  std::vector<std::string> top_metric_outliers;
  std::string training_evidence_summary;
  std::vector<std::string> artifact_bundle_refs;
  std::vector<std::string> supporting_refs;
  std::string observation_personality;
  std::string default_open_chain;
  std::string evidence_focus_summary;
  std::string thread_handoff;
  std::string task_family;
  std::string pipeline_family;
  std::string model_family;
  std::string scenario_family;
  std::string metric_summary_by_stage;
  std::string visual_evidence_summary;
  std::string device_summary;
  std::string business_eval_fields;
  std::string pipeline_link_trace;
  std::string sequence_family;
  std::string stage_transition_summary;
  std::string stage_abnormal_summary;
  std::string test_image_ref;
  std::string visual_evidence_ref_set;
  std::string task_conclusion_ref;
  std::string anomaly_conclusion_ref;
  std::string next_action_ref;
  std::string element_ref_set;
  std::string chain_ref_set;
  std::string stage_ref_set;
  std::string issue_entry_ref_set;
  std::string recommended_image_ref;
  std::string recommended_element_ref;
  std::string recommended_chain_ref;
  std::string recommended_stage_ref;
  std::string issue_kind_hint;
  std::string raw_image_ref;
  std::string edge_image_ref;
  std::string element_relation_image_ref;
  std::string candidate_image_ref;
  std::string match_image_ref;
  std::string geometry_stage;
  std::string candidate_stage;
  std::string match_stage;
  std::string problem_focus_image_ref;
  std::string problem_focus_element_id;
  std::string problem_focus_chain_id;
  std::string problem_issue_type;
  std::string single_image_geometry_conclusion;
  std::string element_conclusion;
  std::string match_conclusion;
  std::string task_conclusion;
  std::string next_step_suggestion;
  std::string gui_chain_summary;
  std::string review_mode;
  std::string default_decision_axis;
  std::string refresh_mode;
  std::vector<std::string> changed_fields;
  std::vector<std::string> changed_element_ids;
  std::vector<std::string> changed_chain_keys;
  std::string refresh_priority;
  std::string tolerance_summary;
  std::string stability_summary;
  std::string element_type_summary;
  std::string element_summary;
  std::string element_chain_summary;
  std::string element_status_summary;
  std::string candidate_status_summary;
  std::string match_status_summary;
  std::string manual_review_signal_summary;
  std::string element_group_summary;
  std::string element_findings;
  std::string element_level_focus;
  std::string focus_refresh_targets;
  std::string local_delta_targets;
  std::string grouped_element_preview;
  std::string focus_element_preview;
  std::string delta_element_preview;
  std::string missing_element_summary;
  std::string drifted_element_summary;
  std::string abnormal_element_summary;
};

struct UnifiedCompareSlice
{
  std::string compare_id;
  std::string compare_type;
  std::string left_ref;
  std::string right_ref;
  std::vector<std::string> compare_dimensions;
  std::string delta_summary;
  std::string risk_level;
  std::string focus_recommendation;
  std::vector<std::string> supporting_refs;
  std::string observation_personality;
  std::string default_open_chain;
  std::string evidence_focus_summary;
  std::string thread_handoff;
  std::string compare_view_mode;
  std::string refresh_mode;
  std::vector<std::string> changed_fields;
  std::vector<std::string> changed_element_ids;
  std::vector<std::string> changed_chain_keys;
  std::string refresh_priority;
  std::string threshold_summary;
  std::string risk_note;
  std::string element_summary;
  std::string element_chain_summary;
  std::string element_status_summary;
  std::string candidate_status_summary;
  std::string match_status_summary;
  std::string manual_review_signal_summary;
  std::string element_group_summary;
  std::string element_findings;
  std::string element_level_focus;
  std::string focus_refresh_targets;
  std::string local_delta_targets;
  std::string grouped_element_preview;
  std::string focus_element_preview;
  std::string delta_element_preview;
  std::string element_level_diff;
  std::string semantic_diff;
  std::string structure_diff;
};

struct UnifiedAnomalyFocusBundle
{
  std::string source_thread;
  std::string task_id;
  std::string batch_id;
  std::vector<std::string> abnormal_image_ids;
  std::string anomaly_type_summary;
  std::vector<std::string> top_focus_objects;
  std::vector<std::string> analysis_suggestions;
  std::string risk_level;
  std::vector<std::string> supporting_refs;
  std::vector<std::string> anomaly_element_ids;
  std::vector<std::string> anomaly_element_types;
  std::string observation_personality;
  std::string default_open_chain;
  std::string evidence_focus_summary;
  std::string thread_handoff;
  std::string anomaly_axis;
  std::string refresh_mode;
  std::vector<std::string> changed_fields;
  std::vector<std::string> changed_element_ids;
  std::vector<std::string> changed_chain_keys;
  std::string refresh_priority;
  std::string stability_summary;
  std::string risk_note;
  std::string element_summary;
  std::string element_chain_summary;
  std::string element_status_summary;
  std::string candidate_status_summary;
  std::string match_status_summary;
  std::string manual_review_signal_summary;
  std::string element_group_summary;
  std::string element_findings;
  std::string element_level_focus;
  std::string focus_refresh_targets;
  std::string local_delta_targets;
  std::string grouped_element_preview;
  std::string focus_element_preview;
  std::string delta_element_preview;
  std::string anomaly_focus_reason;
  std::string raw_image_ref;
  std::string edge_image_ref;
  std::string element_relation_image_ref;
  std::string candidate_image_ref;
  std::string match_image_ref;
  std::string geometry_stage;
  std::string candidate_stage;
  std::string match_stage;
  std::string problem_focus_image_ref;
  std::string problem_focus_element_id;
  std::string problem_focus_chain_id;
  std::string problem_issue_type;
  std::string single_image_geometry_conclusion;
  std::string element_conclusion;
  std::string match_conclusion;
  std::string task_conclusion;
  std::string next_step_suggestion;
  std::string gui_chain_summary;
};

inline std::string BuildPseudoSourceHash(const std::string &seed)
{
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0')
      << static_cast<unsigned long long>(std::hash<std::string>()(seed));
  return out.str();
}
}

#endif




