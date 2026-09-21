#include "parser_test_router.h"

#include "parser_delivery_api.h"

namespace cxparser_ext
{
namespace
{
bool IsOneOf(const std::string &value, const std::vector<std::string> &choices)
{
  for (std::size_t i = 0; i < choices.size(); ++i)
  {
    if (value == choices[i])
      return true;
  }
  return false;
}

PseudoClassSpec BuildImageProbePseudoClass()
{
  PseudoClassSpec pseudo_class;
  pseudo_class.module_name = "testdll_image_probe";
  pseudo_class.class_name = "ImageProbeWrapper";
  pseudo_class.parser_alias = "ImageProbe";

  PseudoMethodSpec load_method;
  load_method.name = "Load";
  load_method.param_types.push_back("const char*");
  load_method.return_type = "void";
  pseudo_class.methods.push_back(load_method);

  PseudoMethodSpec detect_method;
  detect_method.name = "Detect";
  detect_method.param_types.push_back("double");
  detect_method.return_type = "void";
  pseudo_class.methods.push_back(detect_method);

  PseudoMethodSpec score_method;
  score_method.name = "Score";
  score_method.return_type = "double";
  pseudo_class.methods.push_back(score_method);

  return pseudo_class;
}

ExecutionTarget MakeParserEvalTarget(const ParserTestRequest &request,
                                     const std::string &script_text)
{
  ExecutionTarget target;
  target.task_id = request.module + "." + request.layer + "." + request.case_name;
  target.task_name = request.case_name;
  target.trace_id = "trace." + target.task_id;
  target.task_type = task_constants::TaskTypeCoreTest();
  target.execution_mode = task_constants::ExecutionModeMainline();
  target.route_hint = task_constants::RouteDefault();
  target.priority_hint = task_constants::RouteDefault();
  target.module_name = request.module;
  target.target_method = "eval";
  target.script_text = script_text;
  target.module_call.caller_module = "cxparser_test_driver";
  target.module_call.callee_module = request.module;
  target.module_call.protocol_name = "cxparser.module.call";
  target.module_call.capability_name = "script_dispatch";
  target.module_call.method_name = "eval";
  return target;
}

std::string MakeCxscriptMetadataHeader(const ParserTestRequest &request)
{
  std::string script;
  script += "module " + request.module + ";\n";
  script += "layer " + request.layer + ";\n";
  script += "case " + request.case_name + ";\n";
  return script;
}

std::string BuildCxcoreLineMeasurementBalancedCxscript(const ParserTestRequest &request)
{
  std::string script = MakeCxscriptMetadataHeader(request);
  script +=
    "step prepare {\n"
    "double threshold = 0.8;\n"
    "{\n"
    "print(\"line.prepare\");\n"
    "checkpoint line_prepare_enter;\n"
    "}\n"
    "call line_measurement_balanced();\n"
    "breakpoint line_prepare_bp;\n"
    "}\n"
    "step check {\n"
    "check(success == true);\n"
    "check(result_object == \"LineMeasurementOutput\");\n"
    "print(summary);\n"
    "}\n";
  return script;
}

std::string BuildCxcoreCircleMeasurementBalancedCxscript(const ParserTestRequest &request)
{
  std::string script = MakeCxscriptMetadataHeader(request);
  script +=
    "step prepare {\n"
    "double radius_hint = 12.5;\n"
    "{\n"
    "print(\"circle.prepare\");\n"
    "checkpoint circle_prepare_enter;\n"
    "}\n"
    "call circle_measurement_balanced();\n"
    "breakpoint circle_prepare_bp;\n"
    "}\n"
    "step check {\n"
    "check(success == true);\n"
    "check(result_object == \"CircleMeasurementOutput\");\n"
    "print(summary);\n"
    "}\n";
  return script;
}

std::string BuildCxcoreTemplateFeatureMatchCxscript(const ParserTestRequest &request)
{
  std::string script = MakeCxscriptMetadataHeader(request);
  script +=
    "step prepare {\n"
    "double match_floor = 0.7;\n"
    "{\n"
    "print(\"template.prepare\");\n"
    "checkpoint template_prepare_enter;\n"
    "}\n"
    "call template_feature_match();\n"
    "breakpoint template_prepare_bp;\n"
    "}\n"
    "step check {\n"
    "check(success == true);\n"
    "check(result_object == \"MatchOutput\");\n"
    "print(summary);\n"
    "}\n";
  return script;
}

std::string BuildCxcoreTorchHandoffTaskSummaryCxscript(const ParserTestRequest &request)
{
  std::string script = MakeCxscriptMetadataHeader(request);
  script +=
    "step prepare {\n"
    "string acceptance_gate = \"cxcore_code_cleaning_stage\";\n"
    "{\n"
    "print(\"torch_handoff.prepare\");\n"
    "checkpoint torch_handoff_prepare_enter;\n"
    "}\n"
    "call torch_handoff_task_summary();\n"
    "breakpoint torch_handoff_prepare_bp;\n"
    "}\n"
    "step check {\n"
    "check(readresult(\"published_handoff_type\") == \"TorchGeometryHandoff\");\n"
    "check(readresult(\"published_primary_ref\") == \"roi-main\");\n"
    "check(readresult(\"published_route_state\") == \"stay_in_cxcore\");\n"
    "check(readresult(\"published_result_ref\") == \"torch.result.geometry\");\n"
    "check(readresult(\"published_evidence_ref\") == \"torch.evidence.geometry\");\n"
    "check(readresult(\"published_bbox_candidate_list_ref\") == \"torch.bbox_candidates.main\");\n"
    "check(readresult(\"published_template_alignment_ref\") == \"torch.template_alignment.main\");\n"
    "check(readresult(\"published_template_test_alignment_status\") == \"aligned_pass\");\n"
    "check(readresult(\"published_roi_diff_candidate_ref\") == \"torch.roi_diff_candidates.main\");\n"
    "check(readresult(\"published_roi_diff_candidate_count\") == \"3\");\n"
    "check(readresult(\"published_prior_roi_region_ref\") == \"torch.prior_roi_region.main\");\n"
    "check(readresult(\"published_roi_crop_packet_ref\") == \"torch.roi_crop_packet.main\");\n"
    "check(readresult(\"published_roi_crop_count\") == \"3\");\n"
    "check(readresult(\"published_roi_crop_spatial_size\") == \"224x224\");\n"
    "check(readresult(\"published_roi_crop_policy_ref\") == \"torch.roi_crop_policy.main\");\n"
    "check(readresult(\"internal_test_interface_name\") == \"cxcore.internal.manual_ui_local_analysis\");\n"
    "check(readresult(\"internal_test_interface_purpose\") == \"modular_manual_ui_local_analysis\");\n"
    "check(readresult(\"execution_stage_0\") == \"cxcore_code_cleaning_stage\");\n"
    "check(readresult(\"execution_stage_1\") == \"cxcore_modular_manual_ui_acceptance\");\n"
    "check(readresult(\"execution_stage_2\") == \"remote_ai_semantic_acceptance\");\n"
    "check(readresult(\"execution_stage_3\") == \"atomic_semantic_acceptance\");\n"
    "print(summary);\n"
    "}\n";
  return script;
}

bool BuildCxcorePlan(const ParserTestRequest &request, ParserTestPlan &plan)
{
  // Router plans below are execution/test planning artifacts. They should stay
  // downstream of parser semantics and not become a substitute registration or
  // script-entry surface.
  if (request.layer == "smoke" && request.case_name == "line_measurement")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "101");
    plan.target.task_subtype = "cxcore_line_measurement";
    return true;
  }

  if (request.layer == "smoke" && request.case_name == "region_boundary_analysis")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "102");
    plan.target.task_subtype = "cxcore_region_boundary_analysis";
    return true;
  }

  if (request.layer == "smoke" && request.case_name == "template_feature_match")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "103");
    plan.target.task_subtype = "cxcore_template_feature_match";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_measurement")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1031");
    plan.target.task_subtype = "cxcore_line_measurement_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_measurement_probe")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10311");
    plan.target.task_subtype = "cxcore_line_measurement_probe_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_measurement_balanced")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10312");
    plan.target.task_subtype = "cxcore_line_measurement_balanced_cstyle_feature";
    plan.cxscript_text = BuildCxcoreLineMeasurementBalancedCxscript(request);
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_measurement_balanced_probe")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "103121");
    plan.target.task_subtype = "cxcore_line_measurement_balanced_probe_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_measurement_balanced_suite")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "103122");
    plan.target.task_subtype = "cxcore_line_measurement_balanced_suite_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_measurement_golden")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10313");
    plan.target.task_subtype = "cxcore_line_measurement_golden_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_measurement_boundary")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10314");
    plan.target.task_subtype = "cxcore_line_measurement_boundary_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_measurement_noise")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10315");
    plan.target.task_subtype = "cxcore_line_measurement_noise_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_measurement_degenerate")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10316");
    plan.target.task_subtype = "cxcore_line_measurement_degenerate_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "circle_measurement")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "104");
    plan.target.task_subtype = "cxcore_circle_measurement_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "circle_measurement_probe")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1041");
    plan.target.task_subtype = "cxcore_circle_measurement_probe_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "circle_measurement_balanced")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1042");
    plan.target.task_subtype = "cxcore_circle_measurement_balanced_cstyle_feature";
    plan.cxscript_text = BuildCxcoreCircleMeasurementBalancedCxscript(request);
    return true;
  }

  if (request.layer == "feature" && request.case_name == "circle_measurement_balanced_probe")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10421");
    plan.target.task_subtype = "cxcore_circle_measurement_balanced_probe_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "circle_measurement_golden")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1043");
    plan.target.task_subtype = "cxcore_circle_measurement_golden_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "circle_measurement_boundary")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1044");
    plan.target.task_subtype = "cxcore_circle_measurement_boundary_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "circle_measurement_noise")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1045");
    plan.target.task_subtype = "cxcore_circle_measurement_noise_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "circle_measurement_degenerate")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1046");
    plan.target.task_subtype = "cxcore_circle_measurement_degenerate_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "template_feature_match")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1032");
    plan.target.task_subtype = "cxcore_template_feature_match_feature";
    plan.cxscript_text = BuildCxcoreTemplateFeatureMatchCxscript(request);
    return true;
  }

  if (request.layer == "feature" && request.case_name == "template_feature_match_probe")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10321");
    plan.target.task_subtype = "cxcore_template_feature_match_probe_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "template_feature_match_golden")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10322");
    plan.target.task_subtype = "cxcore_template_feature_match_golden_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "template_feature_match_boundary")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10323");
    plan.target.task_subtype = "cxcore_template_feature_match_boundary_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "template_feature_match_noise")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10324");
    plan.target.task_subtype = "cxcore_template_feature_match_noise_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "template_feature_match_degenerate")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10325");
    plan.target.task_subtype = "cxcore_template_feature_match_degenerate_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name.find("rect_formfit_candidate_selection") == 0)
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10326");
    plan.target.task_subtype = "cxcore_rect_formfit_candidate_selection_contract";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "line_template_combo")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1033");
    plan.target.task_subtype = "cxcore_line_template_combo_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "region_boundary_analysis")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1034");
    plan.target.task_subtype = "cxcore_region_boundary_analysis_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "region_boundary_analysis_golden")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10341");
    plan.target.task_subtype = "cxcore_region_boundary_analysis_golden_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "region_boundary_analysis_boundary")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10342");
    plan.target.task_subtype = "cxcore_region_boundary_analysis_boundary_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "region_boundary_analysis_noise")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10343");
    plan.target.task_subtype = "cxcore_region_boundary_analysis_noise_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "region_boundary_analysis_degenerate")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10344");
    plan.target.task_subtype = "cxcore_region_boundary_analysis_degenerate_cstyle_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "ellipse_measurement")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "105");
    plan.target.task_subtype = "cxcore_ellipse_measurement_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "baseline_feature_export")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "106");
    plan.target.task_subtype = "cxcore_baseline_feature_export_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "ai_task_packaging")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "107");
    plan.target.task_subtype = "cxcore_ai_task_packaging_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "torch_handoff_task_summary")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10701");
    plan.target.task_subtype = "cxcore_ai_task_packaging_feature";
    plan.cxscript_text = BuildCxcoreTorchHandoffTaskSummaryCxscript(request);
    return true;
  }

  if (request.layer == "feature" && request.case_name == "feature_validation_suite")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1071");
    plan.target.task_subtype = "cxcore_feature_validation_suite";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "core4_feature_suite")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1072");
    plan.target.task_subtype = "cxcore_core4_feature_suite";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "baseline_upstream_combo")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10721");
    plan.target.task_subtype = "cxcore_baseline_upstream_combo_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "ai_route_ready_combo")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10722");
    plan.target.task_subtype = "cxcore_ai_route_ready_combo_feature";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "bridge_flow_suite")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10723");
    plan.target.task_subtype = "cxcore_bridge_flow_suite";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "classical_flow_suite")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10724");
    plan.target.task_subtype = "cxcore_classical_flow_suite";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "feature_stage1_gate_suite")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "10725");
    plan.target.task_subtype = "cxcore_feature_stage1_gate_suite";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "core3_probe_suite")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1073");
    plan.target.task_subtype = "cxcore_core3_probe_suite";
    return true;
  }

  if (request.layer == "scenario" && request.case_name == "classical_analysis")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "108");
    plan.target.task_subtype = "cxcore_classical_analysis";
    return true;
  }

  if (request.layer == "train" && request.case_name == "baseline_feature_train")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "109");
    plan.target.task_subtype = "cxcore_baseline_feature_train";
    return true;
  }

  if (request.layer == "infer" && request.case_name == "boundary_result_infer_route")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "110");
    plan.target.task_subtype = "cxcore_boundary_result_infer_route";
    return true;
  }

  if (request.layer == "infer" && request.case_name == "region_detection")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "111");
    plan.target.task_subtype = "cxcore_region_detection";
    return true;
  }

  if (request.layer == "smoke" && request.case_name == "minimal_binding")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.requires_binding = true;
    plan.pseudo_class = BuildImageProbePseudoClass();
    plan.target = MakeDeliveryTarget(
      MakeImageProcessRequest("cxcore.smoke.minimal_binding",
                              "trace.cxcore.smoke.minimal_binding",
                              "cxparser_test_driver",
                              plan.pseudo_class.module_name,
                              plan.pseudo_class.parser_alias,
                              "Score",
                              "ImageProbe probe;probe.Load(\"image.png\");probe.Detect(0.8);probe.Score();"));
    plan.target.task_type = task_constants::TaskTypeCoreTest();
    plan.target.task_subtype = task_constants::TaskSubtypeParserEval();
    return true;
  }

  if (request.layer == "feature" && request.case_name == "image_operator_min")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.requires_binding = true;
    plan.pseudo_class = BuildImageProbePseudoClass();
    plan.target = MakeDeliveryTarget(
      MakeImageProcessRequest("cxcore.feature.image_operator_min",
                              "trace.cxcore.feature.image_operator_min",
                              "cxparser_test_driver",
                              plan.pseudo_class.module_name,
                              plan.pseudo_class.parser_alias,
                              "Score",
                              "ImageProbe probe;probe.Load(\"feature.png\");probe.Detect(0.6);probe.Score();"));
    plan.target.task_type = task_constants::TaskTypeCoreTest();
    plan.target.task_subtype = task_constants::TaskSubtypeImageProcess();
    return true;
  }

  if (request.layer == "scenario" && request.case_name == "image_analysis_baseline")
  {
    plan.kind = ptpk_image_analysis;
    plan.supported = true;
    plan.image_request.task_id = "cxcore.scenario.image_analysis_baseline";
    plan.image_request.trace_id = "trace.cxcore.scenario.image_analysis_baseline";
    plan.image_request.module_name = "cxcore";
    plan.image_request.route_hint = task_constants::RouteDefault();
    plan.image_request.image.width = 4;
    plan.image_request.image.height = 4;
    plan.image_request.image.channels = 1;
    plan.image_request.image.bytes.resize(16, 1);

    ImageAnalysisRoiInput roi;
    roi.roi_id = "roi_0";
    roi.bounds.x = 1;
    roi.bounds.y = 1;
    roi.bounds.width = 2;
    roi.bounds.height = 2;
    roi.purpose = "baseline";
    plan.image_request.rois.push_back(roi);
    plan.image_request.operations.push_back(iao_roi_extract);
    plan.image_request.operations.push_back(iao_boundary_trace);
    plan.image_request.operations.push_back(iao_fit_line);
    plan.image_request.operations.push_back(iao_fit_circle);
    plan.image_request.operations.push_back(iao_fit_ellipse);
    plan.image_request.operations.push_back(iao_template_match);
    return true;
  }

  plan.reason = "cxcore case is not registered in first-stage router";
  return false;
}

bool BuildCximagePlan(const ParserTestRequest &request, ParserTestPlan &plan)
{
  if (request.layer == "operator" && request.case_name == "roi_threshold")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "201");
    plan.target.task_subtype = "cximage_roi_threshold";
    return true;
  }

  if (request.layer == "operator" && request.case_name == "roi_edge")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "202");
    plan.target.task_subtype = "cximage_roi_edge";
    return true;
  }

  if (request.layer == "operator" && request.case_name == "roi_gray_count")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "203");
    plan.target.task_subtype = "cximage_roi_gray_count";
    return true;
  }

  if (request.layer == "matcher" && request.case_name == "fastmatch_template")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "204");
    plan.target.task_subtype = "cximage_fastmatch_template";
    return true;
  }

  if (request.layer == "matcher" && request.case_name == "findobject_region")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "205");
    plan.target.task_subtype = "cximage_findobject_region";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "binary_region")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "206");
    plan.target.task_subtype = "cximage_binary_region";
    return true;
  }

  if (request.layer == "feature" && request.case_name == "geometry_topology_pipeline")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "208");
    plan.target.task_subtype = "cximage_geometry_topology_pipeline";
    return true;
  }

  if (request.layer == "embedded_model" && request.case_name == "fastmatch_image_model")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "207");
    plan.target.task_subtype = "cximage_fastmatch_image_model";
    return true;
  }

  plan.reason = "cximage case is not registered in first-stage router";
  return false;
}

bool BuildIntegrationPlan(const ParserTestRequest &request, ParserTestPlan &plan)
{
  if (request.layer == "smoke" && request.case_name == "classical_boundary_smoke")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "301");
    plan.target.task_subtype = "integration_classical_boundary_smoke";
    return true;
  }

  if (request.layer == "smoke" && request.case_name == "template_match_smoke")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "302");
    plan.target.task_subtype = "integration_template_match_smoke";
    return true;
  }

  if (request.layer == "scenario" && request.case_name == "board_inspection_scenario")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "303");
    plan.target.task_subtype = "integration_board_inspection_scenario";
    return true;
  }

  if (request.layer == "train" && request.case_name == "baseline_feature_train")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "304");
    plan.target.task_subtype = "integration_baseline_feature_train";
    return true;
  }

  if (request.layer == "infer" && request.case_name == "classical_infer_route")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "305");
    plan.target.task_subtype = "integration_classical_infer_route";
    return true;
  }

  if (request.layer == "infer" && request.case_name == "detection_infer")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "306");
    plan.target.task_subtype = "integration_detection_infer";
    return true;
  }

  plan.reason = "integration case is not registered in first-stage router";
  return false;
}

bool BuildMlpackPlan(const ParserTestRequest &request, ParserTestPlan &plan)
{
  if (request.layer == "smoke" &&
      IsOneOf(request.case_name, {"baseline_entry_min"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "mlpack_smoke";
    return true;
  }

  if (request.layer == "feature" &&
      IsOneOf(request.case_name, {"model_entry_min",
                                  "minimal_model"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "5");
    plan.target.task_subtype = "mlpack_feature";
    return true;
  }

  if (request.layer == "feature" &&
      IsOneOf(request.case_name, {"baseline_feature_all_v1",
                                  "baseline_feature_core_geom",
                                  "baseline_feature_match_only"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    const std::string scalar =
      request.case_name == "baseline_feature_all_v1" ? "50" :
      request.case_name == "baseline_feature_core_geom" ? "32" : "9";
    plan.target = MakeParserEvalTarget(request, scalar);
    plan.target.task_subtype = "mlpack_baseline_feature";
    return true;
  }

  if (request.layer == "train" &&
      IsOneOf(request.case_name, {"min_train",
                                  "minimal_train"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "0.5");
    plan.target.task_subtype = "mlpack_train";
    return true;
  }

  if (request.layer == "train" &&
      IsOneOf(request.case_name, {"baseline_train_logreg_min",
                                  "baseline_train_rf_min",
                                  "baseline_train_knn_min",
                                  "baseline_logreg_flow_min",
                                  "baseline_knn_flow_min"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    const std::string scalar =
      (request.case_name == "baseline_train_logreg_min" ||
       request.case_name == "baseline_logreg_flow_min") ? "0.51" :
      request.case_name == "baseline_train_rf_min" ? "0.67" : "0.58";
    plan.target = MakeParserEvalTarget(request, scalar);
    plan.target.task_subtype = "mlpack_baseline_train";
    return true;
  }

  if (request.layer == "infer" &&
      IsOneOf(request.case_name, {"min_infer",
                                  "minimal_infer"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "0.9");
    plan.target.task_subtype = "mlpack_infer";
    return true;
  }

  if (request.layer == "infer" &&
      IsOneOf(request.case_name, {"baseline_infer_logreg_min",
                                  "baseline_infer_rf_min",
                                  "baseline_infer_knn_min",
                                  "baseline_logreg_flow_min",
                                  "baseline_knn_flow_min"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    const std::string scalar =
      (request.case_name == "baseline_infer_logreg_min" ||
       request.case_name == "baseline_logreg_flow_min") ? "0.73" :
      request.case_name == "baseline_infer_rf_min" ? "0.88" : "0.79";
    plan.target = MakeParserEvalTarget(request, scalar);
    plan.target.task_subtype = "mlpack_baseline_infer";
    return true;
  }

  if (request.layer == "score" &&
      IsOneOf(request.case_name, {"baseline_score_classification_min",
                                  "baseline_classification_flow_min",
                                  "baseline_cluster_ref_min",
                                  "baseline_distance_ref_min",
                                  "baseline_anomaly_ref_min"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    const std::string scalar =
      request.case_name == "baseline_cluster_ref_min" ? "0.31" :
      request.case_name == "baseline_distance_ref_min" ? "0.42" :
      request.case_name == "baseline_anomaly_ref_min" ? "0.27" : "0.84";
    plan.target = MakeParserEvalTarget(request, scalar);
    plan.target.task_subtype =
      request.case_name == "baseline_cluster_ref_min" ? "mlpack_semantic_cluster" :
      request.case_name == "baseline_distance_ref_min" ? "mlpack_semantic_distance" :
      request.case_name == "baseline_anomaly_ref_min" ? "mlpack_semantic_anomaly" :
      "mlpack_baseline_score";
    return true;
  }

  if (request.layer == "scenario" &&
      request.case_name == "baseline_logreg_chain_min")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "0.84");
    plan.target.task_subtype = "mlpack_baseline_chain";
    return true;
  }

  if (request.layer == "scenario" &&
      request.case_name == "baseline_knn_chain_min")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "0.84");
    plan.target.task_subtype = "mlpack_baseline_chain";
    return true;
  }

  if (request.layer == "scenario" &&
      request.case_name == "baseline_pair_compare_min")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "0.84");
    plan.target.task_subtype = "mlpack_baseline_chain";
    return true;
  }

  plan.reason = "mlpack case is not registered in first-stage router";
  return false;
}

bool BuildTorchPlan(const ParserTestRequest &request, ParserTestPlan &plan)
{
  if (request.layer == "smoke" && request.case_name == "torch_runner_min")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_smoke";
    return true;
  }

  if (request.layer == "train" && request.case_name == "yolo_min_train")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "0.25");
    plan.target.route_hint = task_constants::RouteBatch();
    plan.target.priority_hint = "background";
    plan.target.task_subtype = "torch_train";
    return true;
  }

  if (request.layer == "infer" && request.case_name == "yolo_min_infer")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "0.7");
    plan.target.route_hint = task_constants::RouteRealtime();
    plan.target.priority_hint = task_constants::RouteRealtime();
    plan.target.task_subtype = "torch_infer";
    return true;
  }

  if (request.layer == "feature" &&
      IsOneOf(request.case_name, {"mobilevit_roi_patch_class_label_contract",
                                  "torch.mobilevit.session.feature"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_mobilevit_contract";
    return true;
  }

  if (request.layer == "feature" &&
      IsOneOf(request.case_name, {"torch.resnet50.baseline.feature"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_resnet50_baseline_feature";
    return true;
  }

  if (request.layer == "infer" &&
      IsOneOf(request.case_name, {"torch.mobilevit.unified.infer"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_mobilevit_unified_infer";
    return true;
  }

  if (request.layer == "feature" &&
      IsOneOf(request.case_name, {"deeplab_region_tensor_mask_label_contract",
                                  "torch.deeplab.contract.feature"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_deeplab_contract";
    return true;
  }

  if (request.layer == "infer" &&
      IsOneOf(request.case_name, {"torch.deeplab.unified.infer"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_deeplab_unified_infer";
    return true;
  }

  if (request.layer == "infer" &&
      IsOneOf(request.case_name, {"torch.resnet50.baseline.infer"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_resnet50_baseline_infer";
    return true;
  }

  if (request.layer == "train" &&
      IsOneOf(request.case_name, {"torch.yolov8.mainline.train"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.route_hint = task_constants::RouteBatch();
    plan.target.priority_hint = "background";
    plan.target.task_subtype = "torch_yolov8_mainline_train";
    return true;
  }

  if (request.layer == "train" &&
      IsOneOf(request.case_name, {"torch.mobilevit.mainline.train"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.route_hint = task_constants::RouteBatch();
    plan.target.priority_hint = "background";
    plan.target.task_subtype = "torch_mobilevit_mainline_train";
    return true;
  }

  if (request.layer == "train" &&
      IsOneOf(request.case_name, {"torch.deeplab.mainline.train"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.route_hint = task_constants::RouteBatch();
    plan.target.priority_hint = "background";
    plan.target.task_subtype = "torch_deeplab_mainline_train";
    return true;
  }

  if (request.layer == "feature" &&
      IsOneOf(request.case_name, {"yolov8_image_window_bbox_class_targets_contract",
                                  "torch.yolov8.eval.feature"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_yolov8_contract";
    return true;
  }

  if (request.layer == "scenario" &&
      IsOneOf(request.case_name, {"torch.yolo_mobilevit.infer.scenario"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.route_hint = task_constants::RouteDefault();
    plan.target.task_subtype = "torch_yolo_mobilevit_infer_scenario";
    return true;
  }

  if (request.layer == "feature" &&
      IsOneOf(request.case_name, {"torch.resnet18.baseline.feature"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_resnet18_baseline_feature";
    return true;
  }

  if (request.layer == "infer" &&
      IsOneOf(request.case_name, {"torch.resnet18.baseline.infer"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "torch_resnet18_baseline_infer";
    return true;
  }

  plan.reason = "torch case is not registered in first-stage router";
  return false;
}

bool BuildRagPlan(const ParserTestRequest &request, ParserTestPlan &plan)
{
  if (request.layer == "smoke" && request.case_name == "rag_query_min")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "2");
    plan.target.task_subtype = "rag_smoke";
    return true;
  }

  if (request.layer == "scenario" && request.case_name == "rag_script_assist")
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "5");
    plan.target.task_subtype = "rag_scenario";
    return true;
  }

  plan.reason = "rag case is not registered in first-stage router";
  return false;
}

bool BuildEnsmallenPlan(const ParserTestRequest &request, ParserTestPlan &plan)
{
  if (request.layer == "feature" &&
      IsOneOf(request.case_name, {"geometry_fit_tuning",
                                  "match_score_tuning",
                                  "circle_param_opt",
                                  "ellipse_param_opt",
                                  "match_score_opt"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "ensmallen_flow_host_feature";
    return true;
  }

  if (request.layer == "scenario" &&
      IsOneOf(request.case_name, {"phase1_param_replay",
                                  "halcon_circle_plate_geometry_replay"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "ensmallen_flow_host_scenario";
    return true;
  }

  if (request.layer == "train" &&
      IsOneOf(request.case_name, {"phase1_param_opt",
                                  "halcon_screws_cluster_stability"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "ensmallen_flow_host_train";
    return true;
  }

  if (request.layer == "infer" &&
      IsOneOf(request.case_name, {"phase1_param_eval",
                                  "halcon_universal_joint_match_eval",
                                  "halcon_pcb_focus_interaction_eval"}))
  {
    plan.kind = ptpk_execution_target;
    plan.supported = true;
    plan.target = MakeParserEvalTarget(request, "1");
    plan.target.task_subtype = "ensmallen_flow_host_infer";
    return true;
  }

  plan.reason = "ensmallen_layer case is not registered in first-stage router";
  return false;
}
}

bool BuildTestPlan(const ParserTestRequest &request, ParserTestPlan &plan)
{
  plan = ParserTestPlan();

  if (request.case_name.find("torch.") == 0)
  {
    ParserTestRequest torch_request = request;
    torch_request.module = "torch";
    return BuildTorchPlan(torch_request, plan);
  }

  if (request.module == "cxcore")
    return BuildCxcorePlan(request, plan);
  if (request.module == "cximage")
    return BuildCximagePlan(request, plan);
  if (request.module == "integration")
    return BuildIntegrationPlan(request, plan);
  if (request.module == "mlpack")
    return BuildMlpackPlan(request, plan);
  if (request.module == "torch" || request.module == "torch_module")
    return BuildTorchPlan(request, plan);
  if (request.module == "rag")
    return BuildRagPlan(request, plan);
  if (request.module == "ensmallen_layer")
    return BuildEnsmallenPlan(request, plan);

  plan.reason = "module is not registered in first-stage router";
  return false;
}
}