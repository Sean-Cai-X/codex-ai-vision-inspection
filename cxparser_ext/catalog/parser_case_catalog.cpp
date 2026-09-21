#include "parser_case_catalog.h"

#include "../drivers/parser_dispatch_driver.h"
#include "../pipeline/parser_task_types.h"

namespace cxparser_ext
{
namespace
{
std::vector<std::string> SplitCaseIdentityParts(const std::string &case_id)
{
  std::vector<std::string> parts;
  std::string current;
  for (size_t i = 0; i < case_id.size(); ++i)
  {
    if (case_id[i] == '.')
    {
      if (!current.empty())
        parts.push_back(current);
      current.clear();
      continue;
    }

    current.push_back(case_id[i]);
  }

  if (!current.empty())
    parts.push_back(current);
  return parts;
}

std::string NormalizePublicDispatchCaseId(const ParserDispatchRequest &request)
{
  if (request.case_id.empty())
    return std::string();

  const std::vector<std::string> parts = SplitCaseIdentityParts(request.case_id);
  if (parts.size() == 4 &&
      !request.module.empty() &&
      parts[1] == request.module &&
      parts[3] == request.layer)
    return parts[2];

  if (parts.size() == 4 &&
      !request.integration.empty() &&
      parts[1] == request.integration &&
      parts[3] == request.layer)
    return parts[2];

  return request.case_id;
}

ParserDispatchRequest NormalizeDispatchRequest(const ParserDispatchRequest &request)
{
  ParserDispatchRequest normalized = request;
  if (normalized.module == "torch")
    normalized.module = "torch_module";
  normalized.case_id = NormalizePublicDispatchCaseId(request);
  return normalized;
}

bool MatchRequest(const ParserDispatchRequest &request,
                  const char *layer,
                  const char *module,
                  const char *case_id)
{
  return request.layer == layer &&
         request.module == module &&
         request.case_id == case_id;
}

bool IsCxcoreContractScriptCase(const ParserDispatchRequest &request)
{
  if (request.layer != "feature" || request.module != "cxcore")
    return false;

  const std::string &case_id = request.case_id;
  if (case_id.find("rect_formfit_candidate_selection") == 0)
    return true;
  return case_id == "line_measurement_golden" ||
         case_id == "line_measurement_boundary" ||
         case_id == "line_measurement_noise" ||
         case_id == "line_measurement_degenerate" ||
         case_id == "circle_measurement_golden" ||
         case_id == "circle_measurement_boundary" ||
         case_id == "circle_measurement_noise" ||
         case_id == "circle_measurement_degenerate" ||
         case_id == "template_feature_match_golden" ||
         case_id == "template_feature_match_boundary" ||
         case_id == "template_feature_match_noise" ||
         case_id == "template_feature_match_degenerate" ||
         case_id == "region_boundary_analysis_golden" ||
         case_id == "region_boundary_analysis_boundary" ||
         case_id == "region_boundary_analysis_noise" ||
         case_id == "region_boundary_analysis_degenerate";
}

bool IsTorchContractScriptCase(const ParserDispatchRequest &request)
{
  if (request.module != "torch_module")
    return false;

  const std::string &case_id = request.case_id;
  if (request.layer == "feature")
  {
    return case_id == "mobilevit_roi_patch_class_label_contract" ||
           case_id == "torch.mobilevit.session.feature" ||
           case_id == "torch.resnet50.baseline.feature" ||
           case_id == "torch.resnet18.baseline.feature" ||

           case_id == "deeplab_region_tensor_mask_label_contract" ||
           case_id == "torch.deeplab.contract.feature" ||
           case_id == "yolov8_image_window_bbox_class_targets_contract" ||
           case_id == "torch.yolov8.eval.feature";
  }

  if (request.layer == "infer")
  {
    return case_id == "torch.mobilevit.unified.infer" ||
           case_id == "torch.deeplab.unified.infer" ||
           case_id == "torch.resnet18.baseline.infer" ||

           case_id == "torch.resnet50.baseline.infer";
  }

  if (request.layer == "train")
  {
    return case_id == "torch.yolov8.mainline.train" ||
           case_id == "torch.mobilevit.mainline.train" ||
           case_id == "torch.deeplab.mainline.train";
  }

  if (request.layer == "scenario")
    return case_id == "torch.yolo_mobilevit.infer.scenario";

  return false;
}

const char *NormalizeTorchContractCaseId(const std::string &case_id)
{
  if (case_id == "mobilevit_roi_patch_class_label_contract" ||
      case_id == "torch.mobilevit.session.feature")
    return "mobilevit_roi_patch_class_label_contract";

  if (case_id == "deeplab_region_tensor_mask_label_contract" ||
      case_id == "torch.deeplab.contract.feature")
    return "deeplab_region_tensor_mask_label_contract";

  if (case_id == "yolov8_image_window_bbox_class_targets_contract" ||
      case_id == "torch.yolov8.eval.feature")
    return "yolov8_image_window_bbox_class_targets_contract";

  if (case_id == "torch.mobilevit.unified.infer")
    return "torch.mobilevit.unified.infer";

  if (case_id == "torch.deeplab.unified.infer")
    return "torch.deeplab.unified.infer";


  if (case_id == "torch.resnet18.baseline.feature")
    return "torch.resnet18.baseline.feature";

  if (case_id == "torch.resnet18.baseline.infer")
    return "torch.resnet18.baseline.infer";

  if (case_id == "torch.resnet50.baseline.feature")
    return "torch.resnet50.baseline.feature";

  if (case_id == "torch.resnet50.baseline.infer")
    return "torch.resnet50.baseline.infer";

  if (case_id == "torch.yolov8.mainline.train")
    return "torch.yolov8.mainline.train";

  if (case_id == "torch.mobilevit.mainline.train")
    return "torch.mobilevit.mainline.train";

  if (case_id == "torch.deeplab.mainline.train")
    return "torch.deeplab.mainline.train";

  if (case_id == "torch.yolo_mobilevit.infer.scenario")
    return "torch.yolo_mobilevit.infer.scenario";

  return 0;
}

bool IsRagPairedReplayScriptCase(const ParserDispatchRequest &request)
{
  return request.script_type == "integration" &&
         request.integration == "rag_torch" &&
         request.layer == "scenario" &&
         request.case_id == "paired_replay_transport_switch";
}

void FillCoreCase(ParserDispatchCaseSpec &spec,
                  const char *layer,
                  const char *module,
                  const char *case_id,
                  const char *route,
                  const char *task_subtype,
                  const char *target_class,
                  const char *target_method,
                  const char *script_text)
{
  spec.layer = layer;
  spec.module = module;
  spec.case_id = case_id;
  spec.route = route;
  spec.task_subtype = task_subtype;
  spec.target_class = target_class;
  spec.target_method = target_method;
  spec.script_text = script_text;
  spec.state = "active";
  spec.active_runtime = true;
}

void FillCxcoreContractScriptCase(ParserDispatchCaseSpec &spec,
                                  const char *case_id)
{
  spec = ParserDispatchCaseSpec();
  spec.layer = "feature";
  spec.module = "cxcore";
  spec.case_id = case_id;
  spec.script_path =
    std::string("cxparser/rag_script_cases/cxcore/feature/cxcore_") +
    case_id + "_cstyle_feature.cxsc";
  spec.route = task_constants::RouteDefault();
  spec.task_subtype = case_id;
  spec.target_class = "cxcore_contract_script";
  spec.target_method = "execute";
  spec.state = "active";
  spec.active_runtime = true;
  spec.uses_cxcore_contract_mainline = true;
}

void FillTorchContractScriptCase(ParserDispatchCaseSpec &spec,
                                 const char *layer,
                                 const char *case_id,
                                 const char *script_path)
{
  spec = ParserDispatchCaseSpec();
  spec.layer = layer;
  spec.module = "torch_module";
  spec.case_id = case_id;
  spec.script_path = script_path;
  spec.route = task_constants::RouteDefault();
  spec.task_subtype = case_id;
  spec.target_class = "torch_contract_script";
  spec.target_method = "execute";
  spec.state = "active";
  spec.active_runtime = true;
  spec.uses_torch_contract_mainline = true;
}

void FillPlannedCase(ParserDispatchCaseSpec &spec,
                     const char *layer,
                     const char *module,
                     const char *case_id,
                     const char *state)
{
  spec.layer = layer;
  spec.module = module;
  spec.case_id = case_id;
  spec.state = state;
  spec.active_runtime = false;
}

void FillPlannedScriptCase(ParserDispatchCaseSpec &spec,
                           const char *layer,
                           const char *module,
                           const char *case_id,
                           const char *script_path,
                           const char *state)
{
  FillPlannedCase(spec, layer, module, case_id, state);
  spec.script_path = script_path;
}

void FillActiveScriptCase(ParserDispatchCaseSpec &spec,
                          const char *layer,
                          const char *module,
                          const char *case_id,
                          const char *script_path,
                          const char *route,
                          const char *task_subtype,
                          const char *target_class,
                          const char *target_method)
{
  spec = ParserDispatchCaseSpec();
  spec.layer = layer;
  spec.module = module;
  spec.case_id = case_id;
  spec.script_path = script_path;
  spec.route = route;
  spec.task_subtype = task_subtype;
  spec.target_class = target_class;
  spec.target_method = target_method;
  spec.state = "active";
  spec.active_runtime = true;
}

void FillCximageModuleScriptCase(ParserDispatchCaseSpec &spec,
                                 const char *layer,
                                 const char *case_id,
                                 const char *script_path,
                                 const char *task_subtype)
{
  FillActiveScriptCase(spec,
                       layer,
                       "cximage",
                       case_id,
                       script_path,
                       task_constants::RouteDefault(),
                       task_subtype,
                       "cximage_module_case",
                       "execute");
}

void FillMlpackBaselineScriptCase(ParserDispatchCaseSpec &spec,
                                  const char *layer,
                                  const char *case_id,
                                  const char *script_path,
                                  const char *task_subtype)
{
  FillActiveScriptCase(spec,
                       layer,
                       "mlpack",
                       case_id,
                       script_path,
                       task_constants::RouteDefault(),
                       task_subtype,
                       "mlpack_contract_script",
                       "execute");
  spec.uses_mlpack_baseline_mainline = true;
}

void FillMlpackBaselineScenarioCase(ParserDispatchCaseSpec &spec,
                                    const char *case_id,
                                    const char *script_path,
                                    const char *task_subtype)
{
  FillActiveScriptCase(spec,
                       "scenario",
                       "mlpack",
                       case_id,
                       script_path,
                       task_constants::RouteDefault(),
                       task_subtype,
                       "mlpack_contract_script",
                       "execute");
  spec.uses_mlpack_baseline_mainline = true;
}

void FillMlpackMinimalScriptCase(ParserDispatchCaseSpec &spec,
                                 const char *layer,
                                 const char *case_id,
                                 const char *script_path,
                                 const char *task_subtype)
{
  FillMlpackBaselineScriptCase(spec, layer, case_id, script_path, task_subtype);
}
}

bool ResolveDispatchCase(const ParserDispatchRequest &raw_request,
                         ParserDispatchCaseSpec &spec)
{
  const ParserDispatchRequest request = NormalizeDispatchRequest(raw_request);
  spec = ParserDispatchCaseSpec();

  if (MatchRequest(request, "smoke", "cxcore", "minimal_host"))
  {
    FillCoreCase(spec,
                 "smoke",
                 "cxcore",
                 "minimal_host",
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeParserEval(),
                 "native_expr",
                 "eval",
                 "1+2");
    return true;
  }

  if (MatchRequest(request, "feature", "cxcore", "image_probe_score"))
  {
    FillCoreCase(spec,
                 "feature",
                 "cxcore",
                 "image_probe_score",
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeImageProcess(),
                 "ImageProbe",
                 "Score",
                 "ImageProbe probe;probe.Load(\"image.png\");probe.Detect(0.8);probe.Score();");
    spec.requires_image_probe_binding = true;
    return true;
  }

  if (IsCxcoreContractScriptCase(request))
  {
    FillCxcoreContractScriptCase(spec, request.case_id.c_str());
    return true;
  }

  if (IsTorchContractScriptCase(request))
  {
    const char *normalized_case_id = NormalizeTorchContractCaseId(request.case_id);
    if (normalized_case_id == 0)
      return false;

    if (std::string(normalized_case_id) == "mobilevit_roi_patch_class_label_contract")
    {
      FillTorchContractScriptCase(spec,
                                  "feature",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/feature/torch_mobilevit_session_feature.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "deeplab_region_tensor_mask_label_contract")
    {
      FillTorchContractScriptCase(spec,
                                  "feature",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/feature/torch_deeplab_contract_feature.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "yolov8_image_window_bbox_class_targets_contract")
    {
      FillTorchContractScriptCase(spec,
                                  "feature",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/feature/torch_yolov8_eval_feature.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.mobilevit.unified.infer")
    {
      FillTorchContractScriptCase(spec,
                                  "infer",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/infer/torch_mobilevit_unified_infer.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.deeplab.unified.infer")
    {
      FillTorchContractScriptCase(spec,
                                  "infer",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/infer/torch_deeplab_unified_infer.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.yolo_mobilevit.infer.scenario")
    {
      FillTorchContractScriptCase(spec,
                                  "scenario",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/scenario/torch_yolo_mobilevit_infer_scenario.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.resnet18.baseline.feature")
    {
      FillTorchContractScriptCase(spec,
                                  "feature",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/feature/torch_resnet18_baseline_feature.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.resnet18.baseline.infer")
    {
      FillTorchContractScriptCase(spec,
                                  "infer",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/infer/torch_resnet18_baseline_infer.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.resnet50.baseline.feature")
    {
      FillTorchContractScriptCase(spec,
                                  "feature",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/feature/torch_resnet50_baseline_feature.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.resnet50.baseline.infer")
    {
      FillTorchContractScriptCase(spec,
                                  "infer",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/infer/torch_resnet50_baseline_infer.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.yolov8.mainline.train")
    {
      FillTorchContractScriptCase(spec,
                                  "train",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/train/torch_yolov8_mainline_train.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.mobilevit.mainline.train")
    {
      FillTorchContractScriptCase(spec,
                                  "train",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/train/torch_mobilevit_mainline_train.cxsc");
      return true;
    }

    if (std::string(normalized_case_id) == "torch.deeplab.mainline.train")
    {
      FillTorchContractScriptCase(spec,
                                  "train",
                                  normalized_case_id,
                                  "cxparser/rag_script_cases/torch_module/train/torch_deeplab_mainline_train.cxsc");
      return true;
    }
  }

  if (MatchRequest(request, "feature", "cxcore", "image_probe_flow_exec_v1"))
  {
    FillCoreCase(spec,
                 "feature",
                 "cxcore",
                 "image_probe_flow_exec_v1",
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeImageProcess(),
                 "ImageProbe",
                 "Score",
                 "");
    spec.requires_image_probe_binding = true;
    return true;
  }

  if (MatchRequest(request, "feature", "cxcore", "flow_numeric_bridge_v1"))
  {
    FillCoreCase(spec,
                 "feature",
                 "cxcore",
                 "flow_numeric_bridge_v1",
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeParserEval(),
                 "native_expr",
                 "eval",
                 "double a=1;double d=0;if(a>0){d=d+1;}d;");
    spec.requires_image_probe_binding = true;
    return true;
  }

  if (MatchRequest(request, "feature", "cxcore", "flow_numeric_safe_bridge_v1"))
  {
    FillCoreCase(spec,
                 "feature",
                 "cxcore",
                 "flow_numeric_safe_bridge_v1",
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeParserEval(),
                 "native_expr",
                 "eval",
                 "");
    spec.requires_image_probe_binding = true;
    return true;
  }

  if (MatchRequest(request, "feature", "cxcore", "flow_numeric_if_probe_v1"))
  {
    FillCoreCase(spec,
                 "feature",
                 "cxcore",
                 "flow_numeric_if_probe_v1",
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeParserEval(),
                 "native_expr",
                 "eval",
                 "");
    spec.requires_image_probe_binding = true;
    return true;
  }

  if (MatchRequest(request, "scenario", "cxcore", "image_probe_realtime"))
  {
    FillCoreCase(spec,
                 "scenario",
                 "cxcore",
                 "image_probe_realtime",
                 task_constants::RouteRealtime(),
                 task_constants::TaskSubtypeVideoFrame(),
                 "ImageProbe",
                 "Score",
                 "ImageProbe probe;probe.Load(\"frame.jpg\");probe.Detect(0.6);probe.Score();");
    spec.requires_image_probe_binding = true;
    return true;
  }

  if (MatchRequest(request, "smoke", "rag", "parser_entry"))
  {
    FillCoreCase(spec,
                 "smoke",
                 "rag",
                 "parser_entry",
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeParserEval(),
                 "native_expr",
                 "eval",
                 "40+2");
    return true;
  }

  if (MatchRequest(request, "feature", "rag", "parser_error_probe"))
  {
    FillCoreCase(spec,
                 "feature",
                 "rag",
                 "parser_error_probe",
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeParserEval(),
                 "native_expr",
                 "eval",
                 "3+");
    return true;
  }

  if (MatchRequest(request, "scenario", "rag", "replay_probe"))
  {
    FillCoreCase(spec,
                 "scenario",
                 "rag",
                 "replay_probe",
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeImageProcess(),
                 "ImageProbe",
                 "Score",
                 "ImageProbe probe;probe.Load(\"rag.png\");probe.Detect(0.5);probe.Score();");
    spec.requires_image_probe_binding = true;
    spec.replay_after_run = true;
    return true;
  }

  if (request.script_type == "integration" &&
      request.integration == "video" &&
      request.layer == "infer" &&
      request.case_id == "video_frame_chain")
  {
    spec.layer = "infer";
    spec.module = "torch";
    spec.case_id = "video_frame_chain";
    spec.script_path = "cxscript/integration/video/infer.video_frame_chain.cxs";
    spec.route = task_constants::RouteRealtime();
    spec.task_subtype = "torch_video_frame_chain";
    spec.target_class = "native_expr";
    spec.target_method = "eval";
    spec.state = "planned_clang_bridge_probe";
    spec.active_runtime = false;
    return true;
  }

  if (MatchRequest(request, "smoke", "cxgeom", "cxgeom_bulk_create_presentation_release") ||
      MatchRequest(request, "feature", "cxgeom", "cxgeom_scene_mapping_publish") ||
      MatchRequest(request, "smoke", "cxcloud", "cxcloud_bulk_create_render_release") ||
      MatchRequest(request, "feature", "cxcloud", "cxcloud_scene_mapping_publish") ||
      MatchRequest(request, "scenario", "cxcore", "cxcore_mixed_scene_refresh_latency_watch") ||
      MatchRequest(request, "scenario", "cxparser_ext", "cxparser_ext_drag_refresh_watch") ||
      MatchRequest(request, "train", "torch_module", "torch_module_minimal_image_train") ||
      MatchRequest(request, "infer", "torch_module", "torch_module_minimal_image_infer"))
  {
    FillPlannedCase(spec,
                    request.layer.c_str(),
                    request.module.c_str(),
                    request.case_id.c_str(),
                    "planned_flow_validation_adapter");
    return true;
  }

  if ((MatchRequest(request, "feature", "rag", "geometry_fit_writeback") ||
       MatchRequest(request, "feature", "rag", "match_score_writeback")))
  {
    FillPlannedCase(spec,
                    request.layer.c_str(),
                    "rag",
                    request.case_id.c_str(),
                    "planned_rag_writeback_adapter");
    return true;
  }

  if (request.script_type == "integration" &&
      request.integration == "rag_torch" &&
      request.layer == "scenario" &&
      request.case_id == "test_host_replay")
  {
    FillCoreCase(spec,
                 "scenario",
                 "rag",
                 "test_host_replay",
                 task_constants::RouteReplay(),
                 task_constants::TaskSubtypeImageProcess(),
                 "ImageProbe",
                 "Score",
                 "ImageProbe probe;probe.Load(\"rag_torch.png\");probe.Detect(0.5);probe.Score();");
    spec.requires_image_probe_binding = true;
    spec.replay_after_run = true;
    return true;
  }

  if (IsRagPairedReplayScriptCase(request))
  {
    FillActiveScriptCase(spec,
                         "scenario",
                         "rag",
                         "paired_replay_transport_switch",
                         "cxparser/rag_script_cases/integration/scenario/integration_paired_replay_transport_switch.cxsc",
                         task_constants::RouteReplay(),
                         task_constants::TaskSubtypeImageProcess(),
                         "ImageProbe",
                         "Score");
    spec.requires_image_probe_binding = true;
    spec.replay_after_run = true;
    return true;
  }

  if (MatchRequest(request, "operator", "cximage", "roi_threshold"))
  {
    FillCximageModuleScriptCase(spec,
                                "operator",
                                "roi_threshold",
                                "cxparser/rag_script_cases/cximage/operator/cximage_roi_threshold_operator.cxsc",
                                "cximage_roi_threshold");
    return true;
  }

  if (MatchRequest(request, "operator", "cximage", "roi_edge"))
  {
    FillCximageModuleScriptCase(spec,
                                "operator",
                                "roi_edge",
                                "cxparser/rag_script_cases/cximage/operator/cximage_roi_edge_operator.cxsc",
                                "cximage_roi_edge");
    return true;
  }

  if (MatchRequest(request, "operator", "cximage", "roi_gray_count"))
  {
    FillCximageModuleScriptCase(spec,
                                "operator",
                                "roi_gray_count",
                                "cxparser/rag_script_cases/cximage/operator/cximage_roi_gray_count_operator.cxsc",
                                "cximage_roi_gray_count");
    return true;
  }

  if (MatchRequest(request, "matcher", "cximage", "fastmatch_template"))
  {
    FillCximageModuleScriptCase(spec,
                                "matcher",
                                "fastmatch_template",
                                "cxparser/rag_script_cases/cximage/matcher/cximage_fastmatch_template_matcher.cxsc",
                                "cximage_fastmatch_template");
    return true;
  }

  if (MatchRequest(request, "matcher", "cximage", "fast_template_match"))
  {
    FillCximageModuleScriptCase(spec,
                                "matcher",
                                "fast_template_match",
                                "cxparser/rag_script_cases/cximage/matcher/cximage_fast_template_match_matcher.cxsc",
                                "cximage_fast_template_match");
    return true;
  }

  if (MatchRequest(request, "matcher", "cximage", "findobject_region"))
  {
    FillCximageModuleScriptCase(spec,
                                "matcher",
                                "findobject_region",
                                "cxparser/rag_script_cases/cximage/matcher/cximage_findobject_region_matcher.cxsc",
                                "cximage_findobject_region");
    return true;
  }

  if (MatchRequest(request, "feature", "cximage", "binary_region"))
  {
    FillCximageModuleScriptCase(spec,
                                "feature",
                                "binary_region",
                                "cxparser/rag_script_cases/cximage/feature/cximage_binary_region_feature.cxsc",
                                "cximage_binary_region");
    return true;
  }

  if (MatchRequest(request, "feature", "cximage", "FindCircle"))
  {
    FillCximageModuleScriptCase(spec,
                                "feature",
                                "FindCircle",
                                "cxparser/rag_script_cases/cximage/feature/cximage_findcircle_feature.cxsc",
                                "cximage_findcircle");
    return true;
  }

  if (MatchRequest(request, "feature", "cximage", "line_measure_roi"))
  {
    FillCximageModuleScriptCase(spec,
                                "feature",
                                "line_measure_roi",
                                "cxparser/rag_script_cases/cximage/feature/cximage_line_measure_roi_feature.cxsc",
                                "cximage_line_measure_roi");
    return true;
  }

  if (MatchRequest(request, "feature", "cximage", "circle_measure_fit"))
  {
    FillCximageModuleScriptCase(spec,
                                "feature",
                                "circle_measure_fit",
                                "cxparser/rag_script_cases/cximage/feature/cximage_circle_measure_fit_feature.cxsc",
                                "cximage_circle_measure_fit");
    return true;
  }

  if (MatchRequest(request, "feature", "cximage", "formfit_rect_candidate"))
  {
    FillCximageModuleScriptCase(spec,
                                "feature",
                                "formfit_rect_candidate",
                                "cxparser/rag_script_cases/cximage/feature/cximage_formfit_rect_candidate_feature.cxsc",
                                "cximage_formfit_rect_candidate");
    return true;
  }

  if (MatchRequest(request, "feature", "cximage", "geometry_topology_pipeline"))
  {
    FillCximageModuleScriptCase(spec,
                                "feature",
                                "geometry_topology_pipeline",
                                "cxparser/rag_script_cases/cximage/feature/cximage_geometry_topology_pipeline_feature.cxsc",
                                "cximage_geometry_topology_pipeline");
    return true;
  }

  if (MatchRequest(request, "embedded_model", "cximage", "fastmatch_image_model"))
  {
    FillCximageModuleScriptCase(spec,
                                "embedded_model",
                                "fastmatch_image_model",
                                "cxparser/rag_script_cases/cximage/embedded_model/cximage_fastmatch_image_model_embedded_model.cxsc",
                                "cximage_fastmatch_image_model");
    return true;
  }

  if (MatchRequest(request, "smoke", "torch", "test_host") ||
      MatchRequest(request, "train", "torch", "minimal_train") ||
      MatchRequest(request, "infer", "torch", "minimal_infer"))
  {
    FillPlannedCase(spec,
                    request.layer.c_str(),
                    "torch",
                    request.case_id.c_str(),
                    "planned_runtime_adapter");
    return true;
  }

  if (MatchRequest(request, "feature", "mlpack", "minimal_model") ||
      MatchRequest(request, "feature", "mlpack", "baseline_feature_all_v1") ||
      MatchRequest(request, "train", "mlpack", "minimal_train") ||
      MatchRequest(request, "infer", "mlpack", "minimal_infer"))
  {
    if (MatchRequest(request, "feature", "mlpack", "baseline_feature_all_v1"))
    {
      FillMlpackBaselineScriptCase(spec,
                                   "feature",
                                   "baseline_feature_all_v1",
                                   "cxscript/module/mlpack/feature.baseline_feature_all_v1.cxs",
                                   "mlpack_baseline_feature");
      return true;
    }

    if (MatchRequest(request, "feature", "mlpack", "minimal_model"))
    {
      FillMlpackMinimalScriptCase(spec,
                                  "feature",
                                  "minimal_model",
                                  "cxscript/module/mlpack/feature.minimal_model.cxs",
                                  "mlpack_feature");
      return true;
    }

    if (MatchRequest(request, "train", "mlpack", "minimal_train"))
    {
      FillMlpackMinimalScriptCase(spec,
                                  "train",
                                  "minimal_train",
                                  "cxscript/module/mlpack/train.minimal_train.cxs",
                                  "mlpack_train");
      return true;
    }

    FillMlpackMinimalScriptCase(spec,
                                "infer",
                                "minimal_infer",
                                "cxscript/module/mlpack/infer.minimal_infer.cxs",
                                "mlpack_infer");
    return true;
  }

  if (MatchRequest(request, "train", "mlpack", "baseline_logreg_flow_min"))
  {
    FillMlpackBaselineScriptCase(spec,
                                 "train",
                                 "baseline_logreg_flow_min",
                                 "cxscript/module/mlpack/train/baseline_logreg_flow_min.cxscript",
                                 "mlpack_baseline_train");
    return true;
  }

  if (MatchRequest(request, "infer", "mlpack", "baseline_logreg_flow_min"))
  {
    FillMlpackBaselineScriptCase(spec,
                                 "infer",
                                 "baseline_logreg_flow_min",
                                 "cxscript/module/mlpack/infer/baseline_logreg_flow_min.cxscript",
                                 "mlpack_baseline_infer");
    return true;
  }

  if (MatchRequest(request, "score", "mlpack", "baseline_classification_flow_min"))
  {
    FillMlpackBaselineScriptCase(spec,
                                 "score",
                                 "baseline_classification_flow_min",
                                 "cxscript/module/mlpack/score/baseline_classification_flow_min.cxscript",
                                 "mlpack_baseline_score");
    return true;
  }

  if (MatchRequest(request, "score", "mlpack", "baseline_cluster_ref_min"))
  {
    FillMlpackBaselineScriptCase(spec,
                                 "score",
                                 "baseline_cluster_ref_min",
                                 "cxscript/module/mlpack/score.baseline_cluster_ref_min.cxs",
                                 "mlpack_semantic_cluster");
    return true;
  }

  if (MatchRequest(request, "score", "mlpack", "baseline_distance_ref_min"))
  {
    FillMlpackBaselineScriptCase(spec,
                                 "score",
                                 "baseline_distance_ref_min",
                                 "cxscript/module/mlpack/score.baseline_distance_ref_min.cxs",
                                 "mlpack_semantic_distance");
    return true;
  }

  if (MatchRequest(request, "score", "mlpack", "baseline_anomaly_ref_min"))
  {
    FillMlpackBaselineScriptCase(spec,
                                 "score",
                                 "baseline_anomaly_ref_min",
                                 "cxscript/module/mlpack/score.baseline_anomaly_ref_min.cxs",
                                 "mlpack_semantic_anomaly");
    return true;
  }

  if (MatchRequest(request, "train", "mlpack", "baseline_rf_flow_min"))
  {
    FillMlpackBaselineScriptCase(spec,
                                 "train",
                                 "baseline_rf_flow_min",
                                 "cxscript/module/mlpack/train/baseline_rf_flow_min.cxscript",
                                 "mlpack_baseline_train");
    return true;
  }

  if (MatchRequest(request, "infer", "mlpack", "baseline_rf_flow_min"))
  {
    FillMlpackBaselineScriptCase(spec,
                                 "infer",
                                 "baseline_rf_flow_min",
                                 "cxscript/module/mlpack/infer/baseline_rf_flow_min.cxscript",
                                 "mlpack_baseline_infer");
    return true;
  }

  if (MatchRequest(request, "train", "mlpack", "baseline_knn_flow_min") ||
      MatchRequest(request, "infer", "mlpack", "baseline_knn_flow_min"))
  {
    if (MatchRequest(request, "train", "mlpack", "baseline_knn_flow_min"))
    {
      FillMlpackBaselineScriptCase(spec,
                                   "train",
                                   "baseline_knn_flow_min",
                                   "cxscript/module/mlpack/train/baseline_knn_flow_min.cxscript",
                                   "mlpack_baseline_train");
      return true;
    }

    FillMlpackBaselineScriptCase(spec,
                                 "infer",
                                 "baseline_knn_flow_min",
                                 "cxscript/module/mlpack/infer/baseline_knn_flow_min.cxscript",
                                 "mlpack_baseline_infer");
    return true;
  }

  if (request.script_type == "integration" &&
      request.integration == "mlpack" &&
      request.layer == "scenario" &&
      request.case_id == "baseline_logreg_chain_min")
  {
    FillMlpackBaselineScenarioCase(spec,
                                   "baseline_logreg_chain_min",
                                   "cxscript/integration/mlpack/baseline_logreg_chain_min.cxscript",
                                   "mlpack_baseline_chain");
    return true;
  }

  if (request.script_type == "integration" &&
      request.integration == "mlpack" &&
      request.layer == "scenario" &&
      request.case_id == "baseline_knn_chain_min")
  {
    FillMlpackBaselineScenarioCase(spec,
                                   "baseline_knn_chain_min",
                                   "cxscript/integration/mlpack/baseline_knn_chain_min.cxscript",
                                   "mlpack_baseline_chain");
    return true;
  }

  if (request.script_type == "integration" &&
      request.integration == "mlpack" &&
      request.layer == "scenario" &&
      request.case_id == "baseline_pair_compare_min")
  {
    FillMlpackBaselineScenarioCase(spec,
                                   "baseline_pair_compare_min",
                                   "cxscript/integration/mlpack/baseline_pair_compare_min.cxscript",
                                   "mlpack_baseline_chain");
    return true;
  }

  if (request.script_type == "integration" &&
      request.integration == "torch_geometry" &&
      ((request.layer == "feature" &&
        (request.case_id == "input_prior_contract" ||
         request.case_id == "label_align_contract" ||
         request.case_id == "attach_back_contract")) ||
       (request.layer == "infer" &&
        request.case_id == "replay_contract")))
  {
    FillCoreCase(spec,
                 request.layer.c_str(),
                 "torch_geometry",
                 request.case_id.c_str(),
                 task_constants::RouteDefault(),
                 task_constants::TaskSubtypeParserEval(),
                 "native_expr",
                 "eval",
                 "");
    spec.requires_geometry_contract_binding = true;
    return true;
  }

  if (request.script_type == "integration" &&
      request.integration == "torch_geometry" &&
      ((request.layer == "feature" &&
        (request.case_id == "input_prior_min" ||
         request.case_id == "label_align_min" ||
         request.case_id == "attach_back_min")) ||
       (request.layer == "infer" &&
        request.case_id == "replay_min")))
  {
    FillPlannedCase(spec,
                    request.layer.c_str(),
                    "torch_geometry",
                    request.case_id.c_str(),
                    "planned_torch_geometry_alignment_adapter");
    return true;
  }

  if ((MatchRequest(request, "smoke", "ensmallen_layer", "circle_objective") ||
       MatchRequest(request, "smoke", "ensmallen_layer", "ellipse_objective") ||
       MatchRequest(request, "smoke", "ensmallen_layer", "match_objective")))
  {
    FillPlannedCase(spec,
                    request.layer.c_str(),
                    "ensmallen_layer",
                    request.case_id.c_str(),
                    "planned_ensmallen_runtime_adapter");
    return true;
  }

  if (MatchRequest(request, "scenario", "ensmallen_layer", "phase1_param_replay"))
  {
    FillActiveScriptCase(spec,
                         "scenario",
                         "ensmallen_layer",
                         "phase1_param_replay",
                         "cxparser/rag_script_cases/cxcore/scenario/ensmallen_layer_phase1_param_replay_scenario.cxsc",
                         task_constants::RouteReplay(),
                         "phase1_param_replay",
                         "CxCoreFlowHost",
                         "EnsmallenScenarioReplay");
    return true;
  }

  if (MatchRequest(request, "scenario", "ensmallen_layer", "halcon_circle_plate_geometry_replay"))
  {
    FillActiveScriptCase(spec,
                         "scenario",
                         "ensmallen_layer",
                         "halcon_circle_plate_geometry_replay",
                         "cxparser/rag_script_cases/cxcore/scenario/ensmallen_layer_halcon_circle_plate_geometry_replay_scenario.cxsc",
                         task_constants::RouteReplay(),
                         "halcon_circle_plate_geometry_replay",
                         "CxCoreFlowHost",
                         "EnsmallenScenarioReplay");
    return true;
  }

  if (MatchRequest(request, "train", "ensmallen_layer", "phase1_param_opt"))
  {
    FillActiveScriptCase(spec,
                         "train",
                         "ensmallen_layer",
                         "phase1_param_opt",
                         "cxparser/rag_script_cases/cxcore/train/ensmallen_layer_phase1_param_opt_train.cxsc",
                         task_constants::RouteBatch(),
                         "phase1_param_opt",
                         "CxCoreFlowHost",
                         "EnsmallenSaveBestParams");
    return true;
  }

  if (MatchRequest(request, "train", "ensmallen_layer", "halcon_screws_cluster_stability"))
  {
    FillActiveScriptCase(spec,
                         "train",
                         "ensmallen_layer",
                         "halcon_screws_cluster_stability",
                         "cxparser/rag_script_cases/cxcore/train/ensmallen_layer_halcon_screws_cluster_stability_train.cxsc",
                         task_constants::RouteBatch(),
                         "halcon_screws_cluster_stability",
                         "CxCoreFlowHost",
                         "EnsmallenSaveBestParams");
    return true;
  }

  if (MatchRequest(request, "infer", "ensmallen_layer", "phase1_param_eval"))
  {
    FillActiveScriptCase(spec,
                         "infer",
                         "ensmallen_layer",
                         "phase1_param_eval",
                         "cxparser/rag_script_cases/cxcore/infer/ensmallen_layer_phase1_param_eval_infer.cxsc",
                         task_constants::RouteDefault(),
                         "phase1_param_eval",
                         "CxCoreFlowHost",
                         "EnsmallenInferCompare");
    return true;
  }

  if (MatchRequest(request, "feature", "ensmallen_layer", "geometry_fit_tuning"))
  {
    FillActiveScriptCase(spec,
                         "feature",
                         "ensmallen_layer",
                         "geometry_fit_tuning",
                         "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_geometry_fit_tuning_feature.cxsc",
                         task_constants::RouteDefault(),
                         "geometry_fit_tuning",
                         "CxCoreFlowHost",
                         "RunGeometryFitTuning");
    return true;
  }

  if (MatchRequest(request, "feature", "ensmallen_layer", "match_score_tuning"))
  {
    FillActiveScriptCase(spec,
                         "feature",
                         "ensmallen_layer",
                         "match_score_tuning",
                         "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_match_score_tuning_feature.cxsc",
                         task_constants::RouteDefault(),
                         "match_score_tuning",
                         "CxCoreFlowHost",
                         "RunMatchScoreTuning");
    return true;
  }

  if (MatchRequest(request, "feature", "ensmallen_layer", "circle_param_opt"))
  {
    FillActiveScriptCase(spec,
                         "feature",
                         "ensmallen_layer",
                         "circle_param_opt",
                         "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_circle_param_opt_feature.cxsc",
                         task_constants::RouteDefault(),
                         "circle_param_opt",
                         "CxCoreFlowHost",
                         "ResolveTuningCase");
    return true;
  }

  if (MatchRequest(request, "feature", "ensmallen_layer", "ellipse_param_opt"))
  {
    FillActiveScriptCase(spec,
                         "feature",
                         "ensmallen_layer",
                         "ellipse_param_opt",
                         "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_ellipse_param_opt_feature.cxsc",
                         task_constants::RouteDefault(),
                         "ellipse_param_opt",
                         "CxCoreFlowHost",
                         "ResolveTuningCase");
    return true;
  }

  if (MatchRequest(request, "feature", "ensmallen_layer", "match_score_opt"))
  {
    FillActiveScriptCase(spec,
                         "feature",
                         "ensmallen_layer",
                         "match_score_opt",
                         "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_match_score_opt_feature.cxsc",
                         task_constants::RouteDefault(),
                         "match_score_opt",
                         "CxCoreFlowHost",
                         "ResolveTuningCase");
    return true;
  }

  return false;
}
}