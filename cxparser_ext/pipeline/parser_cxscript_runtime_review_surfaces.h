// Extracted unified review/phase0 surface implementation for cxscript runtime.

bool HasEnsmallenOptimizationEvidence(const CxScriptExecutionResult &result);
std::string BuildEnsmallenObjectiveCurveValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenFeatureDistanceDeltaValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenCandidateRankValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenStabilityScoreValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenConvergenceStatusValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenBestCandidateConfidenceValue(const CxScriptExecutionResult &result);
std::string BuildCandidateStatusSummary(const std::vector<UnifiedDetectionElement> &elements);
std::string BuildMatchStatusSummary(const std::vector<UnifiedDetectionElement> &elements);
std::string BuildManualReviewSignalSummary(const std::vector<UnifiedDetectionElement> &elements);
std::string BuildElementGroupSummary(const std::vector<UnifiedDetectionElement> &elements);
std::string BuildFocusRefreshTargets(const std::vector<UnifiedDetectionElement> &elements);
std::string BuildLocalDeltaTargets(const std::vector<UnifiedDetectionElement> &elements);

void PushSemanticOperationNamedText(std::vector<std::string> &items,
                                    const char *name,
                                    const std::string &value)
{
  if (!value.empty())
    items.push_back(std::string(name) + "=" + value);
}
std::string BuildUnifiedToleranceSummary(const CxScriptExecutionResult &result)
{
  if (result.tolerance.empty())
    return std::string();
  return "tolerance=" + result.tolerance;
}

bool ShouldExposeUnifiedStabilitySummary(const CxScriptExecutionResult &result)
{
  return NormalizeReviewSourceThread(result) == "ensmallen" ||
         !result.train_param_summary.empty() ||
         !result.infer_param_summary.empty() ||
         result.stability_delta != 0.0 ||
         result.accuracy > 0.0 ||
         result.macro_f1 > 0.0;
}

std::string BuildUnifiedStabilitySummary(const CxScriptExecutionResult &result)
{
  if (!ShouldExposeUnifiedStabilitySummary(result))
    return std::string();

  std::vector<std::string> items;
  items.push_back("stability_delta=" + FormatElementNumber(result.stability_delta));
  const std::string stability_score = BuildEnsmallenStabilityScoreValue(result);
  if (!stability_score.empty())
    items.push_back("stability_score=" + stability_score);
  const std::string convergence_status = BuildEnsmallenConvergenceStatusValue(result);
  if (!convergence_status.empty())
    items.push_back("convergence_status=" + convergence_status);
  const std::string best_candidate_confidence =
    BuildEnsmallenBestCandidateConfidenceValue(result);
  if (!best_candidate_confidence.empty())
    items.push_back("best_candidate_confidence=" + best_candidate_confidence);
  if (!result.selected_method.empty())
    items.push_back("selected_method=" + result.selected_method);
  if (!result.ordered_candidates.empty())
    items.push_back("candidate_ordering=" + result.ordered_candidates);
  if (result.accuracy > 0.0)
    items.push_back("accuracy=" + FormatElementNumber(result.accuracy));
  if (result.macro_f1 > 0.0)
    items.push_back("macro_f1=" + FormatElementNumber(result.macro_f1));
  return JoinTextItems(items, ";");
}

std::string ResolveUnifiedThresholdRef(const CxScriptExecutionResult &result)
{
  const std::string named_threshold_ref =
    FindNamedResultFieldValue(result, "bridge", "threshold_ref");
  if (!named_threshold_ref.empty())
    return named_threshold_ref;

  const std::string assigned_threshold_ref =
    FindAssignmentValue(result.input_params, "threshold_ref");
  if (!assigned_threshold_ref.empty())
    return assigned_threshold_ref;

  return ResolveEnsmallenBridgeRef(result, "threshold_ref");
}

std::string BuildUnifiedThresholdSummary(const CxScriptExecutionResult &result)
{
  const std::string threshold_ref = ResolveUnifiedThresholdRef(result);
  std::vector<std::string> items;
  if (!threshold_ref.empty())
    items.push_back("threshold_ref=" + threshold_ref);
  if (HasEnsmallenOptimizationEvidence(result))
  {
    const std::string feature_distance_delta =
      BuildEnsmallenFeatureDistanceDeltaValue(result);
    if (!feature_distance_delta.empty())
      items.push_back("feature_distance_delta=" + feature_distance_delta);
    const std::string candidate_rank = BuildEnsmallenCandidateRankValue(result);
    if (!candidate_rank.empty())
      items.push_back("candidate_rank=" + candidate_rank);
    if (!result.selected_method.empty())
      items.push_back("selected_method=" + result.selected_method);
    if (!result.ordered_candidates.empty())
      items.push_back("candidate_ordering=" + result.ordered_candidates);
    const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
    if (!coverage_gap.empty())
      items.push_back("coverage_gap=" + coverage_gap);
    const std::string risk_axis = BuildEnsmallenRiskAxis(result);
    if (!risk_axis.empty())
      items.push_back("risk_axis=" + risk_axis);
  }
  if (items.empty())
    return std::string();
  return JoinTextItems(items, ";");
}

std::string BuildUnifiedRiskNote(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  const std::string threshold_summary = BuildUnifiedThresholdSummary(result);
  const std::string tolerance_summary = BuildUnifiedToleranceSummary(result);
  const std::string stability_summary = BuildUnifiedStabilitySummary(result);

  if (!threshold_summary.empty())
    items.push_back(threshold_summary);
  if (!tolerance_summary.empty())
    items.push_back(tolerance_summary);
  if (!stability_summary.empty())
    items.push_back(stability_summary);
  if (!result.error_message.empty())
    items.push_back("error=" + result.error_message);
  return JoinTextItems(items, ";");
}

std::string BuildSemanticOperationBaseRef(const CxScriptExecutionResult &result)
{
  if (!result.case_name.empty())
    return result.case_name;
  if (!result.script_name.empty())
    return result.script_name;
  if (!result.script_path.empty())
    return result.script_path;
  if (!result.task_id.empty())
    return result.task_id;
  return "cxscript.semantic_operation";
}

std::string BuildSemanticOperationScriptModuleRef(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  if (!result.kind.empty())
    items.push_back(result.kind);
  if (!result.layer.empty())
    items.push_back(result.layer);
  if (!result.module.empty())
    items.push_back(result.module);
  if (!result.case_name.empty())
    items.push_back(result.case_name);
  if (!items.empty())
    return JoinTextItems(items, ".");
  return BuildSemanticOperationBaseRef(result);
}

std::string BuildSemanticOperationSelectTargets(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  const std::string script_module_ref = BuildSemanticOperationScriptModuleRef(result);
  const std::string input_image_ref =
    FindNamedResultFieldValue(result, "review_image", "input_image_ref");
  const std::string dataset_bridge =
    FindNamedResultFieldValue(result, "bridge", "dataset_bridge");
  const std::string test_bucket =
    FindNamedResultFieldValue(result, "bridge", "test_bucket");
  const std::string test_flow =
    FindNamedResultFieldValue(result, "bridge", "test_flow");
  PushSemanticOperationNamedText(items, "script_module", script_module_ref);
  PushSemanticOperationNamedText(items, "task_case", result.case_name);
  PushSemanticOperationNamedText(items, "test_image", input_image_ref);
  PushSemanticOperationNamedText(items, "mainline", result.layer);
  PushSemanticOperationNamedText(items, "dataset_bridge", dataset_bridge);
  PushSemanticOperationNamedText(items, "test_bucket", test_bucket);
  PushSemanticOperationNamedText(items, "test_flow", test_flow);
  return JoinTextItems(items, ";");
}
std::string BuildSemanticOperationParameterInjectionEntry(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  if (!result.input_params.empty())
    items.push_back("script_params");
  if (!result.required_input_contract.empty())
    items.push_back("required_input_contract");
  if (!result.train_param_summary.empty())
    items.push_back("train_params");
  if (!result.infer_param_summary.empty())
    items.push_back("infer_params");
  const std::string threshold_ref = ResolveUnifiedThresholdRef(result);
  const std::string objective_ref =
    ResolveNamedOrDirectRef(result, "refs", "objective_ref", result.objective_ref);
  const std::string threshold_bridge_ref = ResolveEnsmallenBridgeRef(result, "threshold_ref");
  const std::string crop_policy_ref = ResolveEnsmallenBridgeRef(result, "crop_policy_ref");
  const std::string boundary_error_ref = ResolveEnsmallenBridgeRef(result, "boundary_error_ref");
  const std::string alignment_error_ref = ResolveEnsmallenBridgeRef(result, "alignment_error_ref");
  const std::string best_params_ref =
    ResolveNamedOrDirectRef(result, "refs", "best_params_ref", result.best_params_ref);
  if (!objective_ref.empty())
    items.push_back("objective_ref=" + objective_ref);
  if (!threshold_bridge_ref.empty())
    items.push_back("threshold_ref=" + threshold_bridge_ref);
  if (!crop_policy_ref.empty())
    items.push_back("crop_policy_ref=" + crop_policy_ref);
  if (!boundary_error_ref.empty())
    items.push_back("boundary_error_ref=" + boundary_error_ref);
  if (!alignment_error_ref.empty())
    items.push_back("alignment_error_ref=" + alignment_error_ref);
  if (!best_params_ref.empty())
    items.push_back("best_params_ref=" + best_params_ref);
  if (!threshold_ref.empty() || !result.tolerance.empty())
    items.push_back("geometry_or_threshold");
  if (!result.tolerance.empty())
    items.push_back("tolerance=" + result.tolerance);
  if (items.empty())
    items.push_back("default_runtime_params");
  return JoinTextItems(items, ";");
}

std::string BuildSemanticOperationStageExecutionEntry(const CxScriptExecutionResult &result)
{
  const std::string public_entry = "cxparser_ext_cxscript_cli";
  if (!result.script_path.empty())
    return public_entry + " --script " + result.script_path;
  const std::string script_module_ref = BuildSemanticOperationScriptModuleRef(result);
  if (!script_module_ref.empty())
    return public_entry + " --kind --layer --module --case " + script_module_ref;
  return public_entry;
}

std::string BuildSemanticOperationResultObjectRef(const CxScriptExecutionResult &result)
{
  if (!result.result_object.empty())
    return result.result_object;
  if (!result.task_id.empty())
    return result.task_id;
  return BuildSemanticOperationBaseRef(result) + ".result";
}

std::string BuildSemanticOperationReplayRef(const CxScriptExecutionResult &result)
{
  const std::string replay_log_path =
    FindNamedResultFieldValue(result, "analysis", "replay_log_path");
  if (!replay_log_path.empty())
    return replay_log_path;
  if (!result.script_path.empty())
    return result.script_path;
  return BuildSemanticOperationBaseRef(result) + ".replay";
}

std::string BuildSemanticOperationVisualRef(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  const std::string recommended_image_ref =
    FindNamedResultFieldValue(result, "review_image", "recommended_image_ref");
  const std::string review_visual_ref =
    FindNamedResultFieldValue(result, "review_image", "primary_visual_ref");
  const std::string visual_evidence_ref_set =
    FindNamedResultFieldValue(result, "review_image", "visual_evidence_ref_set");
  const std::string recommended_chain_ref =
    FindNamedResultFieldValue(result, "review_image", "recommended_chain_ref");
  const std::string recommended_stage_ref =
    FindNamedResultFieldValue(result, "review_image", "recommended_stage_ref");
  const std::string task_conclusion_ref =
    FindNamedResultFieldValue(result, "review_image", "task_conclusion_ref");
  const std::string anomaly_conclusion_ref =
    FindNamedResultFieldValue(result, "review_image", "anomaly_conclusion_ref");
  PushSemanticOperationNamedText(items, "input_image", recommended_image_ref);
  PushSemanticOperationNamedText(items, "primary_visual", review_visual_ref);
  PushSemanticOperationNamedText(items, "visual_evidence", visual_evidence_ref_set);
  PushSemanticOperationNamedText(items, "main_chain", recommended_chain_ref);
  PushSemanticOperationNamedText(items, "stage", recommended_stage_ref);
  PushSemanticOperationNamedText(items, "conclusion", task_conclusion_ref);
  PushSemanticOperationNamedText(items, "anomaly_focus", anomaly_conclusion_ref);
  if (!items.empty())
    return JoinTextItems(items, ";");
  const std::string input_image_ref =
    FindNamedResultFieldValue(result, "run_input", "input_image_ref");
  if (!input_image_ref.empty())
    return input_image_ref;
  return SelectReviewInputImageRef(result);
}

std::string BuildSemanticOperationElementTargets(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  const std::string recommended_element_ref =
    FindNamedResultFieldValue(result, "review_image", "recommended_element_ref");
  const std::string element_ref_set =
    FindNamedResultFieldValue(result, "review_image", "element_ref_set");
  const std::string element_summary =
    FindNamedResultFieldValue(result, "review_image", "element_summary");
  const std::string element_status_summary =
    FindNamedResultFieldValue(result, "review_image", "element_status_summary");
  const std::string element_findings =
    FindNamedResultFieldValue(result, "review_image", "element_findings");
  const std::string element_level_focus =
    FindNamedResultFieldValue(result, "review_image", "element_level_focus");
  const std::string chain_ref_set =
    FindNamedResultFieldValue(result, "review_image", "chain_ref_set");
  const std::string chain_summary =
    FindNamedResultFieldValue(result, "review_image", "element_chain_summary");
  PushSemanticOperationNamedText(items, "recommended_element", recommended_element_ref);
  PushSemanticOperationNamedText(items, "elements", element_ref_set);
  PushSemanticOperationNamedText(items, "element_summary", element_summary);
  PushSemanticOperationNamedText(items, "element_status", element_status_summary);
  PushSemanticOperationNamedText(items, "element_findings", element_findings);
  PushSemanticOperationNamedText(items, "element_focus", element_level_focus);
  PushSemanticOperationNamedText(items, "element_chains", chain_ref_set);
  PushSemanticOperationNamedText(items, "chain_summary", chain_summary);
  if (!items.empty())
    return JoinTextItems(items, ";");
  if (!result.bbox_candidate_list_ref.empty())
    return result.bbox_candidate_list_ref;
  if (!result.roi_crop_packet_ref.empty())
    return result.roi_crop_packet_ref;
  if (!result.template_alignment_ref.empty())
    return result.template_alignment_ref;
  return BuildSemanticOperationResultObjectRef(result);
}

std::string BuildSemanticOperationJudgeTargets(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  const std::string issue_hint =
    FindNamedResultFieldValue(result, "review_image", "issue_kind_hint");
  const std::string task_conclusion_ref =
    FindNamedResultFieldValue(result, "review_image", "task_conclusion_ref");
  const std::string anomaly_conclusion_ref =
    FindNamedResultFieldValue(result, "review_image", "anomaly_conclusion_ref");
  const std::string next_action_ref =
    FindNamedResultFieldValue(result, "review_image", "next_action_ref");
  const std::string risk_axis = BuildEnsmallenRiskAxis(result);
  const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
  const std::string stability_summary =
    FindNamedResultFieldValue(result, "review_anomaly", "stability_summary");

  PushSemanticOperationNamedText(items, "issue_kind", issue_hint);
  PushSemanticOperationNamedText(items, "task_conclusion", task_conclusion_ref);
  PushSemanticOperationNamedText(items, "anomaly_conclusion", anomaly_conclusion_ref);
  PushSemanticOperationNamedText(items, "next_action", next_action_ref);
  PushSemanticOperationNamedText(items, "risk_axis", risk_axis);
  PushSemanticOperationNamedText(items, "coverage_gap", coverage_gap);
  PushSemanticOperationNamedText(items, "stability", stability_summary);
  if (!result.error_message.empty())
    items.push_back("error_message");
  if (!result.failure_phase.empty())
    items.push_back("failure_phase=" + result.failure_phase);
  if (items.empty())
    items.push_back(result.success ? "normal" : "needs_review");
  return JoinTextItems(items, ";");
}

std::string BuildSemanticOperationRecordTargets(const CxScriptExecutionResult &result,
                                                const std::string &semantic_atom_ref_set)
{
  std::vector<std::string> items;
  const std::string replay_ref = BuildSemanticOperationReplayRef(result);
  const std::string issue_entry_ref_set =
    FindNamedResultFieldValue(result, "review_image", "issue_entry_ref_set");
  const std::string summary_ref =
    ResolveNamedOrDirectRef(result, "refs", "summary_ref", result.summary_ref);
  const std::string compare_ref =
    ResolveNamedOrDirectRef(result, "refs", "compare_ref", result.compare_ref);
  const std::string best_params_ref =
    ResolveNamedOrDirectRef(result, "refs", "best_params_ref", result.best_params_ref);
  const std::string recommended_stage_ref =
    FindNamedResultFieldValue(result, "review_image", "recommended_stage_ref");
  const std::string next_action_ref =
    FindNamedResultFieldValue(result, "review_image", "next_action_ref");

  PushSemanticOperationNamedText(items, "issue_entry", issue_entry_ref_set);
  PushSemanticOperationNamedText(items, "replay", replay_ref);
  PushSemanticOperationNamedText(items, "summary_ref", summary_ref);
  PushSemanticOperationNamedText(items, "compare_ref", compare_ref);
  PushSemanticOperationNamedText(items, "best_params_ref", best_params_ref);
  PushSemanticOperationNamedText(items, "next_stage", recommended_stage_ref);
  PushSemanticOperationNamedText(items, "next_action", next_action_ref);
  PushSemanticOperationNamedText(items, "operation_atoms", semantic_atom_ref_set);
  return JoinTextItems(items, ";");
}

std::string BuildSemanticOperationAtomRefSet(const CxScriptExecutionResult &result)
{
  std::vector<std::string> atom_refs;
  for (size_t i = 0; i < result.operation_atoms.size(); ++i)
    PushUniqueText(atom_refs, result.operation_atoms[i].atom_id);
  return JoinTextItems(atom_refs, ";");
}

void PushSemanticOperationAtom(CxScriptExecutionResult &result,
                               const std::string &base_ref,
                               const char *stage,
                               const char *action_kind,
                               const std::string &input_ref,
                               const std::string &output_ref,
                               const std::string &status,
                               const std::string &summary)
{
  OperationAtom atom;
  atom.atom_id = base_ref + "." + stage;
  atom.stage = stage;
  atom.action_kind = action_kind;
  atom.input_ref = input_ref;
  atom.output_ref = output_ref;
  atom.status = status;
  atom.summary = summary;
  result.operation_atoms.push_back(atom);
}
std::string BuildTorchTaskEntryName(const CxScriptExecutionResult &result)
{
  if (!result.input_task.empty())
    return result.input_task;
  if (!result.task_id.empty())
    return result.task_id;
  return result.case_name;
}

std::string BuildTorchTaskFamily(const CxScriptExecutionResult &result)
{
  const std::string entry = BuildTorchTaskEntryName(result);
  const std::string hints = result.case_name + ";" + result.module + ";" +
    result.layer + ";" + result.model_name + ";" + entry;
  if (result.layer == "train" || hints.find("train") != std::string::npos)
    return "training";
  if (hints.find("yolo_mobilevit") != std::string::npos ||
      hints.find("mobilevit") != std::string::npos ||
      !result.roi_crop_packet_ref.empty() ||
      !result.published_roi_crop_packet_ref.empty())
    return "roi_reclass";
  if (hints.find("deeplab") != std::string::npos ||
      hints.find("template") != std::string::npos ||
      !result.template_alignment_ref.empty() ||
      !result.roi_diff_candidate_ref.empty())
    return "template_diff";
  if (hints.find("yolo") != std::string::npos ||
      !result.bbox_candidate_list_ref.empty())
    return "detection";
  if (hints.find("baseline") != std::string::npos ||
      !result.baseline_feature_ref.empty())
    return "baseline_feature";
  if (hints.find("resnet") != std::string::npos ||
      hints.find("mobilenet") != std::string::npos ||
      result.accuracy > 0.0 ||
      result.macro_f1 > 0.0)
    return "classification";
  if (result.module == "torch_module" || result.module == "torch")
    return "classification";
  return std::string();
}

std::string BuildTorchPipelineFamily(const CxScriptExecutionResult &result)
{
  const std::string hints = result.case_name + ";" + result.input_task + ";" +
    result.model_name;
  if (hints.find("yolo_mobilevit") != std::string::npos ||
      (!result.bbox_candidate_list_ref.empty() &&
       (!result.roi_crop_packet_ref.empty() || !result.published_roi_crop_packet_ref.empty())))
    return "yolo_then_mobilevit";
  if (hints.find("deeplab") != std::string::npos ||
      !result.template_alignment_ref.empty() ||
      !result.roi_diff_candidate_ref.empty())
    return "template_then_diff";
  if (hints.find("baseline") != std::string::npos ||
      !result.baseline_feature_ref.empty())
    return "baseline_then_compare";
  if (result.module == "torch_module" || result.module == "torch")
    return "single_model";
  return std::string();
}

std::string BuildTorchModelFamily(const CxScriptExecutionResult &result)
{
  if (!result.model_name.empty())
    return result.model_name;
  const std::string hints = result.case_name + ";" + result.input_task + ";" +
    result.consumed_weight_files;
  if (hints.find("mobilevit") != std::string::npos)
    return "mobilevit";
  if (hints.find("deeplab") != std::string::npos)
    return "deeplab";
  if (hints.find("yolo") != std::string::npos)
    return "yolo";
  if (hints.find("resnet50") != std::string::npos)
    return "resnet50";
  if (hints.find("resnet18") != std::string::npos)
    return "resnet18";
  if (hints.find("mobilenet") != std::string::npos)
    return "mobilenetv3";
  return std::string();
}

std::string BuildTorchScenarioFamily(const CxScriptExecutionResult &result)
{
  if (result.module != "torch_module" && result.module != "torch" &&
      result.case_name.find("torch") == std::string::npos)
    return std::string();
  if (result.layer == "scenario")
    return "multi_stage_visual_scenario";
  if (result.layer == "train")
    return "mainline_training";
  if (result.layer == "infer")
    return "unified_infer";
  if (result.layer == "feature")
    return "feature_contract";
  return "torch_multi_task_visual";
}

std::string BuildTorchDeviceEvidence(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  if (!result.requested_device.empty())
  {
    items.push_back("requested_device=" + result.requested_device);
    items.push_back("actual_device=" + result.requested_device);
  }
  if (result.runtime_ms > 0.0)
    items.push_back("runtime_ms=" + FormatElementNumber(result.runtime_ms));
  if (result.fit_time_ms > 0.0)
    items.push_back("train_runtime_ms=" + FormatElementNumber(result.fit_time_ms));
  if (result.infer_time_ms > 0.0)
    items.push_back("infer_runtime_ms=" + FormatElementNumber(result.infer_time_ms));
  return JoinTextItems(items, ";");
}

std::string BuildTorchVisualEvidenceSet(const CxScriptExecutionResult &result,
                                        const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> items;
  if (!image_review.input_image_ref.empty())
    items.push_back("input_image_ref=" + image_review.input_image_ref);
  if (!result.roi_crop_packet_ref.empty())
    items.push_back("roi_crop_packet_ref=" + result.roi_crop_packet_ref);
  else if (!result.published_roi_crop_packet_ref.empty())
    items.push_back("roi_crop_packet_ref=" + result.published_roi_crop_packet_ref);
  if (!result.bbox_candidate_list_ref.empty())
    items.push_back("bbox_overlay_ref=" + result.bbox_candidate_list_ref);
  else if (!result.published_bbox_candidate_list_ref.empty())
    items.push_back("bbox_overlay_ref=" + result.published_bbox_candidate_list_ref);
  if (!result.published_primary_ref.empty())
    items.push_back("segmentation_mask_overlay_ref=" + result.published_primary_ref);
  if (!result.template_alignment_ref.empty())
    items.push_back("template_alignment_ref=" + result.template_alignment_ref);
  else if (!result.published_template_alignment_ref.empty())
    items.push_back("template_alignment_ref=" + result.published_template_alignment_ref);
  if (!result.roi_diff_candidate_ref.empty())
    items.push_back("roi_diff_candidate_ref=" + result.roi_diff_candidate_ref);
  else if (!result.published_roi_diff_candidate_ref.empty())
    items.push_back("roi_diff_candidate_ref=" + result.published_roi_diff_candidate_ref);
  if (!result.attach_back_ref.empty())
    items.push_back("attach_back_overlay_ref=" + result.attach_back_ref);
  return JoinTextItems(items, ";");
}

std::string BuildTorchBusinessEvalFields(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  if (!result.summary.empty())
    items.push_back("image_level_conclusion=" + result.summary);
  if (!result.template_test_alignment_status.empty())
    items.push_back("region_level_conclusion=" + result.template_test_alignment_status);
  if (!result.attach_back_top1_class.empty())
    items.push_back("classification_result=" + result.attach_back_top1_class);
  if (!result.attach_back_confidence.empty())
    items.push_back("confidence=" + result.attach_back_confidence);
  if (!result.failure_mode.empty())
    items.push_back("abnormal_type=" + result.failure_mode);
  if (result.runtime_ms > 0.0)
    items.push_back("runtime_ms=" + FormatElementNumber(result.runtime_ms));
  if (result.fit_time_ms > 0.0)
    items.push_back("train_runtime_ms=" + FormatElementNumber(result.fit_time_ms));
  if (result.infer_time_ms > 0.0)
    items.push_back("infer_runtime_ms=" + FormatElementNumber(result.infer_time_ms));
  if (!result.train_param_summary.empty())
    items.push_back("parameter_summary=" + result.train_param_summary);
  else if (!result.infer_param_summary.empty())
    items.push_back("parameter_summary=" + result.infer_param_summary);
  const std::string recommended_action =
    FindNamedResultFieldValue(result, "analysis", "recommended_action");
  const std::string next_review_action =
    recommended_action.empty()
      ? FindNamedResultFieldValue(result, "analysis", "next_review_action")
      : recommended_action;
  if (!next_review_action.empty())
    items.push_back("recommended_next_action=" + next_review_action);
  return JoinTextItems(items, ";");
}

std::string BuildTorchPipelineLinkTrace(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  if (!result.bbox_candidate_list_ref.empty())
    items.push_back("upstream_candidate=" + result.bbox_candidate_list_ref);
  else if (!result.published_bbox_candidate_list_ref.empty())
    items.push_back("upstream_candidate=" + result.published_bbox_candidate_list_ref);
  if (!result.roi_crop_packet_ref.empty())
    items.push_back("intermediate_roi=" + result.roi_crop_packet_ref);
  else if (!result.published_roi_crop_packet_ref.empty())
    items.push_back("intermediate_roi=" + result.published_roi_crop_packet_ref);
  if (!result.baseline_class_ref.empty())
    items.push_back("downstream_reclass=" + result.baseline_class_ref);
  if (!result.attach_back_confidence.empty())
    items.push_back("class_confidence=" + result.attach_back_confidence);
  if (!result.baseline_feature_ref.empty())
    items.push_back("feature_ref=" + result.baseline_feature_ref);
  if (!result.template_alignment_ref.empty())
    items.push_back("template_alignment=" + result.template_alignment_ref);
  else if (!result.published_template_alignment_ref.empty())
    items.push_back("template_alignment=" + result.published_template_alignment_ref);
  if (!result.roi_diff_candidate_ref.empty())
    items.push_back("diff_candidate=" + result.roi_diff_candidate_ref);
  else if (!result.published_roi_diff_candidate_ref.empty())
    items.push_back("diff_candidate=" + result.published_roi_diff_candidate_ref);
  if (!result.attach_back_ref.empty())
    items.push_back("final_overlay=" + result.attach_back_ref);
  return JoinTextItems(items, ";");
}

std::string BuildTorchSequenceFamily(const CxScriptExecutionResult &result)
{
  const std::string pipeline_family = BuildTorchPipelineFamily(result);
  if (pipeline_family == "yolo_then_mobilevit" || pipeline_family == "template_then_diff")
    return pipeline_family;
  return std::string();
}

std::string BuildTorchLifecycleDomain(const CxScriptExecutionResult &result)
{
  const std::string hints = result.layer + ";" + result.case_name + ";" +
                            result.input_task + ";" + result.dataset_profile;
  if (result.layer == "train" || hints.find("mainline.train") != std::string::npos ||
      hints.find(".train") != std::string::npos || !result.train_param_summary.empty())
    return "train_domain";
  if (result.layer == "infer" || hints.find("unified.infer") != std::string::npos ||
      hints.find(".infer") != std::string::npos || !result.infer_param_summary.empty())
    return "infer_domain";
  return "review_domain";
}

std::string SelectFirstText(const std::string &first, const std::string &second)
{
  return first.empty() ? second : first;
}

std::string BuildTorchSequenceStage(const CxScriptExecutionResult &result)
{
  const std::string sequence_family = BuildTorchSequenceFamily(result);
  if (sequence_family == "yolo_then_mobilevit")
  {
    if (!result.attach_back_ref.empty() || !result.baseline_class_ref.empty())
      return "attach_back";
    if (!result.roi_crop_packet_ref.empty() || !result.published_roi_crop_packet_ref.empty())
      return "roi_reclass";
    if (!result.bbox_candidate_list_ref.empty() || !result.published_bbox_candidate_list_ref.empty())
      return "detect";
  }
  if (sequence_family == "template_then_diff")
  {
    if (!result.roi_diff_candidate_ref.empty() || !result.published_roi_diff_candidate_ref.empty())
      return "diff";
    if (!result.template_alignment_ref.empty() || !result.published_template_alignment_ref.empty())
      return "template_alignment";
  }
  return result.layer.empty() ? std::string("report") : result.layer;
}

std::string BuildTorchSequenceIndex(const CxScriptExecutionResult &result)
{
  const std::string sequence_stage = BuildTorchSequenceStage(result);
  if (sequence_stage == "detect" || sequence_stage == "template_alignment")
    return "0";
  if (sequence_stage == "roi_reclass" || sequence_stage == "diff")
    return "1";
  if (sequence_stage == "attach_back")
    return "2";
  return std::string();
}

SequenceLinkTrace BuildTorchSequenceLinkTrace(const CxScriptExecutionResult &result)
{
  SequenceLinkTrace trace;
  trace.sequence_family = BuildTorchSequenceFamily(result);
  if (trace.sequence_family == "yolo_then_mobilevit")
  {
    trace.from_stage = "detect";
    trace.to_stage = "roi_reclass";
    trace.upstream_artifact_ref =
      SelectFirstText(result.bbox_candidate_list_ref, result.published_bbox_candidate_list_ref);
    trace.intermediate_artifact_ref =
      SelectFirstText(result.roi_crop_packet_ref, result.published_roi_crop_packet_ref);
    trace.downstream_artifact_ref = SelectFirstText(
      SelectFirstText(result.baseline_class_ref, result.baseline_feature_ref),
      result.attach_back_ref);
    trace.attach_back_ref = result.attach_back_ref;
  }
  else if (trace.sequence_family == "template_then_diff")
  {
    trace.from_stage = "template_alignment";
    trace.to_stage = "diff";
    trace.upstream_artifact_ref =
      SelectFirstText(result.template_alignment_ref, result.published_template_alignment_ref);
    trace.intermediate_artifact_ref = result.published_prior_roi_region_ref;
    trace.downstream_artifact_ref =
      SelectFirstText(result.roi_diff_candidate_ref, result.published_roi_diff_candidate_ref);
    trace.attach_back_ref = result.attach_back_ref;
  }
  trace.transition_status =
    trace.sequence_family.empty()
      ? std::string()
      : ((!trace.upstream_artifact_ref.empty() && !trace.downstream_artifact_ref.empty())
           ? std::string("ready")
           : std::string("partial"));
  return trace;
}

std::string SerializeSequenceLinkTrace(const SequenceLinkTrace &trace)
{
  std::vector<std::string> items;
  if (!trace.sequence_family.empty())
    items.push_back("sequence_family=" + trace.sequence_family);
  if (!trace.from_stage.empty())
    items.push_back("from_stage=" + trace.from_stage);
  if (!trace.to_stage.empty())
    items.push_back("to_stage=" + trace.to_stage);
  if (!trace.upstream_artifact_ref.empty())
    items.push_back("upstream_artifact_ref=" + trace.upstream_artifact_ref);
  if (!trace.intermediate_artifact_ref.empty())
    items.push_back("intermediate_artifact_ref=" + trace.intermediate_artifact_ref);
  if (!trace.downstream_artifact_ref.empty())
    items.push_back("downstream_artifact_ref=" + trace.downstream_artifact_ref);
  if (!trace.attach_back_ref.empty())
    items.push_back("attach_back_ref=" + trace.attach_back_ref);
  if (!trace.transition_status.empty())
    items.push_back("transition_status=" + trace.transition_status);
  return JoinTextItems(items, ";");
}

std::string BuildTorchStageTransitionSummary(const SequenceLinkTrace &trace)
{
  if (trace.sequence_family.empty())
    return std::string();
  return trace.from_stage + "->" + trace.to_stage + ":" + trace.transition_status;
}

std::string BuildTorchStageAbnormalSummary(const CxScriptExecutionResult &result,
                                           const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> items;
  if (!result.failure_phase.empty())
    items.push_back("failure_phase=" + result.failure_phase);
  if (!result.failure_mode.empty())
    items.push_back("failure_mode=" + result.failure_mode);
  if (!image_review.anomaly_flags.empty())
    items.push_back("anomaly_flags=" + JoinTextItems(image_review.anomaly_flags, ","));
  if (items.empty())
    return "none";
  return JoinTextItems(items, ";");
}

std::string BuildTorchSequenceCaseRef(const CxScriptExecutionResult &result)
{
  if (!result.case_name.empty())
    return result.case_name;
  if (!result.task_id.empty())
    return result.task_id;
  if (!result.script_name.empty())
    return result.script_name;
  return "torch.sequence";
}

std::string BuildTorchSequenceStageRef(const CxScriptExecutionResult &result,
                                       const std::string &sequence_stage)
{
  return "stage://" + BuildTorchSequenceCaseRef(result) + "/" + sequence_stage;
}

std::string BuildTorchSequenceScriptRef(const CxScriptExecutionResult &result,
                                        const std::string &sequence_family,
                                        const std::string &sequence_stage)
{
  if (!result.script_path.empty())
    return "cxscript://" + result.script_path + "#" + sequence_stage;
  if (sequence_family == "yolo_then_mobilevit")
    return "cxscript://torch_module/scenario/torch_yolo_mobilevit_infer_scenario.cxsc#" +
           sequence_stage;
  if (sequence_family == "template_then_diff")
    return "cxscript://torch_module/infer/torch_deeplab_unified_infer.cxsc#" +
           sequence_stage;
  return "cxscript://torch_module/" + BuildTorchSequenceCaseRef(result) + "#" +
         sequence_stage;
}

std::string BuildTorchSequenceImageRef(const CxScriptExecutionResult &result,
                                       const UnifiedImageReviewRecord &image_review,
                                       const std::string &sequence_stage)
{
  if ((sequence_stage == "detect" || sequence_stage == "template_load" ||
       sequence_stage == "test_load") &&
      !image_review.input_image_ref.empty())
    return image_review.input_image_ref;
  if (sequence_stage == "attach_back" && !result.attach_back_ref.empty())
    return result.attach_back_ref;
  if (!image_review.primary_visual_ref.empty())
    return image_review.primary_visual_ref;
  if (!result.published_primary_ref.empty())
    return result.published_primary_ref;
  if (!result.published_result_ref.empty())
    return result.published_result_ref;
  return image_review.input_image_ref;
}

std::string BuildTorchSequenceConclusionRef(const CxScriptExecutionResult &result,
                                            const std::string &sequence_stage)
{
  return "conclusion://" + BuildTorchSequenceCaseRef(result) + "/" + sequence_stage;
}

std::string BuildTorchSequenceIssueRef(const CxScriptExecutionResult &result,
                                       const std::string &sequence_stage,
                                       const std::string &status)
{
  return "issue://" + BuildTorchSequenceCaseRef(result) + "/" + sequence_stage + "/" +
         (status == "matched" ? std::string("review_confirm") : std::string("watch"));
}

std::vector<std::string> FilterTorchSequenceRefs(const std::vector<std::string> &refs);

std::string BuildTorchLifecycleZone(const std::string &sequence_stage)
{
  if (sequence_stage == "model_init" || sequence_stage == "weights_load" ||
      sequence_stage == "device_select" || sequence_stage == "template_load" ||
      sequence_stage == "test_domain_init")
    return "init";
  if (sequence_stage == "debug_entry" || sequence_stage == "threshold_review")
    return "debug";
  if (sequence_stage == "replay")
    return "replay";
  if (sequence_stage == "reset")
    return "reset";
  return "run";
}

std::string BuildTorchLifecycleInitOnce(const std::string &lifecycle_zone)
{
  return lifecycle_zone == "init" ? "true" : "false";
}

std::string BuildTorchLifecycleRepeatable(const std::string &lifecycle_zone)
{
  return lifecycle_zone == "run" || lifecycle_zone == "debug" ||
                 lifecycle_zone == "replay"
           ? "true"
           : "false";
}

std::string BuildTorchLifecycleReentryPolicy(const std::string &lifecycle_zone)
{
  if (lifecycle_zone == "init")
    return "skip_if_test_domain_active";
  if (lifecycle_zone == "reset")
    return "requires_new_test_domain";
  return "repeatable_without_reinitializing_domain";
}

std::string BuildTorchLifecycleRisk(const std::string &sequence_stage,
                                    const std::string &lifecycle_zone)
{
  if (lifecycle_zone == "init")
    return "reentry_risk=reloading_model_or_rebinding_resources";
  if (sequence_stage == "roi_crop")
    return "reentry_risk=roi_state_overwrite_if_bbox_context_changes";
  if (sequence_stage == "roi_reclass")
    return "reentry_risk=score_changes_with_weight_or_device_context";
  if (sequence_stage == "attach_back")
    return "reentry_risk=overlay_overwrite_without_new_result_ref";
  if (sequence_stage == "template_alignment")
    return "reentry_risk=alignment_context_missing_or_stale";
  if (sequence_stage == "roi_diff")
    return "reentry_risk=diff_candidate_context_missing_or_stale";
  return "reentry_risk=none";
}

std::string BuildTorchSequenceRecord(const CxScriptExecutionResult &result,
                                     const UnifiedImageReviewRecord &image_review,
                                     const std::string &sequence_family,
                                     const std::string &sequence_stage,
                                     int stage_order,
                                     const std::string &status,
                                     const std::vector<std::string> &element_refs,
                                     const std::vector<std::string> &data_refs)
{
  const std::string image_ref = BuildTorchSequenceImageRef(result, image_review, sequence_stage);
  const std::string lifecycle_zone = BuildTorchLifecycleZone(sequence_stage);
  const std::vector<std::string> filtered_element_refs =
    FilterTorchSequenceRefs(element_refs);
  const std::vector<std::string> filtered_data_refs =
    FilterTorchSequenceRefs(data_refs);
  std::vector<std::string> fields;
  fields.push_back("sequence_family=" + sequence_family);
  fields.push_back("sequence_stage=" + sequence_stage);
  fields.push_back("stage_order=" + std::to_string(stage_order));
  fields.push_back("stage_ref=" + BuildTorchSequenceStageRef(result, sequence_stage));
  fields.push_back("script_ref=" +
                   BuildTorchSequenceScriptRef(result, sequence_family, sequence_stage));
  fields.push_back("image_ref=" + image_ref);
  fields.push_back("primary_visual_ref=" + image_review.primary_visual_ref);
  fields.push_back("element_refs=" + JoinTextItems(filtered_element_refs, ","));
  fields.push_back("data_refs=" + JoinTextItems(filtered_data_refs, ","));
  fields.push_back("conclusion_ref=" + BuildTorchSequenceConclusionRef(result, sequence_stage));
  fields.push_back("issue_ref=" + BuildTorchSequenceIssueRef(result, sequence_stage, status));
  fields.push_back("status=" + status);
  fields.push_back("lifecycle_zone=" + lifecycle_zone);
  fields.push_back("init_once=" + BuildTorchLifecycleInitOnce(lifecycle_zone));
  fields.push_back("repeatable=" + BuildTorchLifecycleRepeatable(lifecycle_zone));
  fields.push_back("reentry_policy=" + BuildTorchLifecycleReentryPolicy(lifecycle_zone));
  fields.push_back("lifecycle_risk=" +
                   BuildTorchLifecycleRisk(sequence_stage, lifecycle_zone));
  return JoinTextItems(fields, "|");
}

std::string BuildTorchSequenceStatusForRef(const std::string &primary_ref,
                                           const std::string &fallback_status)
{
  return primary_ref.empty() ? fallback_status : std::string("matched");
}

std::string BuildTorchSequenceSubRef(const std::string &base_ref,
                                     const char *suffix)
{
  return base_ref.empty() ? std::string() : (base_ref + suffix);
}

std::vector<std::string> FilterTorchSequenceRefs(const std::vector<std::string> &refs)
{
  std::vector<std::string> filtered;
  for (size_t i = 0; i < refs.size(); ++i)
    PushUniqueText(filtered, refs[i]);
  return filtered;
}

std::string ExtractDelimitedField(const std::string &record, const char *field_name)
{
  const std::string prefix = std::string(field_name) + "=";
  size_t begin = 0;
  while (begin <= record.size())
  {
    const size_t end = record.find('|', begin);
    const std::string token =
      record.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
    if (token.find(prefix) == 0)
      return token.substr(prefix.size());
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return std::string();
}

std::vector<std::string> BuildTorchSequenceRecords(const CxScriptExecutionResult &result,
                                                   const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> records;
  const std::string sequence_family = BuildTorchSequenceFamily(result);
  const std::string bbox_ref = SelectFirstText(result.bbox_candidate_list_ref,
                                               result.published_bbox_candidate_list_ref);
  const std::string roi_ref = SelectFirstText(result.roi_crop_packet_ref,
                                              result.published_roi_crop_packet_ref);
  const std::string template_ref = SelectFirstText(result.template_alignment_ref,
                                                   result.published_template_alignment_ref);
  const std::string diff_ref = SelectFirstText(result.roi_diff_candidate_ref,
                                               result.published_roi_diff_candidate_ref);
  const std::string lifecycle_domain = BuildTorchLifecycleDomain(result);

  records.push_back(BuildTorchSequenceRecord(result,
                                             image_review,
                                             lifecycle_domain,
                                             "test_domain_init",
                                             1,
                                             lifecycle_domain.empty() ? "watch" : "matched",
                                             std::vector<std::string>{},
                                             std::vector<std::string>{lifecycle_domain,
                                                                      sequence_family}));
  records.push_back(BuildTorchSequenceRecord(result,
                                             image_review,
                                             lifecycle_domain,
                                             "model_init",
                                             2,
                                             lifecycle_domain.empty() ? "watch" : "matched",
                                             std::vector<std::string>{},
                                             std::vector<std::string>{lifecycle_domain,
                                                                      sequence_family}));
  records.push_back(BuildTorchSequenceRecord(result,
                                             image_review,
                                             lifecycle_domain,
                                             "weights_load",
                                             3,
                                             (result.consumed_weight_files.empty() &&
                                              result.consumed_weight_paths.empty())
                                               ? "watch"
                                               : "matched",
                                             std::vector<std::string>{},
                                             std::vector<std::string>{result.consumed_weight_files,
                                                                      result.consumed_weight_paths}));
  records.push_back(BuildTorchSequenceRecord(result,
                                             image_review,
                                             lifecycle_domain,
                                             "device_select",
                                             4,
                                             result.requested_device.empty() ? "watch" : "matched",
                                             std::vector<std::string>{},
                                             std::vector<std::string>{result.requested_device}));

  if (lifecycle_domain == "train_domain")
  {
    const bool train_lifecycle_observed =
      !result.train_param_summary.empty() && result.failure_phase.empty() &&
      result.failure_mode.empty();
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               lifecycle_domain,
                                               "dataset_bind",
                                               10,
                                               result.dataset_profile.empty() ? "watch" : "matched",
                                               std::vector<std::string>{image_review.input_image_ref},
                                               std::vector<std::string>{result.dataset_profile,
                                                                        result.input_task}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               lifecycle_domain,
                                               "train_param_bind",
                                               20,
                                               result.train_param_summary.empty() ? "watch" : "matched",
                                               std::vector<std::string>{},
                                               std::vector<std::string>{result.train_param_summary}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               lifecycle_domain,
                                               "trainer_lifecycle",
                                               30,
                                               train_lifecycle_observed ? "matched" : "watch",
                                               std::vector<std::string>{},
                                               std::vector<std::string>{
                                                 BuildTaskScopedTorchRef(result,
                                                                         "trainer_lifecycle_summary"),
                                                 BuildTaskScopedTorchRef(result,
                                                                         "trainer_flat_run"),
                                                 BuildTaskScopedTorchRef(result,
                                                                         "segmentation_trainer_lifecycle_summary"),
                                                 BuildTaskScopedTorchRef(result,
                                                                         "segmentation_trainer_flat_run")}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               lifecycle_domain,
                                               "train_execute",
                                               40,
                                               result.fit_time_ms > 0.0 ? "matched" : "watch",
                                               std::vector<std::string>{},
                                               std::vector<std::string>{result.train_param_summary}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               lifecycle_domain,
                                               "summary_publish",
                                               50,
                                               result.published_result_ref.empty() &&
                                                       result.result_object.empty()
                                                 ? "watch"
                                                 : "matched",
                                               std::vector<std::string>{},
                                               std::vector<std::string>{
                                                 BuildTaskScopedTorchRef(result,
                                                                         "unified_mainline_summary"),
                                                 BuildTaskScopedTorchRef(result,
                                                                         "segmentation_unified_summary"),
                                                 result.published_result_ref,
                                                 result.result_object}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               lifecycle_domain,
                                               "finish_writeback",
                                               60,
                                               result.published_result_ref.empty() ? "watch" : "matched",
                                               std::vector<std::string>{image_review.primary_visual_ref},
                                               std::vector<std::string>{result.published_result_ref,
                                                                        result.result_object}));
    return records;
  }

  records.push_back(BuildTorchSequenceRecord(result,
                                             image_review,
                                             lifecycle_domain,
                                             "image_input",
                                             5,
                                             image_review.input_image_ref.empty() ? "watch" : "matched",
                                             std::vector<std::string>{image_review.input_image_ref},
                                             std::vector<std::string>{image_review.input_image_ref}));

  if (sequence_family == "yolo_then_mobilevit")
  {
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               sequence_family,
                                               "detect",
                                               10,
                                               BuildTorchSequenceStatusForRef(bbox_ref, "watch"),
                                               std::vector<std::string>{bbox_ref,
                                                                        BuildTorchSequenceSubRef(bbox_ref, "#center_point"),
                                                                        BuildTorchSequenceSubRef(bbox_ref, "#click_point")},
                                               std::vector<std::string>{bbox_ref}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               sequence_family,
                                               "roi_crop",
                                               20,
                                               BuildTorchSequenceStatusForRef(roi_ref, "watch"),
                                               std::vector<std::string>{roi_ref,
                                                                        BuildTorchSequenceSubRef(roi_ref, "#closed_region"),
                                                                        bbox_ref},
                                               std::vector<std::string>{roi_ref}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               sequence_family,
                                               "roi_reclass",
                                               30,
                                               result.attach_back_confidence.empty() ? "watch" : "watch",
                                               std::vector<std::string>{roi_ref,
                                                                        BuildTorchSequenceSubRef(roi_ref, "#closed_region")},
                                               std::vector<std::string>{result.cluster_ref,
                                                                        result.distance_ref,
                                                                        result.anomaly_ref}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               sequence_family,
                                               "attach_back",
                                               40,
                                               BuildTorchSequenceStatusForRef(result.attach_back_ref, "watch"),
                                               std::vector<std::string>{result.attach_back_ref,
                                                                        bbox_ref,
                                                                        roi_ref},
                                               std::vector<std::string>{result.attach_back_ref}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               "template_diff_baseline_compare",
                                               "template_alignment",
                                               50,
                                               BuildTorchSequenceStatusForRef(template_ref, "watch"),
                                               std::vector<std::string>{template_ref},
                                               std::vector<std::string>{template_ref}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               "template_diff_baseline_compare",
                                               "roi_diff",
                                               60,
                                               BuildTorchSequenceStatusForRef(diff_ref, "watch"),
                                               std::vector<std::string>{diff_ref,
                                                                        BuildTorchSequenceSubRef(roi_ref, "#closed_region")},
                                               std::vector<std::string>{diff_ref}));
  }
  else if (sequence_family == "template_then_diff")
  {
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               sequence_family,
                                               "template_load",
                                               10,
                                               result.template_root.empty() ? "watch" : "matched",
                                               std::vector<std::string>{result.template_root},
                                               std::vector<std::string>{result.template_root}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               sequence_family,
                                               "test_load",
                                               20,
                                               result.pairs_ref.empty() ? "watch" : "matched",
                                               std::vector<std::string>{result.pairs_ref},
                                               std::vector<std::string>{result.pairs_ref}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               sequence_family,
                                               "template_alignment",
                                               30,
                                               BuildTorchSequenceStatusForRef(template_ref, "watch"),
                                               std::vector<std::string>{template_ref,
                                                                        BuildTorchSequenceSubRef(template_ref, "#template_to_test_line")},
                                               std::vector<std::string>{template_ref}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               sequence_family,
                                               "roi_diff",
                                               40,
                                               BuildTorchSequenceStatusForRef(diff_ref, "watch"),
                                               std::vector<std::string>{diff_ref},
                                               std::vector<std::string>{diff_ref}));
    records.push_back(BuildTorchSequenceRecord(result,
                                               image_review,
                                               sequence_family,
                                               "baseline_compare",
                                               50,
                                               (!result.baseline_class_ref.empty() ||
                                                !result.baseline_feature_ref.empty())
                                                 ? "matched"
                                                 : "watch",
                                               std::vector<std::string>{result.baseline_class_ref,
                                                                        result.baseline_feature_ref},
                                               std::vector<std::string>{result.baseline_class_ref,
                                                                        result.baseline_feature_ref}));
  }
  return records;
}

std::string BuildTorchSequenceSummary(const std::vector<std::string> &records)
{
  if (records.empty())
    return std::string();
  std::vector<std::string> items;
  for (size_t i = 0; i < records.size(); ++i)
  {
    const std::string stage = ExtractDelimitedField(records[i], "sequence_stage");
    const std::string status = ExtractDelimitedField(records[i], "status");
    if (!stage.empty())
      items.push_back(stage + "=" + (status.empty() ? std::string("unknown") : status));
  }
  return JoinTextItems(items, " -> ");
}

std::string BuildTorchSequenceStatusSummary(const std::vector<std::string> &records)
{
  if (records.empty())
    return std::string();
  int matched = 0;
  int watch = 0;
  int missing = 0;
  int abnormal = 0;
  for (size_t i = 0; i < records.size(); ++i)
  {
    const std::string status = ExtractDelimitedField(records[i], "status");
    if (status == "matched")
      ++matched;
    else if (status == "missing")
      ++missing;
    else if (status == "abnormal")
      ++abnormal;
    else
      ++watch;
  }
  return "matched=" + std::to_string(matched) + ",watch=" + std::to_string(watch) +
         ",missing=" + std::to_string(missing) + ",abnormal=" + std::to_string(abnormal);
}

std::string BuildTorchSequenceFieldSet(const std::vector<std::string> &records,
                                       const char *field_name)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < records.size(); ++i)
    PushUniqueText(items, ExtractDelimitedField(records[i], field_name));
  return JoinTextItems(items, ";");
}

std::string BuildTorchLifecycleSummary(const std::vector<std::string> &records)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < records.size(); ++i)
  {
    const std::string stage = ExtractDelimitedField(records[i], "sequence_stage");
    const std::string zone = ExtractDelimitedField(records[i], "lifecycle_zone");
    const std::string repeatable = ExtractDelimitedField(records[i], "repeatable");
    const std::string init_once = ExtractDelimitedField(records[i], "init_once");
    if (!stage.empty())
      items.push_back(stage + ":" + (zone.empty() ? std::string("unknown") : zone) +
                      ":repeatable=" + repeatable + ":init_once=" + init_once);
  }
  return JoinTextItems(items, " -> ");
}

std::string BuildTorchLifecycleStageRefs(const std::vector<std::string> &records,
                                         const std::string &wanted_zone)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < records.size(); ++i)
  {
    if (ExtractDelimitedField(records[i], "lifecycle_zone") != wanted_zone)
      continue;
    PushUniqueText(items, ExtractDelimitedField(records[i], "stage_ref"));
  }
  return JoinTextItems(items, ";");
}

std::string BuildTorchRepeatableStageRefs(const std::vector<std::string> &records)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < records.size(); ++i)
  {
    if (ExtractDelimitedField(records[i], "repeatable") != "true")
      continue;
    PushUniqueText(items, ExtractDelimitedField(records[i], "stage_ref"));
  }
  return JoinTextItems(items, ";");
}

std::string BuildTorchLifecycleRiskSummary(const std::vector<std::string> &records)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < records.size(); ++i)
  {
    const std::string stage = ExtractDelimitedField(records[i], "sequence_stage");
    const std::string risk = ExtractDelimitedField(records[i], "lifecycle_risk");
    if (!stage.empty() && !risk.empty() && risk != "reentry_risk=none")
      items.push_back(stage + ":" + risk);
  }
  return JoinTextItems(items, ";");
}

std::string BuildGuiElementRefSet(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < elements.size(); ++i)
    PushUniqueText(items, elements[i].element_id);
  return JoinTextItems(items, ";");
}

std::string BuildGuiElementTypeSet(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < elements.size(); ++i)
    PushUniqueText(items, elements[i].element_type);
  return JoinTextItems(items, ";");
}

std::string BuildGuiElementSourceSet(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    PushUniqueText(items, elements[i].provenance);
    PushUniqueText(items, elements[i].source_ref);
  }
  return JoinTextItems(items, ";");
}

std::string BuildGuiElementVisualAnchorSet(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    PushUniqueText(items, elements[i].primary_overlay_ref);
    PushUniqueText(items, elements[i].focus_region_ref);
    PushUniqueText(items, elements[i].local_delta_ref);
  }
  return JoinTextItems(items, ";");
}

std::string BuildGuiTemplateRelationSet(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    PushUniqueText(items, elements[i].template_relation);
    PushUniqueText(items, elements[i].linked_template_element_id);
  }
  return JoinTextItems(items, ";");
}

std::string BuildGuiConsistencyStatusSet(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < elements.size(); ++i)
    PushUniqueText(items, elements[i].consistency_status);
  return JoinTextItems(items, ";");
}

std::string BuildGuiChainRefSet(const std::vector<UnifiedElementChain> &chains)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < chains.size(); ++i)
    PushUniqueText(items, chains[i].chain_id);
  return JoinTextItems(items, ";");
}

std::string BuildGuiChainKeySet(const std::vector<UnifiedElementChain> &chains)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < chains.size(); ++i)
  {
    PushUniqueText(items, chains[i].chain_type);
    PushUniqueText(items, chains[i].template_relation);
  }
  return JoinTextItems(items, ";");
}

std::string BuildGuiChainStatusSet(const std::vector<UnifiedElementChain> &chains)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < chains.size(); ++i)
    PushUniqueText(items, chains[i].chain_status);
  return JoinTextItems(items, ";");
}

std::string BuildGuiChainFocusRefSet(const std::vector<UnifiedElementChain> &chains)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < chains.size(); ++i)
  {
    PushUniqueText(items, chains[i].source_ref);
    PushUniqueText(items, chains[i].target_ref);
    PushUniqueText(items, chains[i].chain_focus);
  }
  return JoinTextItems(items, ";");
}

std::string BuildGuiChainIssueRefSet(const std::vector<UnifiedElementChain> &chains)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < chains.size(); ++i)
  {
    PushUniqueText(items, chains[i].chain_findings);
    if (chains[i].chain_status != "matched")
      PushUniqueText(items, chains[i].chain_id);
  }
  return JoinTextItems(items, ";");
}

std::string SelectGuiRecommendedElementRef(const UnifiedImageReviewRecord &image_review)
{
  for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = image_review.detection_elements[i];
    if (element.consistency_status != "matched" || element.manual_review_signal == "review")
      return element.element_id;
  }
  return image_review.detection_elements.empty() ? std::string() : image_review.detection_elements.front().element_id;
}

std::string SelectGuiRecommendedChainRef(const UnifiedImageReviewRecord &image_review)
{
  for (size_t i = 0; i < image_review.element_chains.size(); ++i)
  {
    const UnifiedElementChain &chain = image_review.element_chains[i];
    if (chain.chain_status != "matched")
      return chain.chain_id;
  }
  return image_review.element_chains.empty() ? std::string() : image_review.element_chains.front().chain_id;
}

std::string BuildGuiIssueKindHint(const CxScriptExecutionResult &result,
                                  const UnifiedImageReviewRecord &image_review)
{
  if (!result.failure_mode.empty() && result.failure_mode != "none")
    return result.failure_mode;
  if (!image_review.anomaly_flags.empty())
    return JoinTextItems(image_review.anomaly_flags, ",");
  if (image_review.drifted_element_count > 0)
    return "element_drift";
  if (image_review.missing_element_count > 0)
    return "missing_element";
  if (image_review.abnormal_element_count > 0)
    return "abnormal_element";
  return "none";
}
std::string BuildEnsmallenGuiSingleImageConclusion(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  const std::string objective_curve = BuildEnsmallenObjectiveCurveValue(result);
  const std::string stability_score = BuildEnsmallenStabilityScoreValue(result);
  const std::string convergence_status = BuildEnsmallenConvergenceStatusValue(result);
  const std::string candidate_rank = BuildEnsmallenCandidateRankValue(result);
  const std::string best_candidate_confidence =
    BuildEnsmallenBestCandidateConfidenceValue(result);

  if (!objective_curve.empty())
    items.push_back("objective=" + objective_curve);
  if (!stability_score.empty())
    items.push_back("stability=" + stability_score);
  if (!convergence_status.empty())
    items.push_back("convergence=" + convergence_status);
  if (!candidate_rank.empty())
    items.push_back("best_candidate=" + candidate_rank);
  if (!best_candidate_confidence.empty())
    items.push_back("confidence=" + best_candidate_confidence);
  if (!result.selected_method.empty())
    items.push_back("method=" + result.selected_method);
  return JoinTextItems(items, ";");
}

std::string SelectEnsmallenRecommendedElementRef(const UnifiedImageReviewRecord &image_review)
{
  const char *preferred_types[] = {
    "parameter_set",
    "optimization_compare",
    "objective_target",
    "optimization_result",
    "anomaly_focus",
    "boundary_metric",
    "alignment_metric",
    "closed_region"
  };

  for (size_t type_index = 0;
       type_index < (sizeof(preferred_types) / sizeof(preferred_types[0]));
       ++type_index)
  {
    for (size_t element_index = 0;
         element_index < image_review.detection_elements.size();
         ++element_index)
    {
      const UnifiedDetectionElement &element =
        image_review.detection_elements[element_index];
      if (element.element_type == preferred_types[type_index] &&
          !element.element_id.empty())
      {
        return element.element_id;
      }
    }
  }

  return SelectGuiRecommendedElementRef(image_review);
}

std::string BuildEnsmallenGuiVisualEvidenceRefSet(const CxScriptExecutionResult &result,
                                                  const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> refs = image_review.visualization_refs;
  PushUniqueText(refs, image_review.input_image_ref);
  PushUniqueText(refs, image_review.primary_visual_ref);
  PushUniqueText(refs, ResolveNamedOrDirectRef(result, "refs", "summary_ref", result.summary_ref));
  PushUniqueText(refs, ResolveNamedOrDirectRef(result, "refs", "compare_ref", result.compare_ref));
  PushUniqueText(refs,
                 ResolveNamedOrDirectRef(result,
                                         "refs",
                                         "replay_ref",
                                         result.replay_ref.empty() ? result.replay_log_path
                                                                   : result.replay_ref));
  PushUniqueText(refs,
                 ResolveNamedOrDirectRef(result,
                                         "refs",
                                         "best_params_ref",
                                         result.best_params_ref));
  PushUniqueText(refs,
                 ResolveNamedOrDirectRef(result,
                                         "refs",
                                         "objective_ref",
                                         result.objective_ref));
  PushUniqueText(refs,
                 ResolveNamedOrDirectRef(result,
                                         "refs",
                                         "optimization_result_ref",
                                         result.optimization_result_ref));
  PushUniqueText(refs, ResolveEnsmallenBridgeRef(result, "threshold_ref"));
  PushUniqueText(refs, ResolveEnsmallenBridgeRef(result, "crop_policy_ref"));
  PushUniqueText(refs, ResolveEnsmallenBridgeRef(result, "boundary_error_ref"));
  PushUniqueText(refs, ResolveEnsmallenBridgeRef(result, "alignment_error_ref"));
  return JoinTextItems(refs, ";");
}

std::string BuildEnsmallenGuiElementConclusionRefSet(const CxScriptExecutionResult &result,
                                                     const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> items;
  const std::string candidate_rank = BuildEnsmallenCandidateRankValue(result);
  const std::string best_candidate_confidence =
    BuildEnsmallenBestCandidateConfidenceValue(result);
  const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
  const std::string risk_axis = BuildEnsmallenRiskAxis(result);

  const std::string element_summary =
    BuildDetectionElementSummary(image_review.detection_elements);
  if (!element_summary.empty())
    items.push_back("elements=" + element_summary);
  if (!image_review.element_level_focus.empty())
    items.push_back("focus=" + image_review.element_level_focus);
  if (!result.selected_method.empty())
    items.push_back("method=" + result.selected_method);
  if (!result.ordered_candidates.empty())
    items.push_back("candidate_ordering=" + result.ordered_candidates);
  if (!candidate_rank.empty())
    items.push_back("candidate_rank=" + candidate_rank);
  if (!best_candidate_confidence.empty())
    items.push_back("best_candidate_confidence=" + best_candidate_confidence);
  if (!coverage_gap.empty())
    items.push_back("coverage_gap=" + coverage_gap);
  if (!risk_axis.empty())
    items.push_back("risk_axis=" + risk_axis);
  return JoinTextItems(items, ";");
}

std::string BuildEnsmallenGuiTaskConclusion(const CxScriptExecutionResult &result)
{
  std::vector<std::string> items;
  const std::string single_image_conclusion =
    BuildEnsmallenGuiSingleImageConclusion(result);
  const std::string feature_distance_delta =
    BuildEnsmallenFeatureDistanceDeltaValue(result);
  const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
  const std::string risk_axis = BuildEnsmallenRiskAxis(result);
  const std::string optimization_signal =
    FindNamedResultFieldValue(result, "analysis", "optimization_signal");

  if (!single_image_conclusion.empty())
    items.push_back(single_image_conclusion);
  if (!feature_distance_delta.empty())
    items.push_back("feature_distance_delta=" + feature_distance_delta);
  if (!coverage_gap.empty())
    items.push_back("coverage_gap=" + coverage_gap);
  if (!risk_axis.empty())
    items.push_back("risk_axis=" + risk_axis);
  if (!optimization_signal.empty())
    items.push_back("signal=" + optimization_signal);
  return JoinTextItems(items, ";");
}

std::string BuildEnsmallenGuiAnomalyConclusion(const CxScriptExecutionResult &result,
                                               const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> items;
  const std::string likely_issue_class =
    FindNamedResultFieldValue(result, "analysis", "likely_issue_class");
  const std::string review_scope = BuildEnsmallenReviewScope(result);
  const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
  const std::string risk_axis = BuildEnsmallenRiskAxis(result);

  if (!likely_issue_class.empty())
    items.push_back("issue_kind=" + likely_issue_class);
  if (!risk_axis.empty())
    items.push_back("risk_axis=" + risk_axis);
  if (!coverage_gap.empty())
    items.push_back("coverage_gap=" + coverage_gap);
  if (!review_scope.empty())
    items.push_back("review_scope=" + review_scope);
  if (items.empty() && !image_review.anomaly_flags.empty())
    items.push_back(JoinTextItems(image_review.anomaly_flags, ";"));
  return JoinTextItems(items, ";");
}

std::string BuildEnsmallenGuiIssueEntryRefSet(const CxScriptExecutionResult &result,
                                              const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> items;
  PushUniqueText(items, image_review.chain_issue_ref);
  PushUniqueText(items, image_review.element_level_focus);
  PushUniqueText(items, image_review.focus_refresh_targets);
  PushUniqueText(items, image_review.local_delta_targets);
  PushUniqueText(items,
                 ResolveNamedOrDirectRef(result, "refs", "best_params_ref", result.best_params_ref));
  PushUniqueText(items,
                 ResolveNamedOrDirectRef(result, "refs", "objective_ref", result.objective_ref));
  PushUniqueText(items, ResolveNamedOrDirectRef(result, "refs", "compare_ref", result.compare_ref));
  PushUniqueText(items,
                 ResolveNamedOrDirectRef(result,
                                         "refs",
                                         "replay_ref",
                                         result.replay_ref.empty() ? result.replay_log_path
                                                                   : result.replay_ref));
  PushUniqueText(items, ResolveEnsmallenBridgeRef(result, "threshold_ref"));
  PushUniqueText(items, ResolveEnsmallenBridgeRef(result, "crop_policy_ref"));
  PushUniqueText(items, ResolveEnsmallenBridgeRef(result, "boundary_error_ref"));
  PushUniqueText(items, ResolveEnsmallenBridgeRef(result, "alignment_error_ref"));

  const std::string recommended_action =
    FindNamedResultFieldValue(result, "analysis", "recommended_action");
  const std::string likely_issue_class =
    FindNamedResultFieldValue(result, "analysis", "likely_issue_class");
  const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
  const std::string risk_axis = BuildEnsmallenRiskAxis(result);

  if (!recommended_action.empty())
    PushUniqueText(items, "action=" + recommended_action);
  if (!likely_issue_class.empty())
    PushUniqueText(items, "issue_kind=" + likely_issue_class);
  if (!coverage_gap.empty())
    PushUniqueText(items, "coverage_gap=" + coverage_gap);
  if (!risk_axis.empty())
    PushUniqueText(items, "risk_axis=" + risk_axis);
  return JoinTextItems(items, ";");
}

std::string BuildEnsmallenGuiRecommendedStageRef(const CxScriptExecutionResult &result,
                                                 const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> items;
  const std::string convergence_status =
    BuildEnsmallenConvergenceStatusValue(result);
  const std::string risk_axis = BuildEnsmallenRiskAxis(result);
  const std::string recommended_action =
    FindNamedResultFieldValue(result, "analysis", "recommended_action");

  if (!image_review.current_stage.empty())
    items.push_back("stage=" + image_review.current_stage);
  if (!convergence_status.empty())
    items.push_back("convergence=" + convergence_status);
  if (!risk_axis.empty())
    items.push_back("risk_axis=" + risk_axis);
  if (!recommended_action.empty())
    items.push_back("action=" + recommended_action);
  return JoinTextItems(items, ";");
}

void PopulateGuiImageInputContract(CxScriptExecutionResult &result,
                                   UnifiedImageReviewRecord &image_review,
                                   const SequenceLinkTrace &sequence_trace)
{
  image_review.test_image_ref = image_review.input_image_ref;
  image_review.visual_evidence_ref_set = JoinTextItems(image_review.visualization_refs, ";");
  image_review.single_image_conclusion_ref =
    image_review.notes.empty() ? image_review.status : image_review.notes;
  image_review.element_conclusion_ref_set =
    BuildDetectionElementSummary(image_review.detection_elements);
  image_review.task_conclusion_ref = result.success ? result.summary : result.error_message;
  image_review.anomaly_conclusion_ref =
    image_review.anomaly_flags.empty() ? "none" : JoinTextItems(image_review.anomaly_flags, ";");
  image_review.next_action_ref = DetermineReviewerActionText(result);
  image_review.element_ref_set = BuildGuiElementRefSet(image_review.detection_elements);
  image_review.element_type = BuildGuiElementTypeSet(image_review.detection_elements);
  image_review.element_source = BuildGuiElementSourceSet(image_review.detection_elements);
  image_review.element_visual_anchor = BuildGuiElementVisualAnchorSet(image_review.detection_elements);
  image_review.template_relation = BuildGuiTemplateRelationSet(image_review.detection_elements);
  image_review.consistency_status = BuildGuiConsistencyStatusSet(image_review.detection_elements);
  image_review.chain_ref_set = BuildGuiChainRefSet(image_review.element_chains);
  image_review.chain_key = BuildGuiChainKeySet(image_review.element_chains);
  image_review.chain_status = BuildGuiChainStatusSet(image_review.element_chains);
  image_review.chain_focus_ref = BuildGuiChainFocusRefSet(image_review.element_chains);
  image_review.chain_issue_ref = BuildGuiChainIssueRefSet(image_review.element_chains);
  std::vector<std::string> stage_refs;
  PushUniqueText(stage_refs, image_review.sequence_trace_ref);
  PushUniqueText(stage_refs, sequence_trace.upstream_artifact_ref);
  PushUniqueText(stage_refs, sequence_trace.intermediate_artifact_ref);
  PushUniqueText(stage_refs, sequence_trace.downstream_artifact_ref);
  PushUniqueText(stage_refs, sequence_trace.attach_back_ref);
  image_review.stage_ref_set = JoinTextItems(stage_refs, ";");
  image_review.current_stage = image_review.sequence_stage.empty() ? image_review.stage : image_review.sequence_stage;
  image_review.upstream_ref = sequence_trace.upstream_artifact_ref;
  image_review.downstream_ref = sequence_trace.downstream_artifact_ref;
  image_review.stage_status = sequence_trace.transition_status.empty() ? image_review.status : sequence_trace.transition_status;
  std::vector<std::string> issue_refs;
  PushUniqueText(issue_refs, image_review.chain_issue_ref);
  PushUniqueText(issue_refs, image_review.element_level_focus);
  PushUniqueText(issue_refs, image_review.focus_refresh_targets);
  PushUniqueText(issue_refs, image_review.local_delta_targets);
  image_review.issue_entry_ref_set = JoinTextItems(issue_refs, ";");
  image_review.recommended_image_ref =
    image_review.primary_visual_ref.empty() ? image_review.input_image_ref : image_review.primary_visual_ref;
  image_review.recommended_element_ref = SelectGuiRecommendedElementRef(image_review);
  image_review.recommended_chain_ref = SelectGuiRecommendedChainRef(image_review);
  image_review.recommended_stage_ref = image_review.current_stage;
  image_review.issue_kind_hint = BuildGuiIssueKindHint(result, image_review);

  if (NormalizeReviewSourceThread(result) == "ensmallen")
  {
    const std::string recommended_action =
      FindNamedResultFieldValue(result, "analysis", "recommended_action");
    const std::string likely_issue_class =
      FindNamedResultFieldValue(result, "analysis", "likely_issue_class");
    const std::string task_conclusion =
      BuildEnsmallenGuiTaskConclusion(result);
    if (!image_review.input_image_ref.empty())
      image_review.recommended_image_ref = image_review.input_image_ref;
    image_review.visual_evidence_ref_set =
      BuildEnsmallenGuiVisualEvidenceRefSet(result, image_review);
    image_review.recommended_element_ref =
      SelectEnsmallenRecommendedElementRef(image_review);
    if (!image_review.default_open_chain.empty())
      image_review.recommended_chain_ref = image_review.default_open_chain;
    image_review.recommended_stage_ref =
      BuildEnsmallenGuiRecommendedStageRef(result, image_review);
    if (!recommended_action.empty())
      image_review.next_action_ref = recommended_action;
    if (!likely_issue_class.empty())
      image_review.issue_kind_hint = likely_issue_class;
    else if (!BuildEnsmallenRiskAxis(result).empty())
      image_review.issue_kind_hint = BuildEnsmallenRiskAxis(result);
    image_review.single_image_conclusion_ref =
      BuildEnsmallenGuiSingleImageConclusion(result);
    image_review.element_conclusion_ref_set =
      BuildEnsmallenGuiElementConclusionRefSet(result, image_review);
    image_review.task_conclusion_ref =
      task_conclusion.empty() ? image_review.task_conclusion_ref : task_conclusion;
    image_review.anomaly_conclusion_ref =
      BuildEnsmallenGuiAnomalyConclusion(result, image_review);
    image_review.issue_entry_ref_set =
      BuildEnsmallenGuiIssueEntryRefSet(result, image_review);
  }
}

void PopulateGuiTaskInputContract(UnifiedTaskReviewBundle &task_review,
                                  const UnifiedImageReviewRecord &image_review)
{
  task_review.test_image_ref = image_review.test_image_ref;
  task_review.visual_evidence_ref_set = image_review.visual_evidence_ref_set;
  task_review.task_conclusion_ref =
    image_review.task_conclusion_ref.empty() ? task_review.current_conclusion
                                           : image_review.task_conclusion_ref;
  task_review.anomaly_conclusion_ref =
    task_review.anomaly_type_distribution.empty() ? image_review.anomaly_conclusion_ref
                                                  : task_review.anomaly_type_distribution;
  task_review.next_action_ref =
    image_review.next_action_ref.empty() ? task_review.next_attention_points
                                        : image_review.next_action_ref;
  task_review.element_ref_set = image_review.element_ref_set;
  task_review.chain_ref_set = image_review.chain_ref_set;
  task_review.stage_ref_set = image_review.stage_ref_set;
  task_review.issue_entry_ref_set = image_review.issue_entry_ref_set;
  task_review.recommended_image_ref = image_review.recommended_image_ref;
  task_review.recommended_element_ref = image_review.recommended_element_ref;
  task_review.recommended_chain_ref = image_review.recommended_chain_ref;
  task_review.recommended_stage_ref = image_review.recommended_stage_ref;
  task_review.issue_kind_hint = image_review.issue_kind_hint;
}

void RefreshPhase0UnifiedObjects(CxScriptExecutionResult &result)
{
  result.task_contexts.clear();
  result.geometry_semantic_types.clear();
  result.geometry_template_specs.clear();
  result.image_acquisition_specs.clear();
  result.training_inputs.clear();
  result.run_inputs.clear();
  result.review_decisions.clear();
  result.flowback_actions.clear();
  result.sequence_link_traces.clear();

  TaskContext task_context;
  task_context.source_thread = NormalizeReviewSourceThread(result);
  task_context.task_id = result.task_id.empty() ? result.case_name : result.task_id;
  task_context.batch_id = BuildReviewBatchId(result);
  task_context.case_name = result.case_name;
  task_context.stage = result.layer.empty() ? "report" : result.layer;
  task_context.task_entry_name = BuildTorchTaskEntryName(result);
  task_context.task_family = BuildTorchTaskFamily(result);
  task_context.pipeline_family = BuildTorchPipelineFamily(result);
  task_context.model_family = BuildTorchModelFamily(result);
  task_context.scenario_family = BuildTorchScenarioFamily(result);
  task_context.requested_device = result.requested_device;
  task_context.actual_device = result.requested_device;
  task_context.device_evidence = BuildTorchDeviceEvidence(result);
  const SequenceLinkTrace sequence_trace = BuildTorchSequenceLinkTrace(result);
  task_context.sequence_family = sequence_trace.sequence_family;
  task_context.sequence_stage = BuildTorchSequenceStage(result);
  task_context.sequence_index = BuildTorchSequenceIndex(result);
  task_context.upstream_ref = sequence_trace.upstream_artifact_ref;
  task_context.downstream_ref = sequence_trace.downstream_artifact_ref;
  result.task_contexts.push_back(task_context);
  if (!sequence_trace.sequence_family.empty())
    result.sequence_link_traces.push_back(sequence_trace);

  GeometrySemanticType geometry_semantic_type;
  geometry_semantic_type.primary_geometry_semantic = DeterminePrimaryGeometrySemantic(result);
  CollectAuxiliaryGeometrySemantics(result,
                                    geometry_semantic_type.primary_geometry_semantic,
                                    geometry_semantic_type.auxiliary_geometry_semantics);
  const char *supported_geometry_semantics[] = {
    "point",
    "click_point",
    "line_segment",
    "open_polyline",
    "closed_region",
    "candidate_region",
    "match_region",
    "circle",
    "arc",
    "composite_template"
  };
  for (size_t i = 0; i < sizeof(supported_geometry_semantics) / sizeof(supported_geometry_semantics[0]); ++i)
    geometry_semantic_type.supported_geometry_semantics.push_back(supported_geometry_semantics[i]);
  geometry_semantic_type.semantic_status =
    geometry_semantic_type.primary_geometry_semantic.empty() ? "partial" : "ready";
  result.geometry_semantic_types.push_back(geometry_semantic_type);

  GeometryTemplateSpec geometry_template_spec;
  geometry_template_spec.primary_geometry_semantic =
    geometry_semantic_type.primary_geometry_semantic;
  geometry_template_spec.auxiliary_geometry_semantics =
    geometry_semantic_type.auxiliary_geometry_semantics;
  geometry_template_spec.template_provenance = DetermineTemplateProvenance(result);
  geometry_template_spec.template_identity = DetermineTemplateIdentity(result);
  geometry_template_spec.review_priority = DetermineTemplateReviewPriority(result);
  result.geometry_template_specs.push_back(geometry_template_spec);

  ImageAcquisitionSpec image_acquisition_spec;
  image_acquisition_spec.scope_type = DetermineImageScopeType(result);
  image_acquisition_spec.execution_mode = DetermineExecutionMode(result);
  image_acquisition_spec.source_image_ref = SelectReviewInputImageRef(result);
  image_acquisition_spec.crop_identity =
    !result.roi_crop_packet_ref.empty() ? result.roi_crop_packet_ref :
    (!result.published_roi_crop_packet_ref.empty() ? result.published_roi_crop_packet_ref :
     (!result.published_prior_roi_region_ref.empty() ? result.published_prior_roi_region_ref :
      (!result.bbox_candidate_list_ref.empty() ? result.bbox_candidate_list_ref :
       image_acquisition_spec.source_image_ref)));
  image_acquisition_spec.provenance = DetermineImageAcquisitionProvenance(result);
  result.image_acquisition_specs.push_back(image_acquisition_spec);

  TrainingInput training_input;
  training_input.task_context = task_context;
  training_input.geometry_template_spec = geometry_template_spec;
  training_input.image_acquisition_spec = image_acquisition_spec;
  training_input.dataset_ref =
    !result.dataset_ref.empty() ? result.dataset_ref :
    (!result.input_dataset.empty() ? result.input_dataset :
     (!result.sample_bundle_ref.empty() ? result.sample_bundle_ref : image_acquisition_spec.source_image_ref));
  training_input.model_route = DetermineModelRoute(result);
  result.training_inputs.push_back(training_input);

  RunInput run_input;
  run_input.task_context = task_context;
  run_input.geometry_template_spec = geometry_template_spec;
  run_input.image_acquisition_spec = image_acquisition_spec;
  run_input.input_image_ref = image_acquisition_spec.source_image_ref;
  run_input.model_route = training_input.model_route;
  result.run_inputs.push_back(run_input);

  ReviewDecision review_decision;
  review_decision.task_context = task_context;
  review_decision.target_object_ref = DeterminePhase0TargetObjectRef(result);
  review_decision.review_status =
    !result.success ? "needs_review" : (result.degraded ? "review_only" : "accepted");
  review_decision.review_reason =
    result.error_message.empty() ? result.summary : result.error_message;
  review_decision.reviewer_action = DetermineReviewerActionText(result);
  result.review_decisions.push_back(review_decision);

  FlowbackAction flowback_action;
  flowback_action.task_context = task_context;
  flowback_action.target_object_ref = review_decision.target_object_ref;
  flowback_action.flowback_type = DetermineFlowbackType(result);
  flowback_action.trigger_reason = review_decision.review_reason;
  flowback_action.next_target = DetermineFlowbackNextTarget(result);
  result.flowback_actions.push_back(flowback_action);

  AddNamedResultObject(result,
                       "task_context",
                       "phase0",
                       "TaskContext",
                       task_context.task_id.empty() ? "partial" : "ready",
                       result.failure_phase);
  AddNamedResultField(result, "task_context", "phase0", "source_thread", task_context.source_thread);
  AddNamedResultField(result, "task_context", "phase0", "task_id", task_context.task_id);
  AddNamedResultField(result, "task_context", "phase0", "batch_id", task_context.batch_id);
  AddNamedResultField(result, "task_context", "phase0", "case_name", task_context.case_name);
  AddNamedResultField(result, "task_context", "phase0", "stage", task_context.stage);
  AddNamedResultField(result, "task_context", "phase0", "task_entry_name", task_context.task_entry_name);
  AddNamedResultField(result, "task_context", "phase0", "task_family", task_context.task_family);
  AddNamedResultField(result, "task_context", "phase0", "pipeline_family", task_context.pipeline_family);
  AddNamedResultField(result, "task_context", "phase0", "model_family", task_context.model_family);
  AddNamedResultField(result, "task_context", "phase0", "scenario_family", task_context.scenario_family);
  AddNamedResultField(result, "task_context", "phase0", "requested_device", task_context.requested_device);
  AddNamedResultField(result, "task_context", "phase0", "actual_device", task_context.actual_device);
  AddNamedResultField(result, "task_context", "phase0", "device_evidence", task_context.device_evidence);
  AddNamedResultField(result, "task_context", "phase0", "sequence_family", task_context.sequence_family);
  AddNamedResultField(result, "task_context", "phase0", "sequence_stage", task_context.sequence_stage);
  AddNamedResultField(result, "task_context", "phase0", "sequence_index", task_context.sequence_index);
  AddNamedResultField(result, "task_context", "phase0", "upstream_ref", task_context.upstream_ref);
  AddNamedResultField(result, "task_context", "phase0", "downstream_ref", task_context.downstream_ref);
  if (!sequence_trace.sequence_family.empty())
  {
    AddNamedResultObject(result,
                         "sequence_link_trace",
                         "phase0",
                         "SequenceLinkTrace",
                         sequence_trace.transition_status,
                         result.failure_phase);
    AddNamedResultField(result, "sequence_link_trace", "phase0", "sequence_family",
                        sequence_trace.sequence_family);
    AddNamedResultField(result, "sequence_link_trace", "phase0", "from_stage",
                        sequence_trace.from_stage);
    AddNamedResultField(result, "sequence_link_trace", "phase0", "to_stage",
                        sequence_trace.to_stage);
    AddNamedResultField(result, "sequence_link_trace", "phase0", "upstream_artifact_ref",
                        sequence_trace.upstream_artifact_ref);
    AddNamedResultField(result, "sequence_link_trace", "phase0", "intermediate_artifact_ref",
                        sequence_trace.intermediate_artifact_ref);
    AddNamedResultField(result, "sequence_link_trace", "phase0", "downstream_artifact_ref",
                        sequence_trace.downstream_artifact_ref);
    AddNamedResultField(result, "sequence_link_trace", "phase0", "attach_back_ref",
                        sequence_trace.attach_back_ref);
    AddNamedResultField(result, "sequence_link_trace", "phase0", "transition_status",
                        sequence_trace.transition_status);
  }

  AddNamedResultObject(result,
                       "geometry_semantic_type",
                       "phase0",
                       "GeometrySemanticType",
                       geometry_semantic_type.semantic_status,
                       std::string());
  AddNamedResultField(result,
                      "geometry_semantic_type",
                      "phase0",
                      "primary_geometry_semantic",
                      geometry_semantic_type.primary_geometry_semantic);
  AddNamedResultField(result,
                      "geometry_semantic_type",
                      "phase0",
                      "auxiliary_geometry_semantics",
                      JoinTextItems(geometry_semantic_type.auxiliary_geometry_semantics, ";"));
  AddNamedResultField(result,
                      "geometry_semantic_type",
                      "phase0",
                      "supported_geometry_semantics",
                      JoinTextItems(geometry_semantic_type.supported_geometry_semantics, ";"));
  AddNamedResultField(result,
                      "geometry_semantic_type",
                      "phase0",
                      "semantic_status",
                      geometry_semantic_type.semantic_status);

  AddNamedResultObject(result,
                       "geometry_template_spec",
                       "phase0",
                       "GeometryTemplateSpec",
                       geometry_template_spec.template_identity.empty() ? "partial" : "ready",
                       std::string());
  AddNamedResultField(result,
                      "geometry_template_spec",
                      "phase0",
                      "primary_geometry_semantic",
                      geometry_template_spec.primary_geometry_semantic);
  AddNamedResultField(result,
                      "geometry_template_spec",
                      "phase0",
                      "auxiliary_geometry_semantics",
                      JoinTextItems(geometry_template_spec.auxiliary_geometry_semantics, ";"));
  AddNamedResultField(result,
                      "geometry_template_spec",
                      "phase0",
                      "template_provenance",
                      geometry_template_spec.template_provenance);
  AddNamedResultField(result,
                      "geometry_template_spec",
                      "phase0",
                      "template_identity",
                      geometry_template_spec.template_identity);
  AddNamedResultField(result,
                      "geometry_template_spec",
                      "phase0",
                      "review_priority",
                      geometry_template_spec.review_priority);

  AddNamedResultObject(result,
                       "image_acquisition_spec",
                       "phase0",
                       "ImageAcquisitionSpec",
                       image_acquisition_spec.source_image_ref.empty() ? "partial" : "ready",
                       std::string());
  AddNamedResultField(result, "image_acquisition_spec", "phase0", "scope_type",
                      image_acquisition_spec.scope_type);
  AddNamedResultField(result, "image_acquisition_spec", "phase0", "execution_mode",
                      image_acquisition_spec.execution_mode);
  AddNamedResultField(result, "image_acquisition_spec", "phase0", "source_image_ref",
                      image_acquisition_spec.source_image_ref);
  AddNamedResultField(result, "image_acquisition_spec", "phase0", "crop_identity",
                      image_acquisition_spec.crop_identity);
  AddNamedResultField(result, "image_acquisition_spec", "phase0", "provenance",
                      image_acquisition_spec.provenance);

  AddNamedResultObject(result,
                       "training_input",
                       "phase0",
                       "TrainingInput",
                       training_input.dataset_ref.empty() ? "partial" : "ready",
                       std::string());
  AddNamedResultField(result, "training_input", "phase0", "task_context",
                      BuildTaskContextRef(training_input.task_context));
  AddNamedResultField(result, "training_input", "phase0", "geometry_template_spec",
                      training_input.geometry_template_spec.template_identity);
  AddNamedResultField(result, "training_input", "phase0", "image_acquisition_spec",
                      BuildImageAcquisitionRef(training_input.image_acquisition_spec));
  AddNamedResultField(result, "training_input", "phase0", "dataset_ref", training_input.dataset_ref);
  AddNamedResultField(result, "training_input", "phase0", "model_route", training_input.model_route);

  AddNamedResultObject(result,
                       "run_input",
                       "phase0",
                       "RunInput",
                       run_input.input_image_ref.empty() ? "partial" : "ready",
                       std::string());
  AddNamedResultField(result, "run_input", "phase0", "task_context",
                      BuildTaskContextRef(run_input.task_context));
  AddNamedResultField(result, "run_input", "phase0", "geometry_template_spec",
                      run_input.geometry_template_spec.template_identity);
  AddNamedResultField(result, "run_input", "phase0", "image_acquisition_spec",
                      BuildImageAcquisitionRef(run_input.image_acquisition_spec));
  AddNamedResultField(result, "run_input", "phase0", "input_image_ref", run_input.input_image_ref);
  AddNamedResultField(result, "run_input", "phase0", "model_route", run_input.model_route);

  AddNamedResultObject(result,
                       "review_decision",
                       "phase0",
                       "ReviewDecision",
                       review_decision.review_status,
                       result.failure_phase);
  AddNamedResultField(result, "review_decision", "phase0", "task_context",
                      BuildTaskContextRef(review_decision.task_context));
  AddNamedResultField(result, "review_decision", "phase0", "target_object_ref",
                      review_decision.target_object_ref);
  AddNamedResultField(result, "review_decision", "phase0", "review_status",
                      review_decision.review_status);
  AddNamedResultField(result, "review_decision", "phase0", "review_reason",
                      review_decision.review_reason);
  AddNamedResultField(result, "review_decision", "phase0", "reviewer_action",
                      review_decision.reviewer_action);

  AddNamedResultObject(result,
                       "flowback_action",
                       "phase0",
                       "FlowbackAction",
                       flowback_action.flowback_type.empty() ? "partial" : "ready",
                       result.failure_phase);
  AddNamedResultField(result, "flowback_action", "phase0", "task_context",
                      BuildTaskContextRef(flowback_action.task_context));
  AddNamedResultField(result, "flowback_action", "phase0", "target_object_ref",
                      flowback_action.target_object_ref);
  AddNamedResultField(result, "flowback_action", "phase0", "flowback_type",
                      flowback_action.flowback_type);
  AddNamedResultField(result, "flowback_action", "phase0", "trigger_reason",
                      flowback_action.trigger_reason);
  AddNamedResultField(result, "flowback_action", "phase0", "next_target",
                      flowback_action.next_target);

  const bool phase0_ready =
    !result.task_contexts.empty() &&
    !result.geometry_semantic_types.empty() &&
    !result.geometry_template_specs.empty() &&
    !result.image_acquisition_specs.empty() &&
    !result.training_inputs.empty() &&
    !result.run_inputs.empty() &&
    !result.review_decisions.empty() &&
    !result.flowback_actions.empty();

  AddNamedResultObject(result,
                       "phase0_contract",
                       "phase0",
                       "Phase0UnifiedObjectGate",
                       phase0_ready ? "ready" : "partial",
                       result.failure_phase);
  AddNamedResultField(result, "phase0_contract", "phase0", "task_context",
                      result.task_contexts.empty() ? "missing" : "ready");
  AddNamedResultField(result, "phase0_contract", "phase0", "geometry_semantic_type",
                      result.geometry_semantic_types.empty() ? "missing" : "ready");
  AddNamedResultField(result, "phase0_contract", "phase0", "geometry_template_spec",
                      result.geometry_template_specs.empty() ? "missing" : "ready");
  AddNamedResultField(result, "phase0_contract", "phase0", "image_acquisition_spec",
                      result.image_acquisition_specs.empty() ? "missing" : "ready");
  AddNamedResultField(result, "phase0_contract", "phase0", "training_input",
                      result.training_inputs.empty() ? "missing" : "ready");
  AddNamedResultField(result, "phase0_contract", "phase0", "run_input",
                      result.run_inputs.empty() ? "missing" : "ready");
  AddNamedResultField(result, "phase0_contract", "phase0", "review_decision",
                      result.review_decisions.empty() ? "missing" : "ready");
  AddNamedResultField(result, "phase0_contract", "phase0", "flowback_action",
                      result.flowback_actions.empty() ? "missing" : "ready");
  AddNamedResultField(result, "phase0_contract", "phase0", "top_level_business_language",
                      "normalized_without_opencv_native");
  AddNamedResultField(result, "phase0_contract", "phase0", "scope_execution_split",
                      image_acquisition_spec.scope_type + "|" + image_acquisition_spec.execution_mode);
  AddNamedResultField(result, "phase0_contract", "phase0", "object_gate_status",
                      phase0_ready ? "ready" : "partial");
}

bool IsCximageGuiContractResult(const CxScriptExecutionResult &result)
{
  return result.module == "cximage" ||
         NormalizeReviewSourceThread(result) == "cximage";
}

std::vector<std::string> BuildCximageGuiRefPool(const CxScriptExecutionResult &result,
                                                const UnifiedImageReviewRecord &image_review,
                                                const CxcoreClassicalReviewAdapterResult &classical_review)
{
  std::vector<std::string> refs;
  PushUniqueText(refs, image_review.input_image_ref);
  PushUniqueText(refs, image_review.primary_visual_ref);
  PushUniqueText(refs, classical_review.primary_visual_ref);
  PushUniqueText(refs, result.candidate_overlay_ref);
  PushUniqueText(refs, result.template_rect_overlay_ref);
  PushUniqueText(refs, result.test_rect_overlay_ref);
  PushUniqueText(refs, result.circle_overlay_ref);
  PushUniqueText(refs, result.circle_edge_overlay_ref);
  PushUniqueText(refs, result.formfit_candidate_overlay_ref);
  PushUniqueText(refs, result.formfit_selection_overlay_ref);
  PushUniqueText(refs, result.region_pattern_overlay_ref);
  PushUniqueText(refs, result.region_pattern_descriptor_ref);
  PushUniqueText(refs, result.fractal_partition_overlay_ref);
  PushUniqueText(refs, result.distance_field_overlay_ref);
  PushUniqueText(refs, result.skeleton_overlay_ref);
  PushUniqueText(refs, result.centerline_overlay_ref);
  PushUniqueText(refs, result.topology_repair_overlay_ref);
  PushUniqueText(refs, result.template_alignment_ref);
  PushUniqueText(refs, result.attach_back_ref);
  PushUniqueText(refs, result.roi_diff_candidate_ref);
  PushUniqueText(refs, result.bbox_candidate_list_ref);
  PushUniqueText(refs, result.roi_crop_packet_ref);
  PushUniqueText(refs, result.published_result_ref);
  PushUniqueText(refs, result.published_primary_ref);
  PushUniqueText(refs, result.published_template_alignment_ref);
  PushUniqueText(refs, result.published_roi_diff_candidate_ref);
  PushUniqueText(refs, result.published_roi_crop_packet_ref);
  PushUniqueText(refs, result.published_bbox_candidate_list_ref);

  for (size_t i = 0; i < classical_review.visualization_refs.size(); ++i)
    PushUniqueText(refs, classical_review.visualization_refs[i]);
  for (size_t i = 0; i < image_review.visualization_refs.size(); ++i)
    PushUniqueText(refs, image_review.visualization_refs[i]);
  for (size_t i = 0; i < image_review.output_image_refs.size(); ++i)
    PushUniqueText(refs, image_review.output_image_refs[i]);
  for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = image_review.detection_elements[i];
    PushUniqueText(refs, element.source_ref);
    PushUniqueText(refs, element.primary_overlay_ref);
    PushUniqueText(refs, element.focus_region_ref);
    PushUniqueText(refs, element.local_delta_ref);
    for (size_t overlay_index = 0; overlay_index < element.overlay_refs.size(); ++overlay_index)
      PushUniqueText(refs, element.overlay_refs[overlay_index]);
  }
  for (size_t i = 0; i < image_review.element_chains.size(); ++i)
  {
    PushUniqueText(refs, image_review.element_chains[i].source_ref);
    PushUniqueText(refs, image_review.element_chains[i].target_ref);
  }
  return refs;
}

std::string FindFirstCximageRefWithTokens(const std::vector<std::string> &refs,
                                          const std::vector<std::string> &tokens)
{
  for (size_t i = 0; i < refs.size(); ++i)
  {
    const std::string lowered_ref = ToLowerText(refs[i]);
    if (lowered_ref.empty())
      continue;
    for (size_t token_index = 0; token_index < tokens.size(); ++token_index)
    {
      const std::string lowered_token = ToLowerText(tokens[token_index]);
      if (!lowered_token.empty() && lowered_ref.find(lowered_token) != std::string::npos)
        return refs[i];
    }
  }
  return std::string();
}

std::string ResolveCximageGuiRawImageRef(const UnifiedImageReviewRecord &image_review,
                                         const CxcoreClassicalReviewAdapterResult &classical_review)
{
  if (!image_review.input_image_ref.empty())
    return image_review.input_image_ref;
  if (!image_review.primary_visual_ref.empty())
    return image_review.primary_visual_ref;
  return classical_review.primary_visual_ref;
}

std::string ResolveCximageGuiEdgeImageRef(const CxScriptExecutionResult &result,
                                          const UnifiedImageReviewRecord &image_review,
                                          const CxcoreClassicalReviewAdapterResult &classical_review)
{
  if (!result.circle_edge_overlay_ref.empty())
    return result.circle_edge_overlay_ref;
  const std::vector<std::string> refs = BuildCximageGuiRefPool(result, image_review, classical_review);
  const std::string edge_ref =
    FindFirstCximageRefWithTokens(refs, std::vector<std::string>{"edge", "edgeband", "canny", "sobel"});
  if (!edge_ref.empty())
    return edge_ref;
  const std::string raw_ref = ResolveCximageGuiRawImageRef(image_review, classical_review);
  return raw_ref.empty() ? image_review.primary_visual_ref : raw_ref;
}

std::string ResolveCximageGuiElementRelationImageRef(const CxScriptExecutionResult &result,
                                                     const UnifiedImageReviewRecord &image_review,
                                                     const CxcoreClassicalReviewAdapterResult &classical_review)
{
  const std::vector<std::string> refs = BuildCximageGuiRefPool(result, image_review, classical_review);
  const std::string relation_ref =
    FindFirstCximageRefWithTokens(refs,
                                  std::vector<std::string>{"point_set",
                                                           "measure_bounds",
                                                           "region_overlay",
                                                           "region_summary",
                                                           "descriptor_compare",
                                                           "circle_overlay",
                                                           "line_point_set",
                                                           "circle_point_set",
                                                           "skeleton",
                                                           "centerline",
                                                           "fractal_partition",
                                                           "topology",
                                                           "region_bounds"});
  if (!relation_ref.empty())
    return relation_ref;
  return ResolveCximageGuiRawImageRef(image_review, classical_review);
}

std::string ResolveCximageGuiCandidateImageRef(const CxScriptExecutionResult &result,
                                               const UnifiedImageReviewRecord &image_review,
                                               const CxcoreClassicalReviewAdapterResult &classical_review)
{
  if (!result.formfit_candidate_overlay_ref.empty())
    return result.formfit_candidate_overlay_ref;
  if (!result.candidate_overlay_ref.empty())
    return result.candidate_overlay_ref;
  if (!result.roi_diff_candidate_ref.empty())
    return result.roi_diff_candidate_ref;
  if (!result.bbox_candidate_list_ref.empty())
    return result.bbox_candidate_list_ref;
  const std::vector<std::string> refs = BuildCximageGuiRefPool(result, image_review, classical_review);
  const std::string candidate_ref =
    FindFirstCximageRefWithTokens(refs,
                                  std::vector<std::string>{"candidate",
                                                           "roi_diff",
                                                           "bbox",
                                                           "selection_overlay",
                                                           "region_overlay"});
  if (!candidate_ref.empty())
    return candidate_ref;
  return ResolveCximageGuiElementRelationImageRef(result, image_review, classical_review);
}

std::string ResolveCximageGuiMatchImageRef(const CxScriptExecutionResult &result,
                                           const UnifiedImageReviewRecord &image_review,
                                           const CxcoreClassicalReviewAdapterResult &classical_review)
{
  if (!result.formfit_selection_overlay_ref.empty())
    return result.formfit_selection_overlay_ref;
  if (!result.test_rect_overlay_ref.empty())
    return result.test_rect_overlay_ref;
  if (!result.template_alignment_ref.empty())
    return result.template_alignment_ref;
  if (!result.attach_back_ref.empty())
    return result.attach_back_ref;
  if (!result.published_result_ref.empty())
    return result.published_result_ref;
  const std::vector<std::string> refs = BuildCximageGuiRefPool(result, image_review, classical_review);
  const std::string match_ref =
    FindFirstCximageRefWithTokens(refs,
                                  std::vector<std::string>{"match",
                                                           "selection",
                                                           "alignment",
                                                           "test_rect",
                                                           "topology_repair",
                                                           "result"});
  if (!match_ref.empty())
    return match_ref;
  return ResolveCximageGuiCandidateImageRef(result, image_review, classical_review);
}

bool HasCximageIssueToken(const std::vector<std::string> &texts,
                          const std::vector<std::string> &tokens)
{
  for (size_t i = 0; i < texts.size(); ++i)
  {
    const std::string lowered_text = ToLowerText(texts[i]);
    if (lowered_text.empty())
      continue;
    for (size_t token_index = 0; token_index < tokens.size(); ++token_index)
    {
      const std::string lowered_token = ToLowerText(tokens[token_index]);
      if (!lowered_token.empty() && lowered_text.find(lowered_token) != std::string::npos)
        return true;
    }
  }
  return false;
}

std::string DetermineCximageGeometryStage(const CxScriptExecutionResult &result,
                                          const UnifiedImageReviewRecord &image_review)
{
  int geometry_count = 0;
  for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
  {
    const std::string &element_type = image_review.detection_elements[i].element_type;
    if (element_type == "point" ||
        element_type == "line_segment" ||
        element_type == "open_polyline" ||
        element_type == "closed_region" ||
        element_type == "circle" ||
        element_type == "arc" ||
        element_type == "click_point")
    {
      ++geometry_count;
    }
  }
  if (geometry_count == 0 &&
      result.region_bounds_count_value <= 0.0 &&
      result.circle_sample_points_value <= 0.0 &&
      result.point_count_value <= 0.0)
    return "geometry_not_ready";
  if (image_review.missing_element_count > 0 ||
      image_review.abnormal_element_count > 0 ||
      image_review.drifted_element_count > 0)
    return "geometry_review";
  return "geometry_ready";
}

std::string DetermineCximageCandidateStage(const CxScriptExecutionResult &result,
                                           const UnifiedImageReviewRecord &image_review)
{
  const bool has_candidate_evidence =
    image_review.candidate_element_count > 0 ||
    result.match_candidate_count_value > 0.0 ||
    !result.candidate_overlay_ref.empty() ||
    !result.formfit_candidate_overlay_ref.empty() ||
    !result.roi_diff_candidate_ref.empty() ||
    !result.bbox_candidate_list_ref.empty();
  if (!has_candidate_evidence)
    return "candidate_not_applicable";
  if (HasCximageIssueToken(image_review.anomaly_flags,
                           std::vector<std::string>{"candidate", "selection", "bbox"}) ||
      ToLowerText(image_review.candidate_status_summary).find("missing") != std::string::npos)
    return "candidate_review";
  return "candidate_ready";
}

std::string DetermineCximageMatchStage(const CxScriptExecutionResult &result,
                                       const UnifiedImageReviewRecord &image_review)
{
  const bool has_match_evidence =
    result.match_top_score_value > 0.0 ||
    !result.test_rect_overlay_ref.empty() ||
    !result.formfit_selection_overlay_ref.empty() ||
    !result.template_alignment_ref.empty() ||
    !result.attach_back_ref.empty() ||
    !image_review.match_status_summary.empty() ||
    !image_review.template_alignment_status.empty();
  if (!has_match_evidence)
    return "match_not_applicable";
  if (HasCximageIssueToken(image_review.anomaly_flags,
                           std::vector<std::string>{"match", "template", "alignment", "fallback", "selection"}) ||
      ToLowerText(image_review.match_status_summary).find("missing") != std::string::npos)
    return "match_review";
  return "match_ready";
}

std::string DetermineCximageProblemIssueType(const UnifiedImageReviewRecord &image_review)
{
  if (image_review.input_image_ref.empty() && image_review.primary_visual_ref.empty())
    return "image_acquisition_issue";
  if (HasCximageIssueToken(image_review.anomaly_flags,
                           std::vector<std::string>{"template", "alignment", "fallback"}))
    return "template_issue";
  if (HasCximageIssueToken(image_review.anomaly_flags,
                           std::vector<std::string>{"candidate", "match", "selection", "bbox"}))
    return "matching_issue";
  if (HasCximageIssueToken(image_review.anomaly_flags,
                           std::vector<std::string>{"topology", "skeleton", "centerline", "fractal"}))
    return "geometry_structure_issue";
  if (HasCximageIssueToken(image_review.anomaly_flags,
                           std::vector<std::string>{"point_set", "circle_fit", "descriptor", "threshold", "geometry", "fit_residual"}))
    return "element_extraction_issue";
  if (!image_review.anomaly_flags.empty() || image_review.status != "normal")
    return "review_focus_needed";
  return "observation_ready";
}

std::string DetermineCximageProblemFocusElementId(const UnifiedImageReviewRecord &image_review)
{
  const char *priority_statuses[] = {"abnormal", "drifted", "missing"};
  for (size_t status_index = 0; status_index < sizeof(priority_statuses) / sizeof(priority_statuses[0]); ++status_index)
  {
    for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
    {
      const UnifiedDetectionElement &element = image_review.detection_elements[i];
      if (element.consistency_status == priority_statuses[status_index] && !element.element_id.empty())
        return element.element_id;
    }
  }
  for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = image_review.detection_elements[i];
    if ((element.semantic_role == "review_target" || element.semantic_role == "candidate") &&
        !element.element_id.empty())
      return element.element_id;
  }
  return image_review.detection_elements.empty() ? std::string()
                                                 : image_review.detection_elements[0].element_id;
}

std::string DetermineCximageProblemFocusChainId(const UnifiedImageReviewRecord &image_review)
{
  for (size_t i = 0; i < image_review.element_chains.size(); ++i)
  {
    const UnifiedElementChain &chain = image_review.element_chains[i];
    if (chain.chain_status != "matched" && !chain.chain_id.empty())
      return chain.chain_id;
  }
  return image_review.element_chains.empty() ? std::string()
                                             : image_review.element_chains[0].chain_id;
}

std::string DetermineCximageProblemFocusImageRef(const std::string &issue_type,
                                                 const std::string &raw_image_ref,
                                                 const std::string &edge_image_ref,
                                                 const std::string &element_relation_image_ref,
                                                 const std::string &candidate_image_ref,
                                                 const std::string &match_image_ref)
{
  if (issue_type == "image_acquisition_issue")
    return raw_image_ref;
  if (issue_type == "template_issue" || issue_type == "matching_issue")
    return !match_image_ref.empty() ? match_image_ref : candidate_image_ref;
  if (issue_type == "element_extraction_issue" || issue_type == "geometry_structure_issue")
    return !element_relation_image_ref.empty() ? element_relation_image_ref : edge_image_ref;
  if (!candidate_image_ref.empty())
    return candidate_image_ref;
  if (!element_relation_image_ref.empty())
    return element_relation_image_ref;
  return raw_image_ref;
}

std::string BuildCximageGuiChainSummary()
{
  return "edge -> candidate -> match || point/line/region -> candidate -> review_signal";
}

std::string BuildCximageSingleImageGeometryConclusion(const UnifiedImageReviewRecord &image_review,
                                                      const std::string &geometry_stage)
{
  std::vector<std::string> items;
  items.push_back("geometry_stage=" + geometry_stage);
  if (!image_review.primary_detection_semantic.empty())
    items.push_back("primary_detection=" + image_review.primary_detection_semantic);
  items.push_back("missing=" + std::to_string(image_review.missing_element_count));
  items.push_back("drifted=" + std::to_string(image_review.drifted_element_count));
  items.push_back("abnormal=" + std::to_string(image_review.abnormal_element_count));
  return JoinTextItems(items, ";");
}

std::string BuildCximageElementConclusion(const UnifiedImageReviewRecord &image_review)
{
  if (!image_review.element_status_summary.empty())
    return image_review.element_status_summary;
  return BuildDetectionElementSummary(image_review.detection_elements);
}

std::string BuildCximageMatchConclusion(const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> items;
  if (!image_review.candidate_status_summary.empty())
    items.push_back("candidate=" + image_review.candidate_status_summary);
  if (!image_review.match_status_summary.empty())
    items.push_back("match=" + image_review.match_status_summary);
  if (!image_review.template_alignment_status.empty())
    items.push_back("template_alignment=" + image_review.template_alignment_status);
  return JoinTextItems(items, ";");
}
void RefreshUnifiedReviewFoundation(CxScriptExecutionResult &result)
{
  RefreshPhase0UnifiedObjects(result);
  result.unified_image_reviews.clear();
  result.unified_task_reviews.clear();
  result.unified_compare_slices.clear();
  result.unified_anomaly_focus_bundles.clear();

  UnifiedImageReviewRecord image_review;
  image_review.source_thread = NormalizeReviewSourceThread(result);
  image_review.task_id = result.task_id;
  image_review.batch_id = BuildReviewBatchId(result);
  image_review.case_name = result.case_name;
  image_review.image_id = SelectReviewImageId(result);
  image_review.stage = result.layer.empty() ? "report" : result.layer;
  image_review.input_image_ref = SelectReviewInputImageRef(result);
  image_review.primary_visual_ref = SelectReviewPrimaryVisualRef(result);
  const CxcoreClassicalReviewAdapterResult classical_review = BuildCxcoreClassicalReviewAdapter(result);
  if (classical_review.matched_case && !classical_review.primary_visual_ref.empty())
    image_review.primary_visual_ref = classical_review.primary_visual_ref;
  image_review.notes = result.summary;
  for (size_t i = 0; i < classical_review.notes.size(); ++i)
  {
    if (classical_review.notes[i].empty())
      continue;
    if (!image_review.notes.empty())
      image_review.notes += " | ";
    image_review.notes += classical_review.notes[i];
  }

  CollectReviewArtifactRefs(result, image_review.artifact_refs);
  if (classical_review.matched_case)
  {
    PushUniqueText(image_review.artifact_refs, classical_review.primary_visual_ref);
    for (size_t i = 0; i < classical_review.visualization_refs.size(); ++i)
      PushUniqueText(image_review.artifact_refs, classical_review.visualization_refs[i]);
    for (size_t i = 0; i < classical_review.phenomenon_evidence.size(); ++i)
      PushUniqueText(image_review.phenomenon_evidence, classical_review.phenomenon_evidence[i]);
  }
  CollectReviewAnomalyFlags(result, image_review.primary_visual_ref, image_review.anomaly_flags);
  for (size_t i = 0; i < classical_review.anomaly_flags.size(); ++i)
    PushUniqueText(image_review.anomaly_flags, classical_review.anomaly_flags[i]);
  CollectUnifiedDetectionElements(result,
                                  image_review.source_thread,
                                  image_review.primary_visual_ref,
                                  image_review.detection_elements);
  image_review.element_chains =
    BuildUnifiedElementChains(result, image_review.detection_elements);
  image_review.primary_detection_semantic = DeterminePrimaryDetectionSemantic(result);
  image_review.template_alignment_status =
    !result.published_template_test_alignment_status.empty()
      ? result.published_template_test_alignment_status
      : (!result.template_test_alignment_status.empty()
           ? result.template_test_alignment_status
           : std::string("not_applicable"));
  image_review.missing_element_count =
    CountDetectionElementsByStatus(image_review.detection_elements, "missing");
  image_review.abnormal_element_count =
    CountDetectionElementsByStatus(image_review.detection_elements, "abnormal");
  image_review.drifted_element_count =
    CountDetectionElementsByStatus(image_review.detection_elements, "drifted");
  image_review.candidate_element_count =
    CountDetectionElementsBySemanticRole(image_review.detection_elements, "candidate");
  image_review.observation_personality =
    BuildObservationPersonality(result, image_review.source_thread);
  image_review.default_open_chain =
    BuildDefaultOpenChain(result, image_review.source_thread);
  image_review.evidence_focus_summary =
    BuildEvidenceFocusSummary(result, image_review);
  if (image_review.missing_element_count > 0)
    PushUniqueText(image_review.anomaly_flags, "element_missing");
  if (image_review.abnormal_element_count > 0)
    PushUniqueText(image_review.anomaly_flags, "element_abnormal");
  if (image_review.drifted_element_count > 0)
    PushUniqueText(image_review.anomaly_flags, "element_drift");
  for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
  {
    PushUniqueText(image_review.artifact_refs, image_review.detection_elements[i].element_id);
    PushUniqueText(image_review.artifact_refs,
                   image_review.detection_elements[i].focus_region_ref);
    PushUniqueText(image_review.artifact_refs,
                   image_review.detection_elements[i].local_delta_ref);
    if (!image_review.detection_elements[i].linked_template_element_id.empty())
    {
      PushUniqueText(image_review.artifact_refs,
                     image_review.detection_elements[i].linked_template_element_id);
    }
  }
  for (size_t i = 0; i < image_review.element_chains.size(); ++i)
  {
    const UnifiedElementChain &chain = image_review.element_chains[i];
    PushUniqueText(image_review.artifact_refs, chain.chain_id);
    PushUniqueText(image_review.artifact_refs, chain.source_ref);
    PushUniqueText(image_review.artifact_refs, chain.target_ref);
    if (!ShouldPromoteChainStatusToAnomaly(result, chain))
      continue;
    if (chain.chain_status == "missing")
      PushUniqueText(image_review.anomaly_flags, "chain_missing_" + chain.chain_type);
    else if (chain.chain_status == "drifted")
      PushUniqueText(image_review.anomaly_flags, "chain_drifted_" + chain.chain_type);
    else if (chain.chain_status == "abnormal")
      PushUniqueText(image_review.anomaly_flags, "chain_abnormal_" + chain.chain_type);
  }

  std::vector<std::string> unified_image_review_missing_fields;
  const bool unified_image_review_required =
    RequiresUnifiedImageReviewRecord(image_review.source_thread);
  const std::string tolerance_summary = BuildUnifiedToleranceSummary(result);
  const std::string stability_summary = BuildUnifiedStabilitySummary(result);
  const std::string threshold_ref = ResolveUnifiedThresholdRef(result);
  const std::string threshold_summary = BuildUnifiedThresholdSummary(result);
  const std::string risk_note = BuildUnifiedRiskNote(result);

  if (result.runtime_ms > 0.0)
  {
    PushReviewMetric(image_review.metrics,
                     "verified_runtime_ms",
                     std::to_string(result.runtime_ms),
                     "ms",
                     ">0",
                     "0",
                     "verified_runtime",
                     result.success ? "ok" : "failed");
  }
  else
  {
    PushReviewMetric(image_review.metrics,
                     "placeholder_runtime_ms",
                     "0",
                     "ms",
                     "placeholder_only",
                     std::string(),
                     "placeholder_runtime",
                     "not_measured");
  }
  if (!result.requested_device.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "requested_device",
                     result.requested_device,
                     std::string(),
                     "cpu/cuda/auto",
                     std::string(),
                     "none",
                     "reported");
  }
  if (!result.train_param_summary.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "train_param_summary",
                     result.train_param_summary,
                     std::string(),
                     "present",
                     std::string(),
                     "none",
                     "reported");
  }
  if (!result.infer_param_summary.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "infer_param_summary",
                     result.infer_param_summary,
                     std::string(),
                     "present",
                     std::string(),
                     "none",
                     "reported");
  }
  if (!result.attach_back_overlay_status.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "attach_back_overlay_status",
                     result.attach_back_overlay_status,
                     std::string(),
                     "present",
                     std::string(),
                     "none",
                     "reported");
  }
  if (!result.attach_back_top1_class.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "attach_back_top1_class",
                     result.attach_back_top1_class,
                     std::string(),
                     "present",
                     std::string(),
                     "none",
                     "reported");
  }
  if (!result.attach_back_confidence.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "attach_back_confidence",
                     result.attach_back_confidence,
                     std::string(),
                     "present",
                     std::string(),
                     "none",
                     "reported");
  }
  const std::string bridge_defect_count = FindNamedResultFieldValue(result, "bridge", "defect_count");
  if (!bridge_defect_count.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "defect_count",
                     bridge_defect_count,
                     "count",
                     ">=0",
                     std::string(),
                     bridge_defect_count == "0" ? "stable" : "elevated",
                     "reported");
  }
  if (!result.roi_diff_candidate_count.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "roi_diff_candidate_count",
                     result.roi_diff_candidate_count,
                     "count",
                     ">=0",
                     std::string(),
                     result.roi_diff_candidate_count == "0" ? "stable" : "elevated",
                     "reported");
  }
  if (result.published_roi_diff_candidate_count != result.roi_diff_candidate_count &&
      !result.published_roi_diff_candidate_count.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "published_roi_diff_candidate_count",
                     result.published_roi_diff_candidate_count,
                     "count",
                     ">=0",
                     std::string(),
                     result.published_roi_diff_candidate_count == "0" ? "stable" : "elevated",
                     "reported");
  }
  if (!result.published_roi_crop_count.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "published_roi_crop_count",
                     result.published_roi_crop_count,
                     "count",
                     ">=0",
                     std::string(),
                     result.published_roi_crop_count == "0" ? "stable" : "elevated",
                     "reported");
  }
  if (!result.published_roi_crop_spatial_size.empty())
  {
    PushReviewMetric(image_review.metrics,
                     "published_roi_crop_spatial_size",
                     result.published_roi_crop_spatial_size,
                     std::string(),
                     "present",
                     std::string(),
                     "none",
                     "reported");
  }
  if (result.fit_time_ms > 0.0)
  {
    PushReviewMetric(image_review.metrics,
                     "fit_time_ms",
                     std::to_string(result.fit_time_ms),
                     "ms",
                     ">=0",
                     std::string(),
                     "stable",
                     "reported");
  }
  if (result.infer_time_ms > 0.0)
  {
    PushReviewMetric(image_review.metrics,
                     "infer_time_ms",
                     std::to_string(result.infer_time_ms),
                     "ms",
                     ">=0",
                     std::string(),
                     "stable",
                     "reported");
  }
  if (result.feature_dim > 0.0)
  {
    PushReviewMetric(image_review.metrics,
                     "feature_dim",
                     std::to_string(result.feature_dim),
                     std::string(),
                     ">=1",
                     std::string(),
                     "reported",
                     "reported");
  }
  if (result.accuracy > 0.0)
  {
    PushReviewMetric(image_review.metrics,
                     "accuracy",
                     std::to_string(result.accuracy),
                     std::string(),
                     "0..1",
                     std::string(),
                     result.accuracy < 0.5 ? "elevated" : "stable",
                     "reported");
  }
  if (result.macro_f1 > 0.0)
  {
    PushReviewMetric(image_review.metrics,
                     "macro_f1",
                     std::to_string(result.macro_f1),
                     std::string(),
                     "0..1",
                     std::string(),
                     result.macro_f1 < 0.5 ? "elevated" : "stable",
                     "reported");
  }
  if (result.prediction_count > 0.0)
  {
    PushReviewMetric(image_review.metrics,
                     "prediction_count",
                     std::to_string(result.prediction_count),
                     "count",
                     ">=0",
                     std::string(),
                     "reported",
                     "reported");
  }
  for (size_t i = 0; i < classical_review.metrics.size(); ++i)
  {
    PushReviewMetric(image_review.metrics,
                     classical_review.metrics[i].metric_name,
                     classical_review.metrics[i].metric_value,
                     classical_review.metrics[i].metric_unit,
                     classical_review.metrics[i].expected_range,
                     classical_review.metrics[i].baseline_value,
                     classical_review.metrics[i].deviation_level,
                     classical_review.metrics[i].metric_status);
  }

  image_review.metric_summary_text = SerializeReviewMetrics(image_review.metrics);
  image_review.status = DetermineReviewStatus(result, image_review.anomaly_flags);
  image_review.thread_handoff = BuildThreadHandoff(result, image_review);
  const SequenceLinkTrace image_sequence_trace = BuildTorchSequenceLinkTrace(result);
  image_review.task_entry_name = BuildTorchTaskEntryName(result);
  image_review.task_family = BuildTorchTaskFamily(result);
  image_review.pipeline_family = BuildTorchPipelineFamily(result);
  image_review.model_family = BuildTorchModelFamily(result);
  image_review.scenario_family = BuildTorchScenarioFamily(result);
  image_review.visual_evidence_set = BuildTorchVisualEvidenceSet(result, image_review);
  image_review.device_evidence = BuildTorchDeviceEvidence(result);
  image_review.business_eval_fields = BuildTorchBusinessEvalFields(result);
  image_review.pipeline_link_trace = BuildTorchPipelineLinkTrace(result);
  image_review.sequence_family = image_sequence_trace.sequence_family;
  image_review.sequence_stage = BuildTorchSequenceStage(result);
  image_review.sequence_index = BuildTorchSequenceIndex(result);
  image_review.sequence_trace_ref = SerializeSequenceLinkTrace(image_sequence_trace);
  const std::vector<std::string> image_sequence_records =
    BuildTorchSequenceRecords(result, image_review);
  image_review.sequence_records = JoinTextItems(image_sequence_records, ";;");
  image_review.sequence_summary = BuildTorchSequenceSummary(image_sequence_records);
  image_review.sequence_status_summary =
    BuildTorchSequenceStatusSummary(image_sequence_records);
  image_review.stage_refs = BuildTorchSequenceFieldSet(image_sequence_records, "stage_ref");
  image_review.script_refs = BuildTorchSequenceFieldSet(image_sequence_records, "script_ref");
  image_review.image_refs = BuildTorchSequenceFieldSet(image_sequence_records, "image_ref");
  image_review.conclusion_refs =
    BuildTorchSequenceFieldSet(image_sequence_records, "conclusion_ref");
  image_review.issue_refs = BuildTorchSequenceFieldSet(image_sequence_records, "issue_ref");
  image_review.lifecycle_summary = BuildTorchLifecycleSummary(image_sequence_records);
  image_review.lifecycle_zone_refs =
    BuildTorchSequenceFieldSet(image_sequence_records, "lifecycle_zone");
  image_review.init_stage_refs = BuildTorchLifecycleStageRefs(image_sequence_records, "init");
  image_review.repeatable_stage_refs =
    BuildTorchRepeatableStageRefs(image_sequence_records);
  image_review.debug_stage_refs = BuildTorchLifecycleStageRefs(image_sequence_records, "debug");
  image_review.replay_stage_refs = BuildTorchLifecycleStageRefs(image_sequence_records, "replay");
  image_review.reset_stage_refs = BuildTorchLifecycleStageRefs(image_sequence_records, "reset");
  image_review.lifecycle_risk_summary =
    BuildTorchLifecycleRiskSummary(image_sequence_records);
  image_review.element_group_summary =
    BuildElementGroupSummary(image_review.detection_elements);
  image_review.element_status_summary =
    BuildElementStatusBoardSummary(image_review.detection_elements);
  image_review.candidate_status_summary =
    BuildCandidateStatusSummary(image_review.detection_elements);
  image_review.match_status_summary =
    BuildMatchStatusSummary(image_review.detection_elements);
  image_review.manual_review_signal_summary =
    BuildManualReviewSignalSummary(image_review.detection_elements);
  image_review.element_findings =
    BuildElementFindingsSummary(image_review.detection_elements);
  image_review.element_level_focus =
    BuildElementLevelFocusSummary(image_review.detection_elements);
  image_review.focus_refresh_targets =
    BuildFocusRefreshTargets(image_review.detection_elements);
  image_review.local_delta_targets =
    BuildLocalDeltaTargets(image_review.detection_elements);
  image_review.grouped_element_preview =
    BuildGroupedElementPreview(image_review.detection_elements);
  image_review.focus_element_preview =
    BuildFocusElementPreview(image_review.detection_elements);
  image_review.delta_element_preview =
    BuildDeltaElementPreview(image_review.detection_elements);
  image_review.refresh_mode =
    (!image_review.local_delta_targets.empty() || !image_review.focus_refresh_targets.empty())
      ? "focus_delta"
      : "snapshot";
  image_review.changed_fields = BuildUnifiedImageChangedFields(image_review);
  image_review.changed_element_ids =
    BuildAllChangedElementIds(image_review.detection_elements);
  image_review.changed_chain_keys =
    BuildAllChangedChainKeys(image_review.element_chains);
  image_review.refresh_priority =
    DetermineImageRefreshPriority(result, image_review);
  if (unified_image_review_required)
  {
    CollectUnifiedImageReviewMissingFields(image_review, unified_image_review_missing_fields);
    if (!unified_image_review_missing_fields.empty())
    {
      PushUniqueText(image_review.anomaly_flags, "unified_image_review_incomplete");
      for (size_t i = 0; i < unified_image_review_missing_fields.size(); ++i)
        PushUniqueText(image_review.anomaly_flags,
                       "missing_" + unified_image_review_missing_fields[i]);
    }
    if (!unified_image_review_missing_fields.empty())
      image_review.status = DetermineReviewStatus(result, image_review.anomaly_flags);
  }
  const bool unified_image_review_ready = unified_image_review_missing_fields.empty();

  PushUniqueText(image_review.output_image_refs, image_review.primary_visual_ref);
  PushUniqueText(image_review.output_image_refs, result.published_primary_ref);
  PushUniqueText(image_review.output_image_refs, result.published_result_ref);
  PushUniqueText(image_review.output_image_refs, result.attach_back_ref);
  PushUniqueText(image_review.output_image_refs, result.cluster_ref);
  PushUniqueText(image_review.output_image_refs, result.distance_ref);
  PushUniqueText(image_review.output_image_refs, result.anomaly_ref);
  PushUniqueText(image_review.output_image_refs, result.predictions_csv);
  PushUniqueText(image_review.output_image_refs, result.output_summary_csv);
  PushUniqueText(image_review.visualization_refs, image_review.primary_visual_ref);
  PushUniqueText(image_review.visualization_refs, result.published_primary_ref);
  PushUniqueText(image_review.visualization_refs, result.published_result_ref);
  PushUniqueText(image_review.visualization_refs, result.template_alignment_ref);
  PushUniqueText(image_review.visualization_refs, result.published_template_alignment_ref);
  PushUniqueText(image_review.visualization_refs, result.roi_diff_candidate_ref);
  PushUniqueText(image_review.visualization_refs, result.published_roi_diff_candidate_ref);
  PushUniqueText(image_review.visualization_refs, result.attach_back_ref);
  PushUniqueText(image_review.visualization_refs, result.bbox_candidate_list_ref);
  PushUniqueText(image_review.visualization_refs, result.published_bbox_candidate_list_ref);
  PushUniqueText(image_review.visualization_refs, result.roi_crop_packet_ref);
  PushUniqueText(image_review.visualization_refs, result.published_roi_crop_packet_ref);
  PushUniqueText(image_review.visualization_refs, result.published_prior_roi_region_ref);
  PushUniqueText(image_review.visualization_refs, result.cluster_ref);
  PushUniqueText(image_review.visualization_refs, result.distance_ref);
  PushUniqueText(image_review.visualization_refs, result.anomaly_ref);
  PushUniqueText(image_review.visualization_refs, result.candidate_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.template_rect_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.test_rect_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.circle_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.circle_edge_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.formfit_candidate_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.formfit_selection_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.region_pattern_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.region_pattern_descriptor_ref);
  PushUniqueText(image_review.visualization_refs, result.fractal_partition_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.distance_field_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.skeleton_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.centerline_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.topology_repair_overlay_ref);
  PushUniqueText(image_review.visualization_refs, result.predictions_csv);
  PushUniqueText(image_review.visualization_refs, result.output_summary_csv);
  for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
  {
    PushUniqueText(image_review.visualization_refs, image_review.detection_elements[i].element_id);
    PushUniqueText(image_review.visualization_refs,
                   image_review.detection_elements[i].focus_region_ref);
    PushUniqueText(image_review.visualization_refs,
                   image_review.detection_elements[i].local_delta_ref);
    if (!image_review.detection_elements[i].linked_template_element_id.empty())
    {
      PushUniqueText(image_review.visualization_refs,
                     image_review.detection_elements[i].linked_template_element_id);
    }
  }
  for (size_t i = 0; i < image_review.element_chains.size(); ++i)
  {
    PushUniqueText(image_review.visualization_refs, image_review.element_chains[i].chain_id);
    PushUniqueText(image_review.visualization_refs, image_review.element_chains[i].source_ref);
    PushUniqueText(image_review.visualization_refs, image_review.element_chains[i].target_ref);
  }
  for (size_t i = 0; i < classical_review.visualization_refs.size(); ++i)
    PushUniqueText(image_review.visualization_refs, classical_review.visualization_refs[i]);
  PushUniqueText(image_review.baseline_refs, result.summary_ref);
  PushUniqueText(image_review.baseline_refs, result.compare_ref);
  PushUniqueText(image_review.baseline_refs, result.replay_ref);
  PushUniqueText(image_review.compare_tags, result.module);
  PushUniqueText(image_review.compare_tags, result.layer);
  if (!result.required_input_contract.empty())
    PushUniqueText(image_review.contract_evidence, "input_contract=" + result.required_input_contract);
  if (!result.required_label_contract.empty())
    PushUniqueText(image_review.contract_evidence, "label_contract=" + result.required_label_contract);
  if (!result.dataset_ref.empty())
    PushUniqueText(image_review.contract_evidence, "dataset_ref=" + result.dataset_ref);
  if (!result.input_dataset.empty())
    PushUniqueText(image_review.contract_evidence, "input_dataset=" + result.input_dataset);
  if (!result.sample_bundle_ref.empty())
    PushUniqueText(image_review.contract_evidence, "sample_bundle_ref=" + result.sample_bundle_ref);
  if (!result.geometry_template_specs.empty())
  {
    PushUniqueText(image_review.contract_evidence, "template_identity=" + result.geometry_template_specs[0].template_identity);
    PushUniqueText(image_review.contract_evidence, "template_provenance=" + result.geometry_template_specs[0].template_provenance);
    PushUniqueText(image_review.contract_evidence, "template_review_priority=" + result.geometry_template_specs[0].review_priority);
  }
  if (!result.image_acquisition_specs.empty())
  {
    PushUniqueText(image_review.contract_evidence, "image_acquisition_spec=" + BuildImageAcquisitionRef(result.image_acquisition_specs[0]));
    PushUniqueText(image_review.contract_evidence, "acquisition_scope=" + result.image_acquisition_specs[0].scope_type);
    PushUniqueText(image_review.contract_evidence, "acquisition_mode=" + result.image_acquisition_specs[0].execution_mode);
    PushUniqueText(image_review.contract_evidence, "acquisition_provenance=" + result.image_acquisition_specs[0].provenance);
  }
  if (!result.consumed_weight_files.empty())
    PushUniqueText(image_review.contract_evidence, "weights=" + result.consumed_weight_files);
  if (!result.consumed_weight_paths.empty())
    PushUniqueText(image_review.contract_evidence, "weight_paths=" + result.consumed_weight_paths);
  if (!result.dataset_profile.empty())
    PushUniqueText(image_review.contract_evidence, "dataset_profile=" + result.dataset_profile);
  if (!result.prepared_root.empty())
    PushUniqueText(image_review.contract_evidence, "prepared_root=" + result.prepared_root);
  if (!result.input_task.empty())
    PushUniqueText(image_review.contract_evidence, "input_task=" + result.input_task);
  if (!result.input_profile.empty())
    PushUniqueText(image_review.contract_evidence, "input_profile=" + result.input_profile);
  if (!result.model_name.empty())
    PushUniqueText(image_review.contract_evidence, "model_name=" + result.model_name);
  if (!result.feature_set.empty())
    PushUniqueText(image_review.contract_evidence, "feature_set=" + result.feature_set);
  if (!result.label_column.empty())
    PushUniqueText(image_review.contract_evidence, "label_column=" + result.label_column);
  if (!result.model_path.empty())
    PushUniqueText(image_review.contract_evidence, "model_path=" + result.model_path);
  if (!result.predictions_csv.empty())
    PushUniqueText(image_review.contract_evidence, "predictions_csv=" + result.predictions_csv);
  if (!result.output_summary_csv.empty())
    PushUniqueText(image_review.contract_evidence, "output_summary_csv=" + result.output_summary_csv);
  if (!result.template_root.empty())
    PushUniqueText(image_review.contract_evidence, "template_root=" + result.template_root);
  if (!result.pairs_ref.empty())
    PushUniqueText(image_review.contract_evidence, "pairs_ref=" + result.pairs_ref);
  if (!image_review.primary_detection_semantic.empty())
    PushUniqueText(image_review.contract_evidence,
                   "primary_detection_semantic=" + image_review.primary_detection_semantic);
  if (!threshold_summary.empty())
    PushUniqueText(image_review.contract_evidence, threshold_summary);
  if (!result.template_test_alignment_status.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "template_alignment_status=" + result.template_test_alignment_status);
  if (!result.published_template_test_alignment_status.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "published_template_alignment_status=" +
                     result.published_template_test_alignment_status);
  if (!result.attach_back_overlay_status.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "attach_back_overlay_status=" + result.attach_back_overlay_status);
  if (!result.attach_back_top1_class.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "attach_back_top1_class=" + result.attach_back_top1_class);
  if (!result.attach_back_confidence.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "attach_back_confidence=" + result.attach_back_confidence);
  if (!result.requested_device.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "requested_device=" + result.requested_device);
  if (result.accuracy > 0.0)
    PushUniqueText(image_review.phenomenon_evidence,
                   "accuracy=" + std::to_string(result.accuracy));
  if (result.macro_f1 > 0.0)
    PushUniqueText(image_review.phenomenon_evidence,
                   "macro_f1=" + std::to_string(result.macro_f1));
  if (result.prediction_count > 0.0)
    PushUniqueText(image_review.phenomenon_evidence,
                   "prediction_count=" + std::to_string(result.prediction_count));
  if (!tolerance_summary.empty())
    PushUniqueText(image_review.phenomenon_evidence, tolerance_summary);
  if (!stability_summary.empty())
    PushUniqueText(image_review.phenomenon_evidence, stability_summary);
  if (!result.published_roi_diff_candidate_count.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "published_roi_diff_candidate_count=" +
                     result.published_roi_diff_candidate_count);
  if (!result.published_roi_crop_count.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "published_roi_crop_count=" + result.published_roi_crop_count);
  if (!result.published_roi_crop_spatial_size.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "published_roi_crop_spatial_size=" +
                     result.published_roi_crop_spatial_size);
  if (!image_review.template_alignment_status.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "unified_template_alignment_status=" +
                     image_review.template_alignment_status);
  if (!image_review.detection_elements.empty())
    PushUniqueText(image_review.phenomenon_evidence,
                   "detection_element_count=" +
                     std::to_string(static_cast<int>(image_review.detection_elements.size())));
  if (!result.error_message.empty())
    PushUniqueText(image_review.phenomenon_evidence, result.error_message);
  const std::string interaction_route =
    FindNamedResultFieldValue(result, "interaction", "route").empty()
      ? FindNamedResultFieldValue(result, "interaction", "interaction_route")
      : FindNamedResultFieldValue(result, "interaction", "route");
  if (!interaction_route.empty())
    PushUniqueText(image_review.interaction_evidence, interaction_route);
  else if (!result.route.empty())
    PushUniqueText(image_review.interaction_evidence, "route=" + result.route);
  const std::string interaction_dataset_bridge =
    FindNamedResultFieldValue(result, "interaction", "dataset_bridge");
  if (!interaction_dataset_bridge.empty())
    PushUniqueText(image_review.interaction_evidence,
                   "dataset_bridge=" + interaction_dataset_bridge);
  const std::string interaction_upstream_refs =
    FindNamedResultFieldValue(result, "interaction", "upstream_refs");
  if (!interaction_upstream_refs.empty())
    PushUniqueText(image_review.interaction_evidence,
                   "upstream_refs=" + interaction_upstream_refs);
  const std::string interaction_downstream_refs =
    FindNamedResultFieldValue(result, "interaction", "downstream_refs");
  if (!interaction_downstream_refs.empty())
    PushUniqueText(image_review.interaction_evidence,
                   "downstream_refs=" + interaction_downstream_refs);
  if (!result.published_route_hint.empty())
    PushUniqueText(image_review.interaction_evidence,
                   "published_route_hint=" + result.published_route_hint);
  if (!result.published_route_state.empty())
    PushUniqueText(image_review.interaction_evidence,
                   "published_route_state=" + result.published_route_state);

  PopulateGuiImageInputContract(result, image_review, image_sequence_trace);

  image_review.changed_fields = BuildUnifiedImageChangedFields(image_review);
  image_review.changed_element_ids =
    BuildAllChangedElementIds(image_review.detection_elements);
  image_review.changed_chain_keys =
    BuildAllChangedChainKeys(image_review.element_chains);
  image_review.refresh_priority =
    DetermineImageRefreshPriority(result, image_review);

  result.unified_image_reviews.push_back(image_review);

  UnifiedTaskReviewBundle task_review;
  task_review.source_thread = image_review.source_thread;
  task_review.task_id = image_review.task_id;
  task_review.batch_id = image_review.batch_id;
  task_review.task_type = result.kind.empty() ? "cxscript_task" : result.kind;
  task_review.case_group = result.module + "." + result.layer;
  task_review.primary_visual_ref = image_review.primary_visual_ref;
  task_review.total_images = static_cast<int>(
    CountDelimitedItems(result.input_sample.empty() ? image_review.image_id : result.input_sample, ';'));
  if (task_review.total_images <= 0)
    task_review.total_images = 1;
  task_review.abnormal_images = image_review.status == "abnormal" ? 1 : 0;
  PushUniqueText(task_review.focus_image_ids, image_review.image_id);
  for (size_t i = 0; i < classical_review.focus_image_ids.size(); ++i)
    PushUniqueText(task_review.focus_image_ids, classical_review.focus_image_ids[i]);
  task_review.metric_summary = image_review.metric_summary_text;
  task_review.stage_summary = result.summary;
  task_review.current_conclusion = result.success ? result.summary : result.error_message;
  task_review.next_attention_points =
    FindNamedResultFieldValue(result, "analysis", "recommended_action");
  if (task_review.next_attention_points.empty())
    task_review.next_attention_points =
      FindNamedResultFieldValue(result, "analysis", "next_review_action");
  if (task_review.next_attention_points.empty())
    task_review.next_attention_points =
      result.success ? "review unified image/task/compare/anomaly bundles"
                     : "inspect anomaly focus bundle and failure evidence";
  if (!classical_review.analysis_suggestions.empty() &&
      (task_review.next_attention_points.empty() || classical_review.matched_case))
    task_review.next_attention_points = JoinTextItems(classical_review.analysis_suggestions, ";");
  if (unified_image_review_required && !unified_image_review_ready)
    task_review.next_attention_points =
      "complete UnifiedImageReviewRecord before task/compare/anomaly review surfaces";
  task_review.status_distribution =
    image_review.status + "=" + std::to_string(task_review.total_images);
  task_review.anomaly_type_distribution =
    image_review.anomaly_flags.empty() ? "none=0"
                                       : JoinTextItems(image_review.anomaly_flags, ",");
  task_review.baseline_compare_summary =
    !result.compare_ref.empty() ? ("compare_ref=" + result.compare_ref)
                                : (!result.summary_ref.empty()
                                     ? ("summary_ref=" + result.summary_ref)
                                     : std::string("no_compare_ref"));
  task_review.review_required_count =
    image_review.status == "review_only" || !image_review.anomaly_flags.empty() ? 1 : 0;
  if (!image_review.anomaly_flags.empty())
    task_review.top_metric_outliers = image_review.anomaly_flags;
  if (!result.train_param_summary.empty())
    task_review.training_evidence_summary = result.train_param_summary;
  else if (!result.infer_param_summary.empty())
    task_review.training_evidence_summary = result.infer_param_summary;
  if (image_review.source_thread == "ensmallen")
  {
    const std::string task_conclusion = BuildEnsmallenGuiTaskConclusion(result);
    const std::string best_params_ref =
      ResolveNamedOrDirectRef(result, "refs", "best_params_ref", result.best_params_ref);
    if (!task_conclusion.empty())
      task_review.current_conclusion = task_conclusion;
    if (!image_review.visual_evidence_ref_set.empty())
      task_review.visual_evidence_summary = image_review.visual_evidence_ref_set;
    if (!best_params_ref.empty() && !task_conclusion.empty())
      task_review.training_evidence_summary = best_params_ref + ";" + task_conclusion;
    else if (!best_params_ref.empty())
      task_review.training_evidence_summary = best_params_ref;
    else if (!task_conclusion.empty())
      task_review.training_evidence_summary = task_conclusion;
  }
  task_review.artifact_bundle_refs = image_review.artifact_refs;
  task_review.observation_personality = image_review.observation_personality;
  task_review.default_open_chain = image_review.default_open_chain;
  task_review.evidence_focus_summary = image_review.evidence_focus_summary;
  task_review.thread_handoff = image_review.thread_handoff;
  task_review.task_family = image_review.task_family;
  task_review.pipeline_family = image_review.pipeline_family;
  task_review.model_family = image_review.model_family;
  task_review.scenario_family = image_review.scenario_family;
  task_review.metric_summary_by_stage = image_review.metric_summary_text;
  task_review.visual_evidence_summary = image_review.visual_evidence_set;
  task_review.device_summary = image_review.device_evidence;
  task_review.business_eval_fields = image_review.business_eval_fields;
  task_review.pipeline_link_trace = image_review.pipeline_link_trace;
  task_review.sequence_family = image_review.sequence_family;
  task_review.stage_transition_summary = BuildTorchStageTransitionSummary(image_sequence_trace);
  task_review.stage_abnormal_summary = BuildTorchStageAbnormalSummary(result, image_review);
  task_review.review_mode = BuildReviewMode(result, image_review.source_thread);
  task_review.default_decision_axis =
    BuildDefaultDecisionAxis(result, image_review.source_thread);
  task_review.tolerance_summary = tolerance_summary;
  task_review.stability_summary = stability_summary;
  PushUniqueText(task_review.supporting_refs, image_review.primary_visual_ref);
  PushUniqueText(task_review.supporting_refs, image_review.input_image_ref);
  PushUniqueText(task_review.supporting_refs, result.summary_ref);
  PushUniqueText(task_review.supporting_refs, result.compare_ref);
  PushUniqueText(task_review.supporting_refs, result.replay_ref);
  PushUniqueText(task_review.supporting_refs, result.baseline_feature_ref);
  PushUniqueText(task_review.supporting_refs, result.attach_back_ref);
  PushUniqueText(task_review.supporting_refs, threshold_ref);
  for (size_t i = 0; i < image_review.artifact_refs.size(); ++i)
    PushUniqueText(task_review.supporting_refs, image_review.artifact_refs[i]);
  task_review.element_type_summary = BuildElementTypeSummary(image_review.detection_elements);
  task_review.element_summary =
    BuildDetectionElementSummary(image_review.detection_elements);
  task_review.element_chain_summary =
    BuildElementChainSummary(image_review.element_chains);
  task_review.element_status_summary =
    BuildElementStatusBoardSummary(image_review.detection_elements);
  task_review.candidate_status_summary =
    BuildCandidateStatusSummary(image_review.detection_elements);
  task_review.match_status_summary =
    BuildMatchStatusSummary(image_review.detection_elements);
  task_review.manual_review_signal_summary =
    BuildManualReviewSignalSummary(image_review.detection_elements);
  task_review.element_group_summary =
    BuildElementGroupSummary(image_review.detection_elements);
  task_review.element_findings =
    BuildElementFindingsSummary(image_review.detection_elements);
  task_review.element_level_focus =
    BuildElementLevelFocusSummary(image_review.detection_elements);
  task_review.focus_refresh_targets = image_review.focus_refresh_targets;
  task_review.local_delta_targets = image_review.local_delta_targets;
  task_review.grouped_element_preview = image_review.grouped_element_preview;
  task_review.focus_element_preview = image_review.focus_element_preview;
  task_review.delta_element_preview = image_review.delta_element_preview;
  task_review.missing_element_summary =
    BuildElementStatusSummary(image_review.detection_elements, "missing");
  task_review.drifted_element_summary =
    BuildElementStatusSummary(image_review.detection_elements, "drifted");
  task_review.abnormal_element_summary =
    BuildElementStatusSummary(image_review.detection_elements, "abnormal");
  PopulateGuiTaskInputContract(task_review, image_review);
  if (classical_review.matched_case && image_review.status != "normal")
    task_review.abnormal_images = 1;
  task_review.refresh_mode =
    (!task_review.local_delta_targets.empty() || !task_review.focus_refresh_targets.empty())
      ? "focus_summary"
      : "summary";
  task_review.changed_fields = BuildUnifiedTaskChangedFields(task_review);
  task_review.changed_element_ids =
    BuildDeltaChangedElementIds(image_review.detection_elements);
  task_review.changed_chain_keys =
    BuildDeltaChangedChainKeys(image_review.element_chains);
  task_review.refresh_priority = DetermineTaskRefreshPriority(task_review);

  result.unified_task_reviews.push_back(task_review);

  UnifiedCompareSlice compare_slice;
  compare_slice.compare_id =
    result.task_id.empty() ? result.case_name + ".review_compare"
                           : result.task_id + ".review_compare";
  compare_slice.left_ref = image_review.primary_visual_ref;
  compare_slice.right_ref =
    !result.compare_ref.empty() ? result.compare_ref :
    (!result.replay_ref.empty() ? result.replay_ref :
     (!result.summary_ref.empty() ? result.summary_ref :
      (!result.baseline_feature_ref.empty() ? result.baseline_feature_ref : result.task_id)));
  compare_slice.compare_type =
    !result.compare_ref.empty() ? "current_vs_baseline" :
    (!result.replay_ref.empty() ? "same_task_cross_stage" : "cross_thread_same_case");
  PushUniqueText(compare_slice.compare_dimensions, "status");
  PushUniqueText(compare_slice.compare_dimensions, "metrics");
  PushUniqueText(compare_slice.compare_dimensions, "visual_evidence");
  std::vector<std::string> compare_delta_items;
  const std::string objective_curve = BuildEnsmallenObjectiveCurveValue(result);
  if (!objective_curve.empty())
    compare_delta_items.push_back("objective_curve=" + objective_curve);
  if (result.objective_delta != 0.0)
    compare_delta_items.push_back("objective_delta=" +
                                  FormatElementNumber(result.objective_delta));
  const std::string feature_distance_delta =
    BuildEnsmallenFeatureDistanceDeltaValue(result);
  if (!feature_distance_delta.empty())
    compare_delta_items.push_back("feature_distance_delta=" + feature_distance_delta);
  const std::string candidate_rank = BuildEnsmallenCandidateRankValue(result);
  if (!candidate_rank.empty())
    compare_delta_items.push_back("candidate_rank=" + candidate_rank);
  if (!result.selected_method.empty())
    compare_delta_items.push_back("selected_method=" + result.selected_method);
  if (!result.ordered_candidates.empty())
    compare_delta_items.push_back("candidate_ordering=" + result.ordered_candidates);
  const std::string best_candidate_confidence =
    BuildEnsmallenBestCandidateConfidenceValue(result);
  if (!best_candidate_confidence.empty())
    compare_delta_items.push_back("best_candidate_confidence=" + best_candidate_confidence);
  compare_slice.delta_summary =
    !compare_delta_items.empty()
      ? JoinTextItems(compare_delta_items, ";")
      : (result.metric_delta != 0.0
           ? ("metric_delta=" + FormatElementNumber(result.metric_delta))
           : task_review.baseline_compare_summary);
  compare_slice.risk_level = DetermineReviewRiskLevel(result, image_review.anomaly_flags);
  compare_slice.focus_recommendation = task_review.next_attention_points;
  compare_slice.observation_personality = image_review.observation_personality;
  compare_slice.default_open_chain = image_review.default_open_chain;
  compare_slice.evidence_focus_summary = image_review.evidence_focus_summary;
  compare_slice.thread_handoff = image_review.thread_handoff;
  compare_slice.compare_view_mode =
    BuildCompareViewMode(result, image_review.source_thread);
  compare_slice.threshold_summary = threshold_summary;
  compare_slice.risk_note = risk_note;
  PushUniqueText(compare_slice.supporting_refs, result.compare_ref);
  PushUniqueText(compare_slice.supporting_refs, result.replay_ref);
  PushUniqueText(compare_slice.supporting_refs, result.summary_ref);
  PushUniqueText(compare_slice.supporting_refs, result.baseline_feature_ref);
  PushUniqueText(compare_slice.supporting_refs, image_review.primary_visual_ref);
  PushUniqueText(compare_slice.supporting_refs, image_review.input_image_ref);
  PushUniqueText(compare_slice.supporting_refs, compare_slice.right_ref);
  PushUniqueText(compare_slice.supporting_refs, threshold_ref);
  for (size_t i = 0; i < image_review.artifact_refs.size(); ++i)
    PushUniqueText(compare_slice.supporting_refs, image_review.artifact_refs[i]);
  for (size_t i = 0; i < image_review.visualization_refs.size(); ++i)
    PushUniqueText(compare_slice.supporting_refs, image_review.visualization_refs[i]);
  compare_slice.element_summary =
    BuildDetectionElementSummary(image_review.detection_elements);
  compare_slice.element_chain_summary =
    BuildElementChainSummary(image_review.element_chains);
  compare_slice.element_status_summary =
    BuildElementStatusBoardSummary(image_review.detection_elements);
  compare_slice.candidate_status_summary =
    BuildCandidateStatusSummary(image_review.detection_elements);
  compare_slice.match_status_summary =
    BuildMatchStatusSummary(image_review.detection_elements);
  compare_slice.manual_review_signal_summary =
    BuildManualReviewSignalSummary(image_review.detection_elements);
  compare_slice.element_group_summary =
    BuildElementGroupSummary(image_review.detection_elements);
  compare_slice.element_findings =
    BuildElementFindingsSummary(image_review.detection_elements);
  compare_slice.element_level_focus =
    BuildElementLevelFocusSummary(image_review.detection_elements);
  compare_slice.focus_refresh_targets = image_review.focus_refresh_targets;
  compare_slice.local_delta_targets = image_review.local_delta_targets;
  compare_slice.grouped_element_preview = image_review.grouped_element_preview;
  compare_slice.focus_element_preview = image_review.focus_element_preview;
  compare_slice.delta_element_preview = image_review.delta_element_preview;
  compare_slice.element_level_diff = BuildElementLevelDiff(image_review);
  compare_slice.semantic_diff = BuildSemanticDiff(image_review);
  compare_slice.structure_diff = BuildStructureDiff(image_review);
  compare_slice.refresh_mode =
    (!compare_slice.local_delta_targets.empty() || !compare_slice.focus_refresh_targets.empty())
      ? "focus_delta"
      : "delta";
  compare_slice.changed_fields = BuildUnifiedCompareChangedFields(compare_slice);
  compare_slice.changed_element_ids =
    BuildDeltaChangedElementIds(image_review.detection_elements);
  compare_slice.changed_chain_keys =
    BuildDeltaChangedChainKeys(image_review.element_chains);
  compare_slice.refresh_priority =
    DetermineCompareRefreshPriority(compare_slice);

  result.unified_compare_slices.push_back(compare_slice);

  UnifiedAnomalyFocusBundle anomaly_bundle;
  anomaly_bundle.source_thread = image_review.source_thread;
  anomaly_bundle.task_id = task_review.task_id;
  anomaly_bundle.batch_id = task_review.batch_id;
  if (!image_review.anomaly_flags.empty() || image_review.status == "abnormal")
    PushUniqueText(anomaly_bundle.abnormal_image_ids, image_review.image_id);
  anomaly_bundle.anomaly_type_summary =
    image_review.anomaly_flags.empty() ? "none" : JoinTextItems(image_review.anomaly_flags, ",");
  PushUniqueText(anomaly_bundle.top_focus_objects, image_review.primary_visual_ref);
  PushUniqueText(anomaly_bundle.top_focus_objects, compare_slice.right_ref);
  for (size_t i = 0; i < classical_review.focus_image_ids.size(); ++i)
    PushUniqueText(anomaly_bundle.top_focus_objects, classical_review.focus_image_ids[i]);
  if (classical_review.analysis_suggestions.empty())
    PushUniqueText(anomaly_bundle.analysis_suggestions, task_review.next_attention_points);
  for (size_t i = 0; i < classical_review.analysis_suggestions.size(); ++i)
    PushUniqueText(anomaly_bundle.analysis_suggestions, classical_review.analysis_suggestions[i]);
  if (!result.error_message.empty())
    PushUniqueText(anomaly_bundle.analysis_suggestions, result.error_message);
  anomaly_bundle.risk_level = compare_slice.risk_level;
  PushUniqueText(anomaly_bundle.supporting_refs, image_review.primary_visual_ref);
  PushUniqueText(anomaly_bundle.supporting_refs, compare_slice.right_ref);
  PushUniqueText(anomaly_bundle.supporting_refs, result.summary_ref);
  PushUniqueText(anomaly_bundle.supporting_refs, result.compare_ref);
  PushUniqueText(anomaly_bundle.supporting_refs, result.replay_ref);
  PushUniqueText(anomaly_bundle.supporting_refs, threshold_ref);
  for (size_t i = 0; i < image_review.artifact_refs.size(); ++i)
    PushUniqueText(anomaly_bundle.supporting_refs, image_review.artifact_refs[i]);
  for (size_t i = 0; i < image_review.visualization_refs.size(); ++i)
    PushUniqueText(anomaly_bundle.supporting_refs, image_review.visualization_refs[i]);
  for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = image_review.detection_elements[i];
    if (element.consistency_status == "matched")
      continue;
    PushUniqueText(anomaly_bundle.anomaly_element_ids, element.element_id);
    PushUniqueText(anomaly_bundle.anomaly_element_types, element.element_type);
  }
  anomaly_bundle.observation_personality = image_review.observation_personality;
  anomaly_bundle.default_open_chain = image_review.default_open_chain;
  anomaly_bundle.evidence_focus_summary = image_review.evidence_focus_summary;
  anomaly_bundle.thread_handoff = image_review.thread_handoff;
  anomaly_bundle.anomaly_axis = BuildAnomalyAxis(result, image_review.source_thread);
  anomaly_bundle.stability_summary = stability_summary;
  anomaly_bundle.risk_note = risk_note;
  anomaly_bundle.element_summary = task_review.element_summary;
  anomaly_bundle.element_chain_summary = task_review.element_chain_summary;
  anomaly_bundle.element_status_summary = task_review.element_status_summary;
  anomaly_bundle.candidate_status_summary = task_review.candidate_status_summary;
  anomaly_bundle.match_status_summary = task_review.match_status_summary;
  anomaly_bundle.manual_review_signal_summary = task_review.manual_review_signal_summary;
  anomaly_bundle.element_group_summary = task_review.element_group_summary;
  anomaly_bundle.element_findings = task_review.element_findings;
  anomaly_bundle.element_level_focus = task_review.element_level_focus;
  anomaly_bundle.focus_refresh_targets = task_review.focus_refresh_targets;
  anomaly_bundle.local_delta_targets = task_review.local_delta_targets;
  anomaly_bundle.grouped_element_preview = task_review.grouped_element_preview;
  anomaly_bundle.focus_element_preview = task_review.focus_element_preview;
  anomaly_bundle.delta_element_preview = task_review.delta_element_preview;
  anomaly_bundle.anomaly_focus_reason = BuildAnomalyFocusReason(image_review);
  anomaly_bundle.refresh_mode =
    (!anomaly_bundle.local_delta_targets.empty() || !anomaly_bundle.focus_refresh_targets.empty())
      ? "focus_delta"
      : ((anomaly_bundle.abnormal_image_ids.empty() && anomaly_bundle.anomaly_element_ids.empty())
           ? "summary"
           : "delta");
  anomaly_bundle.changed_fields = BuildUnifiedAnomalyChangedFields(anomaly_bundle);
  anomaly_bundle.changed_element_ids = anomaly_bundle.anomaly_element_ids;
  anomaly_bundle.changed_chain_keys =
    BuildDeltaChangedChainKeys(image_review.element_chains);
  anomaly_bundle.refresh_priority =
    DetermineAnomalyRefreshPriority(anomaly_bundle);

  result.unified_anomaly_focus_bundles.push_back(anomaly_bundle);

  const std::string manual_review_targets =
    BuildManualReviewTargets(image_review);
  const std::string roi_visual_evidence =
    BuildRoiVisualEvidence(image_review);
  const bool cximage_gui_contract = IsCximageGuiContractResult(result);
  const std::string cximage_raw_image_ref =
    cximage_gui_contract ? ResolveCximageGuiRawImageRef(image_review, classical_review)
                         : std::string();
  const std::string cximage_edge_image_ref =
    cximage_gui_contract ? ResolveCximageGuiEdgeImageRef(result, image_review, classical_review)
                         : std::string();
  const std::string cximage_element_relation_image_ref =
    cximage_gui_contract ? ResolveCximageGuiElementRelationImageRef(result, image_review, classical_review)
                         : std::string();
  const std::string cximage_candidate_image_ref =
    cximage_gui_contract ? ResolveCximageGuiCandidateImageRef(result, image_review, classical_review)
                         : std::string();
  const std::string cximage_match_image_ref =
    cximage_gui_contract ? ResolveCximageGuiMatchImageRef(result, image_review, classical_review)
                         : std::string();
  const std::string cximage_geometry_stage =
    cximage_gui_contract ? DetermineCximageGeometryStage(result, image_review)
                         : std::string();
  const std::string cximage_candidate_stage =
    cximage_gui_contract ? DetermineCximageCandidateStage(result, image_review)
                         : std::string();
  const std::string cximage_match_stage =
    cximage_gui_contract ? DetermineCximageMatchStage(result, image_review)
                         : std::string();
  const std::string cximage_problem_issue_type =
    cximage_gui_contract ? DetermineCximageProblemIssueType(image_review)
                         : std::string();
  const std::string cximage_problem_focus_element_id =
    cximage_gui_contract ? DetermineCximageProblemFocusElementId(image_review)
                         : std::string();
  const std::string cximage_problem_focus_chain_id =
    cximage_gui_contract ? DetermineCximageProblemFocusChainId(image_review)
                         : std::string();
  const std::string cximage_problem_focus_image_ref =
    cximage_gui_contract ? DetermineCximageProblemFocusImageRef(cximage_problem_issue_type,
                                                                cximage_raw_image_ref,
                                                                cximage_edge_image_ref,
                                                                cximage_element_relation_image_ref,
                                                                cximage_candidate_image_ref,
                                                                cximage_match_image_ref)
                         : std::string();
  const std::string cximage_single_image_geometry_conclusion =
    cximage_gui_contract ? BuildCximageSingleImageGeometryConclusion(image_review,
                                                                     cximage_geometry_stage)
                         : std::string();
  const std::string cximage_element_conclusion =
    cximage_gui_contract ? BuildCximageElementConclusion(image_review)
                         : std::string();
  const std::string cximage_match_conclusion =
    cximage_gui_contract ? BuildCximageMatchConclusion(image_review)
                         : std::string();
  const std::string cximage_gui_chain_summary =
    cximage_gui_contract ? BuildCximageGuiChainSummary()
                         : std::string();
  if (cximage_gui_contract)
  {
    image_review.raw_image_ref = cximage_raw_image_ref;
    image_review.edge_image_ref = cximage_edge_image_ref;
    image_review.element_relation_image_ref = cximage_element_relation_image_ref;
    image_review.candidate_image_ref = cximage_candidate_image_ref;
    image_review.match_image_ref = cximage_match_image_ref;
    image_review.geometry_stage = cximage_geometry_stage;
    image_review.candidate_stage = cximage_candidate_stage;
    image_review.match_stage = cximage_match_stage;
    image_review.problem_focus_image_ref = cximage_problem_focus_image_ref;
    image_review.problem_focus_element_id = cximage_problem_focus_element_id;
    image_review.problem_focus_chain_id = cximage_problem_focus_chain_id;
    image_review.problem_issue_type = cximage_problem_issue_type;
    image_review.single_image_geometry_conclusion = cximage_single_image_geometry_conclusion;
    image_review.element_conclusion = cximage_element_conclusion;
    image_review.match_conclusion = cximage_match_conclusion;
    image_review.task_conclusion = task_review.current_conclusion;
    image_review.next_step_suggestion = task_review.next_attention_points;
    image_review.gui_chain_summary = cximage_gui_chain_summary;

    task_review.raw_image_ref = cximage_raw_image_ref;
    task_review.edge_image_ref = cximage_edge_image_ref;
    task_review.element_relation_image_ref = cximage_element_relation_image_ref;
    task_review.candidate_image_ref = cximage_candidate_image_ref;
    task_review.match_image_ref = cximage_match_image_ref;
    task_review.geometry_stage = cximage_geometry_stage;
    task_review.candidate_stage = cximage_candidate_stage;
    task_review.match_stage = cximage_match_stage;
    task_review.problem_focus_image_ref = cximage_problem_focus_image_ref;
    task_review.problem_focus_element_id = cximage_problem_focus_element_id;
    task_review.problem_focus_chain_id = cximage_problem_focus_chain_id;
    task_review.problem_issue_type = cximage_problem_issue_type;
    task_review.single_image_geometry_conclusion = cximage_single_image_geometry_conclusion;
    task_review.element_conclusion = cximage_element_conclusion;
    task_review.match_conclusion = cximage_match_conclusion;
    task_review.task_conclusion = task_review.current_conclusion;
    task_review.next_step_suggestion = task_review.next_attention_points;
    task_review.gui_chain_summary = cximage_gui_chain_summary;

    anomaly_bundle.raw_image_ref = cximage_raw_image_ref;
    anomaly_bundle.edge_image_ref = cximage_edge_image_ref;
    anomaly_bundle.element_relation_image_ref = cximage_element_relation_image_ref;
    anomaly_bundle.candidate_image_ref = cximage_candidate_image_ref;
    anomaly_bundle.match_image_ref = cximage_match_image_ref;
    anomaly_bundle.geometry_stage = cximage_geometry_stage;
    anomaly_bundle.candidate_stage = cximage_candidate_stage;
    anomaly_bundle.match_stage = cximage_match_stage;
    anomaly_bundle.problem_focus_image_ref = cximage_problem_focus_image_ref;
    anomaly_bundle.problem_focus_element_id = cximage_problem_focus_element_id;
    anomaly_bundle.problem_focus_chain_id = cximage_problem_focus_chain_id;
    anomaly_bundle.problem_issue_type = cximage_problem_issue_type;
    anomaly_bundle.single_image_geometry_conclusion = cximage_single_image_geometry_conclusion;
    anomaly_bundle.element_conclusion = cximage_element_conclusion;
    anomaly_bundle.match_conclusion = cximage_match_conclusion;
    anomaly_bundle.task_conclusion = task_review.current_conclusion;
    anomaly_bundle.next_step_suggestion = task_review.next_attention_points;
    anomaly_bundle.gui_chain_summary = cximage_gui_chain_summary;

    image_review.changed_fields = BuildUnifiedImageChangedFields(image_review);
    task_review.changed_fields = BuildUnifiedTaskChangedFields(task_review);
    anomaly_bundle.changed_fields = BuildUnifiedAnomalyChangedFields(anomaly_bundle);
    if (!result.unified_image_reviews.empty())
      result.unified_image_reviews.back() = image_review;
    if (!result.unified_task_reviews.empty())
      result.unified_task_reviews.back() = task_review;
    if (!result.unified_anomaly_focus_bundles.empty())
      result.unified_anomaly_focus_bundles.back() = anomaly_bundle;
  }
  std::vector<std::string> review_board_changed_fields = image_review.changed_fields;
  for (size_t i = 0; i < task_review.changed_fields.size(); ++i)
    PushUniqueText(review_board_changed_fields, task_review.changed_fields[i]);
  for (size_t i = 0; i < compare_slice.changed_fields.size(); ++i)
    PushUniqueText(review_board_changed_fields, compare_slice.changed_fields[i]);
  for (size_t i = 0; i < anomaly_bundle.changed_fields.size(); ++i)
    PushUniqueText(review_board_changed_fields, anomaly_bundle.changed_fields[i]);
  std::vector<std::string> review_board_changed_element_ids = image_review.changed_element_ids;
  for (size_t i = 0; i < task_review.changed_element_ids.size(); ++i)
    PushUniqueText(review_board_changed_element_ids, task_review.changed_element_ids[i]);
  for (size_t i = 0; i < compare_slice.changed_element_ids.size(); ++i)
    PushUniqueText(review_board_changed_element_ids, compare_slice.changed_element_ids[i]);
  for (size_t i = 0; i < anomaly_bundle.changed_element_ids.size(); ++i)
    PushUniqueText(review_board_changed_element_ids, anomaly_bundle.changed_element_ids[i]);
  std::vector<std::string> review_board_changed_chain_keys = image_review.changed_chain_keys;
  for (size_t i = 0; i < task_review.changed_chain_keys.size(); ++i)
    PushUniqueText(review_board_changed_chain_keys, task_review.changed_chain_keys[i]);
  for (size_t i = 0; i < compare_slice.changed_chain_keys.size(); ++i)
    PushUniqueText(review_board_changed_chain_keys, compare_slice.changed_chain_keys[i]);
  for (size_t i = 0; i < anomaly_bundle.changed_chain_keys.size(); ++i)
    PushUniqueText(review_board_changed_chain_keys, anomaly_bundle.changed_chain_keys[i]);
  const std::string review_board_refresh_priority =
    DetermineBoardRefreshPriority(image_review, task_review, compare_slice, anomaly_bundle);

  AddNamedResultObject(result,
                       "review_image",
                       "review",
                       "UnifiedImageReviewRecord",
                       image_review.status,
                       result.failure_phase);
  AddNamedResultField(result, "review_image", "review", "source_thread", image_review.source_thread);
  AddNamedResultField(result, "review_image", "review", "task_id", image_review.task_id);
  AddNamedResultField(result, "review_image", "review", "batch_id", image_review.batch_id);
  AddNamedResultField(result, "review_image", "review", "case_name", image_review.case_name);
  AddNamedResultField(result, "review_image", "review", "image_id", image_review.image_id);
  AddNamedResultField(result, "review_image", "review", "stage", image_review.stage);
  AddNamedResultField(result, "review_image", "review", "input_image_ref", image_review.input_image_ref);
  AddNamedResultField(result, "review_image", "review", "primary_visual_ref", image_review.primary_visual_ref);
  AddNamedResultField(result, "review_image", "review", "status", image_review.status);
  AddNamedResultField(result, "review_image", "review", "metrics", image_review.metric_summary_text);
  AddNamedResultField(result, "review_image", "review", "anomaly_flags",
                      JoinTextItems(image_review.anomaly_flags, ";"));
  AddNamedResultField(result, "review_image", "review", "notes", image_review.notes);
  AddNamedResultField(result, "review_image", "review", "output_image_refs",
                      JoinTextItems(image_review.output_image_refs, ";"));
  AddNamedResultField(result, "review_image", "review", "visualization_refs",
                      JoinTextItems(image_review.visualization_refs, ";"));
  AddNamedResultField(result, "review_image", "review", "baseline_refs",
                      JoinTextItems(image_review.baseline_refs, ";"));
  AddNamedResultField(result, "review_image", "review", "compare_tags",
                      JoinTextItems(image_review.compare_tags, ";"));
  AddNamedResultField(result, "review_image", "review", "artifact_refs",
                      JoinTextItems(image_review.artifact_refs, ";"));
  AddNamedResultField(result, "review_image", "review", "contract_evidence",
                      JoinTextItems(image_review.contract_evidence, ";"));
  AddNamedResultField(result, "review_image", "review", "phenomenon_evidence",
                      JoinTextItems(image_review.phenomenon_evidence, ";"));
  AddNamedResultField(result, "review_image", "review", "interaction_evidence",
                      JoinTextItems(image_review.interaction_evidence, ";"));
  AddNamedResultField(result, "review_image", "review", "observation_personality",
                      image_review.observation_personality);
  AddNamedResultField(result, "review_image", "review", "default_open_chain",
                      image_review.default_open_chain);
  AddNamedResultField(result, "review_image", "review", "evidence_focus_summary",
                      image_review.evidence_focus_summary);
  AddNamedResultField(result, "review_image", "review", "thread_handoff",
                      image_review.thread_handoff);
  if (!result.geometry_template_specs.empty())
  {
    AddNamedResultField(result, "review_image", "review", "geometry_template_spec",
                        result.geometry_template_specs[0].template_identity);
    AddNamedResultField(result, "review_image", "review", "template_provenance",
                        result.geometry_template_specs[0].template_provenance);
    AddNamedResultField(result, "review_image", "review", "template_review_priority",
                        result.geometry_template_specs[0].review_priority);
  }
  if (!result.image_acquisition_specs.empty())
  {
    AddNamedResultField(result, "review_image", "review", "image_acquisition_spec",
                        BuildImageAcquisitionRef(result.image_acquisition_specs[0]));
    AddNamedResultField(result, "review_image", "review", "acquisition_scope",
                        result.image_acquisition_specs[0].scope_type);
    AddNamedResultField(result, "review_image", "review", "acquisition_mode",
                        result.image_acquisition_specs[0].execution_mode);
    AddNamedResultField(result, "review_image", "review", "acquisition_provenance",
                        result.image_acquisition_specs[0].provenance);
  }
  AddNamedResultField(result, "review_image", "review", "task_entry_name",
                      image_review.task_entry_name);
  AddNamedResultField(result, "review_image", "review", "task_family",
                      image_review.task_family);
  AddNamedResultField(result, "review_image", "review", "pipeline_family",
                      image_review.pipeline_family);
  AddNamedResultField(result, "review_image", "review", "model_family",
                      image_review.model_family);
  AddNamedResultField(result, "review_image", "review", "scenario_family",
                      image_review.scenario_family);
  AddNamedResultField(result, "review_image", "review", "visual_evidence_set",
                      image_review.visual_evidence_set);
  AddNamedResultField(result, "review_image", "review", "device_evidence",
                      image_review.device_evidence);
  AddNamedResultField(result, "review_image", "review", "business_eval_fields",
                      image_review.business_eval_fields);
  AddNamedResultField(result, "review_image", "review", "pipeline_link_trace",
                      image_review.pipeline_link_trace);
  AddNamedResultField(result, "review_image", "review", "sequence_family",
                      image_review.sequence_family);
  AddNamedResultField(result, "review_image", "review", "sequence_stage",
                      image_review.sequence_stage);
  AddNamedResultField(result, "review_image", "review", "sequence_index",
                      image_review.sequence_index);
  AddNamedResultField(result, "review_image", "review", "sequence_trace_ref",
                      image_review.sequence_trace_ref);
  AddNamedResultField(result, "review_image", "review", "sequence_records",
                      image_review.sequence_records);
  AddNamedResultField(result, "review_image", "review", "sequence_summary",
                      image_review.sequence_summary);
  AddNamedResultField(result, "review_image", "review", "sequence_status_summary",
                      image_review.sequence_status_summary);
  AddNamedResultField(result, "review_image", "review", "stage_refs",
                      image_review.stage_refs);
  AddNamedResultField(result, "review_image", "review", "script_refs",
                      image_review.script_refs);
  AddNamedResultField(result, "review_image", "review", "image_refs",
                      image_review.image_refs);
  AddNamedResultField(result, "review_image", "review", "conclusion_refs",
                      image_review.conclusion_refs);
  AddNamedResultField(result, "review_image", "review", "issue_refs",
                      image_review.issue_refs);
  AddNamedResultField(result, "review_image", "review", "lifecycle_summary",
                      image_review.lifecycle_summary);
  AddNamedResultField(result, "review_image", "review", "lifecycle_zone_refs",
                      image_review.lifecycle_zone_refs);
  AddNamedResultField(result, "review_image", "review", "init_stage_refs",
                      image_review.init_stage_refs);
  AddNamedResultField(result, "review_image", "review", "repeatable_stage_refs",
                      image_review.repeatable_stage_refs);
  AddNamedResultField(result, "review_image", "review", "debug_stage_refs",
                      image_review.debug_stage_refs);
  AddNamedResultField(result, "review_image", "review", "replay_stage_refs",
                      image_review.replay_stage_refs);
  AddNamedResultField(result, "review_image", "review", "reset_stage_refs",
                      image_review.reset_stage_refs);
  AddNamedResultField(result, "review_image", "review", "lifecycle_risk_summary",
                      image_review.lifecycle_risk_summary);
  AddNamedResultField(result, "review_image", "review", "test_image_ref",
                      image_review.test_image_ref);
  AddNamedResultField(result, "review_image", "review", "visual_evidence_ref_set",
                      image_review.visual_evidence_ref_set);
  AddNamedResultField(result, "review_image", "review", "single_image_conclusion_ref",
                      image_review.single_image_conclusion_ref);
  AddNamedResultField(result, "review_image", "review", "element_conclusion_ref_set",
                      image_review.element_conclusion_ref_set);
  AddNamedResultField(result, "review_image", "review", "task_conclusion_ref",
                      image_review.task_conclusion_ref);
  AddNamedResultField(result, "review_image", "review", "anomaly_conclusion_ref",
                      image_review.anomaly_conclusion_ref);
  AddNamedResultField(result, "review_image", "review", "next_action_ref",
                      image_review.next_action_ref);
  AddNamedResultField(result, "review_image", "review", "element_ref_set",
                      image_review.element_ref_set);
  AddNamedResultField(result, "review_image", "review", "element_type",
                      image_review.element_type);
  AddNamedResultField(result, "review_image", "review", "element_source",
                      image_review.element_source);
  AddNamedResultField(result, "review_image", "review", "element_visual_anchor",
                      image_review.element_visual_anchor);
  AddNamedResultField(result, "review_image", "review", "template_relation",
                      image_review.template_relation);
  AddNamedResultField(result, "review_image", "review", "consistency_status",
                      image_review.consistency_status);
  AddNamedResultField(result, "review_image", "review", "chain_ref_set",
                      image_review.chain_ref_set);
  AddNamedResultField(result, "review_image", "review", "chain_key",
                      image_review.chain_key);
  AddNamedResultField(result, "review_image", "review", "chain_status",
                      image_review.chain_status);
  AddNamedResultField(result, "review_image", "review", "chain_focus_ref",
                      image_review.chain_focus_ref);
  AddNamedResultField(result, "review_image", "review", "chain_issue_ref",
                      image_review.chain_issue_ref);
  AddNamedResultField(result, "review_image", "review", "stage_ref_set",
                      image_review.stage_ref_set);
  AddNamedResultField(result, "review_image", "review", "current_stage",
                      image_review.current_stage);
  AddNamedResultField(result, "review_image", "review", "upstream_ref",
                      image_review.upstream_ref);
  AddNamedResultField(result, "review_image", "review", "downstream_ref",
                      image_review.downstream_ref);
  AddNamedResultField(result, "review_image", "review", "stage_status",
                      image_review.stage_status);
  AddNamedResultField(result, "review_image", "review", "issue_entry_ref_set",
                      image_review.issue_entry_ref_set);
  AddNamedResultField(result, "review_image", "review", "recommended_image_ref",
                      image_review.recommended_image_ref);
  AddNamedResultField(result, "review_image", "review", "recommended_element_ref",
                      image_review.recommended_element_ref);
  AddNamedResultField(result, "review_image", "review", "recommended_chain_ref",
                      image_review.recommended_chain_ref);
  AddNamedResultField(result, "review_image", "review", "recommended_stage_ref",
                      image_review.recommended_stage_ref);
  AddNamedResultField(result, "review_image", "review", "issue_kind_hint",
                      image_review.issue_kind_hint);
  AddNamedResultField(result, "review_image", "review", "refresh_mode",
                      image_review.refresh_mode);
  AddNamedResultField(result, "review_image", "review", "changed_fields",
                      JoinTextItems(image_review.changed_fields, ";"));
  AddNamedResultField(result, "review_image", "review", "changed_element_ids",
                      JoinTextItems(image_review.changed_element_ids, ";"));
  AddNamedResultField(result, "review_image", "review", "changed_chain_keys",
                      JoinTextItems(image_review.changed_chain_keys, ";"));
  AddNamedResultField(result, "review_image", "review", "refresh_priority",
                      image_review.refresh_priority);
  AddNamedResultField(result, "review_image", "review", "detection_elements",
                      SerializeDetectionElements(image_review.detection_elements));
  AddNamedResultField(result, "review_image", "review", "elements",
                      SerializeDetectionElements(image_review.detection_elements));
  AddNamedResultField(result, "review_image", "review", "element_summary",
                      BuildDetectionElementSummary(image_review.detection_elements));
  AddNamedResultField(result, "review_image", "review", "element_chains",
                      SerializeElementChains(image_review.element_chains));
  AddNamedResultField(result, "review_image", "review", "element_chain_summary",
                      BuildElementChainSummary(image_review.element_chains));
  AddNamedResultField(result, "review_image", "review", "element_status_summary",
                      BuildElementStatusBoardSummary(image_review.detection_elements));
  AddNamedResultField(result, "review_image", "review", "candidate_status_summary",
                      image_review.candidate_status_summary);
  AddNamedResultField(result, "review_image", "review", "match_status_summary",
                      image_review.match_status_summary);
  AddNamedResultField(result, "review_image", "review", "manual_review_signal_summary",
                      image_review.manual_review_signal_summary);
  AddNamedResultField(result, "review_image", "review", "element_group_summary",
                      image_review.element_group_summary);
  AddNamedResultField(result, "review_image", "review", "element_findings",
                      BuildElementFindingsSummary(image_review.detection_elements));
  AddNamedResultField(result, "review_image", "review", "element_level_focus",
                      BuildElementLevelFocusSummary(image_review.detection_elements));
  AddNamedResultField(result, "review_image", "review", "focus_refresh_targets",
                      image_review.focus_refresh_targets);
  AddNamedResultField(result, "review_image", "review", "local_delta_targets",
                      image_review.local_delta_targets);
  AddNamedResultField(result, "review_image", "review", "grouped_element_preview",
                      image_review.grouped_element_preview);
  AddNamedResultField(result, "review_image", "review", "focus_element_preview",
                      image_review.focus_element_preview);
  AddNamedResultField(result, "review_image", "review", "delta_element_preview",
                      image_review.delta_element_preview);
  AddNamedResultField(result, "review_image", "review", "manual_review_targets",
                      manual_review_targets);
  AddNamedResultField(result, "review_image", "review", "roi_visual_evidence",
                      roi_visual_evidence);
  AddNamedResultField(result, "review_image", "review", "primary_detection_semantic",
                      image_review.primary_detection_semantic);
  AddNamedResultField(result, "review_image", "review", "template_alignment_status",
                      image_review.template_alignment_status);
  AddNamedResultField(result, "review_image", "review", "missing_element_count",
                      std::to_string(image_review.missing_element_count));
  AddNamedResultField(result, "review_image", "review", "abnormal_element_count",
                      std::to_string(image_review.abnormal_element_count));
  AddNamedResultField(result, "review_image", "review", "drifted_element_count",
                      std::to_string(image_review.drifted_element_count));
  AddNamedResultField(result, "review_image", "review", "candidate_element_count",
                      std::to_string(image_review.candidate_element_count));
  if (cximage_gui_contract)
  {
    AddNamedResultField(result, "review_image", "review", "raw_image_ref",
                        cximage_raw_image_ref);
    AddNamedResultField(result, "review_image", "review", "edge_image_ref",
                        cximage_edge_image_ref);
    AddNamedResultField(result, "review_image", "review", "element_relation_image_ref",
                        cximage_element_relation_image_ref);
    AddNamedResultField(result, "review_image", "review", "candidate_image_ref",
                        cximage_candidate_image_ref);
    AddNamedResultField(result, "review_image", "review", "match_image_ref",
                        cximage_match_image_ref);
    AddNamedResultField(result, "review_image", "review", "geometry_stage",
                        cximage_geometry_stage);
    AddNamedResultField(result, "review_image", "review", "candidate_stage",
                        cximage_candidate_stage);
    AddNamedResultField(result, "review_image", "review", "match_stage",
                        cximage_match_stage);
    AddNamedResultField(result, "review_image", "review", "problem_focus_image_ref",
                        cximage_problem_focus_image_ref);
    AddNamedResultField(result, "review_image", "review", "problem_focus_element_id",
                        cximage_problem_focus_element_id);
    AddNamedResultField(result, "review_image", "review", "problem_focus_chain_id",
                        cximage_problem_focus_chain_id);
    AddNamedResultField(result, "review_image", "review", "problem_issue_type",
                        cximage_problem_issue_type);
    AddNamedResultField(result, "review_image", "review", "single_image_geometry_conclusion",
                        cximage_single_image_geometry_conclusion);
    AddNamedResultField(result, "review_image", "review", "element_conclusion",
                        cximage_element_conclusion);
    AddNamedResultField(result, "review_image", "review", "match_conclusion",
                        cximage_match_conclusion);
    AddNamedResultField(result, "review_image", "review", "task_conclusion",
                        task_review.current_conclusion);
    AddNamedResultField(result, "review_image", "review", "next_step_suggestion",
                        task_review.next_attention_points);
    AddNamedResultField(result, "review_image", "review", "gui_chain_summary",
                        cximage_gui_chain_summary);
  }
  AddNamedResultObject(result,
                       "review_task",
                       "review",
                       "UnifiedTaskReviewBundle",
                       unified_image_review_ready ? (result.success ? "ready" : "partial")
                                                  : "blocked_by_unified_image_review",
                       result.failure_phase);
  AddNamedResultField(result, "review_task", "review", "source_thread", task_review.source_thread);
  AddNamedResultField(result, "review_task", "review", "task_id", task_review.task_id);
  AddNamedResultField(result, "review_task", "review", "batch_id", task_review.batch_id);
  AddNamedResultField(result, "review_task", "review", "task_type", task_review.task_type);
  AddNamedResultField(result, "review_task", "review", "case_group", task_review.case_group);
  AddNamedResultField(result, "review_task", "review", "primary_visual_ref",
                      task_review.primary_visual_ref);
  AddNamedResultField(result, "review_task", "review", "total_images",
                      std::to_string(task_review.total_images));
  AddNamedResultField(result, "review_task", "review", "abnormal_images",
                      std::to_string(task_review.abnormal_images));
  AddNamedResultField(result, "review_task", "review", "focus_image_ids",
                      JoinTextItems(task_review.focus_image_ids, ";"));
  AddNamedResultField(result, "review_task", "review", "metric_summary", task_review.metric_summary);
  AddNamedResultField(result, "review_task", "review", "stage_summary", task_review.stage_summary);
  AddNamedResultField(result, "review_task", "review", "current_conclusion",
                      task_review.current_conclusion);
  AddNamedResultField(result, "review_task", "review", "next_attention_points",
                      task_review.next_attention_points);
  AddNamedResultField(result, "review_task", "review", "status_distribution",
                      task_review.status_distribution);
  AddNamedResultField(result, "review_task", "review", "anomaly_type_distribution",
                      task_review.anomaly_type_distribution);
  AddNamedResultField(result, "review_task", "review", "baseline_compare_summary",
                      task_review.baseline_compare_summary);
  AddNamedResultField(result, "review_task", "review", "review_required_count",
                      std::to_string(task_review.review_required_count));
  AddNamedResultField(result, "review_task", "review", "top_metric_outliers",
                      JoinTextItems(task_review.top_metric_outliers, ";"));
  AddNamedResultField(result, "review_task", "review", "training_evidence_summary",
                      task_review.training_evidence_summary);
  AddNamedResultField(result, "review_task", "review", "artifact_bundle_refs",
                      JoinTextItems(task_review.artifact_bundle_refs, ";"));
  AddNamedResultField(result, "review_task", "review", "supporting_refs",
                      JoinTextItems(task_review.supporting_refs, ";"));
  AddNamedResultField(result, "review_task", "review", "observation_personality",
                      task_review.observation_personality);
  AddNamedResultField(result, "review_task", "review", "default_open_chain",
                      task_review.default_open_chain);
  AddNamedResultField(result, "review_task", "review", "evidence_focus_summary",
                      task_review.evidence_focus_summary);
  AddNamedResultField(result, "review_task", "review", "thread_handoff",
                      task_review.thread_handoff);
  AddNamedResultField(result, "review_task", "review", "task_family",
                      task_review.task_family);
  AddNamedResultField(result, "review_task", "review", "pipeline_family",
                      task_review.pipeline_family);
  AddNamedResultField(result, "review_task", "review", "model_family",
                      task_review.model_family);
  AddNamedResultField(result, "review_task", "review", "scenario_family",
                      task_review.scenario_family);
  AddNamedResultField(result, "review_task", "review", "metric_summary_by_stage",
                      task_review.metric_summary_by_stage);
  AddNamedResultField(result, "review_task", "review", "visual_evidence_summary",
                      task_review.visual_evidence_summary);
  AddNamedResultField(result, "review_task", "review", "device_summary",
                      task_review.device_summary);
  AddNamedResultField(result, "review_task", "review", "business_eval_fields",
                      task_review.business_eval_fields);
  AddNamedResultField(result, "review_task", "review", "pipeline_link_trace",
                      task_review.pipeline_link_trace);
  AddNamedResultField(result, "review_task", "review", "sequence_family",
                      task_review.sequence_family);
  AddNamedResultField(result, "review_task", "review", "stage_transition_summary",
                      task_review.stage_transition_summary);
  AddNamedResultField(result, "review_task", "review", "stage_abnormal_summary",
                      task_review.stage_abnormal_summary);
  AddNamedResultField(result, "review_task", "review", "test_image_ref",
                      task_review.test_image_ref);
  AddNamedResultField(result, "review_task", "review", "visual_evidence_ref_set",
                      task_review.visual_evidence_ref_set);
  AddNamedResultField(result, "review_task", "review", "task_conclusion_ref",
                      task_review.task_conclusion_ref);
  AddNamedResultField(result, "review_task", "review", "anomaly_conclusion_ref",
                      task_review.anomaly_conclusion_ref);
  AddNamedResultField(result, "review_task", "review", "next_action_ref",
                      task_review.next_action_ref);
  AddNamedResultField(result, "review_task", "review", "element_ref_set",
                      task_review.element_ref_set);
  AddNamedResultField(result, "review_task", "review", "chain_ref_set",
                      task_review.chain_ref_set);
  AddNamedResultField(result, "review_task", "review", "stage_ref_set",
                      task_review.stage_ref_set);
  AddNamedResultField(result, "review_task", "review", "issue_entry_ref_set",
                      task_review.issue_entry_ref_set);
  AddNamedResultField(result, "review_task", "review", "recommended_image_ref",
                      task_review.recommended_image_ref);
  AddNamedResultField(result, "review_task", "review", "recommended_element_ref",
                      task_review.recommended_element_ref);
  AddNamedResultField(result, "review_task", "review", "recommended_chain_ref",
                      task_review.recommended_chain_ref);
  AddNamedResultField(result, "review_task", "review", "recommended_stage_ref",
                      task_review.recommended_stage_ref);
  AddNamedResultField(result, "review_task", "review", "issue_kind_hint",
                      task_review.issue_kind_hint);
  AddNamedResultField(result, "review_task", "review", "review_mode",
                      task_review.review_mode);
  AddNamedResultField(result, "review_task", "review", "default_decision_axis",
                      task_review.default_decision_axis);
  AddNamedResultField(result, "review_task", "review", "refresh_mode",
                      task_review.refresh_mode);
  AddNamedResultField(result, "review_task", "review", "changed_fields",
                      JoinTextItems(task_review.changed_fields, ";"));
  AddNamedResultField(result, "review_task", "review", "changed_element_ids",
                      JoinTextItems(task_review.changed_element_ids, ";"));
  AddNamedResultField(result, "review_task", "review", "changed_chain_keys",
                      JoinTextItems(task_review.changed_chain_keys, ";"));
  AddNamedResultField(result, "review_task", "review", "refresh_priority",
                      task_review.refresh_priority);
  AddNamedResultField(result, "review_task", "review", "tolerance_summary",
                      task_review.tolerance_summary);
  AddNamedResultField(result, "review_task", "review", "stability_summary",
                      task_review.stability_summary);
  AddNamedResultField(result, "review_task", "review", "element_type_summary",
                      task_review.element_type_summary);
  AddNamedResultField(result, "review_task", "review", "element_summary",
                      task_review.element_summary);
  AddNamedResultField(result, "review_task", "review", "element_chain_summary",
                      task_review.element_chain_summary);
  AddNamedResultField(result, "review_task", "review", "element_status_summary",
                      task_review.element_status_summary);
  AddNamedResultField(result, "review_task", "review", "candidate_status_summary",
                      task_review.candidate_status_summary);
  AddNamedResultField(result, "review_task", "review", "match_status_summary",
                      task_review.match_status_summary);
  AddNamedResultField(result, "review_task", "review", "manual_review_signal_summary",
                      task_review.manual_review_signal_summary);
  AddNamedResultField(result, "review_task", "review", "element_group_summary",
                      task_review.element_group_summary);
  AddNamedResultField(result, "review_task", "review", "element_findings",
                      task_review.element_findings);
  AddNamedResultField(result, "review_task", "review", "element_level_focus",
                      task_review.element_level_focus);
  AddNamedResultField(result, "review_task", "review", "focus_refresh_targets",
                      task_review.focus_refresh_targets);
  AddNamedResultField(result, "review_task", "review", "local_delta_targets",
                      task_review.local_delta_targets);
  AddNamedResultField(result, "review_task", "review", "grouped_element_preview",
                      task_review.grouped_element_preview);
  AddNamedResultField(result, "review_task", "review", "focus_element_preview",
                      task_review.focus_element_preview);
  AddNamedResultField(result, "review_task", "review", "delta_element_preview",
                      task_review.delta_element_preview);
  AddNamedResultField(result, "review_task", "review", "manual_review_targets",
                      manual_review_targets);
  AddNamedResultField(result, "review_task", "review", "roi_visual_evidence",
                      roi_visual_evidence);
  AddNamedResultField(result, "review_task", "review", "missing_element_summary",
                      task_review.missing_element_summary);
  AddNamedResultField(result, "review_task", "review", "drifted_element_summary",
                      task_review.drifted_element_summary);
  AddNamedResultField(result, "review_task", "review", "abnormal_element_summary",
                      task_review.abnormal_element_summary);
  AddNamedResultField(result, "review_task", "review", "upstream_unified_image_review_status",
                      unified_image_review_ready ? "ready" : "blocked");
  if (cximage_gui_contract)
  {
    AddNamedResultField(result, "review_task", "review", "raw_image_ref",
                        cximage_raw_image_ref);
    AddNamedResultField(result, "review_task", "review", "edge_image_ref",
                        cximage_edge_image_ref);
    AddNamedResultField(result, "review_task", "review", "element_relation_image_ref",
                        cximage_element_relation_image_ref);
    AddNamedResultField(result, "review_task", "review", "candidate_image_ref",
                        cximage_candidate_image_ref);
    AddNamedResultField(result, "review_task", "review", "match_image_ref",
                        cximage_match_image_ref);
    AddNamedResultField(result, "review_task", "review", "geometry_stage",
                        cximage_geometry_stage);
    AddNamedResultField(result, "review_task", "review", "candidate_stage",
                        cximage_candidate_stage);
    AddNamedResultField(result, "review_task", "review", "match_stage",
                        cximage_match_stage);
    AddNamedResultField(result, "review_task", "review", "problem_focus_image_ref",
                        cximage_problem_focus_image_ref);
    AddNamedResultField(result, "review_task", "review", "problem_focus_element_id",
                        cximage_problem_focus_element_id);
    AddNamedResultField(result, "review_task", "review", "problem_focus_chain_id",
                        cximage_problem_focus_chain_id);
    AddNamedResultField(result, "review_task", "review", "problem_issue_type",
                        cximage_problem_issue_type);
    AddNamedResultField(result, "review_task", "review", "single_image_geometry_conclusion",
                        cximage_single_image_geometry_conclusion);
    AddNamedResultField(result, "review_task", "review", "element_conclusion",
                        cximage_element_conclusion);
    AddNamedResultField(result, "review_task", "review", "match_conclusion",
                        cximage_match_conclusion);
    AddNamedResultField(result, "review_task", "review", "task_conclusion",
                        task_review.current_conclusion);
    AddNamedResultField(result, "review_task", "review", "next_step_suggestion",
                        task_review.next_attention_points);
    AddNamedResultField(result, "review_task", "review", "gui_chain_summary",
                        cximage_gui_chain_summary);
  }
  AddNamedResultObject(result,
                       "review_compare",
                       "review",
                       "UnifiedCompareSlice",
                       unified_image_review_ready ? "ready" : "blocked_by_unified_image_review",
                       std::string());
  AddNamedResultField(result, "review_compare", "review", "compare_id", compare_slice.compare_id);
  AddNamedResultField(result, "review_compare", "review", "compare_type", compare_slice.compare_type);
  AddNamedResultField(result, "review_compare", "review", "left_ref", compare_slice.left_ref);
  AddNamedResultField(result, "review_compare", "review", "right_ref", compare_slice.right_ref);
  AddNamedResultField(result, "review_compare", "review", "compare_dimensions",
                      JoinTextItems(compare_slice.compare_dimensions, ";"));
  AddNamedResultField(result, "review_compare", "review", "delta_summary", compare_slice.delta_summary);
  AddNamedResultField(result, "review_compare", "review", "risk_level", compare_slice.risk_level);
  AddNamedResultField(result, "review_compare", "review", "focus_recommendation",
                      compare_slice.focus_recommendation);
  AddNamedResultField(result, "review_compare", "review", "supporting_refs",
                      JoinTextItems(compare_slice.supporting_refs, ";"));
  AddNamedResultField(result, "review_compare", "review", "observation_personality",
                      compare_slice.observation_personality);
  AddNamedResultField(result, "review_compare", "review", "default_open_chain",
                      compare_slice.default_open_chain);
  AddNamedResultField(result, "review_compare", "review", "evidence_focus_summary",
                      compare_slice.evidence_focus_summary);
  AddNamedResultField(result, "review_compare", "review", "thread_handoff",
                      compare_slice.thread_handoff);
  AddNamedResultField(result, "review_compare", "review", "compare_view_mode",
                      compare_slice.compare_view_mode);
  AddNamedResultField(result, "review_compare", "review", "refresh_mode",
                      compare_slice.refresh_mode);
  AddNamedResultField(result, "review_compare", "review", "changed_fields",
                      JoinTextItems(compare_slice.changed_fields, ";"));
  AddNamedResultField(result, "review_compare", "review", "changed_element_ids",
                      JoinTextItems(compare_slice.changed_element_ids, ";"));
  AddNamedResultField(result, "review_compare", "review", "changed_chain_keys",
                      JoinTextItems(compare_slice.changed_chain_keys, ";"));
  AddNamedResultField(result, "review_compare", "review", "refresh_priority",
                      compare_slice.refresh_priority);
  AddNamedResultField(result, "review_compare", "review", "threshold_summary",
                      compare_slice.threshold_summary);
  AddNamedResultField(result, "review_compare", "review", "risk_note",
                      compare_slice.risk_note);
  AddNamedResultField(result, "review_compare", "review", "element_summary",
                      compare_slice.element_summary);
  AddNamedResultField(result, "review_compare", "review", "element_chain_summary",
                      compare_slice.element_chain_summary);
  AddNamedResultField(result, "review_compare", "review", "element_status_summary",
                      compare_slice.element_status_summary);
  AddNamedResultField(result, "review_compare", "review", "candidate_status_summary",
                      compare_slice.candidate_status_summary);
  AddNamedResultField(result, "review_compare", "review", "match_status_summary",
                      compare_slice.match_status_summary);
  AddNamedResultField(result, "review_compare", "review", "manual_review_signal_summary",
                      compare_slice.manual_review_signal_summary);
  AddNamedResultField(result, "review_compare", "review", "element_group_summary",
                      compare_slice.element_group_summary);
  AddNamedResultField(result, "review_compare", "review", "element_findings",
                      compare_slice.element_findings);
  AddNamedResultField(result, "review_compare", "review", "element_level_focus",
                      compare_slice.element_level_focus);
  AddNamedResultField(result, "review_compare", "review", "focus_refresh_targets",
                      compare_slice.focus_refresh_targets);
  AddNamedResultField(result, "review_compare", "review", "local_delta_targets",
                      compare_slice.local_delta_targets);
  AddNamedResultField(result, "review_compare", "review", "grouped_element_preview",
                      compare_slice.grouped_element_preview);
  AddNamedResultField(result, "review_compare", "review", "focus_element_preview",
                      compare_slice.focus_element_preview);
  AddNamedResultField(result, "review_compare", "review", "delta_element_preview",
                      compare_slice.delta_element_preview);
  AddNamedResultField(result, "review_compare", "review", "manual_review_targets",
                      manual_review_targets);
  AddNamedResultField(result, "review_compare", "review", "roi_visual_evidence",
                      roi_visual_evidence);
  AddNamedResultField(result, "review_compare", "review", "element_level_diff",
                      compare_slice.element_level_diff);
  AddNamedResultField(result, "review_compare", "review", "semantic_diff",
                      compare_slice.semantic_diff);
  AddNamedResultField(result, "review_compare", "review", "structure_diff",
                      compare_slice.structure_diff);
  AddNamedResultField(result, "review_compare", "review", "upstream_unified_image_review_status",
                      unified_image_review_ready ? "ready" : "blocked");

  AddNamedResultObject(result,
                       "review_anomaly",
                       "review",
                       "UnifiedAnomalyFocusBundle",
                       !unified_image_review_ready ? "blocked_by_unified_image_review" :
                       (anomaly_bundle.abnormal_image_ids.empty() ? "normal" : "focused"),
                       std::string());
  AddNamedResultField(result, "review_anomaly", "review", "source_thread",
                      anomaly_bundle.source_thread);
  AddNamedResultField(result, "review_anomaly", "review", "task_id", anomaly_bundle.task_id);
  AddNamedResultField(result, "review_anomaly", "review", "batch_id", anomaly_bundle.batch_id);
  AddNamedResultField(result, "review_anomaly", "review", "abnormal_image_ids",
                      JoinTextItems(anomaly_bundle.abnormal_image_ids, ";"));
  AddNamedResultField(result, "review_anomaly", "review", "anomaly_type_summary",
                      anomaly_bundle.anomaly_type_summary);
  AddNamedResultField(result, "review_anomaly", "review", "top_focus_objects",
                      JoinTextItems(anomaly_bundle.top_focus_objects, ";"));
  AddNamedResultField(result, "review_anomaly", "review", "analysis_suggestions",
                      JoinTextItems(anomaly_bundle.analysis_suggestions, ";"));
  AddNamedResultField(result, "review_anomaly", "review", "risk_level",
                      anomaly_bundle.risk_level);
  AddNamedResultField(result, "review_anomaly", "review", "supporting_refs",
                      JoinTextItems(anomaly_bundle.supporting_refs, ";"));
  AddNamedResultField(result, "review_anomaly", "review", "observation_personality",
                      anomaly_bundle.observation_personality);
  AddNamedResultField(result, "review_anomaly", "review", "default_open_chain",
                      anomaly_bundle.default_open_chain);
  AddNamedResultField(result, "review_anomaly", "review", "evidence_focus_summary",
                      anomaly_bundle.evidence_focus_summary);
  AddNamedResultField(result, "review_anomaly", "review", "thread_handoff",
                      anomaly_bundle.thread_handoff);
  AddNamedResultField(result, "review_anomaly", "review", "anomaly_axis",
                      anomaly_bundle.anomaly_axis);
  AddNamedResultField(result, "review_anomaly", "review", "refresh_mode",
                      anomaly_bundle.refresh_mode);
  AddNamedResultField(result, "review_anomaly", "review", "changed_fields",
                      JoinTextItems(anomaly_bundle.changed_fields, ";"));
  AddNamedResultField(result, "review_anomaly", "review", "changed_element_ids",
                      JoinTextItems(anomaly_bundle.changed_element_ids, ";"));
  AddNamedResultField(result, "review_anomaly", "review", "changed_chain_keys",
                      JoinTextItems(anomaly_bundle.changed_chain_keys, ";"));
  AddNamedResultField(result, "review_anomaly", "review", "refresh_priority",
                      anomaly_bundle.refresh_priority);
  AddNamedResultField(result, "review_anomaly", "review", "stability_summary",
                      anomaly_bundle.stability_summary);
  AddNamedResultField(result, "review_anomaly", "review", "risk_note",
                      anomaly_bundle.risk_note);
  AddNamedResultField(result, "review_anomaly", "review", "anomaly_element_ids",
                      JoinTextItems(anomaly_bundle.anomaly_element_ids, ";"));
  AddNamedResultField(result, "review_anomaly", "review", "anomaly_element_types",
                      JoinTextItems(anomaly_bundle.anomaly_element_types, ";"));
  AddNamedResultField(result, "review_anomaly", "review", "element_summary",
                      anomaly_bundle.element_summary);
  AddNamedResultField(result, "review_anomaly", "review", "element_chain_summary",
                      anomaly_bundle.element_chain_summary);
  AddNamedResultField(result, "review_anomaly", "review", "element_status_summary",
                      anomaly_bundle.element_status_summary);
  AddNamedResultField(result, "review_anomaly", "review", "candidate_status_summary",
                      anomaly_bundle.candidate_status_summary);
  AddNamedResultField(result, "review_anomaly", "review", "match_status_summary",
                      anomaly_bundle.match_status_summary);
  AddNamedResultField(result, "review_anomaly", "review", "manual_review_signal_summary",
                      anomaly_bundle.manual_review_signal_summary);
  AddNamedResultField(result, "review_anomaly", "review", "element_group_summary",
                      anomaly_bundle.element_group_summary);
  AddNamedResultField(result, "review_anomaly", "review", "element_findings",
                      anomaly_bundle.element_findings);
  AddNamedResultField(result, "review_anomaly", "review", "element_level_focus",
                      anomaly_bundle.element_level_focus);
  AddNamedResultField(result, "review_anomaly", "review", "focus_refresh_targets",
                      anomaly_bundle.focus_refresh_targets);
  AddNamedResultField(result, "review_anomaly", "review", "local_delta_targets",
                      anomaly_bundle.local_delta_targets);
  AddNamedResultField(result, "review_anomaly", "review", "grouped_element_preview",
                      anomaly_bundle.grouped_element_preview);
  AddNamedResultField(result, "review_anomaly", "review", "focus_element_preview",
                      anomaly_bundle.focus_element_preview);
  AddNamedResultField(result, "review_anomaly", "review", "delta_element_preview",
                      anomaly_bundle.delta_element_preview);
  AddNamedResultField(result, "review_anomaly", "review", "manual_review_targets",
                      manual_review_targets);
  AddNamedResultField(result, "review_anomaly", "review", "roi_visual_evidence",
                      roi_visual_evidence);
  AddNamedResultField(result, "review_anomaly", "review", "anomaly_focus_reason",
                      anomaly_bundle.anomaly_focus_reason);
  AddNamedResultField(result, "review_anomaly", "review", "upstream_unified_image_review_status",
                      unified_image_review_ready ? "ready" : "blocked");
  if (cximage_gui_contract)
  {
    AddNamedResultField(result, "review_anomaly", "review", "raw_image_ref",
                        cximage_raw_image_ref);
    AddNamedResultField(result, "review_anomaly", "review", "edge_image_ref",
                        cximage_edge_image_ref);
    AddNamedResultField(result, "review_anomaly", "review", "element_relation_image_ref",
                        cximage_element_relation_image_ref);
    AddNamedResultField(result, "review_anomaly", "review", "candidate_image_ref",
                        cximage_candidate_image_ref);
    AddNamedResultField(result, "review_anomaly", "review", "match_image_ref",
                        cximage_match_image_ref);
    AddNamedResultField(result, "review_anomaly", "review", "geometry_stage",
                        cximage_geometry_stage);
    AddNamedResultField(result, "review_anomaly", "review", "candidate_stage",
                        cximage_candidate_stage);
    AddNamedResultField(result, "review_anomaly", "review", "match_stage",
                        cximage_match_stage);
    AddNamedResultField(result, "review_anomaly", "review", "problem_focus_image_ref",
                        cximage_problem_focus_image_ref);
    AddNamedResultField(result, "review_anomaly", "review", "problem_focus_element_id",
                        cximage_problem_focus_element_id);
    AddNamedResultField(result, "review_anomaly", "review", "problem_focus_chain_id",
                        cximage_problem_focus_chain_id);
    AddNamedResultField(result, "review_anomaly", "review", "problem_issue_type",
                        cximage_problem_issue_type);
    AddNamedResultField(result, "review_anomaly", "review", "single_image_geometry_conclusion",
                        cximage_single_image_geometry_conclusion);
    AddNamedResultField(result, "review_anomaly", "review", "element_conclusion",
                        cximage_element_conclusion);
    AddNamedResultField(result, "review_anomaly", "review", "match_conclusion",
                        cximage_match_conclusion);
    AddNamedResultField(result, "review_anomaly", "review", "task_conclusion",
                        task_review.current_conclusion);
    AddNamedResultField(result, "review_anomaly", "review", "next_step_suggestion",
                        task_review.next_attention_points);
    AddNamedResultField(result, "review_anomaly", "review", "gui_chain_summary",
                        cximage_gui_chain_summary);
  }
  AddNamedResultObject(result,
                       "review_board",
                       "review",
                       "Phase0ReviewBoardState",
                       result.unified_image_reviews.empty() ? "minimal" :
                       (unified_image_review_ready ? "ready" : "blocked_by_unified_image_review"),
                       result.failure_phase);
  AddNamedResultField(result, "review_board", "review", "image_detail_board",
                      result.unified_image_reviews.empty() ? "missing" :
                      (unified_image_review_ready ? "ready" : "partial"));
  AddNamedResultField(result, "review_board", "review", "image_detail_board_ref",
                      result.unified_image_reviews.empty() ? std::string()
                        : ((result.unified_image_reviews.front().batch_id.empty()
                              ? std::string("review.batch")
                              : result.unified_image_reviews.front().batch_id) +
                           ".image_detail_board.1"));
  AddNamedResultField(result, "review_board", "review", "task_summary_board",
                      result.unified_task_reviews.empty() ? "missing" :
                      (unified_image_review_ready ? "ready" : "blocked"));
  AddNamedResultField(result, "review_board", "review", "task_summary_board_ref",
                      result.unified_task_reviews.empty() ? std::string()
                        : ((result.unified_task_reviews.front().batch_id.empty()
                              ? std::string("review.batch")
                              : result.unified_task_reviews.front().batch_id) +
                           ".task_summary_board.1"));
  AddNamedResultField(result, "review_board", "review", "cross_thread_compare_board",
                      result.unified_compare_slices.empty() ? "missing" :
                      (unified_image_review_ready ? "ready" : "blocked"));
  AddNamedResultField(result, "review_board", "review", "cross_thread_compare_board_ref",
                      result.unified_compare_slices.empty() ? std::string()
                        : (!result.unified_compare_slices.front().compare_id.empty()
                             ? result.unified_compare_slices.front().compare_id
                             : std::string("review.compare.cross_thread_compare_board")));
  AddNamedResultField(result, "review_board", "review", "anomaly_focus_board",
                      result.unified_anomaly_focus_bundles.empty() ? "missing" :
                      (unified_image_review_ready ? "ready" : "blocked"));
  AddNamedResultField(result, "review_board", "review", "anomaly_focus_board_ref",
                      result.unified_anomaly_focus_bundles.empty() ? std::string()
                        : ((result.unified_anomaly_focus_bundles.front().batch_id.empty()
                              ? std::string("review.batch")
                              : result.unified_anomaly_focus_bundles.front().batch_id) +
                           ".anomaly_focus_board.1"));
  AddNamedResultField(result, "review_board", "review", "evidence_surface_status",
                      (unified_image_review_ready &&
                       !result.unified_image_reviews.empty() &&
                       !result.unified_task_reviews.empty() &&
                       !result.unified_compare_slices.empty() &&
                       !result.unified_anomaly_focus_bundles.empty())
                        ? "ready"
                        : (unified_image_review_ready ? "partial"
                                                      : "blocked_by_unified_image_review"));
  AddNamedResultField(result, "review_board", "review", "unified_image_review_contract",
                      unified_image_review_required ? "required" : "optional");
  AddNamedResultField(result, "review_board", "review", "unified_image_review_status",
                      unified_image_review_missing_fields.empty() ? "ready" : "partial");
  AddNamedResultField(result, "review_board", "review", "unified_image_review_missing_fields",
                      JoinTextItems(unified_image_review_missing_fields, ";"));
  AddNamedResultField(result, "review_board", "review", "review_surface_usability",
                      unified_image_review_ready ? "usable"
                                                 : "blocked_by_unified_image_review");
  AddNamedResultField(result, "review_board", "review", "supported_refresh_modes",
                      "snapshot;delta;summary;focus_delta;focus_summary");
  AddNamedResultField(result, "review_board", "review", "snapshot_refresh_ref",
                      "review_image");
  AddNamedResultField(result, "review_board", "review", "summary_refresh_ref",
                      "review_task");
  AddNamedResultField(result, "review_board", "review", "delta_refresh_refs",
                      "review_compare;review_anomaly");
  AddNamedResultField(result, "review_board", "review", "focus_refresh_targets",
                      image_review.focus_refresh_targets);
  AddNamedResultField(result, "review_board", "review", "local_delta_targets",
                      image_review.local_delta_targets);
  AddNamedResultField(result, "review_board", "review", "grouped_element_preview",
                      image_review.grouped_element_preview);
  AddNamedResultField(result, "review_board", "review", "focus_element_preview",
                      image_review.focus_element_preview);
  AddNamedResultField(result, "review_board", "review", "delta_element_preview",
                      image_review.delta_element_preview);
  AddNamedResultField(result, "review_board", "review", "image_refresh_mode",
                      image_review.refresh_mode);
  AddNamedResultField(result, "review_board", "review", "task_refresh_mode",
                      task_review.refresh_mode);
  AddNamedResultField(result, "review_board", "review", "compare_refresh_mode",
                      compare_slice.refresh_mode);
  AddNamedResultField(result, "review_board", "review", "anomaly_refresh_mode",
                      anomaly_bundle.refresh_mode);
  AddNamedResultField(result, "review_board", "review", "changed_fields",
                      JoinTextItems(review_board_changed_fields, ";"));
  AddNamedResultField(result, "review_board", "review", "changed_element_ids",
                      JoinTextItems(review_board_changed_element_ids, ";"));
  AddNamedResultField(result, "review_board", "review", "changed_chain_keys",
                      JoinTextItems(review_board_changed_chain_keys, ";"));
  AddNamedResultField(result, "review_board", "review", "refresh_priority",
                      review_board_refresh_priority);
  AddNamedResultField(result, "review_board", "review", "source_thread",
                      image_review.source_thread);
  AddNamedResultField(result, "review_board", "review", "primary_review_ref",
                      image_review.primary_visual_ref);
}

void RefreshExecutionMultimodalSlices(CxScriptExecutionResult &result)
{
  result.multimodal_slices.clear();
  result.operation_atoms.clear();

  const std::string semantic_operation_base_ref =
    BuildSemanticOperationBaseRef(result) + ".semantic_operation";
  const std::string semantic_script_module_ref =
    BuildSemanticOperationScriptModuleRef(result);
  const std::string semantic_select_targets =
    BuildSemanticOperationSelectTargets(result);
  const std::string semantic_script_action_entry =
    BuildSemanticOperationStageExecutionEntry(result);
  const std::string semantic_parameter_injection_entry =
    BuildSemanticOperationParameterInjectionEntry(result);
  const std::string semantic_stage_execution_entry =
    BuildSemanticOperationStageExecutionEntry(result);
  const std::string semantic_result_object_ref =
    BuildSemanticOperationResultObjectRef(result);
  const std::string semantic_replay_ref =
    BuildSemanticOperationReplayRef(result);
  const std::string semantic_view_ref = BuildSemanticOperationVisualRef(result);
  const std::string semantic_element_targets =
    BuildSemanticOperationElementTargets(result);
  const std::string semantic_judge_targets =
    BuildSemanticOperationJudgeTargets(result);
  const std::string semantic_atom_ref_set =
    semantic_operation_base_ref + ".select;" +
    semantic_operation_base_ref + ".change;" +
    semantic_operation_base_ref + ".run;" +
    semantic_operation_base_ref + ".view;" +
    semantic_operation_base_ref + ".judge;" +
    semantic_operation_base_ref + ".record";

  const std::string semantic_record_targets =
    BuildSemanticOperationRecordTargets(result, semantic_atom_ref_set);

  AddNamedResultObject(result,
                       "semantic_operation_contract",
                       "operation",
                       "SemanticOperationContract",
                       semantic_script_module_ref.empty() ? "partial" : "ready",
                       result.failure_phase);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "script_module_ref", semantic_script_module_ref);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "script_action_entry", semantic_script_action_entry);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "parameter_injection_entry", semantic_parameter_injection_entry);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "stage_execution_entry", semantic_stage_execution_entry);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "result_object_ref", semantic_result_object_ref);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "replay_ref", semantic_replay_ref);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "operation_select_targets", semantic_select_targets);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "operation_modify_targets", semantic_parameter_injection_entry);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "operation_run_targets", semantic_stage_execution_entry);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "operation_view_targets", semantic_view_ref);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "operation_judge_targets", semantic_judge_targets);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "operation_record_targets", semantic_record_targets);
  AddNamedResultField(result, "semantic_operation_contract", "operation",
                      "semantic_operation_summary",
                      "select/change/run/view/judge/record bound to public cxscript entry, unified review refs and ViewController-style semantic actions");
  MultimodalSlice semantic_slice;
  semantic_slice.slice_id =
    result.case_name.empty() ? "cxscript.code_semantic_slice_v1"
                             : result.case_name + ".code_semantic_slice_v1";
  semantic_slice.source_ref = result.script_path.empty() ? result.script_name : result.script_path;
  semantic_slice.source_hash =
    BuildPseudoSourceHash(semantic_slice.source_ref + "|" + result.layer + "|" + result.module);
  semantic_slice.modality = "code";
  semantic_slice.analysis_kind = "code_semantic";
  semantic_slice.result_ref = result.task_id;
  semantic_slice.evidence_ref = result.script_path;
  semantic_slice.log_path = result.script_path;
  semantic_slice.model_ref = result.result_object;
  semantic_slice.confidence = result.success ? 1.0 : 0.5;
  semantic_slice.next_action = result.success ? "consume operation atoms"
                                              : "inspect execution summary and error message";
  semantic_slice.tags.push_back("code_semantic_slice_v1");

  for (size_t i = 0; i < result.declared_types.size(); ++i)
  {
    MultimodalSliceObject object;
    object.object_id = result.declared_types[i].name;
    object.object_kind = "declared_type";
    object.geometry_ref = result.layer;
    object.semantic_label = "cxscript_type";
    object.summary = "declared type participating in script execution";
    object.confidence = 1.0;
    semantic_slice.objects.push_back(object);
  }

  for (size_t i = 0; i < result.named_results.size(); ++i)
  {
    MultimodalSliceObject object;
    object.object_id = result.named_results[i].result_name;
    object.object_kind = "named_result";
    object.geometry_ref = result.named_results[i].stage_name;
    object.semantic_label = "result_object";
    object.summary = result.named_results[i].object_name;
    object.confidence = 1.0;
    semantic_slice.objects.push_back(object);
  }

  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    MultimodalSliceRelation relation;
    relation.relation_kind = "result_field";
    relation.source_object_id = result.result_fields[i].result_name;
    relation.target_object_id = result.result_fields[i].field_name;
    relation.summary = result.result_fields[i].value;
    semantic_slice.relations.push_back(relation);
  }
  result.multimodal_slices.push_back(semantic_slice);

  MultimodalSlice operation_slice;
  operation_slice.slice_id =
    result.case_name.empty() ? "cxscript.operation_chain_v1"
                             : result.case_name + ".operation_chain_v1";
  operation_slice.source_ref = semantic_slice.source_ref;
  operation_slice.source_hash = BuildPseudoSourceHash(operation_slice.slice_id + semantic_slice.source_hash);
  operation_slice.modality = "operation_chain";
  operation_slice.analysis_kind = "operation_atom";
  operation_slice.result_ref = result.task_id;
  operation_slice.evidence_ref = result.summary;
  operation_slice.log_path = result.script_path;
  operation_slice.model_ref = result.result_object;
  operation_slice.confidence = result.success ? 1.0 : 0.5;
  operation_slice.next_action = "inspect stage atoms or replay lines";
  operation_slice.tags.push_back("operation_chain_v1");

  OperationAtom prepare_atom;
  prepare_atom.atom_id = operation_slice.slice_id + ".prepare";
  prepare_atom.stage = "prepare";
  prepare_atom.action_kind = "prepare_inputs";
  prepare_atom.input_ref = semantic_slice.source_ref;
  prepare_atom.output_ref = result.case_name + ".prepare";
  prepare_atom.status = result.variables.empty() ? "minimal" : "ok";
  prepare_atom.summary = "prepare stage derived from declared variables and setup statements";
  result.operation_atoms.push_back(prepare_atom);

  OperationAtom action_atom;
  action_atom.atom_id = operation_slice.slice_id + ".action";
  action_atom.stage = "action";
  action_atom.action_kind = result.bridge_enabled ? "bridge_execute" : "execute_flow";
  action_atom.input_ref = prepare_atom.output_ref;
  action_atom.output_ref = result.result_object;
  action_atom.status = result.success ? "ok" : "failed";
  action_atom.summary = result.summary.empty() ? "main execution atom derived from script runtime"
                                               : result.summary;
  result.operation_atoms.push_back(action_atom);

  OperationAtom check_atom;
  check_atom.atom_id = operation_slice.slice_id + ".check";
  check_atom.stage = "check";
  check_atom.action_kind = "result_check";
  check_atom.input_ref = result.result_object;
  check_atom.output_ref = std::to_string(result.result_fields.size()) + "_fields";
  check_atom.status = result.result_fields.empty() ? "minimal" : "ok";
  check_atom.summary = "check stage derived from named result field reads";
  result.operation_atoms.push_back(check_atom);

  OperationAtom report_atom;
  report_atom.atom_id = operation_slice.slice_id + ".report";
  report_atom.stage = "report";
  report_atom.action_kind = "publish_summary";
  report_atom.input_ref = result.summary;
  report_atom.output_ref = result.task_id;
  report_atom.status = result.summary.empty() ? "minimal" : "ok";
  report_atom.summary = "report stage derived from runtime summary and task id";
  result.operation_atoms.push_back(report_atom);
  PushSemanticOperationAtom(result,
                            semantic_operation_base_ref,
                            "select",
                            "select_script_module_case_image_mainline",
                            semantic_slice.source_ref,
                            semantic_select_targets,
                            semantic_select_targets.empty() ? "partial" : "ok",
                            "choose script module, task/case, test image and visual mainline");
  PushSemanticOperationAtom(result,
                            semantic_operation_base_ref,
                            "change",
                            "change_params_geometry_acquisition_runtime_args",
                            semantic_script_module_ref,
                            semantic_parameter_injection_entry,
                            semantic_parameter_injection_entry.empty() ? "partial" : "ok",
                            "change script parameters, geometry settings, image acquisition and runtime entry args");
  PushSemanticOperationAtom(result,
                            semantic_operation_base_ref,
                            "run",
                            "run_script_stage_chain_replay",
                            semantic_parameter_injection_entry,
                            semantic_stage_execution_entry,
                            result.success ? "ok" : "failed",
                            "run single script, segment flow, full chain or replay through cxparser_ext_cxscript_cli");
  PushSemanticOperationAtom(result,
                            semantic_operation_base_ref,
                            "view",
                            "view_image_element_chain_stage_conclusion",
                            semantic_stage_execution_entry,
                            semantic_view_ref,
                            semantic_view_ref.empty() ? "partial" : "ok",
                            "view input image, element layer, main chain, stage, conclusion and anomaly focus");
  PushSemanticOperationAtom(result,
                            semantic_operation_base_ref,
                            "judge",
                            "judge_normal_issue_kind_first_change",
                            semantic_element_targets,
                            semantic_judge_targets,
                            semantic_judge_targets.empty() ? "partial" : "ok",
                            "judge normal state, issue kind and the first change target");
  PushSemanticOperationAtom(result,
                            semantic_operation_base_ref,
                            "record",
                            "record_manual_judgement_issue_next_action_replay",
                            semantic_judge_targets,
                            semantic_replay_ref,
                            semantic_replay_ref.empty() ? "partial" : "ok",
                            "record manual judgement, issue entry, next action and replay refs");

  for (size_t i = 0; i < result.operation_atoms.size(); ++i)
  {
    MultimodalSliceObject object;
    object.object_id = result.operation_atoms[i].atom_id;
    object.object_kind = "operation_atom";
    object.geometry_ref = result.operation_atoms[i].stage;
    object.semantic_label = result.operation_atoms[i].action_kind;
    object.summary = result.operation_atoms[i].summary;
    object.confidence = result.operation_atoms[i].status == "ok" ? 1.0 : 0.5;
    operation_slice.objects.push_back(object);
  }
  result.multimodal_slices.push_back(operation_slice);

  MultimodalSlice phase0_contract_slice;
  phase0_contract_slice.slice_id =
    result.case_name.empty() ? "cxscript.phase0_contract_v1"
                             : result.case_name + ".phase0_contract_v1";
  phase0_contract_slice.source_ref = semantic_slice.source_ref;
  phase0_contract_slice.source_hash =
    BuildPseudoSourceHash(phase0_contract_slice.slice_id + semantic_slice.source_hash);
  phase0_contract_slice.modality = "phase0_contract";
  phase0_contract_slice.analysis_kind = "unified_object";
  phase0_contract_slice.result_ref = result.task_id;
  phase0_contract_slice.evidence_ref =
    FindNamedResultFieldValue(result, "phase0_contract", "object_gate_status");
  phase0_contract_slice.log_path = result.script_path;
  phase0_contract_slice.model_ref = result.result_object;
  phase0_contract_slice.confidence =
    phase0_contract_slice.evidence_ref == "ready" ? 1.0 : 0.75;
  phase0_contract_slice.next_action = "bind HTML and ImGui consumers to unified objects";
  phase0_contract_slice.tags.push_back("phase0_contract_v1");

  if (!result.task_contexts.empty())
  {
    MultimodalSliceObject object;
    object.object_id = "TaskContext";
    object.object_kind = "phase0_object";
    object.geometry_ref = result.task_contexts[0].stage;
    object.semantic_label = "task_context";
    object.summary = BuildTaskContextRef(result.task_contexts[0]);
    object.confidence = 1.0;
    phase0_contract_slice.objects.push_back(object);
  }
  if (!result.geometry_template_specs.empty())
  {
    MultimodalSliceObject object;
    object.object_id = "GeometryTemplateSpec";
    object.object_kind = "phase0_object";
    object.geometry_ref = result.geometry_template_specs[0].primary_geometry_semantic;
    object.semantic_label = "geometry_template";
    object.summary = result.geometry_template_specs[0].template_identity;
    object.confidence = 1.0;
    phase0_contract_slice.objects.push_back(object);
  }
  if (!result.image_acquisition_specs.empty())
  {
    MultimodalSliceObject object;
    object.object_id = "ImageAcquisitionSpec";
    object.object_kind = "phase0_object";
    object.geometry_ref = result.image_acquisition_specs[0].scope_type;
    object.semantic_label = "image_acquisition";
    object.summary = BuildImageAcquisitionRef(result.image_acquisition_specs[0]);
    object.confidence = 1.0;
    phase0_contract_slice.objects.push_back(object);
  }
  if (!result.training_inputs.empty())
  {
    MultimodalSliceObject object;
    object.object_id = "TrainingInput";
    object.object_kind = "phase0_object";
    object.geometry_ref = result.training_inputs[0].model_route;
    object.semantic_label = "training_input";
    object.summary = result.training_inputs[0].dataset_ref;
    object.confidence = 1.0;
    phase0_contract_slice.objects.push_back(object);
  }
  if (!result.run_inputs.empty())
  {
    MultimodalSliceObject object;
    object.object_id = "RunInput";
    object.object_kind = "phase0_object";
    object.geometry_ref = result.run_inputs[0].model_route;
    object.semantic_label = "run_input";
    object.summary = result.run_inputs[0].input_image_ref;
    object.confidence = 1.0;
    phase0_contract_slice.objects.push_back(object);
  }
  if (!result.review_decisions.empty())
  {
    MultimodalSliceObject object;
    object.object_id = "ReviewDecision";
    object.object_kind = "phase0_object";
    object.geometry_ref = result.review_decisions[0].review_status;
    object.semantic_label = "review_decision";
    object.summary = result.review_decisions[0].reviewer_action;
    object.confidence = result.review_decisions[0].review_status == "accepted" ? 1.0 : 0.75;
    phase0_contract_slice.objects.push_back(object);
  }
  if (!result.flowback_actions.empty())
  {
    MultimodalSliceObject object;
    object.object_id = "FlowbackAction";
    object.object_kind = "phase0_object";
    object.geometry_ref = result.flowback_actions[0].flowback_type;
    object.semantic_label = "flowback_action";
    object.summary = result.flowback_actions[0].next_target;
    object.confidence = 1.0;
    phase0_contract_slice.objects.push_back(object);
  }

  if (!result.task_contexts.empty() && !result.geometry_template_specs.empty())
  {
    MultimodalSliceRelation relation;
    relation.relation_kind = "task_uses_template";
    relation.source_object_id = "TaskContext";
    relation.target_object_id = "GeometryTemplateSpec";
    relation.summary = result.geometry_template_specs[0].review_priority;
    phase0_contract_slice.relations.push_back(relation);
  }
  if (!result.task_contexts.empty() && !result.image_acquisition_specs.empty())
  {
    MultimodalSliceRelation relation;
    relation.relation_kind = "task_uses_acquisition";
    relation.source_object_id = "TaskContext";
    relation.target_object_id = "ImageAcquisitionSpec";
    relation.summary = result.image_acquisition_specs[0].execution_mode;
    phase0_contract_slice.relations.push_back(relation);
  }
  if (!result.review_decisions.empty() && !result.flowback_actions.empty())
  {
    MultimodalSliceRelation relation;
    relation.relation_kind = "review_triggers_flowback";
    relation.source_object_id = "ReviewDecision";
    relation.target_object_id = "FlowbackAction";
    relation.summary = result.flowback_actions[0].flowback_type;
    phase0_contract_slice.relations.push_back(relation);
  }
  result.multimodal_slices.push_back(phase0_contract_slice);
  AppendReviewBoardSlices(result);
}

std::string ResolveReadResultPath(const CxScriptExecutionResult &result, const std::string &path)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    const CxScriptNamedResultField &field = result.result_fields[i];
    if (field.result_name + "." + field.field_name == path ||
        field.stage_name + "." + field.field_name == path)
      return field.value;
  }

  for (size_t i = 0; i < result.named_results.size(); ++i)
  {
    const CxScriptNamedResultObject &item = result.named_results[i];
    if (item.result_name == path || item.stage_name == path)
      return item.object_name;
    if (item.result_name + ".status" == path || item.stage_name + ".status" == path)
      return item.status;
    if (item.result_name + ".failure_stage" == path || item.stage_name + ".failure_stage" == path)
      return item.failure_stage;
    if (item.result_name + ".object" == path || item.stage_name + ".object" == path)
      return item.object_name;
  }

  return std::string();
}

















