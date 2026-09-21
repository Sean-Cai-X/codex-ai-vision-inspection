#pragma once

#include <array>
#include <string>
#include <vector>

struct DevAnalysisGeomAttachmentScene;

enum class DevAnalysisChainId
{
  CxcoreToEnsmallen = 0,
  CxcoreToMlpack,
  CxcoreToTorch
};

enum class DevAnalysisStageId
{
  Preprocess = 0,
  MainAlgorithm,
  Postprocess,
  LearningInput,
  DeepModelInput,
  TrainOrInferReady
};

enum class DevAnalysisJudgment
{
  ContinueAdvance = 0,
  Questionable,
  PushBackThread
};

struct DevAnalysisCaseRecord
{
  std::string name;
  std::string summary;
  std::string script_path;
  std::string entry_path;
  std::string key_checks;
  std::string expected_failure_mode;
  std::string pushback_hint;
};

struct DevAnalysisEntryRecord
{
  std::string module;
  std::string entry;
  std::string action;
  std::string result;
  std::string next_step;
};

struct DevAnalysisMetricSnapshot
{
  float elapsed_ms = 0.0f;
  float memory_mb = 0.0f;
  std::string key_fields;
  std::string blocking_point;
};

struct DevAnalysisRemoteStatus
{
  std::string entry;
  std::string action;
  std::string result;
  std::string next_step;
  std::string log_path;
  std::string blocker;
};

struct DevAnalysisAiCapabilityRow
{
  std::string task_type;
  std::string capability_level;
  std::string evidence_ref;
  std::string risk_text;
  std::string strengthen_thread;
  std::string supervisor_ready;
  std::string next_step;
};

struct DevAnalysisSemanticActionRow
{
  std::string action_id;
  std::string display_name;
  std::string intent;
  std::string required_args;
  std::string success_rule;
  std::string risk_text;
  std::string fallback;
};

struct DevAnalysisOperationSelect
{
  std::string script_module_ref;
  std::string task_case_ref;
  std::string sample_switch_ref;
  std::string test_image_ref;
  std::string primary_chain_ref;
  std::string script_action_ref;
};

struct DevAnalysisOperationAdjust
{
  std::string parameter_injection_ref;
  std::string geometry_setting_ref;
  std::string acquisition_mode_ref;
  std::string runtime_arg_ref;
};

struct DevAnalysisOperationRun
{
  std::string step_run_ref;
  std::string segment_run_ref;
  std::string full_chain_run_ref;
  std::string replay_run_ref;
  std::string trace_run_ref;
};

struct DevAnalysisOperationObserve
{
  std::string input_image_ref;
  std::string primary_visual_ref;
  std::string supporting_image_refs_ref;
  std::string visualization_refs;
  std::string gui_visual_refs;
  std::string statistics_evidence_ref;
  std::string baseline_result_ref;
  std::string stage_semantic_ref;
  std::string problem_entry_ref;
  std::string evidence_ref;
  std::string result_ref;
  std::string elements_ref;
  std::string element_summary_ref;
  std::string element_chains_ref;
  std::string element_chain_summary_ref;
  std::string element_status_summary_ref;
  std::string element_layer_ref;
  std::string main_chain_ref;
  std::string stage_ref;
  std::string stage_execution_entry_ref;
  std::string conclusion_ref;
  std::string anomaly_ref;
  std::string notes_ref;
  std::string thread_handoff_ref;
};

struct DevAnalysisOperationJudge
{
  std::string current_status_ref;
  std::string issue_kind_ref;
  std::string remediation_priority_ref;
  std::string manual_review_gate_ref;
};

struct DevAnalysisOperationRecord
{
  std::string runtime_fillback_status_ref;
  std::string human_note_ref;
  std::string issue_entry_ref;
  std::string next_step_ref;
  std::string replay_ref;
};

struct DevAnalysisState
{
  DevAnalysisChainId selected_chain = DevAnalysisChainId::CxcoreToEnsmallen;
  DevAnalysisStageId selected_stage = DevAnalysisStageId::Preprocess;
  DevAnalysisJudgment judgment = DevAnalysisJudgment::ContinueAdvance;
  int selected_case_index = 0;
  bool show_overlay = true;
  bool show_summary = true;
  bool show_guidance = true;
  bool show_labels = true;
  bool show_anchor_lines = true;
  bool show_primary_visual = true;
  bool show_detected_rect = true;
  std::vector<DevAnalysisCaseRecord> cases;
  std::vector<DevAnalysisEntryRecord> entries;
  DevAnalysisMetricSnapshot metrics;
  DevAnalysisRemoteStatus remote_status;
  std::vector<DevAnalysisAiCapabilityRow> ai_capability_rows;
  std::vector<DevAnalysisSemanticActionRow> semantic_action_rows;
  DevAnalysisOperationSelect operation_select;
  DevAnalysisOperationAdjust operation_adjust;
  DevAnalysisOperationRun operation_run;
  DevAnalysisOperationObserve operation_observe;
  DevAnalysisOperationJudge operation_judge;
  DevAnalysisOperationRecord operation_record;
  int selected_entity_id = 0;
  int selected_action_index = -1;
  std::string selected_case_script_path;
  std::string selected_case_entry_path;
  std::string selected_case_checks;
  std::string selected_case_failure_mode;
  std::string selected_case_pushback_hint;
  std::string forced_workspace_tab;
  std::string display_attachment_summary;
  std::string display_interaction_summary;
  std::string active_visual_path;
  std::vector<std::string> display_layers;
  std::vector<std::string> display_interactions;
  std::string selected_action_id;
  std::string selected_action_intent;
  unsigned int active_visual_texture_id = 0;
  int active_visual_width = 0;
  int active_visual_height = 0;
  bool active_visual_loaded = false;
  bool detected_rect_valid = false;
  float detected_rect_x = 0.0f;
  float detected_rect_y = 0.0f;
  float detected_rect_width = 0.0f;
  float detected_rect_height = 0.0f;
  std::string detected_rect_label;
  std::array<char, 768> parameter_injection_buffer{};
  std::array<char, 768> geometry_setting_buffer{};
  std::array<char, 768> acquisition_mode_buffer{};
  std::array<char, 768> runtime_arg_buffer{};
  std::array<char, 1024> human_note_buffer{};
  std::array<char, 768> issue_entry_buffer{};
  std::array<char, 768> next_step_buffer{};
};

const char* DevAnalysisChainLabel(DevAnalysisChainId chain_id);
const char* DevAnalysisStageLabel(DevAnalysisStageId stage_id);
const char* DevAnalysisJudgmentLabel(DevAnalysisJudgment judgment);

void InitializeDevAnalysisState(DevAnalysisState& state);
void RefreshDevAnalysisChainData(DevAnalysisState& state);
