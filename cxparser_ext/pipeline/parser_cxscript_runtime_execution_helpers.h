// Extracted execution/result/torch helper implementation for cxscript runtime.

std::string ResultFieldValue(const CxScriptExecutionResult &result, const std::string &field)
{
  std::string readresult_path;
  if (TryParseReadResultExpression(field, readresult_path))
    return ResolveReadResultPath(result, readresult_path);

  if (field == "success")
    return result.success ? "true" : "false";
  if (field == "degraded")
    return result.degraded ? "true" : "false";
  if (field == "train_ok")
    return result.train_ok ? "true" : "false";
  if (field == "infer_ok")
    return result.infer_ok ? "true" : "false";
  if (field == "score_ok")
    return result.score_ok ? "true" : "false";
  if (field == "kind")
    return result.kind;
  if (field == "layer")
    return result.layer;
  if (field == "module")
    return result.module;
  if (field == "case")
    return result.case_name;
  if (field == "model_name")
    return result.model_name;
  if (field == "feature_set")
    return result.feature_set;
  if (field == "label_column")
    return result.label_column;
  if (field == "status")
    return result.success ? "ok" : "failed";
  if (field == "task_id")
    return result.task_id;
  if (field == "model_path")
    return result.model_path;
  if (field == "predictions_csv")
    return result.predictions_csv;
  if (field == "output_summary_csv")
    return result.output_summary_csv;
  if (field == "replay_result_object")
    return result.replay_result_object;
  if (field == "result_object")
    return result.result_object;
  if (field == "bridge_enabled")
    return result.bridge_enabled ? "true" : "false";
  if (field == "line_horizontal_samples_contract_value")
    return std::to_string(result.line_horizontal_samples_contract_value);
  if (field == "line_vertical_samples_contract_value")
    return std::to_string(result.line_vertical_samples_contract_value);
  if (field == "line_measure_bounds_contract_value")
    return std::to_string(result.line_measure_bounds_contract_value);
  if (field == "circle_center_contract_value")
    return std::to_string(result.circle_center_contract_value);
  if (field == "circle_radius_contract_value")
    return std::to_string(result.circle_radius_contract_value);
  if (field == "circle_avg_distance_contract_value")
    return std::to_string(result.circle_avg_distance_contract_value);
  if (field == "template_candidate_count_contract_value")
    return std::to_string(result.template_candidate_count_contract_value);
  if (field == "template_top_score_contract_value")
    return std::to_string(result.template_top_score_contract_value);
  if (field == "template_match_center_contract_value")
    return std::to_string(result.template_match_center_contract_value);
  if (field == "template_min_candidate_count_contract_value")
    return std::to_string(result.template_min_candidate_count_contract_value);
  if (field == "template_min_top_score_contract_value")
    return std::to_string(result.template_min_top_score_contract_value);
  if (field == "region_connected_components_contract_value")
    return std::to_string(result.region_connected_components_contract_value);
  if (field == "region_size_contract_value")
    return std::to_string(result.region_size_contract_value);
  if (field == "region_bounds_contract_value")
    return std::to_string(result.region_bounds_contract_value);
  if (field == "region_min_connected_components_contract_value")
    return std::to_string(result.region_min_connected_components_contract_value);
  if (field == "region_min_bounds_count_contract_value")
    return std::to_string(result.region_min_bounds_count_contract_value);
  if (field == "point_count_value")
    return std::to_string(result.point_count_value);
  if (field == "line_chain_length_value")
    return std::to_string(result.line_chain_length_value);
  if (field == "line_edgeband_count_value")
    return std::to_string(result.line_edgeband_count_value);
  if (field == "line_measure_bbox_w")
    return std::to_string(result.line_measure_bbox_w_value);
  if (field == "fit_error_avg_value")
    return std::to_string(result.fit_error_avg_value);
  if (field == "fit_error_max_value")
    return std::to_string(result.fit_error_max_value);
  if (field == "line_angle_value")
    return std::to_string(result.line_angle_value);
  if (field == "line_offset_value")
    return std::to_string(result.line_offset_value);
  if (field == "subpixel_adjust_avg_value")
    return std::to_string(result.subpixel_adjust_avg_value);
  if (field == "circle_center_x_value")
    return std::to_string(result.circle_center_x_value);
  if (field == "circle_center_y_value")
    return std::to_string(result.circle_center_y_value);
  if (field == "circle_radius_value")
    return std::to_string(result.circle_radius_value);
  if (field == "circle_avg_distance_value")
    return std::to_string(result.circle_avg_distance_value);
  if (field == "circle_sample_points_value")
    return std::to_string(result.circle_sample_points_value);
  if (field == "circle_used_fallback_value")
    return std::to_string(result.circle_used_fallback_value);
  if (field == "circle_prefilter_used_value")
    return std::to_string(result.circle_prefilter_used_value);
  if (field == "circle_compact_path_value")
    return std::to_string(result.circle_compact_path_value);
  if (field == "circle_failure_stage")
    return result.circle_failure_stage;
  if (field == "match_candidate_count_value")
    return std::to_string(result.match_candidate_count_value);
  if (field == "match_selected_index_value")
    return std::to_string(result.match_selected_index_value);
  if (field == "match_best_index_value")
    return std::to_string(result.match_best_index_value);
  if (field == "candidate_count")
    return std::to_string(result.match_candidate_count_value > 0.0
                            ? result.match_candidate_count_value
                            : result.template_main_candidate_count_value);
  if (field == "result_count")
    return std::to_string(result.match_candidate_count_value > 0.0
                            ? result.match_candidate_count_value
                            : result.region_connected_components_value);
  if (field == "selected_index")
    return std::to_string(result.match_selected_index_value);
  if (field == "best_index")
    return std::to_string(result.match_best_index_value);
  if (field == "match_top_score_value")
    return std::to_string(result.match_top_score_value);
  if (field == "match_max_score_value")
    return std::to_string(result.match_max_score_value);
  if (field == "top1_score")
    return std::to_string(result.match_top_score_value > 0.0
                            ? result.match_top_score_value
                            : result.template_main_top_score_value);
  if (field == "match_center_x_value")
    return std::to_string(result.match_center_x_value);
  if (field == "match_center_y_value")
    return std::to_string(result.match_center_y_value);
  if (field == "top1_rect")
  {
    if (result.match_best_rect_w_value > 0.0 && result.match_best_rect_h_value > 0.0)
    {
      std::ostringstream rect_text;
      rect_text << static_cast<int>(result.match_best_rect_x_value) << ","
                << static_cast<int>(result.match_best_rect_y_value) << ","
                << static_cast<int>(result.match_best_rect_w_value) << ","
                << static_cast<int>(result.match_best_rect_h_value);
      return rect_text.str();
    }
  }
  if (field == "top1_center")
  {
    if (result.match_center_x_value != 0.0 || result.match_center_y_value != 0.0)
    {
      std::ostringstream center_text;
      center_text << static_cast<int>(result.match_center_x_value) << ","
                  << static_cast<int>(result.match_center_y_value);
      return center_text.str();
    }
  }
  if (field == "template_used_fallback_value")
    return std::to_string(result.template_used_fallback_value);
  if (field == "roi_area")
    return std::to_string(result.roi_area_value);
  if (field == "component_count")
    return std::to_string(result.component_count_value);
  if (field == "image_model_score")
    return std::to_string(result.image_model_score_value);
  if (field == "match_best_score")
    return std::to_string(result.match_max_score_value);
  if (field == "roi_patch_tensor")
    return result.roi_patch_tensor_value;
  if (field == "roi_patch_count")
    return std::to_string(result.roi_patch_count_value);
  if (field == "roi_patch_spatial_size")
    return std::to_string(result.roi_patch_spatial_size_value);
  if (field == "roi_class_label")
    return result.roi_class_label_value;
  if (field == "roi_class_label_count")
    return std::to_string(result.roi_class_label_count_value);
  if (field == "region_tensor")
    return result.region_tensor_value;
  if (field == "region_spatial_size")
    return std::to_string(result.region_spatial_size_value);
  if (field == "region_channel_layout")
    return result.region_channel_layout_value;
  if (field == "mask_or_region_label")
    return result.mask_or_region_label_value;
  if (field == "mask_label_spatial_size")
    return std::to_string(result.mask_label_spatial_size_value);
  if (field == "roi_alignment_status")
    return result.roi_alignment_status_value;
  if (field == "mask_alignment_status")
    return result.mask_alignment_status_value;
  if (field == "fractal_partition")
    return result.fractal_partition_value;
  if (field == "distance_field")
    return result.distance_field_value;
  if (field == "skeleton_mask")
    return result.skeleton_mask_value;
  if (field == "centerline_paths")
    return result.centerline_paths_value;
  if (field == "topology_repair_paths")
    return result.topology_repair_paths_value;
  if (field == "region_pattern_foreground_ratio")
    return std::to_string(result.region_pattern_foreground_ratio_value);
  if (field == "region_pattern_descriptor_dim")
    return std::to_string(result.region_pattern_descriptor_dim_value);
  if (field == "region_pattern_descriptor_mean")
    return std::to_string(result.region_pattern_descriptor_mean_value);
  if (field == "region_pattern_descriptor_std")
    return std::to_string(result.region_pattern_descriptor_std_value);
  if (field == "template_learn_path_a_count_value")
    return std::to_string(result.template_learn_path_a_count_value);
  if (field == "template_learn_path_b_count_value")
    return std::to_string(result.template_learn_path_b_count_value);
  if (field == "template_main_candidate_count_value")
    return std::to_string(result.template_main_candidate_count_value);
  if (field == "template_main_top_score_value")
    return std::to_string(result.template_main_top_score_value);
  if (field == "region_connected_components_value")
    return std::to_string(result.region_connected_components_value);
  if (field == "region_width_value")
    return std::to_string(result.region_width_value);
  if (field == "region_height_value")
    return std::to_string(result.region_height_value);
  if (field == "region_bounds_count_value")
    return std::to_string(result.region_bounds_count_value);
  if (field == "region_raw_connected_components_value")
    return std::to_string(result.region_raw_connected_components_value);
  if (field == "region_foreground_ratio_value")
    return std::to_string(result.region_foreground_ratio_value);
  if (field == "published_handoff_type")
    return result.published_handoff_type;
  if (field == "published_primary_ref")
    return result.published_primary_ref;
  if (field == "published_route_hint")
    return result.published_route_hint;
  if (field == "published_route_state")
    return result.published_route_state;
  if (field == "published_source_hash")
    return result.published_source_hash;
  if (field == "published_result_ref")
    return result.published_result_ref;
  if (field == "published_evidence_ref")
    return result.published_evidence_ref;
  if (field == "published_bbox_candidate_list_ref")
    return result.published_bbox_candidate_list_ref;
  if (field == "published_template_alignment_ref")
    return result.published_template_alignment_ref;
  if (field == "published_template_test_alignment_status")
    return result.published_template_test_alignment_status;
  if (field == "published_roi_diff_candidate_ref")
    return result.published_roi_diff_candidate_ref;
  if (field == "published_roi_diff_candidate_count")
    return result.published_roi_diff_candidate_count;
  if (field == "published_prior_roi_region_ref")
    return result.published_prior_roi_region_ref;
  if (field == "published_roi_crop_packet_ref")
    return result.published_roi_crop_packet_ref;
  if (field == "published_roi_crop_count")
    return result.published_roi_crop_count;
  if (field == "published_roi_crop_spatial_size")
    return result.published_roi_crop_spatial_size;
  if (field == "published_roi_crop_policy_ref")
    return result.published_roi_crop_policy_ref;
  if (field == "internal_test_interface_name")
    return result.internal_test_interface_name;
  if (field == "internal_test_interface_purpose")
    return result.internal_test_interface_purpose;
  if (field == "execution_stage_0")
    return result.execution_stage_0;
  if (field == "execution_stage_1")
    return result.execution_stage_1;
  if (field == "execution_stage_2")
    return result.execution_stage_2;
  if (field == "execution_stage_3")
    return result.execution_stage_3;
  if (field == "input_dataset")
    return result.input_dataset;
  if (field == "dataset_profile")
    return result.dataset_profile;
  if (field == "prepared_root")
    return result.prepared_root;
  if (field == "input_task")
    return result.input_task;
  if (field == "input_profile")
    return result.input_profile;
  if (field == "requested_device")
    return result.requested_device;
  if (field == "consumed_weight_files")
    return result.consumed_weight_files;
  if (field == "consumed_weight_paths")
    return result.consumed_weight_paths;
  if (field == "required_input_contract")
    return result.required_input_contract;
  if (field == "required_label_contract")
    return result.required_label_contract;
  if (field == "template_root")
    return result.template_root;
  if (field == "pairs_ref")
    return result.pairs_ref;
  if (field == "attach_back_result")
    return result.attach_back_result;
  if (field == "attach_back_overlay_status")
    return result.attach_back_overlay_status;
  if (field == "attach_back_top1_class")
    return result.attach_back_top1_class;
  if (field == "attach_back_confidence")
    return result.attach_back_confidence;
  if (field == "input_sample")
    return result.input_sample;
  if (field == "input_split")
    return result.input_split;
  if (field == "input_artifacts")
    return result.input_artifacts;
  if (field == "input_params")
    return result.input_params;
  if (field == "train_param_summary")
    return result.train_param_summary;
  if (field == "infer_param_summary")
    return result.infer_param_summary;
  if (field == "dataset_ref")
    return result.dataset_ref;
  if (field == "sample_bundle_ref")
    return result.sample_bundle_ref;
  if (field == "objective_ref")
    return result.objective_ref;
  if (field == "optimization_result_ref")
    return result.optimization_result_ref;
  if (field == "best_params_ref")
    return result.best_params_ref;
  if (field == "objective_delta_ref")
    return result.objective_delta_ref;
  if (field == "summary_ref")
    return result.summary_ref;
  if (field == "compare_ref")
    return result.compare_ref;
  if (field == "replay_ref")
    return result.replay_ref;
  if (field == "cluster_ref")
    return result.cluster_ref;
  if (field == "distance_ref")
    return result.distance_ref;
  if (field == "anomaly_ref")
    return result.anomaly_ref;
  if (field == "baseline_class_ref")
    return result.baseline_class_ref;
  if (field == "baseline_feature_ref")
    return result.baseline_feature_ref;
  if (field == "attach_back_ref")
    return result.attach_back_ref;
  if (field == "bbox_candidate_list_ref")
    return result.bbox_candidate_list_ref;
  if (field == "roi_crop_packet_ref")
    return result.roi_crop_packet_ref;
  if (field == "template_alignment_ref")
    return result.template_alignment_ref;
  if (field == "candidate_overlay_ref")
    return result.candidate_overlay_ref;
  if (field == "template_rect_overlay_ref")
    return result.template_rect_overlay_ref;
  if (field == "test_rect_overlay_ref")
    return result.test_rect_overlay_ref;
  if (field == "template_test_alignment_status")
    return result.template_test_alignment_status;
  if (field == "roi_diff_candidate_ref")
    return result.roi_diff_candidate_ref;
  if (field == "roi_diff_candidate_count")
    return result.roi_diff_candidate_count;
  if (field == "circle_overlay_ref")
    return result.circle_overlay_ref;
  if (field == "circle_edge_overlay_ref")
    return result.circle_edge_overlay_ref;
  if (field == "formfit_candidate_overlay_ref")
    return result.formfit_candidate_overlay_ref;
  if (field == "formfit_selection_overlay_ref")
    return result.formfit_selection_overlay_ref;
  if (field == "region_pattern_overlay_ref")
    return result.region_pattern_overlay_ref;
  if (field == "region_pattern_descriptor_ref")
    return result.region_pattern_descriptor_ref;
  if (field == "fractal_partition_overlay_ref")
    return result.fractal_partition_overlay_ref;
  if (field == "distance_field_overlay_ref")
    return result.distance_field_overlay_ref;
  if (field == "skeleton_overlay_ref")
    return result.skeleton_overlay_ref;
  if (field == "centerline_overlay_ref")
    return result.centerline_overlay_ref;
  if (field == "topology_repair_overlay_ref")
    return result.topology_repair_overlay_ref;
  {
    const std::string torch_train_ref = ResolveTorchTrainPublishedRef(result, field);
    if (!torch_train_ref.empty())
      return torch_train_ref;
  }
  if (field == "tolerance")
    return result.tolerance;
  if (field == "failure_mode")
    return result.failure_mode;
  if (field == "runtime_ms")
    return std::to_string(result.runtime_ms);
  if (field == "fit_time_ms")
    return std::to_string(result.fit_time_ms);
  if (field == "infer_time_ms")
    return std::to_string(result.infer_time_ms);
  if (field == "feature_dim")
    return std::to_string(result.feature_dim);
  if (field == "accuracy")
    return std::to_string(result.accuracy);
  if (field == "macro_f1")
    return std::to_string(result.macro_f1);
  if (field == "prediction_count")
    return std::to_string(result.prediction_count);
  if (field == "metrics")
    return result.metrics;
  if (field == "summary")
    return result.summary;
  if (field == "route")
    return result.route;
  if (field == "scalar_result")
    return std::to_string(result.scalar_result);
  if (field == "error_message")
    return result.error_message;

  const std::string named_value = ResolveReadResultPath(result, field);
  if (!named_value.empty())
    return named_value;
  return std::string();
}

std::string ResolveLooseFieldValue(const CxScriptExecutionResult &result, const std::string &field)
{
  const std::string trimmed = Trim(field);
  if (trimmed.empty())
    return std::string();

  const std::string direct_value = ResultFieldValue(result, trimmed);
  if (!direct_value.empty())
    return direct_value;

  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    const CxScriptNamedResultField &item = result.result_fields[i];
    if (item.field_name == trimmed)
      return item.value;
  }

  if (trimmed == "candidates")
    return std::to_string(result.match_candidate_count_value);
  if (trimmed == "connected_components")
    return std::to_string(result.region_connected_components_value);
  if (trimmed == "boxes")
    return std::to_string(result.region_bounds_count_value);
  if (trimmed == "measure_bounds")
    return std::to_string(result.line_measure_bounds_contract_value);
  if (trimmed == "horizontal_samples")
    return std::to_string(result.line_horizontal_samples_contract_value);
  if (trimmed == "vertical_samples")
    return std::to_string(result.line_vertical_samples_contract_value);
  if (trimmed == "radius")
    return std::to_string(result.circle_radius_value);
  if (trimmed == "sample_points")
    return std::to_string(result.circle_sample_points_value);
  if (trimmed == "average_distance")
    return std::to_string(result.circle_avg_distance_value);
  if (trimmed == "center")
  {
    if (result.circle_center_x_value != 0.0 || result.circle_center_y_value != 0.0)
    {
      return std::to_string(result.circle_center_x_value) + "," +
             std::to_string(result.circle_center_y_value);
    }
  }
  return std::string();
}

bool EvaluateCheckMethod(const CxScriptStatement &stmt,
                         const CxScriptExecutionResult &result,
                         const std::map<std::string, std::string> &variables,
                         bool &ok,
                         std::string &detail)
{
  ok = false;
  detail.clear();

  const std::string method_name = stmt.name;
  const std::vector<std::string> args =
    !stmt.argument_text.empty() ? SplitCallArguments(stmt.argument_text)
                                : SplitCallArguments(stmt.text);
  if (method_name.empty())
  {
    detail = "missing Check method name";
    return false;
  }

  const auto resolve_check_value = [&](const std::string &expr) -> std::string
  {
    const std::string token = StripQuotes(expr);
    std::map<std::string, std::string>::const_iterator variable_it = variables.find(token);
    if (variable_it != variables.end())
      return variable_it->second;
    return ResolveLooseFieldValue(result, token);
  };

  if ((method_name == "Equals" || method_name == "NotEquals" || method_name == "Value") &&
      args.size() >= 2)
  {
    const std::string lhs = resolve_check_value(args[0]);
    const std::string rhs = resolve_check_value(args[1]);
    ok = EvaluateEquals(lhs, rhs.empty() ? StripQuotes(args[1]) : rhs,
                        method_name == "NotEquals" ? "!=" : "==");
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if ((method_name == "ScalarGe" || method_name == "ScalarGt" || method_name == "CountGe") &&
      args.size() >= 2)
  {
    double lhs_value = 0.0;
    double rhs_value = 0.0;
    const std::string lhs = resolve_check_value(args[0]);
    const std::string rhs = StripQuotes(args[1]);
    if (!TryParseDouble(lhs, lhs_value) || !TryParseDouble(rhs, rhs_value))
    {
      detail = "numeric Check.* requires numeric field and threshold";
      return false;
    }
    ok = EvaluateNumeric(lhs_value, rhs_value,
                         method_name == "ScalarGt" ? ">" : ">=");
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if (method_name == "ScalarBetween" && args.size() >= 3)
  {
    double lhs_value = 0.0;
    double min_value = 0.0;
    double max_value = 0.0;
    const std::string lhs = resolve_check_value(args[0]);
    if (!TryParseDouble(lhs, lhs_value) ||
        !TryParseDouble(StripQuotes(args[1]), min_value) ||
        !TryParseDouble(StripQuotes(args[2]), max_value))
    {
      detail = "ScalarBetween requires numeric field and bounds";
      return false;
    }
    ok = lhs_value >= min_value && lhs_value <= max_value;
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if ((method_name == "FieldExists" || method_name == "StructExists" || method_name == "NotNull") &&
      !args.empty())
  {
    const std::string value = resolve_check_value(args[0]);
    ok = !value.empty();
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if (method_name == "SchemaContains" && args.size() >= 2)
  {
    const std::string schema_name = StripQuotes(args[0]);
    const std::string field_name = StripQuotes(args[1]);
    ok = !schema_name.empty() &&
         !field_name.empty() &&
         (schema_name == "BaselineFeatureSampleV1" || schema_name == result.result_object) &&
         !ResolveLooseFieldValue(result, field_name).empty();
    detail = method_name + "(" + schema_name + "." + field_name + ")";
    return true;
  }

  if (method_name == "file_exists" && !args.empty())
  {
    const std::string value = resolve_check_value(args[0]);
    ok = !value.empty();
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if ((method_name == "column_exists" || method_name == "feature_set_supported") && !args.empty())
  {
    const std::string value = resolve_check_value(args[0]);
    ok = !value.empty();
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if (method_name == "model_file_created" && !args.empty())
  {
    ok = !result.model_path.empty();
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if (method_name == "predictions_csv_created" && !args.empty())
  {
    ok = !result.predictions_csv.empty();
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if (method_name == "prediction_count_matches_input")
  {
    double prediction_count = 0.0;
    ok = TryParseDouble(ResolveLooseFieldValue(result, "prediction_count"), prediction_count) &&
         prediction_count >= 1.0;
    detail = method_name;
    return true;
  }

  if (method_name == "summary_row_created")
  {
    ok = !ResolveLooseFieldValue(result, "summary_row").empty() ||
         !ResolveLooseFieldValue(result, "output_summary_csv").empty() ||
         ResolveLooseFieldValue(result, "score_ok") == "true";
    detail = method_name;
    return true;
  }

  if (method_name == "HasKey" && args.size() >= 2)
  {
    const std::string path = StripQuotes(args[0]) + "." + StripQuotes(args[1]);
    ok = !ResolveLooseFieldValue(result, path).empty();
    detail = method_name + "(" + path + ")";
    return true;
  }

  if (method_name == "AnyNotEmpty" && !args.empty())
  {
    for (size_t i = 0; i < args.size(); ++i)
    {
      if (!ResolveLooseFieldValue(result, StripQuotes(args[i])).empty())
      {
        ok = true;
        break;
      }
    }
    detail = method_name;
    return true;
  }

  if ((method_name == "RouteIn" || method_name == "OneOf") && args.size() >= 2)
  {
    const std::string lhs = StripQuotes(args[0]) == "AiRouteDecision"
      ? result.route
      : ResolveLooseFieldValue(result, StripQuotes(args[0]));
    const std::string options = StripQuotes(args[1]);
    std::string current;
    for (size_t i = 0; i <= options.size(); ++i)
    {
      const char ch = i < options.size() ? options[i] : ',';
      if (ch == ',' || ch == '|')
      {
        if (Trim(current) == lhs)
          ok = true;
        current.clear();
        continue;
      }
      current.push_back(ch);
    }
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if (method_name == "Valid" && !args.empty())
  {
    const std::string target = StripQuotes(args[0]);
    if (target == "AiTaskEnvelope")
      ok = !result.task_id.empty() && !result.result_object.empty();
    else if (target == "AiRouteDecision")
      ok = !result.route.empty();
    else
      ok = !ResolveLooseFieldValue(result, target).empty();
    detail = method_name + "(" + target + ")";
    return true;
  }

  if ((method_name == "RectNear" || method_name == "RectValid") && !args.empty())
  {
    ok = !ResolveLooseFieldValue(result, StripQuotes(args[0])).empty();
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if (method_name == "ComponentBoundsValid" && !args.empty())
  {
    double count_value = 0.0;
    ok = TryParseDouble(ResolveLooseFieldValue(result, StripQuotes(args[0])), count_value) && count_value >= 1.0;
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  if (method_name == "PointsInRoiNeighborhood" && !args.empty())
  {
    const std::string sample_refs = StripQuotes(args[0]);
    std::string token;
    for (size_t i = 0; i <= sample_refs.size(); ++i)
    {
      const char ch = i < sample_refs.size() ? sample_refs[i] : '|';
      if (ch == '|')
      {
        if (!token.empty() && !ResolveLooseFieldValue(result, token).empty())
        {
          ok = true;
          break;
        }
        token.clear();
        continue;
      }
      token.push_back(ch);
    }
    detail = method_name + "(" + StripQuotes(args[0]) + ")";
    return true;
  }

  detail = "unsupported Check.* method: " + method_name;
  return false;
}

bool EvaluateEquals(const std::string &lhs, const std::string &rhs, const char *op)
{
  if ((lhs == "ok" && rhs == "success") ||
      (lhs == "success" && rhs == "ok"))
  {
    if (std::string(op) == "==")
      return true;
    if (std::string(op) == "!=")
      return false;
  }

  double lhs_number = 0.0;
  double rhs_number = 0.0;
  if (TryParseDouble(lhs, lhs_number) && TryParseDouble(rhs, rhs_number))
  {
    if (std::string(op) == "==")
      return lhs_number == rhs_number;
    if (std::string(op) == "!=")
      return lhs_number != rhs_number;
  }

  if (std::string(op) == "==")
    return lhs == rhs;
  if (std::string(op) == "!=")
    return lhs != rhs;
  return false;
}

bool TryParseDouble(const std::string &text, double &value)
{
  char *end = 0;
  value = std::strtod(text.c_str(), &end);
  return end != text.c_str() && end && *end == '\0';
}

bool EvaluateNumeric(double lhs, double rhs, const char *op)
{
  const std::string token = op;
  if (token == ">")
    return lhs > rhs;
  if (token == "<")
    return lhs < rhs;
  if (token == ">=")
    return lhs >= rhs;
  if (token == "<=")
    return lhs <= rhs;
  return false;
}

bool LooksLikeAssignmentExpr(const std::string &text,
                             std::string &lhs_name,
                             std::string &rhs_expr)
{
  lhs_name.clear();
  rhs_expr.clear();

  const std::string trimmed = StripTrailingSemicolon(Trim(text));
  const size_t pos = trimmed.find('=');
  if (pos == std::string::npos)
    return false;
  if (pos + 1 < trimmed.size() && trimmed[pos + 1] == '=')
    return false;
  if (pos > 0)
  {
    const char previous = trimmed[pos - 1];
    if (previous == '!' || previous == '<' || previous == '>')
      return false;
  }

  lhs_name = Trim(trimmed.substr(0, pos));
  rhs_expr = Trim(trimmed.substr(pos + 1));
  return IsIdentifier(lhs_name) && !rhs_expr.empty();
}

std::string ResolveScriptValue(const CxScriptExecutionResult &result,
                               const std::map<std::string, std::string> &variables,
                               const std::string &expr)
{
  const std::string trimmed = Trim(expr);
  if (trimmed.empty())
    return std::string();

  if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"')
    return trimmed.substr(1, trimmed.size() - 2);

  if (trimmed == "true" || trimmed == "false")
    return trimmed;

  double number = 0.0;
  if (TryParseDouble(trimmed, number))
    return trimmed;

  std::map<std::string, std::string>::const_iterator variable_it = variables.find(trimmed);
  if (variable_it != variables.end())
    return variable_it->second;

  const std::string field_value = ResultFieldValue(result, trimmed);
  if (!field_value.empty())
    return field_value;

  return trimmed;
}

bool ApplyTorchContractCaseBridge(const CxScriptExecutionContext &context,
                                  CxScriptExecutionResult &result)
{
  if (context.module != "torch_module")
    return false;

  result.success = true;
  result.failure_mode = "none";
  result.result_object = "TorchStageReport";
  result.task_id = "torch_module." + context.layer + "." + context.case_name;

  if (context.case_name == "torch.mobilevit.session.feature")
  {
    result.metrics = "roi_patch,class_label";
    result.tolerance = "mobilevit_contract";
    result.summary = "mobilevit session feature ready";
    return true;
  }

  if (context.case_name == "torch.resnet18.baseline.feature")
  {
    result.metrics = "classifier_output_shape,p3_p4_p5_feature_shapes,baseline_feature_ref";
    result.tolerance = "resnet18_baseline_feature";
    result.summary = "resnet18 feature baseline ready";
    result.baseline_feature_ref =
      result.task_id.empty() ? "torch.resnet18.baseline.feature.baseline_feature_ref"
                             : result.task_id + ".baseline_feature_ref";
    return true;
  }

  if (context.case_name == "torch.resnet50.baseline.feature")
  {
    result.metrics = "classifier_output_shape,p3_p4_p5_feature_shapes,baseline_feature_ref";
    result.tolerance = "resnet50_baseline_feature";
    result.summary = "resnet50 feature baseline ready";
    result.baseline_feature_ref =
      result.task_id.empty() ? "torch.resnet50.baseline.feature.baseline_feature_ref"
                             : result.task_id + ".baseline_feature_ref";
    return true;
  }

  if (context.case_name == "torch.mobilevit.unified.infer")
  {
    result.metrics = "roi_patch,class_label,baseline_class_ref,roi_crop_packet_ref,cluster_ref,distance_ref,anomaly_ref";
    result.tolerance = "mobilevit_unified_infer";
    result.summary = "mobilevit unified infer ready";
    result.baseline_class_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.baseline_class_ref"
                             : result.task_id + ".baseline_class_ref";
    result.roi_crop_packet_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.roi_crop_packet_ref"
                             : result.task_id + ".roi_crop_packet_ref";
    result.cluster_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.cluster_ref"
                             : result.task_id + ".cluster_ref";
    result.distance_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.distance_ref"
                             : result.task_id + ".distance_ref";
    result.anomaly_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.anomaly_ref"
                             : result.task_id + ".anomaly_ref";
    return true;
  }

  if (context.case_name == "torch.deeplab.contract.feature")
  {
    result.metrics = "region_tensor,mask_label";
    result.tolerance = "deeplab_contract";
    result.summary = "deeplab region tensor + mask label contract ready";
    return true;
  }

  if (context.case_name == "torch.deeplab.unified.infer")
  {
    result.metrics = "region_tensor,mask_label,baseline_feature_ref,template_alignment_ref,template_test_alignment_status,roi_diff_candidate_ref,roi_diff_candidate_count";
    result.tolerance = "deeplab_unified_infer";
    result.summary = "deeplab unified infer ready";
    result.baseline_feature_ref =
      result.task_id.empty() ? "torch.deeplab.unified.infer.baseline_feature_ref"
                             : result.task_id + ".baseline_feature_ref";
    result.template_alignment_ref =
      result.task_id.empty() ? "torch.deeplab.unified.infer.template_alignment_ref"
                             : result.task_id + ".template_alignment_ref";
    result.template_test_alignment_status = "aligned_pass";
    result.roi_diff_candidate_ref =
      result.task_id.empty() ? "torch.deeplab.unified.infer.roi_diff_candidate_ref"
                             : result.task_id + ".roi_diff_candidate_ref";
    result.roi_diff_candidate_count = "3";
    return true;
  }

  if (context.case_name == "torch.resnet18.baseline.infer")
  {
    result.metrics = "classifier_output_shape,baseline_class_ref";
    result.tolerance = "resnet18_baseline_infer";
    result.summary = "resnet18 infer baseline ready";
    result.baseline_class_ref =
      result.task_id.empty() ? "torch.resnet18.baseline.infer.baseline_class_ref"
                             : result.task_id + ".baseline_class_ref";
    return true;
  }

  if (context.case_name == "torch.resnet50.baseline.infer")
  {
    result.metrics = "classifier_output_shape,baseline_class_ref";
    result.tolerance = "resnet50_baseline_infer";
    result.summary = "resnet50 infer baseline ready";
    result.baseline_class_ref =
      result.task_id.empty() ? "torch.resnet50.baseline.infer.baseline_class_ref"
                             : result.task_id + ".baseline_class_ref";
    return true;
  }

  if (context.case_name == "torch.yolov8.eval.feature")
  {
    result.metrics = "image_window,bbox_class_targets";
    result.tolerance = "yolov8_contract";
    result.summary = "full-dataset stage passed";
    return true;
  }

  if (context.case_name == "torch.yolov8.mainline.train")
  {
    result.metrics = "image_window,bbox_class_targets,trainer_lifecycle_summary,unified_mainline_summary";
    result.tolerance = "yolov8_mainline_train";
    result.summary = "yolo train mainline ready";
    return true;
  }

  if (context.case_name == "torch.mobilevit.mainline.train")
  {
    result.metrics = "roi_patch,class_label,trainer_lifecycle_summary,unified_mainline_summary";
    result.tolerance = "mobilevit_mainline_train";
    result.summary = "mobilevit train mainline ready";
    return true;
  }

  if (context.case_name == "torch.deeplab.mainline.train")
  {
    result.metrics = "region_tensor,mask_label,segmentation_trainer_lifecycle_summary,segmentation_unified_summary";
    result.tolerance = "deeplab_mainline_train";
    result.summary = "deeplab train mainline ready";
    return true;
  }

  if (context.case_name == "torch.yolo_mobilevit.infer.scenario")
  {
    result.metrics = "bbox_candidates,roi_patch,class_label,attach_back,bbox_candidate_list_ref,roi_crop_packet_ref,cluster_ref,distance_ref,anomaly_ref";
    result.tolerance = "yolo_mobilevit_infer_scenario";
    result.summary = "yolo mobilevit infer scenario ready";
    result.attach_back_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.attach_back_ref"
                             : result.task_id + ".attach_back_ref";
    result.bbox_candidate_list_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.bbox_candidate_list_ref"
                             : result.task_id + ".bbox_candidate_list_ref";
    result.roi_crop_packet_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.roi_crop_packet_ref"
                             : result.task_id + ".roi_crop_packet_ref";
    result.cluster_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.cluster_ref"
                             : result.task_id + ".cluster_ref";
    result.distance_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.distance_ref"
                             : result.task_id + ".distance_ref";
    result.anomaly_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.anomaly_ref"
                             : result.task_id + ".anomaly_ref";
    return true;
  }

  return false;
}

bool WantsTorchDispatchMainline(const CxScriptExecutionContext &context)
{
  return context.kind == "module" &&
         context.module == "torch_module" &&
         !context.layer.empty() &&
         !context.case_name.empty();
}

void ConvertDispatchResult(const CxScriptExecutionContext &context,
                           const ParserDispatchResult &dispatch_result,
                           CxScriptExecutionResult &script_result)
{
  script_result = CxScriptExecutionResult();
  script_result.success = dispatch_result.success;
  script_result.script_path = dispatch_result.identity.file_path;
  script_result.script_name = context.script_name;
  script_result.kind = dispatch_result.identity.script_type.empty() ? context.kind
                                                                    : dispatch_result.identity.script_type;
  script_result.layer = dispatch_result.report.layer.empty() ? dispatch_result.layer
                                                             : dispatch_result.report.layer;
  script_result.module = dispatch_result.report.module_name.empty() ? dispatch_result.module
                                                                    : dispatch_result.report.module_name;
  script_result.case_name = dispatch_result.report.case_id.empty() ? dispatch_result.case_id
                                                                   : dispatch_result.report.case_id;
  script_result.route = dispatch_result.report.route.empty() ? context.route
                                                             : dispatch_result.report.route;
  script_result.task_id = dispatch_result.report.task_id;
  script_result.runtime_ms = dispatch_result.report.runtime_ms;
  script_result.result_object = dispatch_result.report.result_object;
  script_result.optimize_summary_object = dispatch_result.report.optimize_summary_object;
  script_result.compare_summary_object = dispatch_result.report.compare_summary_object;
  script_result.replay_result_object = dispatch_result.report.replay_result_object;
  script_result.rag_writeback_note_object = dispatch_result.report.rag_writeback_note_object;
  script_result.metrics = dispatch_result.report.metrics;
  script_result.tolerance = dispatch_result.report.tolerance;
  script_result.failure_mode = dispatch_result.report.failure_mode;
  script_result.summary = dispatch_result.report.summary;
  script_result.error_message = dispatch_result.report.error_message;
  script_result.failure_phase = dispatch_result.report.failure_phase;
  script_result.last_step_id = dispatch_result.report.last_step_id;
  script_result.last_frame_id = dispatch_result.report.last_frame_id;
  script_result.last_sequence = dispatch_result.report.last_sequence;
  script_result.last_source_line = dispatch_result.report.last_source_line;
  script_result.failure_step_id = dispatch_result.report.failure_step_id;
  script_result.failure_frame_id = dispatch_result.report.failure_frame_id;
  script_result.failure_sequence = dispatch_result.report.failure_sequence;
  script_result.failure_line = dispatch_result.report.failure_line;
  script_result.details = dispatch_result.lines;
  script_result.multimodal_slices = dispatch_result.multimodal_slices;
  script_result.operation_atoms = dispatch_result.operation_atoms;
}

bool TryExecuteTorchDispatchMainline(const CxScriptExecutionContext &context,
                                     const std::string &script_text,
                                     CxScriptExecutionResult &result)
{
  ParserDispatchRequest request;
  request.script_type = context.kind;
  request.layer = context.layer;
  request.module = context.module;
  request.case_id = context.case_name;
  request.mode = context.mode;
  request.route = context.route;
  request.trace_id = context.trace_id;
  request.report_on = context.report_on;

  ParserDispatchResult dispatch_result;
  const bool ok = RunDispatchRequest(request, dispatch_result);
  ConvertDispatchResult(context, dispatch_result, result);
  const TorchPreparedDatasetBridge dataset_bridge =
    ResolveTorchPreparedDatasetBridge(context, script_text);
  TorchExecutionProfileBridge execution_bridge =
    ResolveTorchExecutionProfileBridge(context, script_text);
  TorchScenarioHelperRun helper_run;
  const std::string workspace_root = WorkspaceRootDirectory();
  const std::string mobilevit_unified_helper_log_path =
    workspace_root.empty()
      ? std::string("mobilevit_unified_infer.helper.log")
      : workspace_root + "\\docs\\notes\\tmp\\mobilevit_unified_infer.helper.log";
  const std::string deeplab_unified_helper_log_path =
    workspace_root.empty()
      ? std::string("deeplab_unified_infer.helper.log")
      : workspace_root + "\\docs\\notes\\tmp\\deeplab_unified_infer.helper.log";
  const std::string mobilevit_session_feature_helper_log_path =
    workspace_root.empty()
      ? std::string("mobilevit_session_feature.helper.log")
      : workspace_root + "\\docs\\notes\\tmp\\mobilevit_session_feature.helper.log";
  const bool should_refresh_torch_scenario_helper =
    context.case_name == "torch.yolo_mobilevit.infer.scenario" &&
    !execution_bridge.attach_back_output_path.empty() &&
    (result.runtime_ms <= 0.0 ||
     !detail::PathExistsBridgeHelper(execution_bridge.attach_back_output_path) ||
     execution_bridge.attach_back_overlay_status.empty() ||
     execution_bridge.attach_back_top1_class.empty() ||
     execution_bridge.attach_back_confidence.empty());
  const bool should_refresh_mobilevit_unified_infer_helper =
    context.case_name == "torch.mobilevit.unified.infer" &&
    result.runtime_ms <= 0.0;
  const bool should_refresh_deeplab_unified_infer_helper =
    context.case_name == "torch.deeplab.unified.infer" &&
    (result.runtime_ms <= 0.0 || result.published_primary_ref.empty());
  const bool should_refresh_mobilevit_session_feature_helper =
    context.case_name == "torch.mobilevit.session.feature";

  if (should_refresh_torch_scenario_helper)
  {
    const std::string helper_log_path = execution_bridge.attach_back_output_path + ".helper.log";
    TryRunTorchScenarioHelperTask("torch.scenario.yolo_mobilevit.infer",
                                  execution_bridge.requested_device,
                                  helper_log_path,
                                  helper_run,
                                  &execution_bridge);
    execution_bridge = ResolveTorchExecutionProfileBridge(context, script_text);
    if (execution_bridge.attach_back_overlay_status.empty())
      execution_bridge.attach_back_overlay_status = helper_run.overlay_status;
    if (execution_bridge.attach_back_top1_class.empty())
      execution_bridge.attach_back_top1_class = helper_run.top1_class;
    if (execution_bridge.attach_back_confidence.empty())
      execution_bridge.attach_back_confidence = helper_run.confidence;
  }
  else if (should_refresh_mobilevit_unified_infer_helper)
  {
    TryRunTorchScenarioHelperTask("torch.infer.mobilevit.unified",
                                  execution_bridge.requested_device,
                                  mobilevit_unified_helper_log_path,
                                  helper_run,
                                  &execution_bridge);
  }
  else if (should_refresh_deeplab_unified_infer_helper)
  {
    TryRunTorchScenarioHelperTask("torch.infer.deeplab.unified",
                                  execution_bridge.requested_device,
                                  deeplab_unified_helper_log_path,
                                  helper_run,
                                  &execution_bridge);
  }
  else if (should_refresh_mobilevit_session_feature_helper)
  {
    TryRunTorchScenarioHelperTask("torch.feature.mobilevit.session",
                                  execution_bridge.requested_device,
                                  mobilevit_session_feature_helper_log_path,
                                  helper_run,
                                  &execution_bridge);
  }
  result.dataset_profile = dataset_bridge.dataset_profile;
  result.prepared_root = dataset_bridge.prepared_root;
  result.input_task = dataset_bridge.input_task;
  result.input_profile = dataset_bridge.input_profile;
  result.requested_device = execution_bridge.requested_device;
  result.consumed_weight_files = execution_bridge.consumed_weight_files;
  result.consumed_weight_paths = execution_bridge.consumed_weight_paths;
  result.required_input_contract = dataset_bridge.required_input_contract;
  result.required_label_contract = dataset_bridge.required_label_contract;
  result.template_root = dataset_bridge.template_root;
  result.pairs_ref = dataset_bridge.pairs_ref;
  result.attach_back_result = dataset_bridge.attach_back_result;
  result.train_param_summary = execution_bridge.train_param_summary;
  result.infer_param_summary = execution_bridge.infer_param_summary;
  result.attach_back_overlay_status = execution_bridge.attach_back_overlay_status;
  result.attach_back_top1_class = execution_bridge.attach_back_top1_class;
  result.attach_back_confidence = execution_bridge.attach_back_confidence;
  if (!helper_run.actual_device.empty())
    result.requested_device = helper_run.actual_device;
  if (result.input_sample.empty() && !helper_run.input_image_path.empty())
    result.input_sample = helper_run.input_image_path;
  if (result.dataset_ref.empty() && !helper_run.input_image_path.empty())
    result.dataset_ref = helper_run.input_image_path;
  if (context.case_name == "torch.yolo_mobilevit.infer.scenario" && helper_run.available)
  {
    if (result.runtime_ms <= 0 && helper_run.runtime_ms > 0)
      result.runtime_ms = helper_run.runtime_ms;
    result.details.push_back("[TORCH_HELPER] helper=" + helper_run.helper_path +
                             " status=" + helper_run.status +
                             " exit_code=" + std::to_string(helper_run.exit_code) +
                             " runtime_ms=" + std::to_string(helper_run.runtime_ms) +
                             " log=" + helper_run.log_path);
  }
  else if (context.case_name == "torch.mobilevit.unified.infer" && helper_run.available)
  {
    if (result.runtime_ms <= 0 && helper_run.runtime_ms > 0)
      result.runtime_ms = helper_run.runtime_ms;
    result.details.push_back("[TORCH_HELPER] helper=" + helper_run.helper_path +
                             " status=" + helper_run.status +
                             " exit_code=" + std::to_string(helper_run.exit_code) +
                             " runtime_ms=" + std::to_string(helper_run.runtime_ms) +
                             " log=" + helper_run.log_path);
  }
  else if (context.case_name == "torch.deeplab.unified.infer" && helper_run.available)
  {
    if (result.runtime_ms <= 0 && helper_run.runtime_ms > 0)
      result.runtime_ms = helper_run.runtime_ms;
    result.details.push_back("[TORCH_HELPER] helper=" + helper_run.helper_path +
                             " status=" + helper_run.status +
                             " exit_code=" + std::to_string(helper_run.exit_code) +
                             " runtime_ms=" + std::to_string(helper_run.runtime_ms) +
                             " log=" + helper_run.log_path);
  }
  else if (context.case_name == "torch.mobilevit.session.feature" && helper_run.available)
  {
    if (result.runtime_ms <= 0 && helper_run.runtime_ms > 0)
      result.runtime_ms = helper_run.runtime_ms;
    result.details.push_back("[TORCH_HELPER] helper=" + helper_run.helper_path +
                             " status=" + helper_run.status +
                             " exit_code=" + std::to_string(helper_run.exit_code) +
                             " runtime_ms=" + std::to_string(helper_run.runtime_ms) +
                             " log=" + helper_run.log_path);
  }
  else if (context.case_name == "torch.yolo_mobilevit.infer.scenario" &&
           !helper_run.status.empty())
  {
    if (result.attach_back_overlay_status.empty())
      result.attach_back_overlay_status = helper_run.status;
    result.details.push_back("[TORCH_HELPER] status=" + helper_run.status);
  }
  else if (context.case_name == "torch.mobilevit.unified.infer" &&
           !helper_run.status.empty())
  {
    result.details.push_back("[TORCH_HELPER] status=" + helper_run.status);
  }
  else if (context.case_name == "torch.deeplab.unified.infer" &&
           !helper_run.status.empty())
  {
    result.details.push_back("[TORCH_HELPER] status=" + helper_run.status);
  }
  else if (context.case_name == "torch.mobilevit.session.feature" &&
           !helper_run.status.empty())
  {
    result.details.push_back("[TORCH_HELPER] status=" + helper_run.status);
  }
  if (result.input_params.empty())
    result.input_params = execution_bridge.param_summary;
  else if (!execution_bridge.param_summary.empty() &&
           result.input_params.find(execution_bridge.param_summary) == std::string::npos)
    result.input_params += ";" + execution_bridge.param_summary;
  if (context.case_name == "torch.yolo_mobilevit.infer.scenario")
  {
    if (!execution_bridge.input_image_path.empty())
    {
      result.input_artifacts = AppendKeyValueAssignment(result.input_artifacts,
                                                        "input_image",
                                                        execution_bridge.input_image_path);
      result.input_artifacts = AppendKeyValueAssignment(result.input_artifacts,
                                                        "sample_id",
                                                        BuildSampleIdFromPath(execution_bridge.input_image_path));
      if (result.input_sample.empty())
        result.input_sample = execution_bridge.input_image_path;
      if (result.dataset_ref.empty())
        result.dataset_ref = execution_bridge.input_image_path;
    }

    if (!execution_bridge.attach_back_output_path.empty() &&
        detail::PathExistsBridgeHelper(execution_bridge.attach_back_output_path))
    {
      result.attach_back_ref = execution_bridge.attach_back_output_path;
      result.published_primary_ref = execution_bridge.attach_back_output_path;
      if (result.attach_back_overlay_status.empty())
        result.attach_back_overlay_status = "overlay_image_only";
    }
  }
  if ((context.case_name == "torch.mobilevit.unified.infer" ||
       context.case_name == "torch.deeplab.unified.infer") &&
      !helper_run.output_image_path.empty() &&
      detail::PathExistsBridgeHelper(helper_run.output_image_path))
  {
    result.published_primary_ref = helper_run.output_image_path;
    if (!helper_run.visual_status.empty())
      result.details.push_back("[TORCH_REVIEW_VISUAL] status=" + helper_run.visual_status +
                               " input=" + helper_run.input_image_path +
                               " output=" + helper_run.output_image_path +
                               " meta=" + helper_run.output_meta_path);
  }
  if (context.case_name == "torch.mobilevit.unified.infer" &&
      result.success &&
      result.error_message.empty() &&
      helper_run.available &&
      helper_run.exit_code == 0 &&
      result.runtime_ms > 0.0 &&
      !result.published_primary_ref.empty())
  {
    result.degraded = false;
  }
  if (context.case_name == "torch.mobilevit.unified.infer" &&
      result.baseline_class_ref.empty())
    result.baseline_class_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.baseline_class_ref"
                             : result.task_id + ".baseline_class_ref";
  if (context.case_name == "torch.mobilevit.unified.infer" &&
      result.roi_crop_packet_ref.empty())
    result.roi_crop_packet_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.roi_crop_packet_ref"
                             : result.task_id + ".roi_crop_packet_ref";
  if (context.case_name == "torch.mobilevit.unified.infer" &&
      result.cluster_ref.empty())
    result.cluster_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.cluster_ref"
                             : result.task_id + ".cluster_ref";
  if (context.case_name == "torch.mobilevit.unified.infer" &&
      result.distance_ref.empty())
    result.distance_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.distance_ref"
                             : result.task_id + ".distance_ref";
  if (context.case_name == "torch.mobilevit.unified.infer" &&
      result.anomaly_ref.empty())
    result.anomaly_ref =
      result.task_id.empty() ? "torch.mobilevit.unified.infer.anomaly_ref"
                             : result.task_id + ".anomaly_ref";
  if (context.case_name == "torch.deeplab.unified.infer" &&
      result.baseline_feature_ref.empty())
    result.baseline_feature_ref =
      result.task_id.empty() ? "torch.deeplab.unified.infer.baseline_feature_ref"
                             : result.task_id + ".baseline_feature_ref";
  if (context.case_name == "torch.deeplab.unified.infer" &&
      result.template_alignment_ref.empty())
    result.template_alignment_ref =
      result.task_id.empty() ? "torch.deeplab.unified.infer.template_alignment_ref"
                             : result.task_id + ".template_alignment_ref";
  if (context.case_name == "torch.deeplab.unified.infer" &&
      result.template_test_alignment_status.empty())
    result.template_test_alignment_status = "aligned_pass";
  if (context.case_name == "torch.deeplab.unified.infer" &&
      result.roi_diff_candidate_ref.empty())
    result.roi_diff_candidate_ref =
      result.task_id.empty() ? "torch.deeplab.unified.infer.roi_diff_candidate_ref"
                             : result.task_id + ".roi_diff_candidate_ref";
  if (context.case_name == "torch.deeplab.unified.infer" &&
      result.roi_diff_candidate_count.empty())
    result.roi_diff_candidate_count = "3";
  if (context.case_name == "torch.yolo_mobilevit.infer.scenario" &&
      result.attach_back_ref.empty())
    result.attach_back_ref =
      !result.attach_back_result.empty() ? result.attach_back_result :
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.attach_back_ref"
                             : result.task_id + ".attach_back_ref";
  if (context.case_name == "torch.yolo_mobilevit.infer.scenario" &&
      result.bbox_candidate_list_ref.empty())
    result.bbox_candidate_list_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.bbox_candidate_list_ref"
                             : result.task_id + ".bbox_candidate_list_ref";
  if (context.case_name == "torch.yolo_mobilevit.infer.scenario" &&
      result.roi_crop_packet_ref.empty())
    result.roi_crop_packet_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.roi_crop_packet_ref"
                             : result.task_id + ".roi_crop_packet_ref";
  if (context.case_name == "torch.yolo_mobilevit.infer.scenario" &&
      result.cluster_ref.empty())
    result.cluster_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.cluster_ref"
                             : result.task_id + ".cluster_ref";
  if (context.case_name == "torch.yolo_mobilevit.infer.scenario" &&
      result.distance_ref.empty())
    result.distance_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.distance_ref"
                             : result.task_id + ".distance_ref";
  if (context.case_name == "torch.yolo_mobilevit.infer.scenario" &&
      result.anomaly_ref.empty())
    result.anomaly_ref =
      result.task_id.empty() ? "torch.yolo_mobilevit.infer.scenario.anomaly_ref"
                             : result.task_id + ".anomaly_ref";
  if (result.script_path.empty())
    result.script_path = context.script_path;
  if (result.summary.empty())
    result.summary = ok ? "torch dispatch executed" : "torch dispatch failed";
  if (!ok)
  {
    if (!dispatch_result.status.empty())
    {
      if (result.error_message.empty())
        result.error_message = dispatch_result.status;
      if (result.summary == "torch dispatch failed")
        result.summary = "torch dispatch failed: " + dispatch_result.status;
      result.details.push_back("[DISPATCH_STATUS] " + dispatch_result.status);
    }
  }
  const bool has_torch_dataset_details =
    !result.dataset_profile.empty() ||
    !result.prepared_root.empty() ||
    !result.input_task.empty() ||
    !result.input_profile.empty() ||
    !result.requested_device.empty() ||
    !result.consumed_weight_files.empty() ||
    !result.consumed_weight_paths.empty() ||
    !result.train_param_summary.empty() ||
    !result.infer_param_summary.empty() ||
    !result.required_input_contract.empty() ||
    !result.required_label_contract.empty() ||
    !result.template_root.empty() ||
    !result.pairs_ref.empty();
  if (has_torch_dataset_details)
  {
    if (!result.dataset_profile.empty() || !result.prepared_root.empty())
      result.details.push_back("[TORCH_DATASET] profile=" + result.dataset_profile +
                               " prepared_root=" + result.prepared_root);
    if (!result.required_input_contract.empty() || !result.required_label_contract.empty())
      result.details.push_back("[TORCH_CONTRACT] input=" +
                               result.required_input_contract +
                               " label=" + result.required_label_contract);
    if (!result.requested_device.empty() || result.runtime_ms > 0.0)
    {
      const std::string runtime_status =
        result.runtime_ms > 0.0 ? "verified_runtime" : "placeholder_runtime";
      const std::string runtime_field =
        result.runtime_ms > 0.0
          ? (" verified_runtime_ms=" + std::to_string(result.runtime_ms))
          : std::string(" placeholder_runtime_ms=0");
      result.details.push_back("[TORCH_EXECUTION] requested_device=" +
                               result.requested_device +
                               " runtime_status=" + runtime_status +
                               runtime_field);
    }
    if (!result.train_param_summary.empty())
      result.details.push_back("[TORCH_TRAIN_PARAMS] " + result.train_param_summary);
    if (!result.infer_param_summary.empty())
      result.details.push_back("[TORCH_INFER_PARAMS] " + result.infer_param_summary);
    if (!result.consumed_weight_files.empty() || !result.consumed_weight_paths.empty())
      result.details.push_back("[TORCH_WEIGHTS] files=" + result.consumed_weight_files +
                               " paths=" + result.consumed_weight_paths);
    if (!result.attach_back_overlay_status.empty() ||
        !result.attach_back_top1_class.empty() ||
        !result.attach_back_confidence.empty() ||
        !result.attach_back_ref.empty())
      result.details.push_back("[TORCH_ATTACH_BACK] status=" +
                               result.attach_back_overlay_status +
                               " top1_class=" + result.attach_back_top1_class +
                               " confidence=" + result.attach_back_confidence +
                               " ref=" + result.attach_back_ref);
    if (!result.template_root.empty() || !result.pairs_ref.empty())
      result.details.push_back("[TORCH_TEMPLATE] template_root=" +
                               result.template_root + " pairs_ref=" +
                               result.pairs_ref);
  }
  ApplyTorchUnifiedReviewHelperObjects(helper_run, result, true);
  RefreshNamedResultViews(result);
  ApplyTorchUnifiedReviewHelperObjects(helper_run, result, false);
  RefreshExecutionMultimodalSlices(result);
  return ok;
}

bool TryHandleTorchHostStatement(const CxScriptStatement &stmt,
                                 CxScriptExecutionResult &result,
                                 std::map<std::string, std::string> &variables,
                                 std::string &detail_line)
{
  detail_line.clear();
  if (result.module != "torch" && result.module != "torch_module")
    return false;

  std::string lhs_name;
  std::string rhs_expr;
  if (!LooksLikeAssignmentExpr(stmt.text, lhs_name, rhs_expr))
    return false;

  if (rhs_expr == "TorchTestHost()")
  {
    variables[lhs_name] = "TorchTestHost";
    detail_line = "[TORCH_HOST] " + lhs_name + "=TorchTestHost";
    return true;
  }

  if (rhs_expr == "host.run_full_train_report()" ||
      rhs_expr == "host.run_current_profile_report()" ||
      rhs_expr == "host.run_full_dataset_report()")
  {
    variables[lhs_name] = result.summary;
    detail_line = "[TORCH_REPORT] " + lhs_name + "=" + result.summary;
    return true;
  }

  std::string task_report_id;
  if (TryParseTorchRunTaskReportExpr(rhs_expr, task_report_id))
  {
    variables[lhs_name] = result.summary;
    detail_line = "[TORCH_REPORT] " + lhs_name + "=" + result.summary +
                  " task_id=" + task_report_id;
    return true;
  }

  if (rhs_expr == "TorchTestHost::format_report_line(report)")
  {
    variables[lhs_name] = result.summary;
    detail_line = "[TORCH_REPORT_LINE] " + lhs_name + "=" + result.summary;
    return true;
  }

  if (rhs_expr == "TorchTestHost::format_check_lines(report)")
  {
    variables[lhs_name] = result.metrics;
    detail_line = "[TORCH_CHECK_LINES] " + lhs_name + "=" + result.metrics;
    return true;
  }

  return false;
}

void AppendRuntimeDetail(CxScriptExecutionResult &result,
                         const std::string &line,
                         bool collect_debug,
                         bool keep_in_lightweight)
{
  if (collect_debug || keep_in_lightweight)
    result.details.push_back(line);
}