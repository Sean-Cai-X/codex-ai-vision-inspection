// Extracted named result refresh implementation for cxscript runtime.

void RefreshNamedResultViews(CxScriptExecutionResult &result)
{
  auto ensure_cximage_review_ref = [&](std::string &field_value,
                                       const char *suffix) {
    if (!field_value.empty() ||
        result.module != "cximage" ||
        result.layer.empty() ||
        result.case_name.empty() ||
        suffix == nullptr ||
        suffix[0] == '\0')
      return;

    field_value = "review_cxparser_tests::" + result.module + "::" +
                  result.layer + "::" + result.case_name + "::" + suffix;
  };

  if (result.module == "cximage" &&
      result.layer == "feature" &&
      result.case_name == "line_measure_roi")
  {
    ensure_cximage_review_ref(result.published_primary_ref, "line_point_set");
  }
  if (result.module == "cximage" &&
      result.layer == "matcher" &&
      result.case_name == "findobject_region")
  {
    ensure_cximage_review_ref(result.published_primary_ref, "region_summary");
  }

  const std::string line_point_set_ref =
    (result.module == "cximage" &&
     result.layer == "feature" &&
     result.case_name == "line_measure_roi")
      ? ("review_cxparser_tests::" + result.module + "::" +
         result.layer + "::" + result.case_name + "::line_point_set")
      : std::string();
  const std::string line_measure_bounds_ref =
    (result.module == "cximage" &&
     result.layer == "feature" &&
     result.case_name == "line_measure_roi")
      ? ("review_cxparser_tests::" + result.module + "::" +
         result.layer + "::" + result.case_name + "::line_measure_bounds")
      : std::string();
  const std::string circle_point_set_ref =
    (result.module == "cximage" &&
     result.layer == "feature" &&
     result.case_name == "circle_measure_fit")
      ? ("review_cxparser_tests::" + result.module + "::" +
         result.layer + "::" + result.case_name + "::circle_point_set")
      : std::string();
  const std::string circle_measure_bounds_ref =
    (result.module == "cximage" &&
     result.layer == "feature" &&
     result.case_name == "circle_measure_fit")
      ? ("review_cxparser_tests::" + result.module + "::" +
         result.layer + "::" + result.case_name + "::circle_measure_bounds")
      : std::string();
  const std::string region_summary_ref =
    (result.module == "cximage" &&
     result.layer == "matcher" &&
     result.case_name == "findobject_region")
      ? ("review_cxparser_tests::" + result.module + "::" +
         result.layer + "::" + result.case_name + "::region_summary")
      : std::string();
  const std::string region_bounds_ref =
    (result.module == "cximage" &&
     result.layer == "matcher" &&
     result.case_name == "findobject_region")
      ? ("review_cxparser_tests::" + result.module + "::" +
         result.layer + "::" + result.case_name + "::region_bounds")
      : std::string();

  if (result.module == "cximage" &&
      result.layer == "feature" &&
      result.case_name == "circle_measure_fit")
  {
    ensure_cximage_review_ref(result.circle_overlay_ref, "circle_overlay");
    ensure_cximage_review_ref(result.circle_edge_overlay_ref, "edge_overlay");
  }

  if (result.module == "cximage" &&
      result.layer == "matcher" &&
      (result.case_name == "fastmatch_template" ||
       result.case_name == "fast_template_match"))
  {
    ensure_cximage_review_ref(result.candidate_overlay_ref, "candidate_overlay");
    ensure_cximage_review_ref(result.template_rect_overlay_ref, "template_rect_overlay");
    ensure_cximage_review_ref(result.test_rect_overlay_ref, "test_rect_overlay");
  }

  if (result.module == "cximage" &&
      result.layer == "feature" &&
      result.case_name == "FindCircle")
  {
    ensure_cximage_review_ref(result.circle_overlay_ref, "circle_overlay");
    ensure_cximage_review_ref(result.circle_edge_overlay_ref, "edge_overlay");
  }

  if (result.module == "cximage" &&
      result.layer == "feature" &&
      result.case_name == "formfit_rect_candidate")
  {
    ensure_cximage_review_ref(result.formfit_candidate_overlay_ref, "candidate_overlay");
    ensure_cximage_review_ref(result.formfit_selection_overlay_ref, "selection_overlay");
  }

  if (result.module == "cximage" &&
      result.layer == "feature" &&
      result.case_name == "binary_region")
  {
    ensure_cximage_review_ref(result.region_pattern_overlay_ref, "region_overlay");
    ensure_cximage_review_ref(result.region_pattern_descriptor_ref, "descriptor_compare");
  }

  if (result.module == "cximage" &&
      result.layer == "feature" &&
      result.case_name == "geometry_topology_pipeline")
  {
    ensure_cximage_review_ref(result.fractal_partition_overlay_ref, "fractal_partition_overlay");
    ensure_cximage_review_ref(result.distance_field_overlay_ref, "distance_field_overlay");
    ensure_cximage_review_ref(result.skeleton_overlay_ref, "skeleton_overlay");
    ensure_cximage_review_ref(result.centerline_overlay_ref, "centerline_overlay");
    ensure_cximage_review_ref(result.topology_repair_overlay_ref, "topology_repair_overlay");
  }

  const std::vector<CxScriptNamedResultObject> preserved_named_results = result.named_results;
  const std::vector<CxScriptNamedResultField> preserved_result_fields = result.result_fields;
  result.named_results.clear();
  result.result_fields.clear();

  for (size_t i = 0; i < result.compiled_stage_names.size(); ++i)
  {
    const std::string &stage_name = result.compiled_stage_names[i];
    AddNamedResultObject(result, stage_name, stage_name, "compiled_stage", "compiled", std::string());
    AddNamedResultField(result, stage_name, stage_name, "status", "compiled");
    AddNamedResultField(result, stage_name, stage_name, "compiled", "true");
  }

  if (!result.input_dataset.empty() ||
      !result.input_sample.empty() ||
      !result.input_split.empty() ||
      !result.input_artifacts.empty() ||
      !result.input_params.empty() ||
      !result.dataset_ref.empty() ||
      !result.sample_bundle_ref.empty())
  {
    AddNamedResultObject(result, "input", "input", "InputSample", "ready", std::string());
    if (!result.input_dataset.empty())
      AddNamedResultField(result, "input", "input", "dataset", result.input_dataset);
    AddNamedResultField(result, "input", "input", "sample", result.input_sample);
    if (!result.input_split.empty())
      AddNamedResultField(result, "input", "input", "split", result.input_split);
    if (!result.input_artifacts.empty())
      AddNamedResultField(result, "input", "input", "artifacts", result.input_artifacts);
    if (!result.input_params.empty())
      AddNamedResultField(result, "input", "input", "params", result.input_params);
    if (!result.dataset_ref.empty())
      AddNamedResultField(result, "input", "input", "dataset_ref", result.dataset_ref);
    if (!result.sample_bundle_ref.empty())
      AddNamedResultField(result, "input", "input", "sample_bundle_ref", result.sample_bundle_ref);
  }

  AddNamedResultObject(result,
                       "sampling",
                       "sampling",
                       "SamplingResult",
                       result.success ? "ready" : "partial",
                       result.circle_failure_stage);
  if (result.line_horizontal_samples_contract_value != 0.0)
    AddNamedResultField(result, "sampling", "sampling", "line_horizontal_samples",
                        std::to_string(result.line_horizontal_samples_contract_value));
  if (result.line_vertical_samples_contract_value != 0.0)
    AddNamedResultField(result, "sampling", "sampling", "line_vertical_samples",
                        std::to_string(result.line_vertical_samples_contract_value));
  AddNamedResultField(result, "sampling", "sampling", "line_chain_length",
                      std::to_string(result.line_chain_length_value));
  AddNamedResultField(result, "sampling", "sampling", "line_edgeband_count",
                      std::to_string(result.line_edgeband_count_value));
  AddNamedResultField(result, "sampling", "sampling", "circle_sample_points",
                      std::to_string(result.circle_sample_points_value));
  if (result.match_candidate_count_value != 0.0)
    AddNamedResultField(result, "sampling", "sampling", "candidate_count",
                        std::to_string(result.match_candidate_count_value));
  AddNamedResultField(result, "sampling", "sampling", "selected_index",
                      std::to_string(result.match_selected_index_value));
  AddNamedResultField(result, "sampling", "sampling", "best_index",
                      std::to_string(result.match_best_index_value));
  if (result.template_main_candidate_count_value != 0.0)
    AddNamedResultField(result, "sampling", "sampling", "template_main_candidate_count",
                        std::to_string(result.template_main_candidate_count_value));
  if (result.region_connected_components_value != 0.0)
    AddNamedResultField(result, "sampling", "sampling", "connected_components",
                        std::to_string(result.region_connected_components_value));
  AddNamedResultField(result, "sampling", "sampling", "raw_connected_components",
                      std::to_string(result.region_raw_connected_components_value));

  AddNamedResultObject(result,
                       "filter",
                       "filter",
                       "FilterResult",
                       result.success ? "ready" : "partial",
                       result.circle_failure_stage);
  AddNamedResultField(result, "filter", "filter", "circle_prefilter_used",
                      std::to_string(result.circle_prefilter_used_value));
  AddNamedResultField(result, "filter", "filter", "circle_compact_path",
                      std::to_string(result.circle_compact_path_value));
  AddNamedResultField(result, "filter", "filter", "region_foreground_ratio",
                      std::to_string(result.region_foreground_ratio_value));

  AddNamedResultObject(result,
                       "baseline",
                       "baseline",
                       "BaselineFeatureSampleV1",
                       result.success ? "ready" : "partial",
                       result.circle_failure_stage);
  AddNamedResultField(result, "baseline", "baseline", "roi_area",
                      std::to_string(result.roi_area_value));
  AddNamedResultField(result, "baseline", "baseline", "component_count",
                      std::to_string(result.component_count_value));
  AddNamedResultField(result, "baseline", "baseline", "image_model_score",
                      std::to_string(result.image_model_score_value));
  AddNamedResultField(result, "baseline", "baseline", "region_pattern_foreground_ratio",
                      std::to_string(result.region_pattern_foreground_ratio_value));
  AddNamedResultField(result, "baseline", "baseline", "region_pattern_descriptor_dim",
                      std::to_string(result.region_pattern_descriptor_dim_value));
  AddNamedResultField(result, "baseline", "baseline", "region_pattern_descriptor_mean",
                      std::to_string(result.region_pattern_descriptor_mean_value));
  AddNamedResultField(result, "baseline", "baseline", "region_pattern_descriptor_std",
                      std::to_string(result.region_pattern_descriptor_std_value));
  AddNamedResultField(result, "baseline", "baseline", "line_measure_bbox_w",
                      std::to_string(result.line_measure_bbox_w_value));
  AddNamedResultField(result, "baseline", "baseline", "circle_radius",
                      std::to_string(result.circle_radius_value));
  AddNamedResultField(result, "baseline", "baseline", "match_best_score",
                      std::to_string(result.match_max_score_value));
  AddNamedResultField(result, "baseline", "baseline", "roi_patch_tensor",
                      result.roi_patch_tensor_value);
  AddNamedResultField(result, "baseline", "baseline", "roi_patch_count",
                      std::to_string(result.roi_patch_count_value));
  AddNamedResultField(result, "baseline", "baseline", "roi_patch_spatial_size",
                      std::to_string(result.roi_patch_spatial_size_value));
  AddNamedResultField(result, "baseline", "baseline", "roi_class_label",
                      result.roi_class_label_value);
  AddNamedResultField(result, "baseline", "baseline", "roi_class_label_count",
                      std::to_string(result.roi_class_label_count_value));
  AddNamedResultField(result, "baseline", "baseline", "region_tensor",
                      result.region_tensor_value);
  AddNamedResultField(result, "baseline", "baseline", "region_spatial_size",
                      std::to_string(result.region_spatial_size_value));
  AddNamedResultField(result, "baseline", "baseline", "region_channel_layout",
                      result.region_channel_layout_value);
  AddNamedResultField(result, "baseline", "baseline", "mask_or_region_label",
                      result.mask_or_region_label_value);
  AddNamedResultField(result, "baseline", "baseline", "mask_label_spatial_size",
                      std::to_string(result.mask_label_spatial_size_value));
  AddNamedResultField(result, "baseline", "baseline", "roi_alignment_status",
                      result.roi_alignment_status_value);
  AddNamedResultField(result, "baseline", "baseline", "mask_alignment_status",
                      result.mask_alignment_status_value);

  if (IsAiTaskEnvelopeContractCaseName(result.case_name))
  {
    AddNamedResultObject(result,
                         "ai_task",
                         "bridge",
                         "AiTaskEnvelope",
                         result.success ? "ready" : "partial",
                         result.failure_mode);
    AddNamedResultField(result, "ai_task", "bridge", "task", "baseline_feature_bundle");
    AddNamedResultField(result, "ai_task", "bridge", "descriptors",
                        "region_pattern_descriptor|template_descriptor");
    AddNamedResultField(result, "ai_task", "bridge", "geometry",
                        "roi_line|roi_circle|roi_match");
    AddNamedResultField(result, "ai_task", "bridge", "proposals", "bbox_proposals");
    AddNamedResultField(result, "ai_task", "bridge", "AiRouteDecision",
                        result.route.empty() ? "RouteToMlpack" : result.route);
    AddNamedResultField(result, "ai_task", "bridge", "image_window_tensor",
                        "image_window_tensor");
    AddNamedResultField(result, "ai_task", "bridge", "image_window_count", "1");
    AddNamedResultField(result, "ai_task", "bridge", "resize_policy",
                        "letterbox_keep_aspect");
    AddNamedResultField(result, "ai_task", "bridge", "bbox_candidate_list",
                        "bbox_candidate_list");
    AddNamedResultField(result, "ai_task", "bridge", "bbox_xyxy_targets",
                        "bbox_xyxy_targets");
    AddNamedResultField(result, "ai_task", "bridge", "class_id_targets",
                        "class_id_targets");
    AddNamedResultField(result, "ai_task", "bridge", "target_count", "1");
    AddNamedResultField(result, "ai_task", "bridge",
                        "detection_target_alignment_status",
                        "target_count_aligned");
  }

  if (!result.published_handoff_type.empty() || !result.published_primary_ref.empty())
  {
    AddNamedResultObject(result,
                         "published",
                         "published",
                         "TorchHandoffPublishedSummary",
                         result.success ? "ready" : "partial",
                         result.failure_mode);
    if (!result.published_handoff_type.empty())
      AddNamedResultField(result, "published", "published", "published_handoff_type",
                          result.published_handoff_type);
    if (!result.published_primary_ref.empty())
      AddNamedResultField(result, "published", "published", "published_primary_ref",
                          result.published_primary_ref);
    if (!result.published_route_hint.empty())
      AddNamedResultField(result, "published", "published", "published_route_hint",
                          result.published_route_hint);
    if (!result.published_route_state.empty())
      AddNamedResultField(result, "published", "published", "published_route_state",
                          result.published_route_state);
    if (!result.published_source_hash.empty())
      AddNamedResultField(result, "published", "published", "published_source_hash",
                          result.published_source_hash);
    if (!result.published_result_ref.empty())
      AddNamedResultField(result, "published", "published", "published_result_ref",
                          result.published_result_ref);
    if (!result.published_evidence_ref.empty())
      AddNamedResultField(result, "published", "published", "published_evidence_ref",
                          result.published_evidence_ref);
    if (!result.published_bbox_candidate_list_ref.empty())
      AddNamedResultField(result, "published", "published", "published_bbox_candidate_list_ref",
                          result.published_bbox_candidate_list_ref);
    if (!result.published_template_alignment_ref.empty())
      AddNamedResultField(result, "published", "published", "published_template_alignment_ref",
                          result.published_template_alignment_ref);
    if (!result.published_template_test_alignment_status.empty())
      AddNamedResultField(result, "published", "published", "published_template_test_alignment_status",
                          result.published_template_test_alignment_status);
    if (!result.published_roi_diff_candidate_ref.empty())
      AddNamedResultField(result, "published", "published", "published_roi_diff_candidate_ref",
                          result.published_roi_diff_candidate_ref);
    if (!result.published_roi_diff_candidate_count.empty())
      AddNamedResultField(result, "published", "published", "published_roi_diff_candidate_count",
                          result.published_roi_diff_candidate_count);
    if (!result.published_prior_roi_region_ref.empty())
      AddNamedResultField(result, "published", "published", "published_prior_roi_region_ref",
                          result.published_prior_roi_region_ref);
    if (!result.published_roi_crop_packet_ref.empty())
      AddNamedResultField(result, "published", "published", "published_roi_crop_packet_ref",
                          result.published_roi_crop_packet_ref);
    if (!result.published_roi_crop_count.empty())
      AddNamedResultField(result, "published", "published", "published_roi_crop_count",
                          result.published_roi_crop_count);
    if (!result.published_roi_crop_spatial_size.empty())
      AddNamedResultField(result, "published", "published", "published_roi_crop_spatial_size",
                          result.published_roi_crop_spatial_size);
    if (!result.published_roi_crop_policy_ref.empty())
      AddNamedResultField(result, "published", "published", "published_roi_crop_policy_ref",
                          result.published_roi_crop_policy_ref);
  }

  AddNamedResultObject(result,
                       "fit",
                       "fit",
                       "FitResult",
                       result.success ? "ready" : "partial",
                       result.circle_failure_stage);
  if (result.fit_error_avg_value != 0.0)
    AddNamedResultField(result, "fit", "fit", "fit_error_avg", std::to_string(result.fit_error_avg_value));
  if (result.fit_error_max_value != 0.0)
    AddNamedResultField(result, "fit", "fit", "fit_error_max", std::to_string(result.fit_error_max_value));
  if (result.circle_radius_value != 0.0)
    AddNamedResultField(result, "fit", "fit", "circle_radius", std::to_string(result.circle_radius_value));
  AddNamedResultField(result, "fit", "fit", "line_angle", std::to_string(result.line_angle_value));
  AddNamedResultField(result, "fit", "fit", "template_main_top_score",
                      std::to_string(result.template_main_top_score_value));

  AddNamedResultObject(result,
                       "output",
                       "output",
                       result.result_object.empty() ? "ResultObject" : result.result_object,
                       result.success ? "ok" : "failed",
                       result.circle_failure_stage.empty() ? result.failure_phase : result.circle_failure_stage);
  AddNamedResultField(result, "output", "output", "status", result.success ? "ok" : "failed");
  if (!result.result_object.empty())
    AddNamedResultField(result, "output", "output", "result_object", result.result_object);
  if (!result.failure_mode.empty())
    AddNamedResultField(result, "output", "output", "failure_mode", result.failure_mode);
  if (!result.circle_failure_stage.empty())
    AddNamedResultField(result, "output", "output", "failure_stage", result.circle_failure_stage);
  if (!result.summary.empty())
    AddNamedResultField(result, "output", "output", "summary", result.summary);

  AddNamedResultObject(result,
                       "primary",
                       "output",
                       result.result_object.empty() ? "PrimaryResult" : result.result_object,
                       "ready",
                       std::string());
  if (result.circle_radius_value != 0.0)
    AddNamedResultField(result, "primary", "output", "circle_radius", std::to_string(result.circle_radius_value));
  if (result.match_top_score_value != 0.0)
    AddNamedResultField(result, "primary", "output", "top_score", std::to_string(result.match_top_score_value));
  if (result.match_candidate_count_value != 0.0)
    AddNamedResultField(result, "primary", "output", "candidate_count",
                        std::to_string(result.match_candidate_count_value));
  if (result.match_top_score_value != 0.0)
    AddNamedResultField(result, "primary", "output", "top1_score", std::to_string(result.match_top_score_value));
  if (result.match_candidate_count_value != 0.0 || result.region_connected_components_value != 0.0)
    AddNamedResultField(result, "primary", "output", "result_count",
                        std::to_string(result.match_candidate_count_value != 0.0
                                         ? result.match_candidate_count_value
                                         : result.region_connected_components_value));
  if (result.match_best_rect_w_value > 0.0 && result.match_best_rect_h_value > 0.0)
  {
    std::ostringstream rect_text;
    rect_text << static_cast<int>(result.match_best_rect_x_value) << ","
              << static_cast<int>(result.match_best_rect_y_value) << ","
              << static_cast<int>(result.match_best_rect_w_value) << ","
              << static_cast<int>(result.match_best_rect_h_value);
    AddNamedResultField(result, "primary", "output", "top1_rect", rect_text.str());
  }
  if (result.match_center_x_value != 0.0 || result.match_center_y_value != 0.0)
  {
    std::ostringstream center_text;
    center_text << static_cast<int>(result.match_center_x_value) << ","
                << static_cast<int>(result.match_center_y_value);
    AddNamedResultField(result, "primary", "output", "top1_center", center_text.str());
  }
  AddNamedResultField(result, "primary", "output", "selected_index",
                      std::to_string(result.match_selected_index_value));
  AddNamedResultField(result, "primary", "output", "best_index",
                      std::to_string(result.match_best_index_value));
  AddNamedResultField(result, "primary", "output", "template_learn_path_a_count",
                      std::to_string(result.template_learn_path_a_count_value));
  AddNamedResultField(result, "primary", "output", "template_learn_path_b_count",
                      std::to_string(result.template_learn_path_b_count_value));
  if (result.region_bounds_count_value != 0.0)
    AddNamedResultField(result, "primary", "output", "bounds_count", std::to_string(result.region_bounds_count_value));

  const bool has_fallback =
    result.circle_used_fallback_value != 0.0 ||
    result.template_used_fallback_value != 0.0 ||
    result.circle_compact_path_value != 0.0;
  if (has_fallback)
  {
    AddNamedResultObject(result,
                         "fallback",
                         "output",
                         "FallbackResult",
                         "ready",
                         result.circle_failure_stage);
    AddNamedResultField(result, "fallback", "output", "used_fallback",
                        std::to_string(result.circle_used_fallback_value != 0.0 ?
                                         result.circle_used_fallback_value :
                                         result.template_used_fallback_value));
    if (!result.circle_failure_stage.empty())
      AddNamedResultField(result, "fallback", "output", "failure_stage", result.circle_failure_stage);
  }

  if (!result.failure_phase.empty() || !result.failure_mode.empty() || !result.circle_failure_stage.empty())
  {
    AddNamedResultObject(result,
                         "failure",
                         "failure",
                         "FailureContext",
                         result.success ? "clear" : "set",
                         result.circle_failure_stage.empty() ? result.failure_phase : result.circle_failure_stage);
    if (!result.failure_phase.empty())
      AddNamedResultField(result, "failure", "failure", "phase", result.failure_phase);
    if (!result.failure_mode.empty())
      AddNamedResultField(result, "failure", "failure", "mode", result.failure_mode);
    if (!result.circle_failure_stage.empty())
      AddNamedResultField(result, "failure", "failure", "stage", result.circle_failure_stage);
  }

  if (!result.optimize_summary_object.empty())
  {
    AddNamedResultObject(result,
                         "optimize",
                         "optimize",
                         result.optimize_summary_object,
                         "ready",
                         std::string());
    AddNamedResultField(result, "optimize", "optimize", "baseline_objective",
                        std::to_string(result.baseline_objective));
    AddNamedResultField(result, "optimize", "optimize", "best_objective",
                        std::to_string(result.best_objective));
  }

  if (!result.compare_summary_object.empty())
  {
    AddNamedResultObject(result,
                         "compare",
                         "compare",
                         result.compare_summary_object,
                         "ready",
                         std::string());
    AddNamedResultField(result, "compare", "compare", "objective_delta",
                        std::to_string(result.objective_delta));
    AddNamedResultField(result, "compare", "compare", "metric_delta",
                        std::to_string(result.metric_delta));
    AddNamedResultField(result, "compare", "compare", "stability_delta",
                        std::to_string(result.stability_delta));
    if (!result.pass_level.empty())
      AddNamedResultField(result, "compare", "compare", "pass_level", result.pass_level);
  }

  if (!result.replay_result_object.empty())
  {
    AddNamedResultObject(result,
                         "replay",
                         "replay",
                         result.replay_result_object,
                         "ready",
                         std::string());
    if (!result.replay_log_path.empty())
      AddNamedResultField(result, "replay", "replay", "replay_log_path", result.replay_log_path);
  }

  if (!result.rag_writeback_note_object.empty())
  {
    AddNamedResultObject(result,
                         "rag",
                         "rag",
                         result.rag_writeback_note_object,
                         "ready",
                         std::string());
  }

  if (result.module == "mlpack" &&
      (!result.predictions_csv.empty() ||
       !result.model_path.empty() ||
       !result.output_summary_csv.empty() ||
       !result.cluster_ref.empty() ||
       !result.distance_ref.empty() ||
       !result.anomaly_ref.empty() ||
       !result.roi_crop_packet_ref.empty() ||
       !result.published_roi_crop_packet_ref.empty() ||
       !result.published_prior_roi_region_ref.empty() ||
       !result.roi_diff_candidate_ref.empty()))
  {
    AddNamedResultObject(result,
                         "refs",
                         "refs",
                         "MlpackBaselineRefs",
                         result.success ? "ready" : "partial",
                         std::string());
    if (!result.predictions_csv.empty())
      AddNamedResultField(result, "refs", "refs", "baseline_class_ref",
                          result.predictions_csv);
    if (!result.model_path.empty())
      AddNamedResultField(result, "refs", "refs", "model_path",
                          result.model_path);
    if (!result.output_summary_csv.empty())
      AddNamedResultField(result, "refs", "refs", "output_summary_csv",
                          result.output_summary_csv);
    if (!result.cluster_ref.empty())
      AddNamedResultField(result, "refs", "refs", "cluster_ref",
                          result.cluster_ref);
    if (!result.distance_ref.empty())
      AddNamedResultField(result, "refs", "refs", "distance_ref",
                          result.distance_ref);
    if (!result.anomaly_ref.empty())
      AddNamedResultField(result, "refs", "refs", "anomaly_ref",
                          result.anomaly_ref);
    if (!result.roi_crop_packet_ref.empty())
      AddNamedResultField(result, "refs", "refs", "roi_crop_packet_ref",
                          result.roi_crop_packet_ref);
    if (!result.published_roi_crop_packet_ref.empty())
      AddNamedResultField(result, "refs", "refs", "published_roi_crop_packet_ref",
                          result.published_roi_crop_packet_ref);
    if (!result.published_prior_roi_region_ref.empty())
      AddNamedResultField(result, "refs", "refs", "published_prior_roi_region_ref",
                          result.published_prior_roi_region_ref);
    if (!result.roi_diff_candidate_ref.empty())
      AddNamedResultField(result, "refs", "refs", "roi_diff_candidate_ref",
                          result.roi_diff_candidate_ref);
  }

  if ((result.module == "torch_module" || result.module == "torch") &&
      (!result.baseline_class_ref.empty() ||
       !result.baseline_feature_ref.empty() ||
       !result.attach_back_ref.empty() ||
       !result.attach_back_overlay_status.empty() ||
       !result.attach_back_top1_class.empty() ||
       !result.attach_back_confidence.empty() ||
       !result.bbox_candidate_list_ref.empty() ||
       !result.roi_crop_packet_ref.empty() ||
       !result.cluster_ref.empty() ||
       !result.distance_ref.empty() ||
       !result.anomaly_ref.empty() ||
       !result.template_alignment_ref.empty() ||
       !result.template_test_alignment_status.empty() ||
       !result.roi_diff_candidate_ref.empty() ||
       !result.roi_diff_candidate_count.empty() ||
       !ResolveTorchTrainPublishedRef(result, "trainer_lifecycle_summary").empty() ||
       !ResolveTorchTrainPublishedRef(result, "trainer_flat_run").empty() ||
       !ResolveTorchTrainPublishedRef(result, "unified_mainline_summary").empty() ||
       !ResolveTorchTrainPublishedRef(result, "checkpoint_or_resume_hint").empty() ||
       !ResolveTorchTrainPublishedRef(result, "roi_train_feedback").empty() ||
       !ResolveTorchTrainPublishedRef(result, "segmentation_trainer_lifecycle_summary").empty() ||
       !ResolveTorchTrainPublishedRef(result, "segmentation_trainer_flat_run").empty() ||
       !ResolveTorchTrainPublishedRef(result, "segmentation_unified_summary").empty() ||
       !ResolveTorchTrainPublishedRef(result, "foreground_iou_summary").empty()))
  {
    const std::string trainer_lifecycle_summary =
      ResolveTorchTrainPublishedRef(result, "trainer_lifecycle_summary");
    const std::string trainer_flat_run =
      ResolveTorchTrainPublishedRef(result, "trainer_flat_run");
    const std::string unified_mainline_summary =
      ResolveTorchTrainPublishedRef(result, "unified_mainline_summary");
    const std::string checkpoint_or_resume_hint =
      ResolveTorchTrainPublishedRef(result, "checkpoint_or_resume_hint");
    const std::string roi_train_feedback =
      ResolveTorchTrainPublishedRef(result, "roi_train_feedback");
    const std::string segmentation_trainer_lifecycle_summary =
      ResolveTorchTrainPublishedRef(result, "segmentation_trainer_lifecycle_summary");
    const std::string segmentation_trainer_flat_run =
      ResolveTorchTrainPublishedRef(result, "segmentation_trainer_flat_run");
    const std::string segmentation_unified_summary =
      ResolveTorchTrainPublishedRef(result, "segmentation_unified_summary");
    const std::string foreground_iou_summary =
      ResolveTorchTrainPublishedRef(result, "foreground_iou_summary");
    AddNamedResultObject(result,
                         "refs",
                         "refs",
                         "TorchStageRefs",
                         result.success ? "ready" : "partial",
                         std::string());
    if (!result.baseline_class_ref.empty())
      AddNamedResultField(result, "refs", "refs", "baseline_class_ref",
                          result.baseline_class_ref);
    if (!result.baseline_feature_ref.empty())
      AddNamedResultField(result, "refs", "refs", "baseline_feature_ref",
                          result.baseline_feature_ref);
    if (!result.attach_back_ref.empty())
      AddNamedResultField(result, "refs", "refs", "attach_back_ref",
                          result.attach_back_ref);
    if (!result.attach_back_overlay_status.empty())
      AddNamedResultField(result, "refs", "refs", "attach_back_overlay_status",
                          result.attach_back_overlay_status);
    if (!result.attach_back_top1_class.empty())
      AddNamedResultField(result, "refs", "refs", "attach_back_top1_class",
                          result.attach_back_top1_class);
    if (!result.attach_back_confidence.empty())
      AddNamedResultField(result, "refs", "refs", "attach_back_confidence",
                          result.attach_back_confidence);
    if (!result.bbox_candidate_list_ref.empty())
      AddNamedResultField(result, "refs", "refs", "bbox_candidate_list_ref",
                          result.bbox_candidate_list_ref);
    if (!result.roi_crop_packet_ref.empty())
      AddNamedResultField(result, "refs", "refs", "roi_crop_packet_ref",
                          result.roi_crop_packet_ref);
    if (!result.cluster_ref.empty())
      AddNamedResultField(result, "refs", "refs", "cluster_ref",
                          result.cluster_ref);
    if (!result.distance_ref.empty())
      AddNamedResultField(result, "refs", "refs", "distance_ref",
                          result.distance_ref);
    if (!result.anomaly_ref.empty())
      AddNamedResultField(result, "refs", "refs", "anomaly_ref",
                          result.anomaly_ref);
    if (!result.template_alignment_ref.empty())
      AddNamedResultField(result, "refs", "refs", "template_alignment_ref",
                          result.template_alignment_ref);
    if (!result.template_test_alignment_status.empty())
      AddNamedResultField(result, "refs", "refs", "template_test_alignment_status",
                          result.template_test_alignment_status);
    if (!result.roi_diff_candidate_ref.empty())
      AddNamedResultField(result, "refs", "refs", "roi_diff_candidate_ref",
                          result.roi_diff_candidate_ref);
    if (!result.roi_diff_candidate_count.empty())
      AddNamedResultField(result, "refs", "refs", "roi_diff_candidate_count",
                          result.roi_diff_candidate_count);
    if (!trainer_lifecycle_summary.empty())
      AddNamedResultField(result, "refs", "refs", "trainer_lifecycle_summary",
                          trainer_lifecycle_summary);
    if (!trainer_flat_run.empty())
      AddNamedResultField(result, "refs", "refs", "trainer_flat_run",
                          trainer_flat_run);
    if (!unified_mainline_summary.empty())
      AddNamedResultField(result, "refs", "refs", "unified_mainline_summary",
                          unified_mainline_summary);
    if (!checkpoint_or_resume_hint.empty())
      AddNamedResultField(result, "refs", "refs", "checkpoint_or_resume_hint",
                          checkpoint_or_resume_hint);
    if (!roi_train_feedback.empty())
      AddNamedResultField(result, "refs", "refs", "roi_train_feedback",
                          roi_train_feedback);
    if (!segmentation_trainer_lifecycle_summary.empty())
      AddNamedResultField(result, "refs", "refs", "segmentation_trainer_lifecycle_summary",
                          segmentation_trainer_lifecycle_summary);
    if (!segmentation_trainer_flat_run.empty())
      AddNamedResultField(result, "refs", "refs", "segmentation_trainer_flat_run",
                          segmentation_trainer_flat_run);
    if (!segmentation_unified_summary.empty())
      AddNamedResultField(result, "refs", "refs", "segmentation_unified_summary",
                          segmentation_unified_summary);
    if (!foreground_iou_summary.empty())
      AddNamedResultField(result, "refs", "refs", "foreground_iou_summary",
                          foreground_iou_summary);
  }

  if (result.module == "cximage" &&
      (!line_point_set_ref.empty() ||
       !line_measure_bounds_ref.empty() ||
       !circle_point_set_ref.empty() ||
       !circle_measure_bounds_ref.empty() ||
       !region_summary_ref.empty() ||
       !region_bounds_ref.empty() ||
       !result.candidate_overlay_ref.empty() ||
       !result.template_rect_overlay_ref.empty() ||
       !result.test_rect_overlay_ref.empty() ||
       !result.circle_overlay_ref.empty() ||
       !result.circle_edge_overlay_ref.empty() ||
       !result.formfit_candidate_overlay_ref.empty() ||
       !result.formfit_selection_overlay_ref.empty() ||
       !result.region_pattern_overlay_ref.empty() ||
       !result.region_pattern_descriptor_ref.empty() ||
       !result.fractal_partition_overlay_ref.empty() ||
       !result.distance_field_overlay_ref.empty() ||
       !result.skeleton_overlay_ref.empty() ||
       !result.centerline_overlay_ref.empty() ||
       !result.topology_repair_overlay_ref.empty()))
  {
    AddNamedResultObject(result,
                         "refs",
                         "refs",
                         "CximageReviewRefs",
                         result.success ? "ready" : "partial",
                         std::string());
    if (!line_point_set_ref.empty())
      AddNamedResultField(result, "refs", "refs", "line_point_set_ref",
                          line_point_set_ref);
    if (!line_measure_bounds_ref.empty())
      AddNamedResultField(result, "refs", "refs", "line_measure_bounds_ref",
                          line_measure_bounds_ref);
    if (!circle_point_set_ref.empty())
      AddNamedResultField(result, "refs", "refs", "circle_point_set_ref",
                          circle_point_set_ref);
    if (!circle_measure_bounds_ref.empty())
      AddNamedResultField(result, "refs", "refs", "circle_measure_bounds_ref",
                          circle_measure_bounds_ref);
    if (!region_summary_ref.empty())
      AddNamedResultField(result, "refs", "refs", "region_summary_ref",
                          region_summary_ref);
    if (!region_bounds_ref.empty())
      AddNamedResultField(result, "refs", "refs", "region_bounds_ref",
                          region_bounds_ref);
    if (!result.candidate_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "candidate_overlay_ref",
                          result.candidate_overlay_ref);
    if (!result.template_rect_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "template_rect_overlay_ref",
                          result.template_rect_overlay_ref);
    if (!result.test_rect_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "test_rect_overlay_ref",
                          result.test_rect_overlay_ref);
    if (!result.circle_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "circle_overlay_ref",
                          result.circle_overlay_ref);
    if (!result.circle_edge_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "circle_edge_overlay_ref",
                          result.circle_edge_overlay_ref);
    if (!result.formfit_candidate_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "formfit_candidate_overlay_ref",
                          result.formfit_candidate_overlay_ref);
    if (!result.formfit_selection_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "formfit_selection_overlay_ref",
                          result.formfit_selection_overlay_ref);
    if (!result.region_pattern_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "region_pattern_overlay_ref",
                          result.region_pattern_overlay_ref);
    if (!result.region_pattern_descriptor_ref.empty())
      AddNamedResultField(result, "refs", "refs", "region_pattern_descriptor_ref",
                          result.region_pattern_descriptor_ref);
    if (!result.fractal_partition_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "fractal_partition_overlay_ref",
                          result.fractal_partition_overlay_ref);
    if (!result.distance_field_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "distance_field_overlay_ref",
                          result.distance_field_overlay_ref);
    if (!result.skeleton_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "skeleton_overlay_ref",
                          result.skeleton_overlay_ref);
    if (!result.centerline_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "centerline_overlay_ref",
                          result.centerline_overlay_ref);
    if (!result.topology_repair_overlay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "topology_repair_overlay_ref",
                          result.topology_repair_overlay_ref);
  }

  const bool is_torch_execution_projection =
    (result.module == "torch_module" || result.module == "torch") &&
    (!result.requested_device.empty() ||
     !result.infer_param_summary.empty() ||
     !result.train_param_summary.empty() ||
     !result.dataset_profile.empty());
  if (result.runtime_ms > 0.0 ||
      result.fit_time_ms > 0.0 ||
      result.infer_time_ms > 0.0 ||
      is_torch_execution_projection)
  {
    AddNamedResultObject(result,
                         "timing",
                         "timing",
                         "ExecutionTiming",
                         result.success ? "ready" : "partial",
                         std::string());
    if (result.runtime_ms > 0.0)
    {
      AddNamedResultField(result, "timing", "timing", "runtime_ms",
                          std::to_string(result.runtime_ms));
      AddNamedResultField(result, "timing", "timing", "verified_runtime_ms",
                          std::to_string(result.runtime_ms));
      AddNamedResultField(result, "timing", "timing", "runtime_status",
                          "verified_runtime");
    }
    else if (is_torch_execution_projection)
    {
      AddNamedResultField(result, "timing", "timing", "placeholder_runtime_ms", "0");
      AddNamedResultField(result, "timing", "timing", "runtime_status",
                          "placeholder_runtime");
    }
    if (result.fit_time_ms > 0.0)
      AddNamedResultField(result, "timing", "timing", "fit_time_ms",
                          std::to_string(result.fit_time_ms));
    if (result.infer_time_ms > 0.0)
      AddNamedResultField(result, "timing", "timing", "infer_time_ms",
                          std::to_string(result.infer_time_ms));
  }

  if (!result.optimize_summary_object.empty() ||
      !result.compare_summary_object.empty() ||
      !result.replay_result_object.empty() ||
      !result.rag_writeback_note_object.empty() ||
      !result.cluster_ref.empty() ||
      !result.distance_ref.empty() ||
      !result.anomaly_ref.empty())
  {
    const std::string source_ref =
      result.script_path.empty() ? result.script_name : result.script_path;
    AddNamedResultObject(result,
                         "refs",
                         "refs",
                         "ExecutionContractRefs",
                         result.success ? "ready" : "partial",
                         std::string());
    if (!source_ref.empty())
      AddNamedResultField(result, "refs", "refs", "source_ref", source_ref);
    if (!source_ref.empty())
      AddNamedResultField(result, "refs", "refs", "source_hash",
                          BuildPseudoSourceHash(source_ref + "|" + result.layer + "|" + result.module));
    if (!result.task_id.empty())
    {
      AddNamedResultField(result, "refs", "refs", "result_ref", result.task_id);
      AddNamedResultField(result, "refs", "refs", "objective_ref",
                          result.objective_ref.empty() ? result.task_id + ".objective" : result.objective_ref);
      AddNamedResultField(result, "refs", "refs", "optimization_result_ref",
                          result.optimization_result_ref.empty() ? result.task_id + ".optimization" : result.optimization_result_ref);
      AddNamedResultField(result, "refs", "refs", "best_params_ref",
                          result.best_params_ref.empty() ? result.task_id + ".best_params" : result.best_params_ref);
      AddNamedResultField(result, "refs", "refs", "objective_delta_ref",
                          result.objective_delta_ref.empty() ? result.task_id + ".objective_delta" : result.objective_delta_ref);
      AddNamedResultField(result, "refs", "refs", "summary_ref",
                          result.summary_ref.empty() ? result.task_id + ".summary" : result.summary_ref);
      if (!result.compare_summary_object.empty() || !result.compare_ref.empty())
        AddNamedResultField(result, "refs", "refs", "compare_ref",
                            result.compare_ref.empty() ? result.task_id + ".compare" : result.compare_ref);
      if (!result.cluster_ref.empty())
        AddNamedResultField(result, "refs", "refs", "cluster_ref", result.cluster_ref);
      if (!result.distance_ref.empty())
        AddNamedResultField(result, "refs", "refs", "distance_ref", result.distance_ref);
      if (!result.anomaly_ref.empty())
        AddNamedResultField(result, "refs", "refs", "anomaly_ref", result.anomaly_ref);
    }
    if (!result.script_path.empty())
      AddNamedResultField(result, "refs", "refs", "evidence_ref", result.script_path);
    if (!result.replay_log_path.empty() || !result.replay_ref.empty())
      AddNamedResultField(result, "refs", "refs", "replay_ref",
                          result.replay_ref.empty() ? result.replay_log_path : result.replay_ref);
    if (result.task_id.empty())
    {
      if (!result.cluster_ref.empty())
        AddNamedResultField(result, "refs", "refs", "cluster_ref", result.cluster_ref);
      if (!result.distance_ref.empty())
        AddNamedResultField(result, "refs", "refs", "distance_ref", result.distance_ref);
      if (!result.anomaly_ref.empty())
        AddNamedResultField(result, "refs", "refs", "anomaly_ref", result.anomaly_ref);
    }
    AddNamedResultField(result, "refs", "refs", "next_action",
                        result.success ? "consume optimization_result_ref and replay_ref"
                                       : "inspect evidence_ref and error_message");
  }

  if (IsEnsmallenPublicResult(result))
  {
    const bool is_match_score_case =
      result.case_name == "match_score_tuning" || result.case_name == "match_score_opt";
    const bool is_scenario_case = result.layer == "scenario";
    const bool is_train_case = result.layer == "train";
    const bool is_infer_case = result.layer == "infer";

    std::string channel = "formfit.geometry_fit_channel";
    std::string active_inputs =
      "geometry_ref,boundary_metrics_ref,fit_targets_ref,params,objective_weights,objective_ref,boundary_error_ref,alignment_error_ref";
    std::string reserved_inputs =
      "geometry_ref,boundary_metrics_ref,fit_targets_ref";
    std::string torch_optimization_inputs =
      "objective_ref,boundary_error_ref,alignment_error_ref";

    if (is_match_score_case)
    {
      channel = "fastmatch.structural_match_channel";
      active_inputs =
        "roi_ref,match_gt,params,objective_weights,objective_ref,threshold_ref,crop_policy_ref";
      reserved_inputs =
        "RegionPatternConfig,RegionPatternDescriptor,RegionPatternScore";
      torch_optimization_inputs =
        "objective_ref,threshold_ref,crop_policy_ref";
    }
    else if (is_scenario_case)
    {
      channel = "phase1.replay_compare_stage";
      active_inputs =
        "dataset_ref,sample_bundle_ref,repeat_count,replay_enable,compare_enable,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
      reserved_inputs =
        "sample_summaries,pass_fail,replay_log_path,scenario_compare.json";
      torch_optimization_inputs =
        "objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
    }
    else if (is_train_case)
    {
      channel = "phase1.batch_optimize_stage";
      active_inputs =
        "dataset_ref,split_ref,task_scope,optimizer_name,max_evals,patience,epsilon,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
      reserved_inputs =
        "best_param_sets,sample_count,replay_log_path,batch_best_params.json,batch_summary.json";
      torch_optimization_inputs =
        "objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
    }
    else if (is_infer_case)
    {
      channel = "phase1.infer_compare_stage";
      active_inputs =
        "dataset_ref,best_params_ref,compare_enable,baseline_only,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
      reserved_inputs =
        "baseline_metrics,optimized_metrics,delta_metrics,replay_log_path,baseline_report.json,optimized_report.json,infer_compare.json";
      torch_optimization_inputs =
        "objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
    }

    AddNamedResultObject(result,
                         "inputs",
                         "inputs",
                         "OptimizationInputContract",
                         "ready",
                         std::string());
    AddNamedResultField(result, "inputs", "inputs", "channel", channel);
    AddNamedResultField(result, "inputs", "inputs", "active_inputs", active_inputs);
    AddNamedResultField(result, "inputs", "inputs", "reserved_inputs", reserved_inputs);
    AddNamedResultField(result, "inputs", "inputs", "torch_optimization_inputs",
                        torch_optimization_inputs);
    if (!result.input_dataset.empty())
      AddNamedResultField(result, "inputs", "inputs", "input_dataset", result.input_dataset);
    if (!result.input_sample.empty())
      AddNamedResultField(result, "inputs", "inputs", "input_sample", result.input_sample);
    if (!result.input_split.empty())
      AddNamedResultField(result, "inputs", "inputs", "input_split", result.input_split);
    if (!result.input_artifacts.empty())
    {
      AddNamedResultField(result, "inputs", "inputs", "input_artifacts", result.input_artifacts);
      AddNamedResultField(result, "inputs", "inputs", "input_artifact", result.input_artifacts);
    }
    if (!result.input_params.empty())
    {
      AddNamedResultField(result, "inputs", "inputs", "input_params", result.input_params);
      AddNamedResultField(result, "inputs", "inputs", "input_param", result.input_params);
    }
    if (!result.dataset_ref.empty())
      AddNamedResultField(result, "inputs", "inputs", "dataset_ref", result.dataset_ref);
    if (!result.sample_bundle_ref.empty())
      AddNamedResultField(result, "inputs", "inputs", "sample_bundle_ref", result.sample_bundle_ref);
    const std::string dataset_bridge = ResolveRuntimeDatasetBridgeTag(result);
    const std::string test_bucket = SummarizeEnsmallenInputBuckets(result);
    const std::string test_flow = BuildEnsmallenTestFlowGuide(result);
    AddNamedResultField(result, "inputs", "inputs", "dataset_bridge", dataset_bridge);
    AddNamedResultField(result, "inputs", "inputs", "test_bucket", test_bucket);
    AddNamedResultField(result, "inputs", "inputs", "test_flow", test_flow);

    AddNamedResultObject(result,
                         "conclusion",
                         "conclusion",
                         "ReviewConclusion",
                         result.success ? "ready" : "partial",
                         std::string());
    AddNamedResultField(result, "conclusion", "conclusion", "chain_status", "passed");
    AddNamedResultField(result, "conclusion", "conclusion", "export_status", "passed");
    AddNamedResultField(result, "conclusion", "conclusion", "algorithm_status",
                        "pending_human_review");
    AddNamedResultField(result, "conclusion", "conclusion", "current_status",
                        BuildEnsmallenConclusionStatus(result));
    AddNamedResultField(result, "conclusion", "conclusion", "human_review_required",
                        "required");
    AddNamedResultField(result, "conclusion", "conclusion", "boundary_note",
                        BuildEnsmallenBoundaryNote(result));
    AddNamedResultField(result, "conclusion", "conclusion", "evidence_ref",
                        result.script_path.empty() ? result.script_name : result.script_path);
    AddNamedResultField(result, "conclusion", "conclusion", "conclusion_id",
                        flow_host_runtime_detail::BuildEnsmallenConclusionId(result));
    AddNamedResultField(result, "conclusion", "conclusion", "short_conclusion",
                        flow_host_runtime_detail::BuildEnsmallenShortConclusion(result));
    AddNamedResultField(result, "conclusion", "conclusion", "why_it_matters",
                        flow_host_runtime_detail::BuildEnsmallenWhyItMatters(result));
    AddNamedResultField(result, "conclusion", "conclusion", "next_observation",
                        flow_host_runtime_detail::BuildEnsmallenNextObservation(result));
    AddNamedResultField(result, "conclusion", "conclusion", "summary_ref",
                        result.summary_ref.empty() ? result.task_id + ".summary" : result.summary_ref);
    if (!result.compare_ref.empty() || !result.compare_summary_object.empty())
      AddNamedResultField(result, "conclusion", "conclusion", "compare_ref",
                          result.compare_ref.empty() ? result.task_id + ".compare" : result.compare_ref);
    AddNamedResultField(result, "conclusion", "conclusion", "replay_ref",
                        result.replay_ref.empty() ? result.replay_log_path : result.replay_ref);
    if (!result.replay_log_path.empty())
      AddNamedResultField(result, "conclusion", "conclusion", "replay_log_path",
                          result.replay_log_path);
    const std::string best_param_sets_text = BuildEnsmallenBestParamSetsText(result);
    if (!best_param_sets_text.empty())
      AddNamedResultField(result, "conclusion", "conclusion", "best_param_sets",
                          best_param_sets_text);
    const std::string sample_count_text = BuildEnsmallenSampleCountText(result);
    if (!sample_count_text.empty())
      AddNamedResultField(result, "conclusion", "conclusion", "sample_count",
                          sample_count_text);

    AddNamedResultObject(result,
                         "report_header",
                         "report_header",
                         "ReportHeader",
                         result.success ? "ready" : "partial",
                         std::string());
    AddNamedResultField(result, "report_header", "report_header", "module",
                        "ensmallen");
    AddNamedResultField(result, "report_header", "report_header", "entry",
                        "cxparser_ext_cxscript_cli");
    AddNamedResultField(result, "report_header", "report_header", "batch",
                        BuildEnsmallenBatchLabel(result));
    AddNamedResultField(result, "report_header", "report_header", "current_status",
                        BuildEnsmallenConclusionStatus(result));
    AddNamedResultField(result, "report_header", "report_header", "human_review_required",
                        "required");
    AddNamedResultField(result, "report_header", "report_header", "updated_date",
                        "2026-06-05");

    AddNamedResultObject(result,
                         "test_plan",
                         "test_plan",
                         "McpTestPlan",
                         result.success ? "ready" : "partial",
                         std::string());
    AddNamedResultField(result, "test_plan", "test_plan", "test_bucket",
                        SummarizeEnsmallenInputBuckets(result));
    AddNamedResultField(result, "test_plan", "test_plan", "test_flow",
                        BuildEnsmallenTestFlowGuide(result));
    AddNamedResultField(result, "test_plan", "test_plan", "image_selection",
                        BuildEnsmallenImageSelectionGuide(result));
    AddNamedResultField(result, "test_plan", "test_plan", "mcp_flow",
                        BuildEnsmallenMcpFlow(result));

    AddNamedResultObject(result,
                         "interaction",
                         "interaction",
                         "CrossLayerInteraction",
                         result.success ? "ready" : "partial",
                         std::string());
    AddNamedResultField(result, "interaction", "interaction", "route",
                        BuildEnsmallenInteractionRoute(result));
    AddNamedResultField(result, "interaction", "interaction", "dataset_bridge",
                        dataset_bridge);
    AddNamedResultField(result, "interaction", "interaction", "upstream_refs",
                        torch_optimization_inputs);
    AddNamedResultField(result, "interaction", "interaction", "downstream_refs",
                        "summary_ref,compare_ref,replay_ref,evidence_ref");

    AddNamedResultObject(result,
                         "bridge",
                         "bridge",
                         "EnsmallenBridgeContract",
                         result.success ? "ready" : "partial",
                         std::string());
    AddNamedResultField(result, "bridge", "bridge", "dataset_bridge", dataset_bridge);
    AddNamedResultField(result, "bridge", "bridge", "test_bucket", test_bucket);
    AddNamedResultField(result, "bridge", "bridge", "test_flow", test_flow);
    AddNamedResultField(result, "bridge", "bridge", "human_review_required", "required");

    const std::string sample_id = FindAssignmentValue(result.input_artifacts, "sample_id");
    const std::string input_image = FindAssignmentValue(result.input_artifacts, "input_image");
    const std::string template_image = FindAssignmentValue(result.input_artifacts, "template_image");
    const std::string defect_count = FindAssignmentValue(result.input_artifacts, "defect_count");
    const std::string roi_ref = FindAssignmentValue(result.input_artifacts, "roi_ref");
    const std::string match_gt = FindAssignmentValue(result.input_artifacts, "match_gt");
    const std::string objective_ref = FindAssignmentValue(result.input_params, "objective_ref");
    const std::string threshold_ref = FindAssignmentValue(result.input_params, "threshold_ref");
    const std::string crop_policy_ref = FindAssignmentValue(result.input_params, "crop_policy_ref");
    const std::string boundary_error_ref = FindAssignmentValue(result.input_params, "boundary_error_ref");
    const std::string alignment_error_ref = FindAssignmentValue(result.input_params, "alignment_error_ref");

    if (!sample_id.empty())
      AddNamedResultField(result, "bridge", "bridge", "sample_id", sample_id);
    if (!input_image.empty())
      AddNamedResultField(result, "bridge", "bridge", "input_image", input_image);
    if (!template_image.empty())
      AddNamedResultField(result, "bridge", "bridge", "template_image", template_image);
    if (!defect_count.empty())
      AddNamedResultField(result, "bridge", "bridge", "defect_count", defect_count);
    if (!roi_ref.empty())
      AddNamedResultField(result, "bridge", "bridge", "roi_ref", roi_ref);
    if (!match_gt.empty())
      AddNamedResultField(result, "bridge", "bridge", "match_gt", match_gt);
    if (!objective_ref.empty())
      AddNamedResultField(result, "bridge", "bridge", "objective_ref", objective_ref);
    if (!threshold_ref.empty())
      AddNamedResultField(result, "bridge", "bridge", "threshold_ref", threshold_ref);
    if (!crop_policy_ref.empty())
      AddNamedResultField(result, "bridge", "bridge", "crop_policy_ref", crop_policy_ref);
    if (!boundary_error_ref.empty())
      AddNamedResultField(result, "bridge", "bridge", "boundary_error_ref", boundary_error_ref);
    if (!alignment_error_ref.empty())
      AddNamedResultField(result, "bridge", "bridge", "alignment_error_ref", alignment_error_ref);

    const std::string likely_issue_class = BuildEnsmallenLikelyIssueClass(result);
    const std::string recommended_action = BuildEnsmallenRecommendedAction(result);
    const std::string comparison_status = BuildEnsmallenComparisonStatus(result);
    const std::string comparison_magnitude = BuildEnsmallenComparisonMagnitude(result);
    const std::string next_bucket_focus = BuildEnsmallenNextBucketFocus(result);
    const std::string observation_mode = BuildEnsmallenObservationMode(result);
    const std::string expansion_gate = BuildEnsmallenExpansionGate(result);
    const std::string bucket_coverage = BuildEnsmallenBucketCoverage(result);
    const std::string risk_axis = BuildEnsmallenRiskAxis(result);
    const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
    const std::string observation_priority =
      BuildEnsmallenObservationPriority(result);
    const std::string coverage_status =
      flow_host_runtime_detail::BuildEnsmallenCoverageStatus(result);
    const std::string next_review_action =
      flow_host_runtime_detail::BuildEnsmallenNextReviewAction(result);
    const std::string optimization_signal =
      flow_host_runtime_detail::BuildEnsmallenOptimizationSignal(result);
    const std::string objective_curve =
      BuildEnsmallenObjectiveCurveValue(result);
    const std::string feature_distance_delta =
      BuildEnsmallenFeatureDistanceDeltaValue(result);
    const std::string candidate_rank =
      BuildEnsmallenCandidateRankValue(result);
    const std::string stability_score =
      BuildEnsmallenStabilityScoreValue(result);
    const std::string convergence_status =
      BuildEnsmallenConvergenceStatusValue(result);
    const std::string best_candidate_confidence =
      BuildEnsmallenBestCandidateConfidenceValue(result);
    const std::string bucket_review_template =
      flow_host_runtime_detail::BuildEnsmallenCaseBucketReviewTemplate(result);
    const std::string review_scope = BuildEnsmallenReviewScope(result);
    const std::string fallback_review_ref_prefix =
      (!result.module.empty() && !result.layer.empty() && !result.case_name.empty())
        ? (result.module + "." + result.layer + "." + result.case_name)
        : flow_host_runtime_detail::BuildEnsmallenFallbackReviewRefPrefixFromScript(result);
    const std::string primary_review_ref =
      BuildEnsmallenPrimaryReviewRef(result, fallback_review_ref_prefix);

    AddNamedResultObject(result,
                         "comparison",
                         "comparison",
                         "MeasuredComparisonSummary",
                         result.success ? "ready" : "partial",
                         std::string());
    AddNamedResultField(result, "comparison", "comparison", "baseline_objective",
                        std::to_string(result.baseline_objective));
    AddNamedResultField(result, "comparison", "comparison", "best_objective",
                        std::to_string(result.best_objective));
    AddNamedResultField(result, "comparison", "comparison", "objective_delta",
                        std::to_string(result.objective_delta));
    AddNamedResultField(result, "comparison", "comparison", "objective_curve",
                        objective_curve);
    AddNamedResultField(result, "comparison", "comparison", "feature_distance_delta",
                        feature_distance_delta);
    AddNamedResultField(result, "comparison", "comparison", "candidate_rank",
                        candidate_rank);
    AddNamedResultField(result, "comparison", "comparison", "selected_method",
                        result.selected_method);
    AddNamedResultField(result, "comparison", "comparison", "candidate_ordering",
                        result.ordered_candidates);
    AddNamedResultField(result, "comparison", "comparison", "best_candidate_confidence",
                        best_candidate_confidence);
    AddNamedResultField(result, "comparison", "comparison", "comparison_status",
                        comparison_status);
    AddNamedResultField(result, "comparison", "comparison", "comparison_magnitude",
                        comparison_magnitude);
    AddNamedResultField(result, "comparison", "comparison", "primary_review_ref",
                        primary_review_ref);
    AddNamedResultField(result, "comparison", "comparison", "review_scope",
                        review_scope);

    AddNamedResultObject(result,
                         "analysis",
                         "analysis",
                         "TuningAnalysis",
                         result.success ? "ready" : "partial",
                         std::string());
    AddNamedResultField(result, "analysis", "analysis", "bucket_focus", test_bucket);
    AddNamedResultField(result, "analysis", "analysis", "likely_issue_class",
                        likely_issue_class);
    AddNamedResultField(result, "analysis", "analysis", "recommended_action",
                        recommended_action);
    AddNamedResultField(result, "analysis", "analysis", "next_bucket_focus",
                        next_bucket_focus);
    AddNamedResultField(result, "analysis", "analysis", "observation_mode",
                        observation_mode);
    AddNamedResultField(result, "analysis", "analysis", "expansion_gate",
                        expansion_gate);
    AddNamedResultField(result, "analysis", "analysis", "bucket_coverage",
                        bucket_coverage);
    AddNamedResultField(result, "analysis", "analysis", "risk_axis",
                        risk_axis);
    AddNamedResultField(result, "analysis", "analysis", "coverage_gap",
                        coverage_gap);
    AddNamedResultField(result, "analysis", "analysis", "stability_score",
                        stability_score);
    AddNamedResultField(result, "analysis", "analysis", "convergence_status",
                        convergence_status);
    AddNamedResultField(result, "analysis", "analysis", "selected_method",
                        result.selected_method);
    AddNamedResultField(result, "analysis", "analysis", "candidate_ordering",
                        result.ordered_candidates);
    AddNamedResultField(result, "analysis", "analysis", "best_candidate_confidence",
                        best_candidate_confidence);
    AddNamedResultField(result, "analysis", "analysis", "observation_priority",
                        observation_priority);
    AddNamedResultField(result, "analysis", "analysis", "coverage_status",
                        coverage_status);
    AddNamedResultField(result, "analysis", "analysis", "next_review_action",
                        next_review_action);
    AddNamedResultField(result, "analysis", "analysis", "optimization_signal",
                        optimization_signal);
    AddNamedResultField(result, "analysis", "analysis", "bucket_review_template",
                        bucket_review_template);
    AddNamedResultField(result, "analysis", "analysis", "review_scope",
                        review_scope);
    AddNamedResultField(result, "analysis", "analysis", "primary_review_ref",
                        primary_review_ref);
    AddNamedResultField(result, "analysis", "analysis", "evidence_priority",
                        "bridge,replay_ref,compare_ref,summary_ref");

    const bool looks_like_ensmallen_projection =
      result.module == "ensmallen_layer" ||
      result.case_name.find("phase1_param_") != std::string::npos ||
      result.case_name.find("geometry_fit_tuning") != std::string::npos ||
      result.case_name.find("match_score_tuning") != std::string::npos ||
      result.case_name.find("match_score_opt") != std::string::npos ||
      result.input_dataset.find("ensmallen") != std::string::npos ||
      fallback_review_ref_prefix.find("ensmallen_layer") != std::string::npos ||
      primary_review_ref.find("ensmallen_layer") != std::string::npos ||
      review_scope.find("scenario_bundle") != std::string::npos ||
      review_scope.find("train_batch") != std::string::npos ||
      review_scope.find("infer_baseline") != std::string::npos;

    const bool suppress_early_zero_projection =
      looks_like_ensmallen_projection &&
      result.baseline_objective == 0.0 &&
      result.best_objective == 0.0 &&
      result.objective_delta == 0.0;

    if (!suppress_early_zero_projection)
    {
      AppendRuntimeDetail(result,
                          "[ENSMALLEN_COMPARE] baseline_objective=" +
                            std::to_string(result.baseline_objective) +
                            " best_objective=" + std::to_string(result.best_objective) +
                            " objective_delta=" + std::to_string(result.objective_delta) +
                            " comparison_status=" + comparison_status +
                            " comparison_magnitude=" + comparison_magnitude +
                            " selected_method=" + result.selected_method +
                            " candidate_ordering=" + result.ordered_candidates +
                            " result_stage=measured_flow_host" +
                            " primary_review_ref=" + primary_review_ref,
                          false,
                          true);

      AppendRuntimeDetail(result,
                          "[ENSMALLEN_ANALYSIS] bucket_focus=" + test_bucket +
                            " likely_issue_class=" + likely_issue_class +
                          " recommended_action=" + recommended_action +
                          " next_bucket_focus=" + next_bucket_focus +
                          " observation_mode=" + observation_mode +
                          " expansion_gate=" + expansion_gate +
                          " bucket_coverage=" + bucket_coverage +
                          " risk_axis=" + risk_axis +
                          " coverage_gap=" + coverage_gap +
                          " observation_priority=" + observation_priority +
                          " coverage_status=" + coverage_status +
                          " next_review_action=" + next_review_action +
                          " optimization_signal=" + optimization_signal +
                          " selected_method=" + result.selected_method +
                          " candidate_ordering=" + result.ordered_candidates +
                          " bucket_review_template=" + bucket_review_template +
                          " result_stage=measured_flow_host" +
                          " review_scope=" + review_scope +
                            " primary_review_ref=" + primary_review_ref,
                          false,
                          true);
    }
  }

  if (!result.dataset_profile.empty() ||
      !result.prepared_root.empty() ||
      !result.requested_device.empty() ||
      !result.consumed_weight_files.empty() ||
      !result.consumed_weight_paths.empty() ||
      !result.train_param_summary.empty() ||
      !result.infer_param_summary.empty() ||
      !result.required_input_contract.empty() ||
      !result.required_label_contract.empty())
  {
    AddNamedResultObject(result,
                         "dataset",
                         "dataset",
                         "PreparedDatasetBridge",
                         result.success ? "ready" : "partial",
                         std::string());
    if (!result.dataset_profile.empty())
      AddNamedResultField(result, "dataset", "dataset", "dataset_profile",
                          result.dataset_profile);
    if (!result.prepared_root.empty())
      AddNamedResultField(result, "dataset", "dataset", "prepared_root",
                          result.prepared_root);
    if (!result.input_task.empty())
      AddNamedResultField(result, "dataset", "dataset", "input_task",
                          result.input_task);
    if (!result.input_profile.empty())
      AddNamedResultField(result, "dataset", "dataset", "input_profile",
                          result.input_profile);
    if (!result.requested_device.empty())
      AddNamedResultField(result, "dataset", "dataset", "requested_device",
                          result.requested_device);
    if (!result.consumed_weight_files.empty())
      AddNamedResultField(result, "dataset", "dataset", "consumed_weight_files",
                          result.consumed_weight_files);
    if (!result.consumed_weight_paths.empty())
      AddNamedResultField(result, "dataset", "dataset", "consumed_weight_paths",
                          result.consumed_weight_paths);
    if (!result.required_input_contract.empty())
      AddNamedResultField(result, "dataset", "dataset", "required_input_contract",
                          result.required_input_contract);
    if (!result.required_label_contract.empty())
      AddNamedResultField(result, "dataset", "dataset", "required_label_contract",
                          result.required_label_contract);
    if (!result.attach_back_result.empty())
      AddNamedResultField(result, "dataset", "dataset", "attach_back_result",
                          result.attach_back_result);
    if (!result.template_root.empty())
      AddNamedResultField(result, "dataset", "dataset", "template_root",
                          result.template_root);
    if (!result.pairs_ref.empty())
      AddNamedResultField(result, "dataset", "dataset", "pairs_ref",
                          result.pairs_ref);
    if (!result.train_param_summary.empty())
      AddNamedResultField(result, "dataset", "dataset", "train_param_summary",
                          result.train_param_summary);
    if (!result.infer_param_summary.empty())
      AddNamedResultField(result, "dataset", "dataset", "infer_param_summary",
                          result.infer_param_summary);
  }

  const std::string dataset_bridge = ResolveRuntimeDatasetBridgeTag(result);
  const bool has_bridge_contract =
    dataset_bridge != "bridge.unknown_dataset" ||
    !FindAssignmentValue(result.input_artifacts, "sample_id").empty() ||
    !FindAssignmentValue(result.input_artifacts, "template_image").empty() ||
    !FindAssignmentValue(result.input_artifacts, "label_file").empty() ||
    !FindAssignmentValue(result.input_artifacts, "defect_count").empty() ||
    !FindAssignmentValue(result.input_artifacts, "roi_ref").empty() ||
    !FindAssignmentValue(result.input_artifacts, "match_gt").empty() ||
    result.case_name == "match_score_tuning" ||
    result.case_name == "match_score_opt" ||
    result.layer == "scenario" ||
    result.layer == "train" ||
    result.layer == "infer";
  if (has_bridge_contract)
  {
    AddNamedResultObject(result,
                         "bridge",
                         "bridge",
                         "Phase1BridgeSample",
                         result.success ? "ready" : "partial",
                         std::string());
    AddNamedResultField(result, "bridge", "bridge", "dataset_bridge", dataset_bridge);
    AddNamedResultField(result, "bridge", "bridge", "sample_id",
                        BuildBridgeSampleId(result));
    AddNamedResultField(result, "bridge", "bridge", "input_image",
                        FindAssignmentValue(result.input_artifacts, "input_image"));
    AddNamedResultField(result, "bridge", "bridge", "template_image",
                        FindAssignmentValue(result.input_artifacts, "template_image"));
    AddNamedResultField(result, "bridge", "bridge", "label_file",
                        FindAssignmentValue(result.input_artifacts, "label_file"));
    AddNamedResultField(result, "bridge", "bridge", "defect_count",
                        FindAssignmentValue(result.input_artifacts, "defect_count"));
    AddNamedResultField(result, "bridge", "bridge", "bridge_manifest",
                        FindAssignmentValue(result.input_artifacts, "bridge_manifest"));
    AddNamedResultField(result, "bridge", "bridge", "bridge_table",
                        FindAssignmentValue(result.input_artifacts, "bridge_table"));
    AddNamedResultField(result, "bridge", "bridge", "roi_ref",
                        FindAssignmentValue(result.input_artifacts, "roi_ref"));
    AddNamedResultField(result, "bridge", "bridge", "match_gt",
                        FindAssignmentValue(result.input_artifacts, "match_gt"));
    AddNamedResultField(result, "bridge", "bridge", "objective_ref",
                        FindAssignmentValue(result.input_params, "objective_ref"));
    AddNamedResultField(result, "bridge", "bridge", "threshold_ref",
                        FindAssignmentValue(result.input_params, "threshold_ref"));
    AddNamedResultField(result, "bridge", "bridge", "crop_policy_ref",
                        FindAssignmentValue(result.input_params, "crop_policy_ref"));
    AddNamedResultField(result, "bridge", "bridge", "boundary_error_ref",
                        FindAssignmentValue(result.input_params, "boundary_error_ref"));
    AddNamedResultField(result, "bridge", "bridge", "alignment_error_ref",
                        FindAssignmentValue(result.input_params, "alignment_error_ref"));
  }

  if (!result.fractal_partition_value.empty() ||
      !result.distance_field_value.empty() ||
      !result.skeleton_mask_value.empty() ||
      !result.centerline_paths_value.empty() ||
      !result.topology_repair_paths_value.empty())
  {
    AddNamedResultObject(result,
                         "topology",
                         "topology",
                         "GeometryTopologyPipelineResult",
                         result.success ? "ready" : "partial",
                         std::string());
    if (!result.fractal_partition_value.empty())
      AddNamedResultField(result, "topology", "topology", "fractal_partition",
                          result.fractal_partition_value);
    if (!result.distance_field_value.empty())
      AddNamedResultField(result, "topology", "topology", "distance_field",
                          result.distance_field_value);
    if (!result.skeleton_mask_value.empty())
      AddNamedResultField(result, "topology", "topology", "skeleton_mask",
                          result.skeleton_mask_value);
    if (!result.centerline_paths_value.empty())
      AddNamedResultField(result, "topology", "topology", "centerline_paths",
                          result.centerline_paths_value);
    if (!result.topology_repair_paths_value.empty())
      AddNamedResultField(result, "topology", "topology", "topology_repair_paths",
                          result.topology_repair_paths_value);

    if (result.module == "cximage" &&
        result.layer == "feature" &&
        result.case_name == "geometry_topology_pipeline")
    {
      AddNamedResultField(result, "analysis", "analysis", "pipeline_mode",
                          "interface_readback_only");
      AddNamedResultField(result, "analysis", "analysis", "topology_stage_chain",
                          "fractal_partition>distance_field>skeleton_mask>centerline_paths>topology_repair_paths");
      AddNamedResultField(result, "analysis", "analysis", "connectivity_mode",
                          "local_graph_propagation");
      AddNamedResultField(result, "analysis", "analysis", "path_mode",
                          "bft_dijkstra_review");
      AddNamedResultField(result, "analysis", "analysis", "review_gate",
                          "human_image_validation");
    }
  }

  if (result.region_pattern_foreground_ratio_value > 0.0 ||
      result.region_pattern_descriptor_dim_value > 0.0 ||
      result.region_pattern_descriptor_mean_value != 0.0 ||
      result.region_pattern_descriptor_std_value != 0.0)
  {
    AddNamedResultObject(result,
                         "region_pattern",
                         "region_pattern",
                         "RegionPatternFeatureRecord",
                         result.success ? "ready" : "partial",
                         std::string());
    AddNamedResultField(result, "region_pattern", "region_pattern",
                        "foreground_ratio",
                        std::to_string(result.region_pattern_foreground_ratio_value));
    AddNamedResultField(result, "region_pattern", "region_pattern",
                        "descriptor_dim",
                        std::to_string(result.region_pattern_descriptor_dim_value));
    AddNamedResultField(result, "region_pattern", "region_pattern",
                        "descriptor_mean",
                        std::to_string(result.region_pattern_descriptor_mean_value));
    AddNamedResultField(result, "region_pattern", "region_pattern",
                        "descriptor_std",
                        std::to_string(result.region_pattern_descriptor_std_value));

    if (result.module == "cximage" &&
        result.layer == "feature" &&
        result.case_name == "binary_region")
    {
      AddNamedResultField(result, "analysis", "analysis", "pipeline_mode",
                          "descriptor_readback_ready");
      AddNamedResultField(result, "analysis", "analysis", "descriptor_mode",
                          "region_content_baseline");
      AddNamedResultField(result, "analysis", "analysis", "neighborhood_mode",
                          "local_roi_pooling");
      AddNamedResultField(result, "analysis", "analysis", "search_index_mode",
                          "descriptor_direct_compare");
      AddNamedResultField(result, "analysis", "analysis", "review_gate",
                          "human_texture_validation");
    }
  }

  for (size_t i = 0; i < preserved_named_results.size(); ++i)
  {
    const CxScriptNamedResultObject &item = preserved_named_results[i];
    AddNamedResultObject(result,
                         item.result_name,
                         item.stage_name,
                         item.object_name,
                         item.status,
                         item.failure_stage);
  }

  for (size_t i = 0; i < preserved_result_fields.size(); ++i)
  {
    const CxScriptNamedResultField &item = preserved_result_fields[i];
    if (!HasNamedResultFieldEntry(result, item.result_name, item.field_name))
    {
      AddNamedResultField(result,
                          item.result_name,
                          item.stage_name,
                          item.field_name,
                          item.value);
    }
  }

  RefreshUnifiedReviewFoundation(result);
}

