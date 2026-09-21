#ifndef CXPARSER_EXT_CXSCRIPT_RUNTIME_TYPES_H
#define CXPARSER_EXT_CXSCRIPT_RUNTIME_TYPES_H

#include "../meta/parser_multimodal_slice_types.h"

#include <string>
#include <vector>

namespace cxparser_ext
{
struct CxscriptIdentity
{
  std::string script_type;
  std::string module_name;
  std::string integration_name;
  std::string layer;
  std::string case_id;
  std::string file_path;
};

struct CxscriptExecutionArgs
{
  std::string script_type;
  std::string module_name;
  std::string integration_name;
  std::string layer;
  std::string case_id;
  std::string mode;
  std::string route;
  std::string trace_id;
  std::string script_path;
  bool report_on = false;
};

struct CxscriptExecutionContext
{
  std::string caller_module;
  std::string callee_module;
  std::string route;
  std::string execution_mode;
  std::string trace_id;
};

struct CxscriptHeaderMetadata
{
  std::string kind;
  std::string layer;
  std::string module_name;
  std::string case_name;
  std::string mode;
  bool has_report = false;
  bool report_on = false;
};

struct CxscriptOptimizationSnapshot
{
  bool defined = false;
  double objective = 0.0;
  std::string primary_metric_name;
  double primary_metric_value = 0.0;
  std::string stability_metric_name;
  double stability_metric_value = 0.0;
};

struct CxscriptOptimizationCompare
{
  bool defined = false;
  double objective_delta_abs = 0.0;
  double objective_delta_ratio = 0.0;
  double primary_metric_delta = 0.0;
  double stability_delta = 0.0;
  int eval_count = 0;
  bool converged = false;
  std::string stop_reason;
  std::string pass_level;
  std::string replay_log_path;
};

struct CxscriptFlowProfile
{
  std::string script_style;
  bool has_prepare = false;
  bool has_action = false;
  bool has_check = false;
  bool has_report = false;
};

struct CxscriptBasicSemanticProfile
{
  bool has_declaration = false;
  bool has_assignment = false;
  bool has_expression_stmt = false;
  bool has_call_stmt = false;
  bool has_if_block = false;
  bool has_check_call = false;
  bool has_print_call = false;
  bool has_register_class = false;
  bool has_register_fun = false;
};

struct CxscriptBindingSemanticProfile
{
  bool requires_registered_binding = false;
  bool uses_object_binding = false;
  bool uses_explicit_registration = false;
  std::string binding_scope;
};

struct CxscriptLayerProfile
{
  bool has_source_text = false;
  bool has_normalized_text = false;
  bool has_linear_ir = false;
  bool has_compile_bridge = false;
  bool has_execution_text = false;
  std::string execution_text_kind;
  bool bridge_exec_safe = false;
  std::string bridge_exec_subset;
  std::string bridge_exec_reason;
  std::string fallback_reason;
};

struct CxscriptExecutionResult
{
  bool success = false;
  std::string status;
  CxscriptIdentity identity;
  CxscriptExecutionContext context;
  CxscriptHeaderMetadata header_metadata;
  std::string script_origin;
  bool report_requested = false;
  std::string error_kind;
  std::string error_message;
  double scalar_result = 0.0;
  double runtime_ms = 0.0;
  int parser_error_code = -1;
  int parser_error_pos = -1;
  std::string parser_error_token;
  std::string parser_error_expr;
  int last_step_id = 0;
  int last_frame_id = 0;
  int last_sequence = 0;
  int last_source_line = 0;
  int failure_step_id = 0;
  int failure_frame_id = 0;
  int failure_sequence = 0;
  int failure_line = 0;
  std::string failure_phase;
  std::vector<std::string> modules;
  int accepted_task_count = 0;
  int executed_task_count = 0;
  int replay_count = 0;
  std::string replay_source_task_id;
  int replay_stage_count = 0;
  CxscriptFlowProfile flow_profile;
  CxscriptBasicSemanticProfile basic_semantics;
  CxscriptBindingSemanticProfile binding_semantics;
  CxscriptLayerProfile layer_profile;
  bool ir_valid = false;
  int ir_op_count = 0;
  int ir_stmt_count = 0;
  int ir_block_count = 0;
  std::string ir_error_message;
  std::string compile_text;
  CxscriptOptimizationSnapshot baseline_snapshot;
  CxscriptOptimizationSnapshot best_snapshot;
  CxscriptOptimizationCompare optimization_compare;
  int bridge_point_count = 0;
  int bridge_matched_call_count = 0;
  int bridge_unresolved_call_count = 0;
  std::string bridge_summary;
  std::vector<std::string> bridge_point_lines;
  std::vector<std::string> lines;
  std::vector<std::string> fragment_ids;
  std::vector<std::string> bundle_ids;
  std::vector<std::string> workflow_path_groups;
};

struct CxscriptRuntimeReport
{
  std::string script_type;
  std::string module_name;
  std::string integration_name;
  std::string layer;
  std::string case_id;
  std::string mode;
  std::string route;
  std::string file_path;
  std::string script_origin;
  CxscriptHeaderMetadata header_metadata;
  std::string status;
  bool success = false;
  bool skipped = false;
  bool report_requested = false;
  std::string task_id;
  std::string result_object;
  std::string optimize_summary_object;
  std::string compare_summary_object;
  std::string replay_result_object;
  std::string rag_writeback_note_object;
  std::string metrics;
  std::string tolerance;
  std::string failure_mode;
  std::string summary;
  std::string error_kind;
  std::string error_message;
  double scalar_result = 0.0;
  double runtime_ms = 0.0;
  int parser_error_code = -1;
  int parser_error_pos = -1;
  std::string parser_error_token;
  std::string parser_error_expr;
  int last_step_id = 0;
  int last_frame_id = 0;
  int last_sequence = 0;
  int last_source_line = 0;
  int failure_step_id = 0;
  int failure_frame_id = 0;
  int failure_sequence = 0;
  int failure_line = 0;
  std::string failure_phase;
  int accepted_task_count = 0;
  int executed_task_count = 0;
  int replay_count = 0;
  std::string replay_source_task_id;
  int replay_stage_count = 0;
  CxscriptFlowProfile flow_profile;
  CxscriptBasicSemanticProfile basic_semantics;
  CxscriptBindingSemanticProfile binding_semantics;
  CxscriptLayerProfile layer_profile;
  bool ir_valid = false;
  int ir_op_count = 0;
  int ir_stmt_count = 0;
  int ir_block_count = 0;
  std::string ir_error_message;
  std::string compile_text;
  CxscriptOptimizationSnapshot baseline_snapshot;
  CxscriptOptimizationSnapshot best_snapshot;
  CxscriptOptimizationCompare optimization_compare;
  int bridge_point_count = 0;
  int bridge_matched_call_count = 0;
  int bridge_unresolved_call_count = 0;
  std::string bridge_summary;
  std::vector<std::string> bridge_point_lines;
  std::vector<std::string> modules;
  std::vector<std::string> fragment_ids;
  std::vector<std::string> bundle_ids;
  std::vector<std::string> workflow_path_groups;
  std::vector<MultimodalSlice> multimodal_slices;
  std::vector<OperationAtom> operation_atoms;
};
}

#endif
