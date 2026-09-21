// Extracted unified review helper implementation for cxscript runtime.

std::string BuildDetectionElementSummaryByField(const std::vector<UnifiedDetectionElement> &elements,
                                                const char *field_name)
{
  std::vector<std::string> summary_items;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = elements[i];
    std::string value;
    if (std::strcmp(field_name, "element_status_summary") == 0)
      value = element.element_status_summary;
    else if (std::strcmp(field_name, "candidate_status") == 0)
      value = element.candidate_status;
    else if (std::strcmp(field_name, "match_status") == 0)
      value = element.match_status;
    else if (std::strcmp(field_name, "manual_review_signal") == 0)
      value = element.manual_review_signal;
    else if (std::strcmp(field_name, "review_signal") == 0)
      value = element.manual_review_signal;
    else if (std::strcmp(field_name, "element_group_label") == 0)
      value = element.element_group_label;
    else if (std::strcmp(field_name, "element_source") == 0)
      value = element.provenance;
    else if (std::strcmp(field_name, "focus_region_ref") == 0)
      value = element.focus_region_ref;
    else if (std::strcmp(field_name, "local_delta_ref") == 0)
      value = element.local_delta_ref;
    else if (std::strcmp(field_name, "element_findings") == 0)
      value = element.element_findings;
    else if (std::strcmp(field_name, "geometry_summary") == 0)
      value = element.element_findings.empty() ? element.geometry_payload
                                               : element.element_findings;
    else if (std::strcmp(field_name, "overlay_ref") == 0)
      value = element.primary_overlay_ref;
    else if (std::strcmp(field_name, "element_level_focus") == 0)
      value = element.element_level_focus;
    if (value.empty())
      continue;
    summary_items.push_back(element.element_id + "=" + value);
  }
  return JoinTextItems(summary_items, " || ");
}

std::string BuildDetectionElementSummary(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> summary_items;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = elements[i];
    std::vector<std::string> fields;
    fields.push_back("type=" + element.element_type);
    fields.push_back("role=" + element.semantic_role);
    fields.push_back("source=" + element.provenance);
    fields.push_back("status=" + element.consistency_status);
    if (!element.template_relation.empty())
      fields.push_back("template=" + element.template_relation);
    summary_items.push_back(element.element_id + "=" + JoinTextItems(fields, ","));
  }
  return JoinTextItems(summary_items, " || ");
}

bool HasEnsmallenOptimizationEvidence(const CxScriptExecutionResult &result)
{
  return NormalizeReviewSourceThread(result) == "ensmallen" ||
         result.baseline_objective != 0.0 ||
         result.best_objective != 0.0 ||
         result.objective_delta != 0.0 ||
         result.metric_delta != 0.0 ||
         result.stability_delta != 0.0 ||
         result.selected_candidate_score_value > 0.0 ||
         result.selected_candidate_index_value >= 0.0 ||
         result.candidate_count_value > 0.0 ||
         result.match_candidate_count_value > 0.0;
}

double ResolveEnsmallenCandidateCountValue(const CxScriptExecutionResult &result)
{
  if (result.candidate_count_value > 0.0)
    return result.candidate_count_value;
  if (result.match_candidate_count_value > 0.0)
    return result.match_candidate_count_value;
  return 0.0;
}

std::string BuildEnsmallenObjectiveCurveValue(const CxScriptExecutionResult &result)
{
  if (!HasEnsmallenOptimizationEvidence(result) ||
      (result.baseline_objective == 0.0 &&
       result.best_objective == 0.0 &&
       result.objective_delta == 0.0))
    return std::string();

  return FormatElementNumber(result.baseline_objective) + "->" +
         FormatElementNumber(result.best_objective);
}

std::string BuildEnsmallenFeatureDistanceDeltaValue(const CxScriptExecutionResult &result)
{
  if (!HasEnsmallenOptimizationEvidence(result) && result.metric_delta == 0.0)
    return std::string();
  return FormatElementNumber(result.metric_delta);
}

std::string BuildEnsmallenCandidateRankValue(const CxScriptExecutionResult &result)
{
  const double candidate_count = ResolveEnsmallenCandidateCountValue(result);
  if (result.selected_candidate_index_value < 0.0 || candidate_count <= 0.0)
    return std::string();

  int rank = static_cast<int>(result.selected_candidate_index_value);
  int total = static_cast<int>(candidate_count);
  if (rank <= 0)
    rank = 1;
  if (total < rank)
    total = rank;
  return std::to_string(rank) + "/" + std::to_string(total);
}

double ComputeEnsmallenStabilityScore(const CxScriptExecutionResult &result)
{
  double score =
    result.selected_candidate_score_value > 0.0
      ? result.selected_candidate_score_value
      : (result.objective_delta < -0.000001
           ? 0.78
           : (result.objective_delta > 0.000001 ? 0.35 : 0.58));

  if (result.objective_delta <= -0.100000)
    score += 0.05;
  else if (result.objective_delta <= -0.020000)
    score += 0.02;
  else if (result.objective_delta > 0.000001)
    score -= 0.12;

  if (result.stability_delta <= -0.030000)
    score += 0.04;
  else if (result.stability_delta < 0.0)
    score += 0.02;
  else if (result.stability_delta > 0.000001)
    score -= (result.stability_delta > 0.12 ? 0.12 : result.stability_delta);

  const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
  if (!coverage_gap.empty() && coverage_gap != "no_coverage_gap")
  {
    if (coverage_gap == "missing_G4_pipeline_bundle")
      score -= 0.06;
    else if (coverage_gap == "limited_bucket_evidence")
      score -= 0.12;
    else
      score -= 0.08;
  }

  if (result.selected_candidate_index_value > 1.0)
    score -= 0.07 * (result.selected_candidate_index_value - 1.0);

  const std::string risk_axis = BuildEnsmallenRiskAxis(result);
  if (risk_axis == "candidate_ordering")
    score -= 0.06;
  else if (risk_axis == "boundary_and_roi")
    score -= 0.03;
  else if (risk_axis == "bundle_aggregation")
    score -= 0.02;

  if (score < 0.0)
    score = 0.0;
  if (score > 1.0)
    score = 1.0;
  return score;
}

std::string BuildEnsmallenStabilityScoreValue(const CxScriptExecutionResult &result)
{
  if (!HasEnsmallenOptimizationEvidence(result))
    return std::string();
  return FormatElementNumber(ComputeEnsmallenStabilityScore(result));
}

std::string BuildEnsmallenConvergenceStatusValue(const CxScriptExecutionResult &result)
{
  if (!HasEnsmallenOptimizationEvidence(result))
    return std::string();
  if (result.objective_delta > 0.000001 || result.stability_delta > 0.000001)
    return "watch";
  if (result.objective_delta < -0.000001 || result.stability_delta < -0.000001)
    return "converged";
  return "flat";
}

std::string BuildEnsmallenBestCandidateConfidenceValue(const CxScriptExecutionResult &result)
{
  if (!HasEnsmallenOptimizationEvidence(result))
    return std::string();

  const double stability_score = ComputeEnsmallenStabilityScore(result);
  const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
  const bool no_coverage_gap =
    coverage_gap.empty() || coverage_gap == "no_coverage_gap";
  const bool top_rank =
    result.selected_candidate_index_value < 0.0 ||
    result.selected_candidate_index_value <= 1.0;
  const bool strong_improvement = result.objective_delta <= -0.100000;
  const bool improved = result.objective_delta < -0.000001;
  const bool stable_gain = result.stability_delta <= -0.020000;
  const bool high_score = result.selected_candidate_score_value >= 0.86;

  if (no_coverage_gap &&
      top_rank &&
      high_score &&
      strong_improvement &&
      stable_gain &&
      stability_score >= 0.85)
    return "credible";
  if (top_rank && improved && stability_score >= 0.74)
    return no_coverage_gap ? "usable" : "guarded";
  if (improved && stability_score >= 0.60)
    return "guarded";
  return "watch";
}

const UnifiedDetectionElement *FindDetectionElementById(
  const std::vector<UnifiedDetectionElement> &elements,
  const std::string &element_id)
{
  if (element_id.empty())
    return nullptr;

  for (size_t i = 0; i < elements.size(); ++i)
  {
    if (elements[i].element_id == element_id)
      return &elements[i];
  }
  return nullptr;
}

void PushChainElementIfPresent(const std::vector<UnifiedDetectionElement> &elements,
                               std::vector<std::string> &element_ids,
                               const std::string &element_id)
{
  if (FindDetectionElementById(elements, element_id) != nullptr)
    PushUniqueText(element_ids, element_id);
}

std::string SelectUnifiedChainRef(const CxScriptExecutionResult &result,
                                  const std::string &chain_type)
{
  const std::string region_summary_ref =
    FindNamedResultFieldValue(result, "refs", "region_summary_ref");
  const std::string region_bounds_ref =
    FindNamedResultFieldValue(result, "refs", "region_bounds_ref");
  const std::string bridge_roi_ref =
    ResolveNamedOrDirectRef(result,
                            "bridge",
                            "roi_ref",
                            FindAssignmentValue(result.input_artifacts, "roi_ref"));
  const std::string bridge_template_image =
    ResolveNamedOrDirectRef(result,
                            "bridge",
                            "template_image",
                            FindAssignmentValue(result.input_artifacts, "template_image"));
  if (chain_type == "bbox")
    return result.bbox_candidate_list_ref.empty() ? result.published_bbox_candidate_list_ref
                                                  : result.bbox_candidate_list_ref;
  if (chain_type == "roi_crop")
  {
    const std::string roi_crop_ref =
      result.roi_crop_packet_ref.empty() ? result.published_roi_crop_packet_ref
                                         : result.roi_crop_packet_ref;
    if (!roi_crop_ref.empty())
      return roi_crop_ref;
    if (IsEnsmallenRoiBridgeCase(result))
      return bridge_roi_ref;
    return std::string();
  }
  if (chain_type == "template_alignment")
  {
    const std::string template_alignment_ref =
      result.template_alignment_ref.empty() ? result.published_template_alignment_ref
                                            : result.template_alignment_ref;
    if (!template_alignment_ref.empty())
      return template_alignment_ref;
    if (IsEnsmallenTemplateBridgeCase(result))
    {
      if (!result.compare_ref.empty())
        return result.compare_ref;
      if (!result.summary_ref.empty())
        return result.summary_ref;
      return bridge_template_image;
    }
    return std::string();
  }
  if (chain_type == "roi_diff")
  {
    const std::string roi_diff_ref =
      result.roi_diff_candidate_ref.empty() ? result.published_roi_diff_candidate_ref
                                            : result.roi_diff_candidate_ref;
    if (!roi_diff_ref.empty())
      return roi_diff_ref;

    if (IsCximageClassicalReviewCase(result))
    {
      if (result.case_name == "line_measure_roi")
      {
        const std::string line_measure_bounds_ref =
          FindNamedResultFieldValue(result, "refs", "line_measure_bounds_ref");
        if (!line_measure_bounds_ref.empty())
          return line_measure_bounds_ref;
        return FindNamedResultFieldValue(result, "refs", "line_point_set_ref");
      }
      if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
      {
        if (!result.circle_edge_overlay_ref.empty())
          return result.circle_edge_overlay_ref + "#arc_support";
        const std::string circle_measure_bounds_ref =
          FindNamedResultFieldValue(result, "refs", "circle_measure_bounds_ref");
        if (!circle_measure_bounds_ref.empty())
          return circle_measure_bounds_ref;
        return FindNamedResultFieldValue(result, "refs", "circle_point_set_ref");
      }
      if (result.case_name == "fast_template_match" ||
          result.case_name == "fastmatch_template")
      {
        if (!result.candidate_overlay_ref.empty())
          return result.candidate_overlay_ref + "#candidate_pool";
        if (!result.test_rect_overlay_ref.empty())
          return result.test_rect_overlay_ref + "#test_region";
        if (!result.template_rect_overlay_ref.empty())
          return result.template_rect_overlay_ref + "#template_anchor";
      }
      if (result.case_name == "formfit_rect_candidate")
      {
        if (!result.formfit_candidate_overlay_ref.empty())
          return result.formfit_candidate_overlay_ref + "#candidate_pool";
        if (!result.formfit_selection_overlay_ref.empty())
          return result.formfit_selection_overlay_ref + "#selected_region";
      }
      if (result.case_name == "binary_region")
      {
        if (!result.region_pattern_descriptor_ref.empty())
          return result.region_pattern_descriptor_ref + "#descriptor_support";
        if (!result.region_pattern_overlay_ref.empty())
          return result.region_pattern_overlay_ref + "#region_pattern";
      }
      if (result.case_name == "findobject_region")
      {
        if (!region_summary_ref.empty())
          return region_summary_ref + "#candidate_pool";
        if (!region_bounds_ref.empty())
          return region_bounds_ref;
      }
    }
    if (IsEnsmallenPublicResult(result))
    {
      if (!result.anomaly_ref.empty())
        return result.anomaly_ref;
      if (!result.compare_ref.empty())
        return result.compare_ref;
      if (IsEnsmallenRoiBridgeCase(result))
        return bridge_roi_ref;
    }
  }
  return std::string();
}

bool IsUnifiedChainExpected(const CxScriptExecutionResult &result,
                            const std::string &chain_type,
                            const std::string &chain_ref)
{
  if (!chain_ref.empty())
    return true;

  const std::string profile = ToLowerText(result.dataset_profile + " " +
                                         result.input_task + " " +
                                         result.case_name + " " +
                                         result.summary);
  if (chain_type == "bbox")
  {
    if (IsEnsmallenPublicResult(result))
      return false;
    return profile.find("yolo") != std::string::npos ||
           profile.find("detection") != std::string::npos ||
           profile.find("two_stage") != std::string::npos;
  }
  if (chain_type == "roi_crop")
  {
    if (IsEnsmallenRoiBridgeCase(result))
      return true;
    return profile.find("roi") != std::string::npos ||
           profile.find("two_stage") != std::string::npos ||
           profile.find("mobilevit") != std::string::npos;
  }
  if (chain_type == "template_alignment")
  {
    if (IsEnsmallenTemplateBridgeCase(result))
      return true;
    return profile.find("detection_with_template_pair") != std::string::npos ||
           profile.find("template") != std::string::npos ||
           profile.find("deeplab") != std::string::npos;
  }
  if (chain_type == "roi_diff")
  {
    if (IsEnsmallenTemplateBridgeCase(result) || IsEnsmallenRoiBridgeCase(result))
      return true;
    if (IsCximageClassicalReviewCase(result))
      return true;
    return profile.find("detection_with_template_pair") != std::string::npos ||
           profile.find("template") != std::string::npos ||
           profile.find("diff") != std::string::npos ||
           profile.find("deeplab") != std::string::npos ||
           !result.roi_diff_candidate_count.empty();
  }
  return false;
}

std::string DetermineCximageClassicalRoiDiffStatus(
  const CxScriptExecutionResult &result,
  const std::string &chain_ref)
{
  if (!result.error_message.empty())
    return "abnormal";
  if (!result.failure_mode.empty() && result.failure_mode != "none")
    return "abnormal";
  if (chain_ref.empty())
    return "missing";

  if (result.case_name == "line_measure_roi")
  {
    if (result.fit_error_max_value > 2.0 || result.subpixel_adjust_avg_value > 1.5)
      return "drifted";
    return "matched";
  }

  if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
  {
    if (!result.circle_failure_stage.empty() && result.circle_failure_stage != "none")
      return "abnormal";
    if (result.circle_avg_distance_value > 2.0)
      return "drifted";
    return "matched";
  }

  if (result.case_name == "fast_template_match" ||
      result.case_name == "fastmatch_template")
  {
    if (result.match_candidate_count_value <= 0.0)
      return "missing";
    const bool rect_missing =
      result.match_best_rect_w_value <= 0.0 || result.match_best_rect_h_value <= 0.0;
    const bool low_score =
      result.match_top_score_value > 0.0 && result.match_top_score_value < 0.85;
    const bool excessive_candidates = result.match_candidate_count_value > 2.0;
    if (rect_missing || low_score || excessive_candidates)
      return "drifted";
    return "matched";
  }

  if (result.case_name == "formfit_rect_candidate")
  {
    if (result.match_candidate_count_value <= 0.0)
      return "missing";
    if (result.formfit_compare_rect_center_delta_value > 3.0)
      return "drifted";
    return "matched";
  }

  if (result.case_name == "binary_region")
  {
    if (result.region_pattern_foreground_ratio_value < 0.1 ||
        result.region_pattern_foreground_ratio_value > 0.9 ||
        result.region_pattern_descriptor_std_value > 0.35)
      return "drifted";
    return "matched";
  }

  if (result.case_name == "findobject_region")
  {
    if (result.match_candidate_count_value <= 0.0)
      return "missing";
    if (result.match_candidate_count_value > 1.0 ||
        result.region_foreground_ratio_value < 0.05 ||
        result.region_foreground_ratio_value > 0.95)
      return "drifted";
    return "matched";
  }

  return "matched";
}

std::string DetermineUnifiedChainStatus(const CxScriptExecutionResult &result,
                                        const std::string &chain_type,
                                        const std::string &chain_ref)
{
  if (IsEnsmallenPublicResult(result))
  {
    if (!result.error_message.empty() ||
        (!result.failure_mode.empty() && result.failure_mode != "none"))
      return "abnormal";

    const std::string comparison_status = BuildEnsmallenComparisonStatus(result);
    if (chain_ref.empty())
      return IsUnifiedChainExpected(result, chain_type, chain_ref) ? "missing" : "watch";

    if (chain_type == "roi_diff")
    {
      if (comparison_status == "regressed" ||
          result.metric_delta > 0.000001 ||
          result.stability_delta > 0.000001)
        return "drifted";
      return "matched";
    }

    if (chain_type == "template_alignment")
    {
      if ((BuildEnsmallenRiskAxis(result) == "alignment_and_interaction" ||
           BuildEnsmallenRiskAxis(result) == "roi_focus_and_interaction") &&
          comparison_status == "regressed")
        return "drifted";
      return "matched";
    }

    if (chain_type == "roi_crop")
    {
      if (BuildEnsmallenRiskAxis(result) == "boundary_and_roi" &&
          comparison_status == "regressed")
        return "drifted";
      return "matched";
    }
  }

  if (chain_type == "template_alignment")
  {
    const std::string alignment_status =
      ToLowerText(!result.published_template_test_alignment_status.empty()
                    ? result.published_template_test_alignment_status
                    : result.template_test_alignment_status);
    if (!alignment_status.empty())
    {
      if (alignment_status.find("fail") != std::string::npos ||
          alignment_status.find("mismatch") != std::string::npos ||
          alignment_status.find("drift") != std::string::npos)
        return "drifted";
      if (alignment_status.find("missing") != std::string::npos ||
          alignment_status.find("not_found") != std::string::npos)
        return "missing";
      if (alignment_status.find("abnormal") != std::string::npos ||
          alignment_status.find("error") != std::string::npos)
        return "abnormal";
      if (!chain_ref.empty())
        return "matched";
    }
  }

  if (chain_type == "roi_diff")
  {
    if (IsCximageClassicalReviewCase(result))
      return DetermineCximageClassicalRoiDiffStatus(result, chain_ref);

    const std::string diff_count =
      !result.published_roi_diff_candidate_count.empty()
        ? result.published_roi_diff_candidate_count
        : result.roi_diff_candidate_count;
    if (!diff_count.empty() && diff_count != "0")
      return "drifted";
  }

  if (!chain_ref.empty())
    return "matched";
  return IsUnifiedChainExpected(result, chain_type, chain_ref) ? "missing" : "watch";
}

std::string BuildUnifiedChainFocus(const CxScriptExecutionResult &result,
                                   const std::string &chain_type,
                                   const std::string &chain_status)
{
  if (IsEnsmallenPublicResult(result))
  {
    if (chain_type == "bbox")
      return "ensmallen currently consumes post-detection evidence; keep bbox chain as watch unless upstream bbox is attached";
    if (chain_type == "roi_crop")
    {
      if (chain_status == "watch" && IsEnsmallenGeometryOnlyReplayCase(result))
        return "roi chain is not applicable for geometry-fit replay without roi_ref input";
      return chain_status == "matched"
               ? "verify optimization still lands inside declared roi_ref and crop policy"
               : "materialize roi_ref or explain why roi chain is not applicable";
    }
    if (chain_type == "template_alignment")
      return chain_status == "matched"
               ? "verify template/test alignment evidence against compare and replay outputs"
               : "materialize template pair evidence or mark alignment chain unresolved";
    if (chain_type == "roi_diff")
      return chain_status == "matched"
               ? "inspect compare/anomaly evidence for roi-level drift after optimization"
               : "materialize compare or anomaly evidence for roi diff review";
  }

  if (chain_type == "bbox")
    return chain_status == "matched" ? "verify bbox candidate ordering and overlay placement"
                                     : "materialize bbox_candidate_list_ref or explain detection bypass";
  if (chain_type == "roi_crop")
    return chain_status == "matched" ? "verify ROI crop packet aligns with bbox and class attach-back"
                                     : "materialize roi_crop_packet_ref and closed-region crop evidence";
  if (chain_type == "template_alignment")
    return chain_status == "matched" ? "verify template/test alignment relation and drift tolerance"
                                     : "materialize template_alignment_ref or mark template chain not applicable";
  if (chain_type == "roi_diff")
  {
    if (IsCximageClassicalReviewCase(result))
    {
      if (result.case_name == "line_measure_roi")
        return "inspect line fit residuals, subpixel adjustment, and ROI bounds continuity";
      if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
        return "inspect arc completeness, circle residual drift, and ellipse-like rejection evidence";
      if (result.case_name == "fast_template_match" ||
          result.case_name == "fastmatch_template")
        return "inspect template/test rect drift, candidate ordering, and small rotation-scale tolerance";
      if (result.case_name == "formfit_rect_candidate")
        return "inspect candidate pool ranking, selected geometry, and rect-center drift";
      if (result.case_name == "binary_region")
        return "inspect descriptor separability against brightness, grayscale, and region mask drift";
      if (result.case_name == "findobject_region")
        return "inspect region candidate multiplicity, top1 region stability, and threshold sensitivity";
    }
    return chain_status == "matched" ? "verify diff candidate count and review target ordering"
                                     : "materialize roi_diff_candidate_ref when template/diff review is expected";
  }
  return "review chain evidence";
}

std::string BuildUnifiedChainFindings(const CxScriptExecutionResult &result,
                                      const std::string &chain_type,
                                      const std::string &chain_ref)
{
  if (IsEnsmallenPublicResult(result))
  {
    const std::string roi_ref =
      ResolveNamedOrDirectRef(result,
                              "bridge",
                              "roi_ref",
                              FindAssignmentValue(result.input_artifacts, "roi_ref"));
    const std::string template_image =
      ResolveNamedOrDirectRef(result,
                              "bridge",
                              "template_image",
                              FindAssignmentValue(result.input_artifacts, "template_image"));
    const std::string input_image =
      ResolveNamedOrDirectRef(result,
                              "bridge",
                              "input_image",
                              FindAssignmentValue(result.input_artifacts, "input_image"));
    const std::string objective_ref =
      ResolveNamedOrDirectRef(result, "refs", "objective_ref", result.objective_ref);
    if (chain_type == "bbox")
      return "bbox_chain=watch;upstream_bbox_ref=" +
             (chain_ref.empty() ? std::string("missing") : chain_ref);
    if (chain_type == "roi_crop")
    {
      if (roi_ref.empty() && IsEnsmallenGeometryOnlyReplayCase(result))
      {
        const std::string task_scope =
          FindAssignmentValue(result.input_params, "task_scope");
        return "roi_ref=not_applicable;geometry_ref=" +
               ResolveNamedOrDirectRef(result,
                                       "bridge",
                                       "geometry_ref",
                                       FindAssignmentValue(result.input_artifacts, "geometry_ref")) +
               ";task_scope=" + task_scope;
      }
      return "roi_ref=" + (roi_ref.empty() ? std::string("missing") : roi_ref) +
             ";input_image=" + input_image +
             ";crop_policy_ref=" + ResolveEnsmallenBridgeRef(result, "crop_policy_ref");
    }
    if (chain_type == "template_alignment")
      return "template_image=" + (template_image.empty() ? std::string("missing") : template_image) +
             ";compare_ref=" + result.compare_ref +
             ";replay_ref=" + (result.replay_ref.empty() ? result.replay_log_path
                                                         : result.replay_ref);
    if (chain_type == "roi_diff")
      return "roi_diff_ref=" + (chain_ref.empty() ? std::string("missing") : chain_ref) +
             ";objective_ref=" + objective_ref +
             ";objective_delta=" + FormatElementNumber(result.objective_delta) +
             ";anomaly_ref=" + result.anomaly_ref;
  }

  if (chain_type == "bbox")
    return chain_ref.empty() ? "bbox_candidate_list_ref=missing"
                             : "bbox_candidate_list_ref=" + chain_ref;
  if (chain_type == "roi_crop")
    return chain_ref.empty() ? "roi_crop_packet_ref=missing"
                             : "roi_crop_packet_ref=" + chain_ref +
                                 ";roi_crop_count=" + result.published_roi_crop_count +
                                 ";roi_crop_spatial_size=" +
                                 result.published_roi_crop_spatial_size;
  if (chain_type == "template_alignment")
    return chain_ref.empty() ? "template_alignment_ref=missing"
                             : "template_alignment_ref=" + chain_ref +
                                 ";template_status=" +
                                 (!result.published_template_test_alignment_status.empty()
                                    ? result.published_template_test_alignment_status
                                    : result.template_test_alignment_status);
  if (chain_type == "roi_diff")
  {
    if (IsCximageClassicalReviewCase(result))
    {
      std::vector<std::string> items;
      items.push_back(chain_ref.empty() ? "roi_diff_ref=missing" : "roi_diff_ref=" + chain_ref);
      if (result.case_name == "line_measure_roi")
      {
        items.push_back("fit_error_max=" + FormatElementNumber(result.fit_error_max_value));
        items.push_back("subpixel_adjust_avg=" +
                        FormatElementNumber(result.subpixel_adjust_avg_value));
      }
      else if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
      {
        items.push_back("circle_avg_distance=" +
                        FormatElementNumber(result.circle_avg_distance_value));
        items.push_back("failure_stage=" +
                        (result.circle_failure_stage.empty() ? "none"
                                                             : result.circle_failure_stage));
      }
      else if (result.case_name == "fast_template_match" ||
               result.case_name == "fastmatch_template")
      {
        items.push_back("candidate_count=" +
                        FormatElementNumber(result.match_candidate_count_value));
        items.push_back("top1_score=" + FormatElementNumber(result.match_top_score_value));
        items.push_back("top1_rect=" +
                        FormatElementNumber(result.match_best_rect_x_value) + "," +
                        FormatElementNumber(result.match_best_rect_y_value) + "," +
                        FormatElementNumber(result.match_best_rect_w_value) + "," +
                        FormatElementNumber(result.match_best_rect_h_value));
      }
      else if (result.case_name == "formfit_rect_candidate")
      {
        items.push_back("candidate_count=" +
                        FormatElementNumber(result.match_candidate_count_value));
        items.push_back("best_score=" + FormatElementNumber(result.match_top_score_value));
        items.push_back("center_delta=" +
                        FormatElementNumber(result.formfit_compare_rect_center_delta_value));
      }
      else if (result.case_name == "binary_region")
      {
        items.push_back("foreground_ratio=" +
                        FormatElementNumber(result.region_pattern_foreground_ratio_value));
        items.push_back("descriptor_std=" +
                        FormatElementNumber(result.region_pattern_descriptor_std_value));
        items.push_back("components=" +
                        FormatElementNumber(result.region_connected_components_value));
      }
      else if (result.case_name == "findobject_region")
      {
        items.push_back("result_count=" +
                        FormatElementNumber(result.match_candidate_count_value));
        items.push_back("top1_rect=" +
                        FormatElementNumber(result.match_best_rect_x_value) + "," +
                        FormatElementNumber(result.match_best_rect_y_value) + "," +
                        FormatElementNumber(result.match_best_rect_w_value) + "," +
                        FormatElementNumber(result.match_best_rect_h_value));
        items.push_back("foreground_ratio=" +
                        FormatElementNumber(result.region_foreground_ratio_value));
      }
      return JoinTextItems(items, ";");
    }

    return chain_ref.empty() ? "roi_diff_candidate_ref=missing"
                             : "roi_diff_candidate_ref=" + chain_ref +
                                 ";roi_diff_candidate_count=" +
                                 (!result.published_roi_diff_candidate_count.empty()
                                    ? result.published_roi_diff_candidate_count
                                    : result.roi_diff_candidate_count);
  }
  return std::string();
}

void AddUnifiedElementChain(std::vector<UnifiedElementChain> &chains,
                            const CxScriptExecutionResult &result,
                            const std::vector<UnifiedDetectionElement> &elements,
                            const std::string &chain_type)
{
  UnifiedElementChain chain;
  chain.chain_type = chain_type;
  chain.chain_id = result.task_id.empty() ? (chain_type + ".chain")
                                          : (result.task_id + "." + chain_type + ".chain");
  chain.source_ref = SelectUnifiedChainRef(result, chain_type);
  chain.chain_status = DetermineUnifiedChainStatus(result, chain_type, chain.source_ref);
  chain.target_ref =
    chain.source_ref.empty()
      ? SelectFirstNonEmptyText(std::vector<std::string>{result.published_primary_ref,
                                                         result.published_result_ref,
                                                         result.attach_back_ref,
                                                         result.task_id})
      : chain.source_ref;

  const std::string bbox_ref = SelectUnifiedChainRef(result, "bbox");
  const std::string roi_ref = SelectUnifiedChainRef(result, "roi_crop");
  const std::string template_ref = SelectUnifiedChainRef(result, "template_alignment");
  const std::string diff_ref = SelectUnifiedChainRef(result, "roi_diff");

  if (chain_type == "bbox")
  {
    PushChainElementIfPresent(elements, chain.element_ids, bbox_ref);
    PushChainElementIfPresent(elements, chain.element_ids, bbox_ref + "#center_point");
    PushChainElementIfPresent(elements, chain.element_ids, bbox_ref + "#click_point");
    chain.template_relation = "bbox_to_roi_seed";
  }
  else if (chain_type == "roi_crop")
  {
    PushChainElementIfPresent(elements, chain.element_ids, roi_ref);
    PushChainElementIfPresent(elements, chain.element_ids, roi_ref + "#closed_region");
    PushChainElementIfPresent(elements, chain.element_ids, bbox_ref);
    chain.template_relation = "bbox_to_roi_crop";
  }
  else if (chain_type == "template_alignment")
  {
    PushChainElementIfPresent(elements, chain.element_ids, template_ref);
    PushChainElementIfPresent(elements, chain.element_ids, template_ref + "#template_to_test_line");
    PushChainElementIfPresent(elements, chain.element_ids, diff_ref);
    chain.template_relation = "template_to_test_alignment";
  }
  else if (chain_type == "roi_diff")
  {
    PushChainElementIfPresent(elements, chain.element_ids, diff_ref);
    PushChainElementIfPresent(elements, chain.element_ids, roi_ref + "#closed_region");
    PushChainElementIfPresent(elements, chain.element_ids, template_ref);
    chain.template_relation = "template_diff_to_roi_review";
  }

  chain.chain_findings = BuildUnifiedChainFindings(result, chain_type, chain.source_ref);
  chain.chain_focus = BuildUnifiedChainFocus(result, chain_type, chain.chain_status);
  chain.chain_summary =
    "type=" + chain.chain_type +
    ",status=" + chain.chain_status +
    ",source=" + (chain.source_ref.empty() ? std::string("none") : chain.source_ref) +
    ",elements=" + std::to_string(static_cast<int>(chain.element_ids.size()));
  chains.push_back(chain);
}

std::vector<UnifiedElementChain> BuildUnifiedElementChains(
  const CxScriptExecutionResult &result,
  const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<UnifiedElementChain> chains;
  AddUnifiedElementChain(chains, result, elements, "bbox");
  AddUnifiedElementChain(chains, result, elements, "roi_crop");
  AddUnifiedElementChain(chains, result, elements, "template_alignment");
  AddUnifiedElementChain(chains, result, elements, "roi_diff");
  return chains;
}

std::string SerializeElementChains(const std::vector<UnifiedElementChain> &chains)
{
  std::vector<std::string> serialized_chains;
  for (size_t i = 0; i < chains.size(); ++i)
  {
    const UnifiedElementChain &chain = chains[i];
    std::vector<std::string> fields;
    fields.push_back("chain_id=" + chain.chain_id);
    fields.push_back("chain_type=" + chain.chain_type);
    fields.push_back("chain_status=" + chain.chain_status);
    fields.push_back("source_ref=" + chain.source_ref);
    fields.push_back("target_ref=" + chain.target_ref);
    fields.push_back("element_ids=" + JoinTextItems(chain.element_ids, ";"));
    fields.push_back("template_relation=" + chain.template_relation);
    fields.push_back("chain_summary=" + chain.chain_summary);
    fields.push_back("chain_findings=" + chain.chain_findings);
    fields.push_back("chain_focus=" + chain.chain_focus);
    serialized_chains.push_back(JoinTextItems(fields, "|"));
  }
  return JoinTextItems(serialized_chains, ";;");
}

std::string BuildElementChainSummary(const std::vector<UnifiedElementChain> &chains)
{
  std::vector<std::string> summary_items;
  for (size_t i = 0; i < chains.size(); ++i)
  {
    const UnifiedElementChain &chain = chains[i];
    summary_items.push_back(chain.chain_type + "=" + chain.chain_status +
                            "(" + std::to_string(static_cast<int>(chain.element_ids.size())) +
                            ")");
  }
  return JoinTextItems(summary_items, ",");
}

bool ShouldPromoteChainStatusToAnomaly(const CxScriptExecutionResult &result,
                                       const UnifiedElementChain &chain)
{
  if (!IsCximageClassicalReviewCase(result))
    return chain.chain_status == "missing" ||
           chain.chain_status == "drifted" ||
           chain.chain_status == "abnormal";

  if (chain.chain_type == "roi_diff")
    return chain.chain_status == "missing" ||
           chain.chain_status == "drifted" ||
           chain.chain_status == "abnormal";

  if (chain.chain_status == "abnormal")
    return true;

  return false;
}

void AddDetectionElement(std::vector<UnifiedDetectionElement> &elements,
                         const CxScriptExecutionResult &result,
                         const std::string &source_thread,
                         const std::string &element_id,
                         const std::string &element_type,
                         const std::string &semantic_role,
                         const std::vector<std::string> &geometry_refs,
                         const std::string &preferred_provenance,
                         const std::string &fallback_status,
                         const std::string &linked_template_element_id)
{
  if (element_id.empty())
    return;

  UnifiedDetectionElement element;
  element.element_id = element_id;
  element.element_type = element_type;
  element.semantic_role = semantic_role;
  element.geometry_payload =
    BuildDetectionGeometryPayload(geometry_refs.empty() ? std::vector<std::string>(1, element_id)
                                                        : geometry_refs);
  element.source_ref = SelectFirstNonEmptyText(geometry_refs);
  element.primary_overlay_ref = SelectFirstNonEmptyText(geometry_refs);
  element.overlay_refs = geometry_refs;
  element.provenance = DetermineDetectionProvenance(source_thread, preferred_provenance);
  element.consistency_status =
    DetermineDetectionConsistencyStatus(result, element_id, fallback_status);
  element.candidate_status =
    DetermineDetectionCandidateStatus(result,
                                      element.element_type,
                                      element.semantic_role,
                                      element.consistency_status);
  element.match_status =
    DetermineDetectionMatchStatus(result,
                                  element.element_type,
                                  element.semantic_role,
                                  element.consistency_status);
  element.manual_review_signal =
    BuildDetectionManualReviewSignal(result,
                                     element.element_type,
                                     element.semantic_role,
                                     element.consistency_status);
  element.confidence =
    DetermineDetectionConfidence(element.semantic_role, element.consistency_status);
  element.linked_template_element_id = linked_template_element_id;
  element.template_relation =
    BuildDetectionTemplateRelation(result,
                                   element.element_type,
                                   element.semantic_role,
                                   element.linked_template_element_id);
  element.drift_summary =
    BuildDetectionDriftSummary(result, element.element_type, element.consistency_status);
  element.element_group_id =
    BuildDetectionElementGroupId(result,
                                 element.element_type,
                                 element.semantic_role);
  element.element_group_label =
    BuildDetectionElementGroupLabel(result,
                                    element.element_type,
                                    element.semantic_role);
  element.focus_region_ref =
    BuildDetectionFocusRegionRef(element.element_id,
                                 element.source_ref,
                                 element.primary_overlay_ref,
                                 element.linked_template_element_id);
  element.local_delta_ref =
    BuildDetectionLocalDeltaRef(element.element_id,
                                element.source_ref,
                                element.primary_overlay_ref,
                                element.linked_template_element_id,
                                element.consistency_status,
                                element.semantic_role);
  element.element_findings =
    BuildDetectionElementFindings(result, element.element_type, element.semantic_role);
  element.element_level_focus =
    BuildDetectionElementLevelFocus(result,
                                    element.element_type,
                                    element.semantic_role,
                                    element.consistency_status);
  element.element_status_summary = BuildDetectionElementStatusSummary(element);
  PushUniqueDetectionElement(elements, element);
}

std::string DeterminePrimaryDetectionSemantic(const CxScriptExecutionResult &result)
{
  if (IsEnsmallenPublicResult(result))
  {
    if (!result.cluster_ref.empty() || !result.distance_ref.empty() || !result.anomaly_ref.empty())
      return "ensmallen_cluster_stability_review";
    if (!result.compare_ref.empty())
      return "ensmallen_compare_review";
    if (!result.replay_ref.empty())
      return "ensmallen_replay_review";
    if (!result.summary_ref.empty() || !result.optimization_result_ref.empty())
      return "ensmallen_optimization_review";
  }

  if (!result.template_alignment_ref.empty() || !result.published_template_alignment_ref.empty())
    return "template_pair_alignment";
  if (!result.bbox_candidate_list_ref.empty() || !result.published_bbox_candidate_list_ref.empty())
    return "bbox_candidate_detection";
  if (!result.roi_crop_packet_ref.empty() || !result.published_roi_crop_packet_ref.empty())
    return "roi_crop_extraction";
  if (!result.roi_diff_candidate_ref.empty() || !result.published_roi_diff_candidate_ref.empty())
    return "roi_difference_review";
  if (!result.region_pattern_overlay_ref.empty() || !result.region_pattern_descriptor_ref.empty())
    return "region_pattern_review";
  if (!result.circle_overlay_ref.empty() || !result.circle_edge_overlay_ref.empty())
    return "circle_measurement";
  if (!result.formfit_candidate_overlay_ref.empty() || !result.formfit_selection_overlay_ref.empty())
    return "formfit_candidate_selection";
  if (!result.centerline_overlay_ref.empty() || !result.distance_field_overlay_ref.empty())
    return "geometry_topology_review";
  if (!result.cluster_ref.empty() || !result.distance_ref.empty() || !result.anomaly_ref.empty())
    return "distribution_scoring";
  if (result.module == "cximage" && result.case_name == "line_measure_roi")
    return "line_measurement";
  if (result.module == "cximage" && result.case_name == "findobject_region")
    return "region_matching";
  return "visual_detection_review";
}

void CollectUnifiedDetectionElements(const CxScriptExecutionResult &result,
                                     const std::string &source_thread,
                                     const std::string &primary_visual_ref,
                                     std::vector<UnifiedDetectionElement> &elements)
{
  const std::string line_point_set_ref =
    FindNamedResultFieldValue(result, "refs", "line_point_set_ref");
  const std::string line_measure_bounds_ref =
    FindNamedResultFieldValue(result, "refs", "line_measure_bounds_ref");
  const std::string circle_point_set_ref =
    FindNamedResultFieldValue(result, "refs", "circle_point_set_ref");
  const std::string circle_measure_bounds_ref =
    FindNamedResultFieldValue(result, "refs", "circle_measure_bounds_ref");
  const std::string region_summary_ref =
    FindNamedResultFieldValue(result, "refs", "region_summary_ref");
  const std::string region_bounds_ref =
    FindNamedResultFieldValue(result, "refs", "region_bounds_ref");
  const std::string findobject_candidate_element_id =
    !region_summary_ref.empty() ? (region_summary_ref + "#candidate_pool") : std::string();
  const std::string linked_template_ref =
    !result.published_template_alignment_ref.empty() ? result.published_template_alignment_ref
                                                     : result.template_alignment_ref;
  const std::string fastmatch_template_element_id =
    !result.template_rect_overlay_ref.empty() ? (result.template_rect_overlay_ref + "#template_anchor")
                                              : std::string();
  const std::string fastmatch_test_element_id =
    !result.test_rect_overlay_ref.empty() ? (result.test_rect_overlay_ref + "#test_region")
                                          : std::string();
  const std::string fastmatch_candidate_element_id =
    !result.candidate_overlay_ref.empty() ? (result.candidate_overlay_ref + "#candidate_pool")
                                          : std::string();
  const std::string circle_arc_element_id =
    !result.circle_edge_overlay_ref.empty() ? (result.circle_edge_overlay_ref + "#arc_support")
                                            : std::string();
  const std::string formfit_candidate_element_id =
    !result.formfit_candidate_overlay_ref.empty()
      ? (result.formfit_candidate_overlay_ref + "#candidate_pool")
      : std::string();
  const std::string formfit_selection_element_id =
    !result.formfit_selection_overlay_ref.empty()
      ? (result.formfit_selection_overlay_ref + "#selected_region")
      : std::string();
  const std::string region_pattern_primary_element_id =
    !result.region_pattern_overlay_ref.empty()
      ? (result.region_pattern_overlay_ref + "#region_pattern")
      : std::string();
  const std::string region_pattern_descriptor_element_id =
    !result.region_pattern_descriptor_ref.empty()
      ? (result.region_pattern_descriptor_ref + "#descriptor_support")
      : std::string();
  const std::string bbox_candidate_ref =
    result.bbox_candidate_list_ref.empty() ? result.published_bbox_candidate_list_ref
                                           : result.bbox_candidate_list_ref;
  const std::string roi_crop_packet_ref =
    result.roi_crop_packet_ref.empty() ? result.published_roi_crop_packet_ref
                                       : result.roi_crop_packet_ref;
  const std::string roi_diff_candidate_ref =
    result.roi_diff_candidate_ref.empty() ? result.published_roi_diff_candidate_ref
                                          : result.roi_diff_candidate_ref;
  const std::string prior_roi_region_ref = result.published_prior_roi_region_ref;
  const bool has_torch_element_refs =
    source_thread == "torch" &&
    (!bbox_candidate_ref.empty() ||
     !roi_crop_packet_ref.empty() ||
     !roi_diff_candidate_ref.empty() ||
     !prior_roi_region_ref.empty() ||
     !linked_template_ref.empty() ||
     !result.attach_back_ref.empty());
  const std::string torch_region_anchor_ref =
    has_torch_element_refs
      ? SelectFirstNonEmptyText(std::vector<std::string>{bbox_candidate_ref,
                                                         roi_crop_packet_ref,
                                                         roi_diff_candidate_ref,
                                                         prior_roi_region_ref,
                                                         linked_template_ref,
                                                         result.attach_back_ref,
                                                         primary_visual_ref})
      : std::string();
  const std::string torch_click_point_element_id =
    !torch_region_anchor_ref.empty() ? (torch_region_anchor_ref + "#click_point") : std::string();
  const std::string torch_center_point_element_id =
    !torch_region_anchor_ref.empty() ? (torch_region_anchor_ref + "#center_point") : std::string();
  const std::string torch_alignment_line_element_id =
    !linked_template_ref.empty() ? (linked_template_ref + "#template_to_test_line") : std::string();
  const std::string line_sample_point_element_id =
    !line_point_set_ref.empty() ? (line_point_set_ref + "#sample_point") : std::string();
  const std::string circle_center_point_element_id =
    !result.circle_overlay_ref.empty() ? (result.circle_overlay_ref + "#center_point") : std::string();
  const std::string ensmallen_summary_ref =
    ResolveNamedOrDirectRef(result, "refs", "summary_ref", result.summary_ref);
  const std::string ensmallen_compare_ref =
    ResolveNamedOrDirectRef(result, "refs", "compare_ref", result.compare_ref);
  const std::string ensmallen_replay_ref =
    ResolveNamedOrDirectRef(result, "refs", "replay_ref",
                            result.replay_ref.empty() ? result.replay_log_path
                                                      : result.replay_ref);
  const std::string ensmallen_best_params_ref =
    ResolveNamedOrDirectRef(result, "refs", "best_params_ref", result.best_params_ref);
  const std::string ensmallen_objective_ref =
    ResolveNamedOrDirectRef(result, "refs", "objective_ref", result.objective_ref);
  const std::string ensmallen_optimization_result_ref =
    ResolveNamedOrDirectRef(result, "refs", "optimization_result_ref",
                            result.optimization_result_ref);
  const std::string ensmallen_threshold_ref =
    ResolveEnsmallenBridgeRef(result, "threshold_ref");
  const std::string ensmallen_crop_policy_ref =
    ResolveEnsmallenBridgeRef(result, "crop_policy_ref");
  const std::string ensmallen_boundary_error_ref =
    ResolveEnsmallenBridgeRef(result, "boundary_error_ref");
  const std::string ensmallen_alignment_error_ref =
    ResolveEnsmallenBridgeRef(result, "alignment_error_ref");
  const std::string ensmallen_roi_ref =
    ResolveNamedOrDirectRef(result,
                            "bridge",
                            "roi_ref",
                            FindAssignmentValue(result.input_artifacts, "roi_ref"));

  AddDetectionElement(elements,
                      result,
                      source_thread,
                      fastmatch_template_element_id,
                      "candidate_region",
                      "template_anchor",
                      std::vector<std::string>{result.template_rect_overlay_ref},
                      "manual",
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      fastmatch_test_element_id,
                      "match_region",
                      "review_target",
                      std::vector<std::string>{result.test_rect_overlay_ref,
                                               result.candidate_overlay_ref},
                      "manual",
                      "matched",
                      fastmatch_template_element_id);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      fastmatch_candidate_element_id,
                      "candidate_region",
                      "candidate",
                      std::vector<std::string>{result.candidate_overlay_ref,
                                               result.test_rect_overlay_ref},
                      "manual",
                      result.match_candidate_count_value > 4.0 ? "drifted" : "matched",
                      fastmatch_template_element_id);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      circle_arc_element_id,
                      "arc",
                      "auxiliary",
                      std::vector<std::string>{result.circle_edge_overlay_ref,
                                               result.circle_overlay_ref},
                      "manual",
                      result.circle_avg_distance_value > 2.0 ? "drifted" : "matched",
                      circle_point_set_ref.empty() ? result.circle_overlay_ref : circle_point_set_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      formfit_candidate_element_id,
                      "candidate_region",
                      "candidate",
                      std::vector<std::string>{result.formfit_candidate_overlay_ref,
                                               result.formfit_selection_overlay_ref},
                      "manual",
                      result.formfit_compare_rect_center_delta_value > 3.0 ? "drifted" : "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      formfit_selection_element_id,
                      "match_region",
                      "review_target",
                      std::vector<std::string>{result.formfit_selection_overlay_ref,
                                               result.formfit_candidate_overlay_ref},
                      "manual",
                      result.formfit_compare_rect_center_delta_value > 3.0 ? "drifted" : "matched",
                      formfit_candidate_element_id);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      region_pattern_primary_element_id,
                      "candidate_region",
                      "primary",
                      std::vector<std::string>{result.region_pattern_overlay_ref,
                                               result.region_pattern_descriptor_ref},
                      "manual",
                      (result.region_pattern_foreground_ratio_value < 0.1 ||
                       result.region_pattern_foreground_ratio_value > 0.9) ? "drifted" : "matched",
                      region_pattern_descriptor_element_id);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      region_pattern_descriptor_element_id,
                      "candidate_region",
                      "auxiliary",
                      std::vector<std::string>{result.region_pattern_descriptor_ref,
                                               result.region_pattern_overlay_ref},
                      "manual",
                      result.region_pattern_descriptor_std_value > 0.35 ? "drifted" : "matched",
                      region_pattern_primary_element_id);

  AddDetectionElement(elements,
                      result,
                      source_thread,
                      linked_template_ref,
                      "closed_region",
                      "primary",
                      std::vector<std::string>{result.template_alignment_ref,
                                               result.published_template_alignment_ref,
                                               result.template_rect_overlay_ref,
                                               result.test_rect_overlay_ref},
                      std::string(),
                      (!result.template_test_alignment_status.empty() ||
                       !result.published_template_test_alignment_status.empty())
                        ? DetermineDetectionConsistencyStatus(result,
                                                              linked_template_ref,
                                                              "matched")
                        : "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      torch_alignment_line_element_id,
                      "line_segment",
                      "auxiliary",
                      std::vector<std::string>{linked_template_ref,
                                               result.template_alignment_ref,
                                               result.published_template_alignment_ref,
                                               result.template_rect_overlay_ref,
                                               result.test_rect_overlay_ref},
                      std::string(),
                      "matched",
                      linked_template_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      roi_diff_candidate_ref,
                      "candidate_region",
                      "review_target",
                      std::vector<std::string>{result.roi_diff_candidate_ref,
                                               result.published_roi_diff_candidate_ref,
                                               result.test_rect_overlay_ref},
                      std::string(),
                      "drifted",
                      linked_template_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      bbox_candidate_ref,
                      "candidate_region",
                      "candidate",
                      std::vector<std::string>{result.bbox_candidate_list_ref,
                                               result.published_bbox_candidate_list_ref,
                                               result.candidate_overlay_ref},
                      std::string(),
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      roi_crop_packet_ref,
                      "candidate_region",
                      "auxiliary",
                      std::vector<std::string>{result.roi_crop_packet_ref,
                                               result.published_roi_crop_packet_ref,
                                               result.published_prior_roi_region_ref},
                      std::string(),
                      "matched",
                      linked_template_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      roi_crop_packet_ref.empty() ? std::string() : (roi_crop_packet_ref + "#closed_region"),
                      "closed_region",
                      "review_target",
                      std::vector<std::string>{roi_crop_packet_ref,
                                               result.roi_crop_packet_ref,
                                               result.published_roi_crop_packet_ref,
                                               result.published_prior_roi_region_ref},
                      std::string(),
                      "matched",
                      bbox_candidate_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      prior_roi_region_ref,
                      "closed_region",
                      "auxiliary",
                      std::vector<std::string>{result.published_prior_roi_region_ref,
                                               result.roi_crop_packet_ref,
                                               result.published_roi_crop_packet_ref},
                      std::string(),
                      "matched",
                      linked_template_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      torch_center_point_element_id,
                      "point",
                      "auxiliary",
                      std::vector<std::string>{torch_region_anchor_ref,
                                               bbox_candidate_ref,
                                               roi_crop_packet_ref,
                                               linked_template_ref},
                      std::string(),
                      "matched",
                      torch_region_anchor_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      torch_click_point_element_id,
                      "click_point",
                      "review_target",
                      std::vector<std::string>{torch_region_anchor_ref,
                                               result.attach_back_ref,
                                               primary_visual_ref},
                      "manual",
                      "matched",
                      torch_region_anchor_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      line_point_set_ref,
                      "open_polyline",
                      "primary",
                      std::vector<std::string>{line_point_set_ref,
                                               line_measure_bounds_ref},
                      "manual",
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      line_sample_point_element_id,
                      "point",
                      "auxiliary",
                      std::vector<std::string>{line_point_set_ref,
                                               line_measure_bounds_ref},
                      "manual",
                      "matched",
                      line_measure_bounds_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      line_measure_bounds_ref,
                      "line_segment",
                      "review_target",
                      std::vector<std::string>{line_measure_bounds_ref,
                                               line_point_set_ref},
                      "manual",
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      circle_center_point_element_id,
                      "point",
                      "primary",
                      std::vector<std::string>{result.circle_overlay_ref,
                                               circle_point_set_ref,
                                               result.circle_edge_overlay_ref},
                      "manual",
                      "matched",
                      circle_point_set_ref.empty() ? result.circle_overlay_ref : circle_point_set_ref);
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      circle_point_set_ref.empty() ? result.circle_overlay_ref : circle_point_set_ref,
                      "circle",
                      "primary",
                      std::vector<std::string>{circle_point_set_ref,
                                               result.circle_overlay_ref,
                                               result.circle_edge_overlay_ref},
                      "manual",
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      circle_measure_bounds_ref,
                      "candidate_region",
                      "review_target",
                      std::vector<std::string>{circle_measure_bounds_ref,
                                               circle_point_set_ref,
                                               result.circle_overlay_ref},
                      "manual",
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      findobject_candidate_element_id,
                      "candidate_region",
                      "candidate",
                      std::vector<std::string>{region_summary_ref,
                                               region_bounds_ref},
                      "manual",
                      result.match_candidate_count_value > 1.0 ? "drifted" : "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      region_bounds_ref.empty() ? region_summary_ref : region_bounds_ref,
                      "match_region",
                      "primary",
                      std::vector<std::string>{region_bounds_ref,
                                               region_summary_ref,
                                               result.region_pattern_overlay_ref},
                      "manual",
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      result.formfit_candidate_overlay_ref,
                      "candidate_region",
                      "candidate",
                      std::vector<std::string>{result.formfit_candidate_overlay_ref,
                                               result.formfit_selection_overlay_ref},
                      "manual",
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      result.formfit_selection_overlay_ref,
                      "match_region",
                      "review_target",
                      std::vector<std::string>{result.formfit_selection_overlay_ref,
                                               result.formfit_candidate_overlay_ref},
                      "manual",
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      result.distance_field_overlay_ref,
                      "closed_region",
                      "auxiliary",
                      std::vector<std::string>{result.distance_field_overlay_ref,
                                               result.skeleton_overlay_ref,
                                               result.centerline_overlay_ref,
                                               result.topology_repair_overlay_ref},
                      "manual",
                      "matched",
                      std::string());
  AddDetectionElement(elements,
                      result,
                      source_thread,
                      result.centerline_overlay_ref.empty() ? result.skeleton_overlay_ref
                                                            : result.centerline_overlay_ref,
                      "open_polyline",
                      "primary",
                      std::vector<std::string>{result.centerline_overlay_ref,
                                               result.skeleton_overlay_ref,
                                               result.topology_repair_overlay_ref},
                      "manual",
                      "matched",
                      std::string());

  if (IsEnsmallenPublicResult(result))
  {
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_roi_ref,
                        "closed_region",
                        "review_target",
                        std::vector<std::string>{ensmallen_roi_ref,
                                                 ensmallen_compare_ref,
                                                 ensmallen_objective_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "boundary_metric"),
                        ensmallen_objective_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_summary_ref,
                        "optimization_summary",
                        result.layer == "train" ? "primary" : "auxiliary",
                        std::vector<std::string>{ensmallen_summary_ref,
                                                 ensmallen_optimization_result_ref,
                                                 ensmallen_compare_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "optimization_summary"),
                        ensmallen_objective_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_compare_ref,
                        "optimization_compare",
                        "review_target",
                        std::vector<std::string>{ensmallen_compare_ref,
                                                 ensmallen_summary_ref,
                                                 ensmallen_replay_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "optimization_compare"),
                        ensmallen_objective_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_replay_ref,
                        "replay_trace",
                        "auxiliary",
                        std::vector<std::string>{ensmallen_replay_ref,
                                                 ensmallen_compare_ref,
                                                 ensmallen_summary_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "replay_trace"),
                        ensmallen_compare_ref.empty() ? ensmallen_objective_ref
                                                      : ensmallen_compare_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_optimization_result_ref,
                        "optimization_result",
                        "primary",
                        std::vector<std::string>{ensmallen_optimization_result_ref,
                                                 ensmallen_best_params_ref,
                                                 ensmallen_summary_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "optimization_result"),
                        ensmallen_objective_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_best_params_ref,
                        "parameter_set",
                        "selected",
                        std::vector<std::string>{ensmallen_best_params_ref,
                                                 ensmallen_optimization_result_ref,
                                                 ensmallen_objective_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "parameter_set"),
                        ensmallen_objective_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_objective_ref,
                        "objective_target",
                        "review_target",
                        std::vector<std::string>{ensmallen_objective_ref,
                                                 ensmallen_boundary_error_ref,
                                                 ensmallen_alignment_error_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "objective_target"),
                        std::string());
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_threshold_ref,
                        "threshold_policy",
                        "auxiliary",
                        std::vector<std::string>{ensmallen_threshold_ref,
                                                 ensmallen_objective_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "threshold_policy"),
                        ensmallen_objective_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_crop_policy_ref,
                        "crop_policy",
                        "auxiliary",
                        std::vector<std::string>{ensmallen_crop_policy_ref,
                                                 ensmallen_objective_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "crop_policy"),
                        ensmallen_objective_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_boundary_error_ref,
                        "boundary_metric",
                        "review_target",
                        std::vector<std::string>{ensmallen_boundary_error_ref,
                                                 ensmallen_objective_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "boundary_metric"),
                        ensmallen_objective_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        ensmallen_alignment_error_ref,
                        "alignment_metric",
                        "review_target",
                        std::vector<std::string>{ensmallen_alignment_error_ref,
                                                 ensmallen_objective_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "alignment_metric"),
                        ensmallen_objective_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        result.cluster_ref,
                        "cluster_group",
                        "primary",
                        std::vector<std::string>{result.cluster_ref,
                                                 result.distance_ref,
                                                 result.anomaly_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "cluster_group"),
                        result.baseline_feature_ref.empty() ? ensmallen_objective_ref
                                                            : result.baseline_feature_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        result.distance_ref,
                        "distance_measure",
                        "auxiliary",
                        std::vector<std::string>{result.distance_ref,
                                                 result.cluster_ref,
                                                 result.anomaly_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "distance_measure"),
                        result.cluster_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        result.anomaly_ref,
                        "anomaly_focus",
                        "review_target",
                        std::vector<std::string>{result.anomaly_ref,
                                                 result.distance_ref,
                                                 result.cluster_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "anomaly_focus"),
                        result.cluster_ref.empty() ? ensmallen_compare_ref
                                                   : result.cluster_ref);
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        result.baseline_feature_ref,
                        "baseline_feature",
                        "template_anchor",
                        std::vector<std::string>{result.baseline_feature_ref,
                                                 result.baseline_class_ref,
                                                 result.compare_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "baseline_feature"),
                        std::string());
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        result.baseline_class_ref,
                        "baseline_class",
                        "template_anchor",
                        std::vector<std::string>{result.baseline_class_ref,
                                                 result.baseline_feature_ref},
                        "mixed",
                        DetermineEnsmallenElementFallbackStatus(result,
                                                                "baseline_class"),
                        result.baseline_feature_ref);
  }

  if (elements.empty() && !primary_visual_ref.empty())
  {
    const std::string fallback_status =
      result.success ? "matched" : std::string("abnormal");
    AddDetectionElement(elements,
                        result,
                        source_thread,
                        primary_visual_ref,
                        "candidate_region",
                        "review_target",
                        std::vector<std::string>{primary_visual_ref},
                        std::string(),
                        fallback_status,
                        linked_template_ref);
  }
}

int CountDetectionElementsByStatus(const std::vector<UnifiedDetectionElement> &elements,
                                   const std::string &status)
{
  int count = 0;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    if (elements[i].consistency_status == status)
      ++count;
  }
  return count;
}

int CountDetectionElementsBySemanticRole(const std::vector<UnifiedDetectionElement> &elements,
                                         const std::string &semantic_role)
{
  int count = 0;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    if (elements[i].semantic_role == semantic_role)
      ++count;
  }
  return count;
}

std::string BuildElementTypeSummary(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> summary_items;
  std::vector<std::string> seen_types;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const std::string &element_type = elements[i].element_type;
    if (element_type.empty())
      continue;

    bool already_seen = false;
    for (size_t seen_index = 0; seen_index < seen_types.size(); ++seen_index)
    {
      if (seen_types[seen_index] == element_type)
      {
        already_seen = true;
        break;
      }
    }
    if (already_seen)
      continue;

    int type_count = 0;
    for (size_t j = 0; j < elements.size(); ++j)
    {
      if (elements[j].element_type == element_type)
        ++type_count;
    }
    seen_types.push_back(element_type);
    summary_items.push_back(element_type + "=" + std::to_string(type_count));
  }
  return JoinTextItems(summary_items, ",");
}

std::string BuildElementStatusSummary(const std::vector<UnifiedDetectionElement> &elements,
                                      const std::string &status)
{
  std::vector<std::string> element_ids;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    if (elements[i].consistency_status == status)
      PushUniqueText(element_ids, elements[i].element_id);
  }

  if (element_ids.empty())
    return "count=0";
  return "count=" + std::to_string(static_cast<int>(element_ids.size())) +
         "|ids=" + JoinTextItems(element_ids, ",");
}

std::string SerializeDetectionElements(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> serialized_elements;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = elements[i];
    std::vector<std::string> element_fields;
    element_fields.push_back("element_id=" + element.element_id);
    element_fields.push_back("element_type=" + element.element_type);
    element_fields.push_back("semantic_role=" + element.semantic_role);
    element_fields.push_back("geometry_payload=" + element.geometry_payload);
    element_fields.push_back("source_ref=" + element.source_ref);
    element_fields.push_back("primary_overlay_ref=" + element.primary_overlay_ref);
    element_fields.push_back("overlay_ref=" + element.primary_overlay_ref);
    element_fields.push_back("overlay_refs=" + JoinTextItems(element.overlay_refs, ";"));
    element_fields.push_back("confidence=" + element.confidence);
    element_fields.push_back("provenance=" + element.provenance);
    element_fields.push_back("element_source=" + element.provenance);
    element_fields.push_back("consistency_status=" + element.consistency_status);
    element_fields.push_back("candidate_status=" + element.candidate_status);
    element_fields.push_back("match_status=" + element.match_status);
    element_fields.push_back("manual_review_signal=" + element.manual_review_signal);
    element_fields.push_back("review_signal=" + element.manual_review_signal);
    element_fields.push_back("linked_template_element_id=" + element.linked_template_element_id);
    element_fields.push_back("template_relation=" + element.template_relation);
    element_fields.push_back("drift_summary=" + element.drift_summary);
    element_fields.push_back("element_group_id=" + element.element_group_id);
    element_fields.push_back("element_group_label=" + element.element_group_label);
    element_fields.push_back("focus_region_ref=" + element.focus_region_ref);
    element_fields.push_back("local_delta_ref=" + element.local_delta_ref);
    element_fields.push_back("element_status_summary=" + element.element_status_summary);
    element_fields.push_back("element_findings=" + element.element_findings);
    element_fields.push_back("geometry_summary=" +
                             (element.element_findings.empty() ? element.geometry_payload
                                                               : element.element_findings));
    element_fields.push_back("element_level_focus=" + element.element_level_focus);
    serialized_elements.push_back(JoinTextItems(element_fields, "|"));
  }
  return JoinTextItems(serialized_elements, ";;");
}

std::string BuildElementStatusBoardSummary(const std::vector<UnifiedDetectionElement> &elements)
{
  return BuildDetectionElementSummaryByField(elements, "element_status_summary");
}

std::string BuildElementFindingsSummary(const std::vector<UnifiedDetectionElement> &elements)
{
  return BuildDetectionElementSummaryByField(elements, "element_findings");
}

std::string BuildElementLevelFocusSummary(const std::vector<UnifiedDetectionElement> &elements)
{
  return BuildDetectionElementSummaryByField(elements, "element_level_focus");
}

std::string BuildCandidateStatusSummary(const std::vector<UnifiedDetectionElement> &elements)
{
  return BuildDetectionElementSummaryByField(elements, "candidate_status");
}

std::string BuildMatchStatusSummary(const std::vector<UnifiedDetectionElement> &elements)
{
  return BuildDetectionElementSummaryByField(elements, "match_status");
}

std::string BuildManualReviewSignalSummary(const std::vector<UnifiedDetectionElement> &elements)
{
  return BuildDetectionElementSummaryByField(elements, "manual_review_signal");
}

std::string BuildElementGroupSummary(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> seen_group_ids;
  std::vector<std::string> summary_items;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = elements[i];
    if (element.element_group_id.empty())
      continue;

    bool already_seen = false;
    for (size_t j = 0; j < seen_group_ids.size(); ++j)
    {
      if (seen_group_ids[j] == element.element_group_id)
      {
        already_seen = true;
        break;
      }
    }
    if (already_seen)
      continue;

    int group_count = 0;
    for (size_t group_index = 0; group_index < elements.size(); ++group_index)
    {
      if (elements[group_index].element_group_id == element.element_group_id)
        ++group_count;
    }
    seen_group_ids.push_back(element.element_group_id);
    summary_items.push_back(element.element_group_id +
                            "[" + element.element_group_label + "]=" +
                            std::to_string(group_count));
  }
  return JoinTextItems(summary_items, " || ");
}

std::string BuildFocusRefreshTargets(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> targets;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = elements[i];
    if (element.focus_region_ref.empty())
      continue;
    const bool focus_required =
      element.consistency_status != "matched" ||
      element.semantic_role == "candidate" ||
      element.semantic_role == "review_target" ||
      element.semantic_role == "primary";
    if (!focus_required)
      continue;

    std::vector<std::string> parts;
    parts.push_back("element_id=" + element.element_id);
    parts.push_back("group=" + element.element_group_label);
    parts.push_back("focus_ref=" + element.focus_region_ref);
    targets.push_back(JoinTextItems(parts, "|"));
  }
  return JoinTextItems(targets, ";;");
}

std::string BuildLocalDeltaTargets(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> targets;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = elements[i];
    if (element.local_delta_ref.empty())
      continue;
    const bool delta_required =
      element.consistency_status != "matched" ||
      element.semantic_role == "candidate" ||
      element.semantic_role == "review_target";
    if (!delta_required)
      continue;

    std::vector<std::string> parts;
    parts.push_back("element_id=" + element.element_id);
    parts.push_back("status=" + element.consistency_status);
    parts.push_back("delta_ref=" + element.local_delta_ref);
    targets.push_back(JoinTextItems(parts, "|"));
  }
  return JoinTextItems(targets, ";;");
}

bool ShouldPromoteElementIntoCompactPreview(const UnifiedDetectionElement &element)
{
  if (element.consistency_status != "matched")
    return true;
  if (element.semantic_role == "candidate" ||
      element.semantic_role == "review_target" ||
      element.semantic_role == "primary")
    return true;
  if (!element.candidate_status.empty() ||
      !element.match_status.empty() ||
      !element.manual_review_signal.empty() ||
      !element.focus_region_ref.empty() ||
      !element.local_delta_ref.empty())
    return true;
  return false;
}

std::string BuildCompactElementPreview(const UnifiedDetectionElement &element)
{
  std::vector<std::string> parts;
  parts.push_back("element_id=" + element.element_id);
  parts.push_back("type=" + element.element_type);
  parts.push_back("role=" + element.semantic_role);
  parts.push_back("status=" + element.consistency_status);
  if (!element.candidate_status.empty())
    parts.push_back("candidate=" + element.candidate_status);
  if (!element.match_status.empty())
    parts.push_back("match=" + element.match_status);
  if (!element.manual_review_signal.empty())
    parts.push_back("manual=" + element.manual_review_signal);
  if (!element.element_group_label.empty())
    parts.push_back("group=" + element.element_group_label);
  if (!element.template_relation.empty())
    parts.push_back("template_relation=" + element.template_relation);
  if (!element.drift_summary.empty())
    parts.push_back("drift=" + element.drift_summary);
  if (!element.focus_region_ref.empty())
    parts.push_back("focus_ref=" + element.focus_region_ref);
  if (!element.local_delta_ref.empty())
    parts.push_back("delta_ref=" + element.local_delta_ref);
  if (!element.primary_overlay_ref.empty())
    parts.push_back("overlay=" + element.primary_overlay_ref);
  return JoinTextItems(parts, "|");
}

std::string BuildGroupedElementPreview(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> seen_group_ids;
  std::vector<std::string> groups;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = elements[i];
    if (element.element_group_id.empty())
      continue;

    bool already_seen = false;
    for (size_t j = 0; j < seen_group_ids.size(); ++j)
    {
      if (seen_group_ids[j] == element.element_group_id)
      {
        already_seen = true;
        break;
      }
    }
    if (already_seen)
      continue;

    seen_group_ids.push_back(element.element_group_id);
    int count = 0;
    std::vector<std::string> type_items;
    std::vector<std::string> status_items;
    std::vector<std::string> focus_items;
    std::vector<std::string> sample_elements;
    for (size_t k = 0; k < elements.size(); ++k)
    {
      const UnifiedDetectionElement &group_element = elements[k];
      if (group_element.element_group_id != element.element_group_id)
        continue;

      ++count;
      PushUniqueText(type_items, group_element.element_type);
      PushUniqueText(status_items, group_element.consistency_status);
      if (!group_element.focus_region_ref.empty())
        PushUniqueText(focus_items, group_element.focus_region_ref);
      if (sample_elements.size() < 3)
        sample_elements.push_back(group_element.element_id);
    }

    std::vector<std::string> parts;
    parts.push_back("group_id=" + element.element_group_id);
    parts.push_back("label=" + element.element_group_label);
    parts.push_back("count=" + std::to_string(count));
    if (!type_items.empty())
      parts.push_back("types=" + JoinTextItems(type_items, ","));
    if (!status_items.empty())
      parts.push_back("statuses=" + JoinTextItems(status_items, ","));
    if (!focus_items.empty())
      parts.push_back("focus_refs=" + JoinTextItems(focus_items, ","));
    if (!sample_elements.empty())
      parts.push_back("sample_ids=" + JoinTextItems(sample_elements, ","));
    groups.push_back(JoinTextItems(parts, "|"));
  }
  return JoinTextItems(groups, ";;");
}

std::string BuildFocusElementPreview(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> previews;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = elements[i];
    if (element.focus_region_ref.empty())
      continue;
    if (!ShouldPromoteElementIntoCompactPreview(element))
      continue;
    previews.push_back(BuildCompactElementPreview(element));
  }
  return JoinTextItems(previews, ";;");
}

std::string BuildDeltaElementPreview(const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> previews;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = elements[i];
    const bool delta_required =
      element.consistency_status != "matched" || !element.local_delta_ref.empty();
    if (!delta_required)
      continue;
    previews.push_back(BuildCompactElementPreview(element));
  }
  return JoinTextItems(previews, ";;");
}

void PushChangedField(std::vector<std::string> &changed_fields,
                      const std::string &field_name,
                      bool should_include = true)
{
  if (!should_include || field_name.empty())
    return;
  PushUniqueText(changed_fields, field_name);
}

std::vector<std::string> BuildAllChangedElementIds(
  const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> changed_element_ids;
  for (size_t i = 0; i < elements.size(); ++i)
    PushUniqueText(changed_element_ids, elements[i].element_id);
  return changed_element_ids;
}

std::vector<std::string> BuildDeltaChangedElementIds(
  const std::vector<UnifiedDetectionElement> &elements)
{
  std::vector<std::string> changed_element_ids;
  for (size_t i = 0; i < elements.size(); ++i)
  {
    if (elements[i].consistency_status == "matched")
      continue;
    PushUniqueText(changed_element_ids, elements[i].element_id);
  }
  return changed_element_ids;
}

std::vector<std::string> BuildAllChangedChainKeys(
  const std::vector<UnifiedElementChain> &chains)
{
  std::vector<std::string> changed_chain_keys;
  for (size_t i = 0; i < chains.size(); ++i)
  {
    PushUniqueText(changed_chain_keys,
                   chains[i].chain_id.empty() ? chains[i].chain_type : chains[i].chain_id);
  }
  return changed_chain_keys;
}

std::vector<std::string> BuildDeltaChangedChainKeys(
  const std::vector<UnifiedElementChain> &chains)
{
  std::vector<std::string> changed_chain_keys;
  for (size_t i = 0; i < chains.size(); ++i)
  {
    if (chains[i].chain_status == "matched")
      continue;
    PushUniqueText(changed_chain_keys,
                   chains[i].chain_id.empty() ? chains[i].chain_type : chains[i].chain_id);
  }
  return changed_chain_keys;
}

std::vector<std::string> BuildUnifiedImageChangedFields(const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> changed_fields;
  PushChangedField(changed_fields, "source_thread");
  PushChangedField(changed_fields, "task_id");
  PushChangedField(changed_fields, "batch_id", !image_review.batch_id.empty());
  PushChangedField(changed_fields, "case_name");
  PushChangedField(changed_fields, "image_id");
  PushChangedField(changed_fields, "stage");
  PushChangedField(changed_fields, "input_image_ref");
  PushChangedField(changed_fields, "primary_visual_ref");
  PushChangedField(changed_fields, "status");
  PushChangedField(changed_fields, "metrics", !image_review.metric_summary_text.empty());
  PushChangedField(changed_fields, "anomaly_flags", !image_review.anomaly_flags.empty());
  PushChangedField(changed_fields, "notes", !image_review.notes.empty());
  PushChangedField(changed_fields, "output_image_refs", !image_review.output_image_refs.empty());
  PushChangedField(changed_fields, "visualization_refs", !image_review.visualization_refs.empty());
  PushChangedField(changed_fields, "artifact_refs", !image_review.artifact_refs.empty());
  PushChangedField(changed_fields, "contract_evidence", !image_review.contract_evidence.empty());
  PushChangedField(changed_fields, "phenomenon_evidence", !image_review.phenomenon_evidence.empty());
  PushChangedField(changed_fields, "interaction_evidence", !image_review.interaction_evidence.empty());
  PushChangedField(changed_fields, "observation_personality",
                   !image_review.observation_personality.empty());
  PushChangedField(changed_fields, "default_open_chain", !image_review.default_open_chain.empty());
  PushChangedField(changed_fields, "evidence_focus_summary",
                   !image_review.evidence_focus_summary.empty());
  PushChangedField(changed_fields, "thread_handoff", !image_review.thread_handoff.empty());
  PushChangedField(changed_fields, "task_entry_name", !image_review.task_entry_name.empty());
  PushChangedField(changed_fields, "task_family", !image_review.task_family.empty());
  PushChangedField(changed_fields, "pipeline_family", !image_review.pipeline_family.empty());
  PushChangedField(changed_fields, "model_family", !image_review.model_family.empty());
  PushChangedField(changed_fields, "scenario_family", !image_review.scenario_family.empty());
  PushChangedField(changed_fields, "visual_evidence_set", !image_review.visual_evidence_set.empty());
  PushChangedField(changed_fields, "device_evidence", !image_review.device_evidence.empty());
  PushChangedField(changed_fields, "business_eval_fields", !image_review.business_eval_fields.empty());
  PushChangedField(changed_fields, "pipeline_link_trace", !image_review.pipeline_link_trace.empty());
  PushChangedField(changed_fields, "sequence_family", !image_review.sequence_family.empty());
  PushChangedField(changed_fields, "sequence_stage", !image_review.sequence_stage.empty());
  PushChangedField(changed_fields, "sequence_index", !image_review.sequence_index.empty());
  PushChangedField(changed_fields, "sequence_trace_ref", !image_review.sequence_trace_ref.empty());
  PushChangedField(changed_fields, "sequence_records", !image_review.sequence_records.empty());
  PushChangedField(changed_fields, "sequence_summary", !image_review.sequence_summary.empty());
  PushChangedField(changed_fields, "sequence_status_summary",
                   !image_review.sequence_status_summary.empty());
  PushChangedField(changed_fields, "stage_refs", !image_review.stage_refs.empty());
  PushChangedField(changed_fields, "script_refs", !image_review.script_refs.empty());
  PushChangedField(changed_fields, "image_refs", !image_review.image_refs.empty());
  PushChangedField(changed_fields, "conclusion_refs", !image_review.conclusion_refs.empty());
  PushChangedField(changed_fields, "issue_refs", !image_review.issue_refs.empty());
  PushChangedField(changed_fields, "lifecycle_summary", !image_review.lifecycle_summary.empty());
  PushChangedField(changed_fields, "lifecycle_zone_refs", !image_review.lifecycle_zone_refs.empty());
  PushChangedField(changed_fields, "init_stage_refs", !image_review.init_stage_refs.empty());
  PushChangedField(changed_fields, "repeatable_stage_refs", !image_review.repeatable_stage_refs.empty());
  PushChangedField(changed_fields, "debug_stage_refs", !image_review.debug_stage_refs.empty());
  PushChangedField(changed_fields, "replay_stage_refs", !image_review.replay_stage_refs.empty());
  PushChangedField(changed_fields, "reset_stage_refs", !image_review.reset_stage_refs.empty());
  PushChangedField(changed_fields, "lifecycle_risk_summary",
                   !image_review.lifecycle_risk_summary.empty());
  PushChangedField(changed_fields, "test_image_ref", !image_review.test_image_ref.empty());
  PushChangedField(changed_fields, "visual_evidence_ref_set", !image_review.visual_evidence_ref_set.empty());
  PushChangedField(changed_fields, "single_image_conclusion_ref", !image_review.single_image_conclusion_ref.empty());
  PushChangedField(changed_fields, "element_conclusion_ref_set", !image_review.element_conclusion_ref_set.empty());
  PushChangedField(changed_fields, "task_conclusion_ref", !image_review.task_conclusion_ref.empty());
  PushChangedField(changed_fields, "anomaly_conclusion_ref", !image_review.anomaly_conclusion_ref.empty());
  PushChangedField(changed_fields, "next_action_ref", !image_review.next_action_ref.empty());
  PushChangedField(changed_fields, "element_ref_set", !image_review.element_ref_set.empty());
  PushChangedField(changed_fields, "element_type", !image_review.element_type.empty());
  PushChangedField(changed_fields, "element_source", !image_review.element_source.empty());
  PushChangedField(changed_fields, "element_visual_anchor", !image_review.element_visual_anchor.empty());
  PushChangedField(changed_fields, "template_relation", !image_review.template_relation.empty());
  PushChangedField(changed_fields, "consistency_status", !image_review.consistency_status.empty());
  PushChangedField(changed_fields, "chain_ref_set", !image_review.chain_ref_set.empty());
  PushChangedField(changed_fields, "chain_key", !image_review.chain_key.empty());
  PushChangedField(changed_fields, "chain_status", !image_review.chain_status.empty());
  PushChangedField(changed_fields, "chain_focus_ref", !image_review.chain_focus_ref.empty());
  PushChangedField(changed_fields, "chain_issue_ref", !image_review.chain_issue_ref.empty());
  PushChangedField(changed_fields, "stage_ref_set", !image_review.stage_ref_set.empty());
  PushChangedField(changed_fields, "current_stage", !image_review.current_stage.empty());
  PushChangedField(changed_fields, "upstream_ref", !image_review.upstream_ref.empty());
  PushChangedField(changed_fields, "downstream_ref", !image_review.downstream_ref.empty());
  PushChangedField(changed_fields, "stage_status", !image_review.stage_status.empty());
  PushChangedField(changed_fields, "issue_entry_ref_set", !image_review.issue_entry_ref_set.empty());
  PushChangedField(changed_fields, "recommended_image_ref", !image_review.recommended_image_ref.empty());
  PushChangedField(changed_fields, "recommended_element_ref", !image_review.recommended_element_ref.empty());
  PushChangedField(changed_fields, "recommended_chain_ref", !image_review.recommended_chain_ref.empty());
  PushChangedField(changed_fields, "recommended_stage_ref", !image_review.recommended_stage_ref.empty());
  PushChangedField(changed_fields, "issue_kind_hint", !image_review.issue_kind_hint.empty());
  PushChangedField(changed_fields, "raw_image_ref", !image_review.raw_image_ref.empty());
  PushChangedField(changed_fields, "edge_image_ref", !image_review.edge_image_ref.empty());
  PushChangedField(changed_fields, "element_relation_image_ref", !image_review.element_relation_image_ref.empty());
  PushChangedField(changed_fields, "candidate_image_ref", !image_review.candidate_image_ref.empty());
  PushChangedField(changed_fields, "match_image_ref", !image_review.match_image_ref.empty());
  PushChangedField(changed_fields, "geometry_stage", !image_review.geometry_stage.empty());
  PushChangedField(changed_fields, "candidate_stage", !image_review.candidate_stage.empty());
  PushChangedField(changed_fields, "match_stage", !image_review.match_stage.empty());
  PushChangedField(changed_fields, "problem_focus_image_ref", !image_review.problem_focus_image_ref.empty());
  PushChangedField(changed_fields, "problem_focus_element_id", !image_review.problem_focus_element_id.empty());
  PushChangedField(changed_fields, "problem_focus_chain_id", !image_review.problem_focus_chain_id.empty());
  PushChangedField(changed_fields, "problem_issue_type", !image_review.problem_issue_type.empty());
  PushChangedField(changed_fields, "single_image_geometry_conclusion", !image_review.single_image_geometry_conclusion.empty());
  PushChangedField(changed_fields, "element_conclusion", !image_review.element_conclusion.empty());
  PushChangedField(changed_fields, "match_conclusion", !image_review.match_conclusion.empty());
  PushChangedField(changed_fields, "task_conclusion", !image_review.task_conclusion.empty());
  PushChangedField(changed_fields, "next_step_suggestion", !image_review.next_step_suggestion.empty());
  PushChangedField(changed_fields, "gui_chain_summary", !image_review.gui_chain_summary.empty());
  PushChangedField(changed_fields, "detection_elements", !image_review.detection_elements.empty());
  PushChangedField(changed_fields, "element_chains", !image_review.element_chains.empty());
  PushChangedField(changed_fields, "primary_detection_semantic",
                   !image_review.primary_detection_semantic.empty());
  PushChangedField(changed_fields, "template_alignment_status",
                   !image_review.template_alignment_status.empty());
  PushChangedField(changed_fields, "element_group_summary",
                   !image_review.element_group_summary.empty());
  PushChangedField(changed_fields, "element_status_summary",
                   !image_review.element_status_summary.empty());
  PushChangedField(changed_fields, "candidate_status_summary",
                   !image_review.candidate_status_summary.empty());
  PushChangedField(changed_fields, "match_status_summary",
                   !image_review.match_status_summary.empty());
  PushChangedField(changed_fields, "manual_review_signal_summary",
                   !image_review.manual_review_signal_summary.empty());
  PushChangedField(changed_fields, "element_findings",
                   !image_review.element_findings.empty());
  PushChangedField(changed_fields, "element_level_focus",
                   !image_review.element_level_focus.empty());
  PushChangedField(changed_fields, "focus_refresh_targets",
                   !image_review.focus_refresh_targets.empty());
  PushChangedField(changed_fields, "local_delta_targets",
                   !image_review.local_delta_targets.empty());
  PushChangedField(changed_fields, "grouped_element_preview",
                   !image_review.grouped_element_preview.empty());
  PushChangedField(changed_fields, "focus_element_preview",
                   !image_review.focus_element_preview.empty());
  PushChangedField(changed_fields, "delta_element_preview",
                   !image_review.delta_element_preview.empty());
  PushChangedField(changed_fields, "missing_element_count");
  PushChangedField(changed_fields, "abnormal_element_count");
  PushChangedField(changed_fields, "drifted_element_count");
  PushChangedField(changed_fields, "candidate_element_count");
  return changed_fields;
}

std::vector<std::string> BuildUnifiedTaskChangedFields(const UnifiedTaskReviewBundle &task_review)
{
  std::vector<std::string> changed_fields;
  PushChangedField(changed_fields, "source_thread");
  PushChangedField(changed_fields, "task_id");
  PushChangedField(changed_fields, "batch_id", !task_review.batch_id.empty());
  PushChangedField(changed_fields, "task_type");
  PushChangedField(changed_fields, "case_group");
  PushChangedField(changed_fields, "primary_visual_ref", !task_review.primary_visual_ref.empty());
  PushChangedField(changed_fields, "total_images");
  PushChangedField(changed_fields, "abnormal_images");
  PushChangedField(changed_fields, "focus_image_ids", !task_review.focus_image_ids.empty());
  PushChangedField(changed_fields, "metric_summary", !task_review.metric_summary.empty());
  PushChangedField(changed_fields, "stage_summary", !task_review.stage_summary.empty());
  PushChangedField(changed_fields, "current_conclusion", !task_review.current_conclusion.empty());
  PushChangedField(changed_fields, "next_attention_points",
                   !task_review.next_attention_points.empty());
  PushChangedField(changed_fields, "status_distribution", !task_review.status_distribution.empty());
  PushChangedField(changed_fields, "anomaly_type_distribution",
                   !task_review.anomaly_type_distribution.empty());
  PushChangedField(changed_fields, "baseline_compare_summary",
                   !task_review.baseline_compare_summary.empty());
  PushChangedField(changed_fields, "review_required_count");
  PushChangedField(changed_fields, "top_metric_outliers", !task_review.top_metric_outliers.empty());
  PushChangedField(changed_fields, "training_evidence_summary",
                   !task_review.training_evidence_summary.empty());
  PushChangedField(changed_fields, "artifact_bundle_refs", !task_review.artifact_bundle_refs.empty());
  PushChangedField(changed_fields, "supporting_refs", !task_review.supporting_refs.empty());
  PushChangedField(changed_fields, "observation_personality",
                   !task_review.observation_personality.empty());
  PushChangedField(changed_fields, "default_open_chain", !task_review.default_open_chain.empty());
  PushChangedField(changed_fields, "evidence_focus_summary",
                   !task_review.evidence_focus_summary.empty());
  PushChangedField(changed_fields, "thread_handoff", !task_review.thread_handoff.empty());
  PushChangedField(changed_fields, "task_family", !task_review.task_family.empty());
  PushChangedField(changed_fields, "pipeline_family", !task_review.pipeline_family.empty());
  PushChangedField(changed_fields, "model_family", !task_review.model_family.empty());
  PushChangedField(changed_fields, "scenario_family", !task_review.scenario_family.empty());
  PushChangedField(changed_fields, "metric_summary_by_stage", !task_review.metric_summary_by_stage.empty());
  PushChangedField(changed_fields, "visual_evidence_summary", !task_review.visual_evidence_summary.empty());
  PushChangedField(changed_fields, "device_summary", !task_review.device_summary.empty());
  PushChangedField(changed_fields, "business_eval_fields", !task_review.business_eval_fields.empty());
  PushChangedField(changed_fields, "pipeline_link_trace", !task_review.pipeline_link_trace.empty());
  PushChangedField(changed_fields, "sequence_family", !task_review.sequence_family.empty());
  PushChangedField(changed_fields, "stage_transition_summary", !task_review.stage_transition_summary.empty());
  PushChangedField(changed_fields, "stage_abnormal_summary", !task_review.stage_abnormal_summary.empty());
  PushChangedField(changed_fields, "test_image_ref", !task_review.test_image_ref.empty());
  PushChangedField(changed_fields, "visual_evidence_ref_set", !task_review.visual_evidence_ref_set.empty());
  PushChangedField(changed_fields, "task_conclusion_ref", !task_review.task_conclusion_ref.empty());
  PushChangedField(changed_fields, "anomaly_conclusion_ref", !task_review.anomaly_conclusion_ref.empty());
  PushChangedField(changed_fields, "next_action_ref", !task_review.next_action_ref.empty());
  PushChangedField(changed_fields, "element_ref_set", !task_review.element_ref_set.empty());
  PushChangedField(changed_fields, "chain_ref_set", !task_review.chain_ref_set.empty());
  PushChangedField(changed_fields, "stage_ref_set", !task_review.stage_ref_set.empty());
  PushChangedField(changed_fields, "issue_entry_ref_set", !task_review.issue_entry_ref_set.empty());
  PushChangedField(changed_fields, "recommended_image_ref", !task_review.recommended_image_ref.empty());
  PushChangedField(changed_fields, "recommended_element_ref", !task_review.recommended_element_ref.empty());
  PushChangedField(changed_fields, "recommended_chain_ref", !task_review.recommended_chain_ref.empty());
  PushChangedField(changed_fields, "recommended_stage_ref", !task_review.recommended_stage_ref.empty());
  PushChangedField(changed_fields, "issue_kind_hint", !task_review.issue_kind_hint.empty());
  PushChangedField(changed_fields, "raw_image_ref", !task_review.raw_image_ref.empty());
  PushChangedField(changed_fields, "edge_image_ref", !task_review.edge_image_ref.empty());
  PushChangedField(changed_fields, "element_relation_image_ref", !task_review.element_relation_image_ref.empty());
  PushChangedField(changed_fields, "candidate_image_ref", !task_review.candidate_image_ref.empty());
  PushChangedField(changed_fields, "match_image_ref", !task_review.match_image_ref.empty());
  PushChangedField(changed_fields, "geometry_stage", !task_review.geometry_stage.empty());
  PushChangedField(changed_fields, "candidate_stage", !task_review.candidate_stage.empty());
  PushChangedField(changed_fields, "match_stage", !task_review.match_stage.empty());
  PushChangedField(changed_fields, "problem_focus_image_ref", !task_review.problem_focus_image_ref.empty());
  PushChangedField(changed_fields, "problem_focus_element_id", !task_review.problem_focus_element_id.empty());
  PushChangedField(changed_fields, "problem_focus_chain_id", !task_review.problem_focus_chain_id.empty());
  PushChangedField(changed_fields, "problem_issue_type", !task_review.problem_issue_type.empty());
  PushChangedField(changed_fields, "single_image_geometry_conclusion", !task_review.single_image_geometry_conclusion.empty());
  PushChangedField(changed_fields, "element_conclusion", !task_review.element_conclusion.empty());
  PushChangedField(changed_fields, "match_conclusion", !task_review.match_conclusion.empty());
  PushChangedField(changed_fields, "task_conclusion", !task_review.task_conclusion.empty());
  PushChangedField(changed_fields, "next_step_suggestion", !task_review.next_step_suggestion.empty());
  PushChangedField(changed_fields, "gui_chain_summary", !task_review.gui_chain_summary.empty());
  PushChangedField(changed_fields, "review_mode", !task_review.review_mode.empty());
  PushChangedField(changed_fields, "default_decision_axis",
                   !task_review.default_decision_axis.empty());
  PushChangedField(changed_fields, "tolerance_summary", !task_review.tolerance_summary.empty());
  PushChangedField(changed_fields, "stability_summary", !task_review.stability_summary.empty());
  PushChangedField(changed_fields, "element_type_summary", !task_review.element_type_summary.empty());
  PushChangedField(changed_fields, "element_summary", !task_review.element_summary.empty());
  PushChangedField(changed_fields, "element_chain_summary",
                   !task_review.element_chain_summary.empty());
  PushChangedField(changed_fields, "element_status_summary",
                   !task_review.element_status_summary.empty());
  PushChangedField(changed_fields, "candidate_status_summary",
                   !task_review.candidate_status_summary.empty());
  PushChangedField(changed_fields, "match_status_summary",
                   !task_review.match_status_summary.empty());
  PushChangedField(changed_fields, "manual_review_signal_summary",
                   !task_review.manual_review_signal_summary.empty());
  PushChangedField(changed_fields, "element_group_summary",
                   !task_review.element_group_summary.empty());
  PushChangedField(changed_fields, "element_findings", !task_review.element_findings.empty());
  PushChangedField(changed_fields, "element_level_focus",
                   !task_review.element_level_focus.empty());
  PushChangedField(changed_fields, "focus_refresh_targets",
                   !task_review.focus_refresh_targets.empty());
  PushChangedField(changed_fields, "local_delta_targets",
                   !task_review.local_delta_targets.empty());
  PushChangedField(changed_fields, "grouped_element_preview",
                   !task_review.grouped_element_preview.empty());
  PushChangedField(changed_fields, "focus_element_preview",
                   !task_review.focus_element_preview.empty());
  PushChangedField(changed_fields, "delta_element_preview",
                   !task_review.delta_element_preview.empty());
  PushChangedField(changed_fields, "missing_element_summary",
                   !task_review.missing_element_summary.empty());
  PushChangedField(changed_fields, "drifted_element_summary",
                   !task_review.drifted_element_summary.empty());
  PushChangedField(changed_fields, "abnormal_element_summary",
                   !task_review.abnormal_element_summary.empty());
  return changed_fields;
}

std::vector<std::string> BuildUnifiedCompareChangedFields(const UnifiedCompareSlice &compare_slice)
{
  std::vector<std::string> changed_fields;
  PushChangedField(changed_fields, "compare_id");
  PushChangedField(changed_fields, "compare_type");
  PushChangedField(changed_fields, "left_ref", !compare_slice.left_ref.empty());
  PushChangedField(changed_fields, "right_ref", !compare_slice.right_ref.empty());
  PushChangedField(changed_fields, "compare_dimensions", !compare_slice.compare_dimensions.empty());
  PushChangedField(changed_fields, "delta_summary", !compare_slice.delta_summary.empty());
  PushChangedField(changed_fields, "risk_level", !compare_slice.risk_level.empty());
  PushChangedField(changed_fields, "focus_recommendation",
                   !compare_slice.focus_recommendation.empty());
  PushChangedField(changed_fields, "supporting_refs", !compare_slice.supporting_refs.empty());
  PushChangedField(changed_fields, "observation_personality",
                   !compare_slice.observation_personality.empty());
  PushChangedField(changed_fields, "default_open_chain", !compare_slice.default_open_chain.empty());
  PushChangedField(changed_fields, "evidence_focus_summary",
                   !compare_slice.evidence_focus_summary.empty());
  PushChangedField(changed_fields, "thread_handoff", !compare_slice.thread_handoff.empty());
  PushChangedField(changed_fields, "compare_view_mode", !compare_slice.compare_view_mode.empty());
  PushChangedField(changed_fields, "threshold_summary", !compare_slice.threshold_summary.empty());
  PushChangedField(changed_fields, "risk_note", !compare_slice.risk_note.empty());
  PushChangedField(changed_fields, "element_summary", !compare_slice.element_summary.empty());
  PushChangedField(changed_fields, "element_chain_summary",
                   !compare_slice.element_chain_summary.empty());
  PushChangedField(changed_fields, "element_status_summary",
                   !compare_slice.element_status_summary.empty());
  PushChangedField(changed_fields, "candidate_status_summary",
                   !compare_slice.candidate_status_summary.empty());
  PushChangedField(changed_fields, "match_status_summary",
                   !compare_slice.match_status_summary.empty());
  PushChangedField(changed_fields, "manual_review_signal_summary",
                   !compare_slice.manual_review_signal_summary.empty());
  PushChangedField(changed_fields, "element_group_summary",
                   !compare_slice.element_group_summary.empty());
  PushChangedField(changed_fields, "element_findings", !compare_slice.element_findings.empty());
  PushChangedField(changed_fields, "element_level_focus",
                   !compare_slice.element_level_focus.empty());
  PushChangedField(changed_fields, "focus_refresh_targets",
                   !compare_slice.focus_refresh_targets.empty());
  PushChangedField(changed_fields, "local_delta_targets",
                   !compare_slice.local_delta_targets.empty());
  PushChangedField(changed_fields, "grouped_element_preview",
                   !compare_slice.grouped_element_preview.empty());
  PushChangedField(changed_fields, "focus_element_preview",
                   !compare_slice.focus_element_preview.empty());
  PushChangedField(changed_fields, "delta_element_preview",
                   !compare_slice.delta_element_preview.empty());
  PushChangedField(changed_fields, "element_level_diff",
                   !compare_slice.element_level_diff.empty());
  PushChangedField(changed_fields, "semantic_diff", !compare_slice.semantic_diff.empty());
  PushChangedField(changed_fields, "structure_diff", !compare_slice.structure_diff.empty());
  return changed_fields;
}

std::vector<std::string> BuildUnifiedAnomalyChangedFields(
  const UnifiedAnomalyFocusBundle &anomaly_bundle)
{
  std::vector<std::string> changed_fields;
  PushChangedField(changed_fields, "source_thread");
  PushChangedField(changed_fields, "task_id");
  PushChangedField(changed_fields, "batch_id", !anomaly_bundle.batch_id.empty());
  PushChangedField(changed_fields, "abnormal_image_ids", !anomaly_bundle.abnormal_image_ids.empty());
  PushChangedField(changed_fields, "anomaly_type_summary",
                   !anomaly_bundle.anomaly_type_summary.empty());
  PushChangedField(changed_fields, "top_focus_objects", !anomaly_bundle.top_focus_objects.empty());
  PushChangedField(changed_fields, "analysis_suggestions",
                   !anomaly_bundle.analysis_suggestions.empty());
  PushChangedField(changed_fields, "risk_level", !anomaly_bundle.risk_level.empty());
  PushChangedField(changed_fields, "supporting_refs", !anomaly_bundle.supporting_refs.empty());
  PushChangedField(changed_fields, "anomaly_element_ids",
                   !anomaly_bundle.anomaly_element_ids.empty());
  PushChangedField(changed_fields, "anomaly_element_types",
                   !anomaly_bundle.anomaly_element_types.empty());
  PushChangedField(changed_fields, "observation_personality",
                   !anomaly_bundle.observation_personality.empty());
  PushChangedField(changed_fields, "default_open_chain",
                   !anomaly_bundle.default_open_chain.empty());
  PushChangedField(changed_fields, "evidence_focus_summary",
                   !anomaly_bundle.evidence_focus_summary.empty());
  PushChangedField(changed_fields, "thread_handoff", !anomaly_bundle.thread_handoff.empty());
  PushChangedField(changed_fields, "anomaly_axis", !anomaly_bundle.anomaly_axis.empty());
  PushChangedField(changed_fields, "stability_summary",
                   !anomaly_bundle.stability_summary.empty());
  PushChangedField(changed_fields, "risk_note", !anomaly_bundle.risk_note.empty());
  PushChangedField(changed_fields, "element_summary", !anomaly_bundle.element_summary.empty());
  PushChangedField(changed_fields, "element_chain_summary",
                   !anomaly_bundle.element_chain_summary.empty());
  PushChangedField(changed_fields, "element_status_summary",
                   !anomaly_bundle.element_status_summary.empty());
  PushChangedField(changed_fields, "candidate_status_summary",
                   !anomaly_bundle.candidate_status_summary.empty());
  PushChangedField(changed_fields, "match_status_summary",
                   !anomaly_bundle.match_status_summary.empty());
  PushChangedField(changed_fields, "manual_review_signal_summary",
                   !anomaly_bundle.manual_review_signal_summary.empty());
  PushChangedField(changed_fields, "element_group_summary",
                   !anomaly_bundle.element_group_summary.empty());
  PushChangedField(changed_fields, "element_findings", !anomaly_bundle.element_findings.empty());
  PushChangedField(changed_fields, "element_level_focus",
                   !anomaly_bundle.element_level_focus.empty());
  PushChangedField(changed_fields, "focus_refresh_targets",
                   !anomaly_bundle.focus_refresh_targets.empty());
  PushChangedField(changed_fields, "local_delta_targets",
                   !anomaly_bundle.local_delta_targets.empty());
  PushChangedField(changed_fields, "grouped_element_preview",
                   !anomaly_bundle.grouped_element_preview.empty());
  PushChangedField(changed_fields, "focus_element_preview",
                   !anomaly_bundle.focus_element_preview.empty());
  PushChangedField(changed_fields, "delta_element_preview",
                   !anomaly_bundle.delta_element_preview.empty());
  PushChangedField(changed_fields, "anomaly_focus_reason",
                   !anomaly_bundle.anomaly_focus_reason.empty());
  PushChangedField(changed_fields, "raw_image_ref", !anomaly_bundle.raw_image_ref.empty());
  PushChangedField(changed_fields, "edge_image_ref", !anomaly_bundle.edge_image_ref.empty());
  PushChangedField(changed_fields, "element_relation_image_ref", !anomaly_bundle.element_relation_image_ref.empty());
  PushChangedField(changed_fields, "candidate_image_ref", !anomaly_bundle.candidate_image_ref.empty());
  PushChangedField(changed_fields, "match_image_ref", !anomaly_bundle.match_image_ref.empty());
  PushChangedField(changed_fields, "geometry_stage", !anomaly_bundle.geometry_stage.empty());
  PushChangedField(changed_fields, "candidate_stage", !anomaly_bundle.candidate_stage.empty());
  PushChangedField(changed_fields, "match_stage", !anomaly_bundle.match_stage.empty());
  PushChangedField(changed_fields, "problem_focus_image_ref", !anomaly_bundle.problem_focus_image_ref.empty());
  PushChangedField(changed_fields, "problem_focus_element_id", !anomaly_bundle.problem_focus_element_id.empty());
  PushChangedField(changed_fields, "problem_focus_chain_id", !anomaly_bundle.problem_focus_chain_id.empty());
  PushChangedField(changed_fields, "problem_issue_type", !anomaly_bundle.problem_issue_type.empty());
  PushChangedField(changed_fields, "single_image_geometry_conclusion", !anomaly_bundle.single_image_geometry_conclusion.empty());
  PushChangedField(changed_fields, "element_conclusion", !anomaly_bundle.element_conclusion.empty());
  PushChangedField(changed_fields, "match_conclusion", !anomaly_bundle.match_conclusion.empty());
  PushChangedField(changed_fields, "task_conclusion", !anomaly_bundle.task_conclusion.empty());
  PushChangedField(changed_fields, "next_step_suggestion", !anomaly_bundle.next_step_suggestion.empty());
  PushChangedField(changed_fields, "gui_chain_summary", !anomaly_bundle.gui_chain_summary.empty());
  return changed_fields;
}

std::string DetermineImageRefreshPriority(const CxScriptExecutionResult &result,
                                          const UnifiedImageReviewRecord &image_review)
{
  if (!result.success ||
      !result.error_message.empty() ||
      image_review.status == "abnormal")
    return "high";
  if (!image_review.anomaly_flags.empty() ||
      image_review.missing_element_count > 0 ||
      image_review.abnormal_element_count > 0 ||
      image_review.drifted_element_count > 0)
    return "elevated";
  if (!image_review.local_delta_targets.empty() ||
      !image_review.focus_refresh_targets.empty())
    return "elevated";
  return "normal";
}

std::string DetermineImageRefreshPriority(const UnifiedImageReviewRecord &image_review)
{
  if (image_review.status == "abnormal")
    return "high";
  if (!image_review.anomaly_flags.empty() ||
      image_review.missing_element_count > 0 ||
      image_review.abnormal_element_count > 0 ||
      image_review.drifted_element_count > 0)
    return "elevated";
  if (!image_review.local_delta_targets.empty() ||
      !image_review.focus_refresh_targets.empty())
    return "elevated";
  return "normal";
}

std::string DetermineTaskRefreshPriority(const UnifiedTaskReviewBundle &task_review)
{
  if (task_review.abnormal_images > 0 || task_review.review_required_count > 0)
    return "elevated";
  if (!task_review.local_delta_targets.empty() ||
      !task_review.focus_refresh_targets.empty())
    return "elevated";
  return "normal";
}

std::string DetermineCompareRefreshPriority(const UnifiedCompareSlice &compare_slice)
{
  const std::string risk_level = ToLowerText(compare_slice.risk_level);
  if (risk_level == "high" || risk_level == "abnormal")
    return "high";
  if (risk_level == "medium" || risk_level == "elevated" || risk_level == "review_only")
    return "elevated";
  if (!compare_slice.local_delta_targets.empty() ||
      !compare_slice.focus_refresh_targets.empty())
    return "elevated";
  return "normal";
}

std::string DetermineAnomalyRefreshPriority(const UnifiedAnomalyFocusBundle &anomaly_bundle)
{
  const std::string risk_level = ToLowerText(anomaly_bundle.risk_level);
  if (!anomaly_bundle.abnormal_image_ids.empty() ||
      !anomaly_bundle.anomaly_element_ids.empty() ||
      risk_level == "high" ||
      risk_level == "abnormal")
    return "high";
  if (!anomaly_bundle.analysis_suggestions.empty() ||
      risk_level == "medium" ||
      !anomaly_bundle.local_delta_targets.empty() ||
      !anomaly_bundle.focus_refresh_targets.empty())
    return "elevated";
  return "normal";
}

std::string DetermineBoardRefreshPriority(const UnifiedImageReviewRecord &image_review,
                                          const UnifiedTaskReviewBundle &task_review,
                                          const UnifiedCompareSlice &compare_slice,
                                          const UnifiedAnomalyFocusBundle &anomaly_bundle)
{
  const std::string image_priority = DetermineImageRefreshPriority(image_review);
  const std::string compare_priority = DetermineCompareRefreshPriority(compare_slice);
  const std::string anomaly_priority = DetermineAnomalyRefreshPriority(anomaly_bundle);
  if (image_priority == "high" || compare_priority == "high" || anomaly_priority == "high")
    return "high";
  if (image_priority == "elevated" || compare_priority == "elevated" ||
      anomaly_priority == "elevated" || DetermineTaskRefreshPriority(task_review) == "elevated")
    return "elevated";
  return "normal";
}

std::string BuildObservationPersonality(const CxScriptExecutionResult &result,
                                        const std::string &source_thread)
{
  if (source_thread == "torch")
    return "deep_model_result_evidence";
  if (source_thread == "mlpack")
    return "baseline_statistical_evidence";
  if (source_thread == "cximage")
  {
    if (result.case_name == "binary_region")
      return "traditional_visual_texture_evidence";
    if (result.case_name == "geometry_topology_pipeline")
      return "traditional_visual_topology_evidence";
    return "classical_geometry_evidence";
  }
  if (source_thread == "ensmallen")
    return "parameter_optimization_evidence";
  return "unified_review_evidence";
}

std::string BuildReviewMode(const CxScriptExecutionResult &result,
                            const std::string &source_thread)
{
  if (source_thread == "torch")
    return "image_to_roi_to_template_review";
  if (source_thread == "mlpack")
    return "baseline_score_and_distance_review";
  if (source_thread == "cximage")
  {
    if (result.case_name == "binary_region")
      return "texture_region_consistency_review";
    if (result.case_name == "geometry_topology_pipeline")
      return "topology_stage_consistency_review";
    return "classical_geometry_element_review";
  }
  if (source_thread == "ensmallen")
    return "optimize_compare_replay_review";
  return "unified_review_mode";
}

std::string BuildDefaultOpenChain(const CxScriptExecutionResult &result,
                                  const std::string &source_thread)
{
  if (source_thread == "torch")
    return "bbox -> roi_crop -> template_alignment -> roi_diff";
  if (source_thread == "mlpack")
    return "score -> distance -> cluster_or_anomaly -> baseline_compare";
  if (source_thread == "cximage")
  {
    if (result.case_name == "binary_region" ||
        result.case_name == "geometry_topology_pipeline")
      return "edge_or_point_or_line_or_region -> candidate -> match -> review_signal";
    return "edge_or_point_or_line_or_region -> candidate -> match -> review_signal";
  }
  if (source_thread == "ensmallen")
    return "params -> objective -> stability -> best_candidate";
  return "input -> evidence -> compare -> review";
}

std::string BuildDefaultDecisionAxis(const CxScriptExecutionResult &result,
                                     const std::string &source_thread)
{
  if (source_thread == "torch")
    return "what_the_model_saw / what_is_missing / where_drift_appears / whether_runtime_fields_are_verified";
  if (source_thread == "mlpack")
    return "score / distance / anomaly / baseline_compare / threshold_stability";
  if (source_thread == "cximage")
  {
    if (result.case_name == "binary_region")
      return "whether_texture_regions_are_stable / whether_descriptor_changes_follow_image_changes / whether_gray_color_fallback_is_explicit / whether_review_signals_are_explicit";
    if (result.case_name == "geometry_topology_pipeline")
      return "whether_topology_stages_are_complete / whether_structure_is_continuous / where_repair_paths_drift / whether_review_signals_are_explicit";
    return "whether_geometry_elements_are_complete / whether_candidates_are_clear / whether_template_matching_is_stable / whether_review_signals_are_explicit";
  }
  if (source_thread == "ensmallen")
    return "params / objective / stability / best_candidate / coverage_gap / risk_axis";
  return "evidence / compare / anomaly / next_action";
}

std::string BuildCompareViewMode(const CxScriptExecutionResult &result,
                                 const std::string &source_thread)
{
  if (source_thread == "torch")
    return "model_output_chain";
  if (source_thread == "mlpack")
    return "baseline_scoring_chain";
  if (source_thread == "cximage")
  {
    if (result.case_name == "binary_region")
      return "texture_descriptor_chain";
    if (result.case_name == "geometry_topology_pipeline")
      return "topology_stage_chain";
    return "geometry_element_chain";
  }
  if (source_thread == "ensmallen")
    return "optimization_evidence_chain";
  return "unified_compare_chain";
}

std::string BuildAnomalyAxis(const CxScriptExecutionResult &result,
                             const std::string &source_thread)
{
  if (source_thread == "torch")
    return "roi_diff / template_alignment / missing_detection_evidence";
  if (source_thread == "mlpack")
    return "artifact_gap / score_support_gap / baseline_closure_gap";
  if (source_thread == "cximage")
  {
    if (result.case_name == "binary_region")
      return "roi_diff / descriptor_plateau / brightness_or_texture_separability";
    if (result.case_name == "geometry_topology_pipeline")
      return "distance_field / centerline_break / topology_repair_drift";
    return "roi_diff / arc_drift / candidate_semantics";
  }
  if (source_thread == "ensmallen")
    return "stability / best_candidate / coverage_gap";
  return "evidence_gap / compare_gap / anomaly_gap";
}

void PushIfPresent(std::vector<std::string> &items,
                   const std::string &candidate,
                   const std::string &label)
{
  if (!candidate.empty())
    PushUniqueText(items, label);
}

std::string BuildObservedEvidenceSurface(const CxScriptExecutionResult &result,
                                         const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> observed;
  const std::string &source_thread = image_review.source_thread;

  if (source_thread == "torch")
  {
    PushIfPresent(observed, result.bbox_candidate_list_ref, "bbox_candidate_list_ref");
    PushIfPresent(observed, result.published_bbox_candidate_list_ref, "published_bbox_candidate_list_ref");
    PushIfPresent(observed, result.roi_crop_packet_ref, "roi_crop_packet_ref");
    PushIfPresent(observed, result.published_roi_crop_packet_ref, "published_roi_crop_packet_ref");
    PushIfPresent(observed, result.template_alignment_ref, "template_alignment_ref");
    PushIfPresent(observed, result.published_template_alignment_ref, "published_template_alignment_ref");
    PushIfPresent(observed, result.roi_diff_candidate_ref, "roi_diff_candidate_ref");
    PushIfPresent(observed, result.published_roi_diff_candidate_ref, "published_roi_diff_candidate_ref");
    if (!result.requested_device.empty())
      PushUniqueText(observed, "requested_device");
    if (result.runtime_ms > 0.0)
      PushUniqueText(observed, "runtime_ms");
    PushIfPresent(observed, result.train_param_summary, "train_param_summary");
    PushIfPresent(observed, result.infer_param_summary, "infer_param_summary");
  }
  else if (source_thread == "mlpack")
  {
    PushIfPresent(observed, result.distance_ref, "distance_ref");
    PushIfPresent(observed, result.cluster_ref, "cluster_ref");
    PushIfPresent(observed, result.anomaly_ref, "anomaly_ref");
    PushIfPresent(observed, result.summary_ref, "summary_ref");
    PushIfPresent(observed, result.compare_ref, "compare_ref");
    PushIfPresent(observed, result.roi_crop_packet_ref, "roi_crop_packet_ref");
    PushIfPresent(observed, result.published_roi_crop_packet_ref, "published_roi_crop_packet_ref");
    PushIfPresent(observed, result.roi_diff_candidate_ref, "roi_diff_candidate_ref");
    PushIfPresent(observed, result.predictions_csv, "predictions_csv");
    PushIfPresent(observed, result.output_summary_csv, "output_summary_csv");
  }
  else if (source_thread == "cximage")
  {
    PushIfPresent(observed, result.circle_overlay_ref, "circle_overlay_ref");
    PushIfPresent(observed, result.circle_edge_overlay_ref, "circle_edge_overlay_ref");
    PushIfPresent(observed, result.formfit_candidate_overlay_ref, "formfit_candidate_overlay_ref");
    PushIfPresent(observed, result.formfit_selection_overlay_ref, "formfit_selection_overlay_ref");
    PushIfPresent(observed, result.region_pattern_overlay_ref, "region_pattern_overlay_ref");
    PushIfPresent(observed, result.template_alignment_ref, "template_alignment_ref");
    PushIfPresent(observed, result.roi_diff_candidate_ref, "roi_diff_candidate_ref");
    if (!image_review.detection_elements.empty())
      PushUniqueText(observed, "detection_elements");
    if (!image_review.focus_refresh_targets.empty())
      PushUniqueText(observed, "focus_refresh_targets");
    if (!image_review.local_delta_targets.empty())
      PushUniqueText(observed, "local_delta_targets");
    if (!image_review.grouped_element_preview.empty())
      PushUniqueText(observed, "grouped_element_preview");
    if (!image_review.focus_element_preview.empty())
      PushUniqueText(observed, "focus_element_preview");
    if (!image_review.delta_element_preview.empty())
      PushUniqueText(observed, "delta_element_preview");
  }
  else if (source_thread == "ensmallen")
  {
    PushIfPresent(observed, result.optimization_result_ref, "optimization_result_ref");
    PushIfPresent(observed, result.summary_ref, "summary_ref");
    PushIfPresent(observed, result.compare_ref, "compare_ref");
    PushIfPresent(observed, result.replay_ref, "replay_ref");
    PushIfPresent(observed, result.cluster_ref, "cluster_ref");
    PushIfPresent(observed, result.distance_ref, "distance_ref");
    PushIfPresent(observed, result.anomaly_ref, "anomaly_ref");
    if (result.best_objective != 0.0 || result.baseline_objective != 0.0 || result.objective_delta != 0.0)
      PushUniqueText(observed, "objective_triplet");
    PushIfPresent(observed, BuildEnsmallenBestParamSetsText(result), "best_param_sets");
  }

  return JoinTextItems(observed, ",");
}

std::string BuildEvidenceFocusSummary(const CxScriptExecutionResult &result,
                                      const UnifiedImageReviewRecord &image_review)
{
  std::string summary;
  if (image_review.source_thread == "torch")
  {
    summary =
      "Start from model outputs, verify ROI closure, then inspect template alignment drift and roi_diff evidence.";
  }
  else if (image_review.source_thread == "mlpack")
  {
    summary =
      "Start from score and distance, then inspect cluster or anomaly outputs, and finally verify baseline compare and threshold stability.";
  }
  else if (image_review.source_thread == "cximage")
  {
    if (result.case_name == "binary_region")
    {
      summary =
        "Start from region overlays and descriptor-support elements, then verify whether descriptor outputs actually change with texture, color, brightness, and focus variation.";
    }
    else if (result.case_name == "geometry_topology_pipeline")
    {
      summary =
        "Start from distance-field and centerline elements, then verify whether the stage chain preserves radial or weak-geometry topology before trusting repair outputs.";
    }
    else
    {
      summary =
        "Start from geometry elements and candidate regions, then verify template matching stability and manual review signals.";
    }
  }
  else if (image_review.source_thread == "ensmallen")
  {
    summary =
      "Start from parameter sets and objective movement, then inspect stability, candidate ranking, coverage gaps, and risk axes.";
    std::vector<std::string> evidence_items;
    const std::string best_params_ref =
      ResolveNamedOrDirectRef(result, "refs", "best_params_ref", result.best_params_ref);
    const std::string best_param_sets =
      BuildEnsmallenBestParamSetsText(result);
    const std::string coverage_gap =
      FindNamedResultFieldValue(result, "analysis", "coverage_gap");
    const std::string risk_axis =
      FindNamedResultFieldValue(result, "analysis", "risk_axis");
    const std::string optimization_signal =
      FindNamedResultFieldValue(result, "analysis", "optimization_signal");
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
    if (!best_params_ref.empty())
      evidence_items.push_back("best_params_ref=" + best_params_ref);
    if (!best_param_sets.empty())
      evidence_items.push_back("best_param_sets=" + best_param_sets);
    if (!result.selected_method.empty())
      evidence_items.push_back("selected_method=" + result.selected_method);
    if (!result.ordered_candidates.empty())
      evidence_items.push_back("candidate_ordering=" + result.ordered_candidates);
    if (!objective_curve.empty())
      evidence_items.push_back("objective_curve=" + objective_curve);
    if (result.baseline_objective != 0.0 || result.best_objective != 0.0 ||
        result.objective_delta != 0.0)
    {
      evidence_items.push_back("baseline_objective=" +
                               FormatElementNumber(result.baseline_objective));
      evidence_items.push_back("best_objective=" +
                               FormatElementNumber(result.best_objective));
      evidence_items.push_back("objective_delta=" +
                               FormatElementNumber(result.objective_delta));
    }
    if (result.stability_delta != 0.0)
      evidence_items.push_back("stability_delta=" +
                               FormatElementNumber(result.stability_delta));
    if (!stability_score.empty())
      evidence_items.push_back("stability_score=" + stability_score);
    if (!feature_distance_delta.empty())
      evidence_items.push_back("feature_distance_delta=" + feature_distance_delta);
    if (result.match_candidate_count_value > 0.0)
      evidence_items.push_back("candidate_count=" +
                               FormatElementNumber(result.match_candidate_count_value));
    else if (result.candidate_count_value > 0.0)
      evidence_items.push_back("candidate_count=" +
                               FormatElementNumber(result.candidate_count_value));
    if (result.selected_candidate_index_value >= 0.0)
      evidence_items.push_back("selected_candidate_index=" +
                               FormatElementNumber(result.selected_candidate_index_value));
    if (result.selected_candidate_score_value > 0.0)
      evidence_items.push_back("selected_candidate_score=" +
                               FormatElementNumber(result.selected_candidate_score_value));
    if (!candidate_rank.empty())
      evidence_items.push_back("candidate_rank=" + candidate_rank);
    if (!convergence_status.empty())
      evidence_items.push_back("convergence_status=" + convergence_status);
    if (!best_candidate_confidence.empty())
      evidence_items.push_back("best_candidate_confidence=" + best_candidate_confidence);
    if (!coverage_gap.empty())
      evidence_items.push_back("coverage_gap=" + coverage_gap);
    if (!risk_axis.empty())
      evidence_items.push_back("risk_axis=" + risk_axis);
    if (!optimization_signal.empty())
      evidence_items.push_back("optimization_signal=" + optimization_signal);
    if (!evidence_items.empty())
      summary += " evidence=" + JoinTextItems(evidence_items, ";");
  }
  else
  {
    summary = "Inspect the live evidence chain before relying on template-only summaries.";
  }

  const std::string observed_surface = BuildObservedEvidenceSurface(result, image_review);
  if (!observed_surface.empty())
    summary += " observed_surface=" + observed_surface;
  return summary;
}

std::string BuildReviewGapFocus(const CxScriptExecutionResult &result,
                                const UnifiedImageReviewRecord &image_review)
{
  if (!result.error_message.empty())
    return result.error_message;
  if (image_review.missing_element_count > 0)
    return "missing_element_count=" + std::to_string(image_review.missing_element_count);
  if (image_review.abnormal_element_count > 0)
    return "abnormal_element_count=" + std::to_string(image_review.abnormal_element_count);
  if (image_review.drifted_element_count > 0)
    return "drifted_element_count=" + std::to_string(image_review.drifted_element_count);
  if (image_review.source_thread == "torch" &&
      result.template_alignment_ref.empty() &&
      result.published_template_alignment_ref.empty())
  {
    return "template_alignment_ref_gap";
  }
  if (image_review.source_thread == "mlpack" &&
      result.roi_crop_packet_ref.empty() &&
      result.published_roi_crop_packet_ref.empty())
  {
    return "roi_crop_packet_ref_gap";
  }
  if (image_review.source_thread == "ensmallen" &&
      (result.summary_ref.empty() || result.compare_ref.empty() || result.replay_ref.empty()))
  {
    return "optimization_result_chain_gap";
  }
  if (result.runtime_ms <= 0.0)
    return "runtime_ms_not_reported";
  return "replace_template_data_with_live_objects";
}

std::string BuildThreadHandoff(const CxScriptExecutionResult &result,
                               const UnifiedImageReviewRecord &image_review)
{
  std::string handoff;
  if (image_review.source_thread == "torch")
  {
    handoff =
      "Keep stabilizing bbox, ROI, template alignment, roi_diff, weight provenance, and runtime fields inside unified review objects.";
  }
  else if (image_review.source_thread == "mlpack")
  {
    handoff =
      "Keep objectizing score, distance, cluster_or_anomaly, baseline compare, and ROI evidence instead of narrative-only output.";
  }
  else if (image_review.source_thread == "cximage")
  {
    handoff =
      "Keep promoting points, lines, regions, circles, arcs, edges, and candidate regions into elements and anomaly bundles.";
  }
  else if (image_review.source_thread == "ensmallen")
  {
    handoff =
      "Keep promoting params, objective traces, stability guards, best candidate evidence, coverage gaps, and risk axes into unified objects.";
  }
  else
  {
    handoff = "Keep replacing placeholder review data with live runtime evidence objects.";
  }

  const std::string gap_focus = BuildReviewGapFocus(result, image_review);
  if (!gap_focus.empty())
    handoff += " current_focus=" + gap_focus;
  return handoff;
}

std::string BuildElementLevelDiff(const UnifiedImageReviewRecord &image_review)
{
  return "total=" + std::to_string(static_cast<int>(image_review.detection_elements.size())) +
         ";matched=" +
         std::to_string(CountDetectionElementsByStatus(image_review.detection_elements, "matched")) +
         ";missing=" + std::to_string(image_review.missing_element_count) +
         ";drifted=" + std::to_string(image_review.drifted_element_count) +
         ";abnormal=" + std::to_string(image_review.abnormal_element_count);
}

std::string BuildSemanticDiff(const UnifiedImageReviewRecord &image_review)
{
  return "primary_detection_semantic=" + image_review.primary_detection_semantic +
         ";template_alignment_status=" + image_review.template_alignment_status +
         ";candidate_element_count=" + std::to_string(image_review.candidate_element_count) +
         ";manual_review_signal_summary=" + image_review.manual_review_signal_summary;
}

std::string BuildStructureDiff(const UnifiedImageReviewRecord &image_review)
{
  return "element_type_summary=" + BuildElementTypeSummary(image_review.detection_elements) +
         ";element_group_summary=" + image_review.element_group_summary +
         ";grouped_element_preview=" + image_review.grouped_element_preview +
         ";focus_element_preview=" + image_review.focus_element_preview +
         ";delta_element_preview=" + image_review.delta_element_preview +
         ";element_count=" +
         std::to_string(static_cast<int>(image_review.detection_elements.size()));
}

std::string BuildAnomalyFocusReason(const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> reasons;
  if (image_review.missing_element_count > 0)
    reasons.push_back("missing=" + std::to_string(image_review.missing_element_count));
  if (image_review.drifted_element_count > 0)
    reasons.push_back("drifted=" + std::to_string(image_review.drifted_element_count));
  if (image_review.abnormal_element_count > 0)
    reasons.push_back("abnormal=" + std::to_string(image_review.abnormal_element_count));
  if (reasons.empty())
  {
    std::vector<std::string> watch_chains;
    for (size_t i = 0; i < image_review.element_chains.size(); ++i)
    {
      const UnifiedElementChain &chain = image_review.element_chains[i];
      if (chain.chain_status == "watch" && !chain.chain_type.empty())
        PushUniqueText(watch_chains, chain.chain_type);
    }
    if (!watch_chains.empty())
      reasons.push_back("watch_chain=" + JoinTextItems(watch_chains, ","));
  }
  if (reasons.empty() && !image_review.local_delta_targets.empty())
    reasons.push_back("local_delta_targets");
  if (reasons.empty() && !image_review.focus_refresh_targets.empty())
    reasons.push_back("focus_refresh_targets");
  if (reasons.empty())
    reasons.push_back("matched_only");
  return JoinTextItems(reasons, ";");
}

std::string BuildManualReviewTargets(const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> targets;
  if (!image_review.primary_visual_ref.empty())
    targets.push_back("primary_visual_ref=" + image_review.primary_visual_ref);

  for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
  {
    const UnifiedDetectionElement &element = image_review.detection_elements[i];
    const bool roi_related =
      element.element_id.find("roi") != std::string::npos ||
      element.source_ref.find("roi") != std::string::npos ||
      element.element_type == "closed_region" ||
      element.semantic_role == "candidate" ||
      element.semantic_role == "review_target";
    const bool review_required =
      roi_related ||
      element.consistency_status != "matched" ||
      !element.element_findings.empty() ||
      !element.element_level_focus.empty();
    if (!review_required)
      continue;

    std::vector<std::string> parts;
    parts.push_back("element_id=" + element.element_id);
    parts.push_back("element_type=" + element.element_type);
    parts.push_back("semantic_role=" + element.semantic_role);
    parts.push_back("status=" + element.consistency_status);
    if (!element.candidate_status.empty())
      parts.push_back("candidate_status=" + element.candidate_status);
    if (!element.match_status.empty())
      parts.push_back("match_status=" + element.match_status);
    if (!element.manual_review_signal.empty())
      parts.push_back("manual_review_signal=" + element.manual_review_signal);
    if (!element.element_group_label.empty())
      parts.push_back("group=" + element.element_group_label);
    if (!element.source_ref.empty())
      parts.push_back("source_ref=" + element.source_ref);
    if (!element.primary_overlay_ref.empty())
      parts.push_back("primary_overlay_ref=" + element.primary_overlay_ref);
    if (!element.focus_region_ref.empty())
      parts.push_back("focus_region_ref=" + element.focus_region_ref);
    if (!element.local_delta_ref.empty())
      parts.push_back("local_delta_ref=" + element.local_delta_ref);
    if (!element.element_findings.empty())
      parts.push_back("findings=" + element.element_findings);
    if (!element.element_level_focus.empty())
      parts.push_back("focus=" + element.element_level_focus);
    targets.push_back(JoinTextItems(parts, "|"));
  }

  return JoinTextItems(targets, ";;");
}

std::string BuildRoiVisualEvidence(const UnifiedImageReviewRecord &image_review)
{
  std::vector<std::string> refs;
  if (!image_review.primary_visual_ref.empty())
    PushUniqueText(refs, image_review.primary_visual_ref);

  for (size_t i = 0; i < image_review.visualization_refs.size(); ++i)
  {
    const std::string &ref = image_review.visualization_refs[i];
    if (ref.find("roi") != std::string::npos ||
        ref.find("manual_review") != std::string::npos ||
        ref.find(".png") != std::string::npos)
    {
      PushUniqueText(refs, ref);
    }
  }

  for (size_t i = 0; i < image_review.detection_elements.size(); ++i)
  {
    if (!image_review.detection_elements[i].focus_region_ref.empty())
      PushUniqueText(refs, image_review.detection_elements[i].focus_region_ref);
    if (!image_review.detection_elements[i].local_delta_ref.empty())
      PushUniqueText(refs, image_review.detection_elements[i].local_delta_ref);
  }

  return JoinTextItems(refs, ";");
}

std::string BuildReviewBatchId(const CxScriptExecutionResult &result)
{
  if (!result.task_id.empty())
    return result.task_id + ".batch";
  if (!result.module.empty() && !result.layer.empty() && !result.case_name.empty())
    return result.module + "." + result.layer + "." + result.case_name + ".batch";
  return "review.batch";
}

std::string SelectReviewImageId(const CxScriptExecutionResult &result)
{
  const std::string sample_id = FindNamedResultFieldValue(result, "bridge", "sample_id");
  if (!sample_id.empty())
    return sample_id;
  if (!result.input_sample.empty())
    return result.input_sample;
  if (!result.case_name.empty())
    return result.case_name;
  return result.task_id.empty() ? "image.unknown" : result.task_id;
}

std::string SelectReviewInputImageRef(const CxScriptExecutionResult &result)
{
  const std::string bridge_image = FindNamedResultFieldValue(result, "bridge", "input_image");
  if (!bridge_image.empty())
    return bridge_image;
  if (!result.dataset_ref.empty())
    return result.dataset_ref;
  if (!result.input_sample.empty())
    return result.input_sample;
  const std::string source_ref = FindNamedResultFieldValue(result, "refs", "source_ref");
  if (!source_ref.empty())
    return source_ref;
  const std::string evidence_ref = FindNamedResultFieldValue(result, "refs", "evidence_ref");
  if (!evidence_ref.empty())
    return evidence_ref;
  if (!result.published_evidence_ref.empty())
    return result.published_evidence_ref;
  if (!result.script_path.empty())
    return result.script_path;
  if (!result.script_name.empty())
    return result.script_name;
  if (!result.case_name.empty())
    return result.case_name;
  return result.task_id;
}

std::string SelectReviewPrimaryVisualRef(const CxScriptExecutionResult &result)
{
  const std::string candidates[] = {
    result.published_primary_ref,
    result.published_result_ref,
    result.published_template_alignment_ref,
    result.published_roi_diff_candidate_ref,
    result.published_roi_crop_packet_ref,
    result.published_bbox_candidate_list_ref,
    result.template_alignment_ref,
    result.roi_diff_candidate_ref,
    result.roi_crop_packet_ref,
    result.bbox_candidate_list_ref,
    result.attach_back_ref,
    result.candidate_overlay_ref,
    result.template_rect_overlay_ref,
    result.test_rect_overlay_ref,
    result.circle_overlay_ref,
    result.circle_edge_overlay_ref,
    result.formfit_candidate_overlay_ref,
    result.formfit_selection_overlay_ref,
    result.region_pattern_overlay_ref,
    result.region_pattern_descriptor_ref,
    result.fractal_partition_overlay_ref,
    result.distance_field_overlay_ref,
    result.skeleton_overlay_ref,
    result.centerline_overlay_ref,
    result.topology_repair_overlay_ref,
    result.cluster_ref,
    result.distance_ref,
    result.anomaly_ref,
    result.baseline_feature_ref,
    result.baseline_class_ref,
    result.predictions_csv,
    result.output_summary_csv,
    result.model_path,
    result.compare_ref,
    result.replay_ref,
    result.summary_ref,
    result.result_object,
    result.task_id
  };

  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
  {
    if (!candidates[i].empty())
      return candidates[i];
  }

  return SelectReviewInputImageRef(result);
}

void PushReviewMetric(std::vector<UnifiedReviewMetric> &metrics,
                      const std::string &metric_name,
                      const std::string &metric_value,
                      const std::string &metric_unit,
                      const std::string &expected_range,
                      const std::string &baseline_value,
                      const std::string &deviation_level,
                      const std::string &metric_status)
{
  if (metric_name.empty() || metric_value.empty())
    return;

  UnifiedReviewMetric metric;
  metric.metric_name = metric_name;
  metric.metric_value = metric_value;
  metric.metric_unit = metric_unit;
  metric.expected_range = expected_range;
  metric.baseline_value = baseline_value;
  metric.deviation_level = deviation_level;
  metric.metric_status = metric_status;
  metrics.push_back(metric);
}

std::string SerializeReviewMetrics(const std::vector<UnifiedReviewMetric> &metrics)
{
  std::vector<std::string> items;
  for (size_t i = 0; i < metrics.size(); ++i)
  {
    std::string item = metrics[i].metric_name + "=" + metrics[i].metric_value;
    if (!metrics[i].metric_unit.empty())
      item += metrics[i].metric_unit;
    if (!metrics[i].metric_status.empty())
      item += "(" + metrics[i].metric_status + ")";
    items.push_back(item);
  }
  return JoinTextItems(items, ";");
}

std::string DetermineReviewStatus(const CxScriptExecutionResult &result,
                                  const std::vector<std::string> &anomaly_flags)
{
  if (!result.success)
    return "abnormal";
  if (IsCximageClassicalReviewCase(result) && anomaly_flags.empty() &&
      result.error_message.empty())
    return "normal";
  if (!anomaly_flags.empty() || result.degraded)
    return "review_only";
  return "normal";
}

std::string DetermineReviewRiskLevel(const CxScriptExecutionResult &result,
                                     const std::vector<std::string> &anomaly_flags)
{
  if (!result.success)
    return "high";
  if (IsCximageClassicalReviewCase(result) && anomaly_flags.empty() &&
      result.error_message.empty())
    return "low";
  if (!anomaly_flags.empty() || result.degraded)
    return "medium";
  return "low";
}

void CollectReviewArtifactRefs(const CxScriptExecutionResult &result,
                               std::vector<std::string> &artifact_refs)
{
  const std::string refs[] = {
    result.summary_ref,
    result.compare_ref,
    result.replay_ref,
    result.cluster_ref,
    result.distance_ref,
    result.anomaly_ref,
    result.baseline_class_ref,
    result.baseline_feature_ref,
    result.attach_back_ref,
    result.bbox_candidate_list_ref,
    result.roi_crop_packet_ref,
    result.template_alignment_ref,
    result.roi_diff_candidate_ref,
    result.candidate_overlay_ref,
    result.template_rect_overlay_ref,
    result.test_rect_overlay_ref,
    result.circle_overlay_ref,
    result.circle_edge_overlay_ref,
    result.formfit_candidate_overlay_ref,
    result.formfit_selection_overlay_ref,
    result.region_pattern_overlay_ref,
    result.region_pattern_descriptor_ref,
    result.fractal_partition_overlay_ref,
    result.distance_field_overlay_ref,
    result.skeleton_overlay_ref,
    result.centerline_overlay_ref,
    result.topology_repair_overlay_ref,
    result.published_primary_ref,
    result.published_result_ref,
    result.published_evidence_ref,
    result.published_bbox_candidate_list_ref,
    result.published_template_alignment_ref,
    result.published_roi_diff_candidate_ref,
    result.published_prior_roi_region_ref,
    result.published_roi_crop_packet_ref,
    result.published_roi_crop_policy_ref,
    result.predictions_csv,
    result.output_summary_csv,
    result.model_path,
    result.template_root,
    result.pairs_ref,
    result.published_evidence_ref,
    result.published_result_ref
  };

  for (size_t i = 0; i < sizeof(refs) / sizeof(refs[0]); ++i)
    PushUniqueText(artifact_refs, refs[i]);
}

void CollectReviewAnomalyFlags(const CxScriptExecutionResult &result,
                               const std::string &primary_visual_ref,
                               std::vector<std::string> &anomaly_flags)
{
  if (!result.success)
    PushUniqueText(anomaly_flags, "stage_inconsistency");
  if (result.degraded)
    PushUniqueText(anomaly_flags, "manual_review_required");
  if (!result.failure_mode.empty() && result.failure_mode != "none")
    PushUniqueText(anomaly_flags, result.failure_mode);
  if (!result.error_message.empty())
    PushUniqueText(anomaly_flags, "contract_incomplete");
  if (primary_visual_ref.empty())
    PushUniqueText(anomaly_flags, "artifact_missing");
  if (result.metric_delta > 0.0 || result.objective_delta > 0.0)
    PushUniqueText(anomaly_flags, "baseline_drift");
}

std::string ToLowerText(const std::string &text)
{
  std::string lowered = text;
  for (size_t i = 0; i < lowered.size(); ++i)
    lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
  return lowered;
}

bool IsCximageClassicalReviewCase(const CxScriptExecutionResult &result)
{
  const bool known_case =
    result.case_name == "line_measure_roi" ||
    result.case_name == "FindCircle" ||
    result.case_name == "circle_measure_fit" ||
    result.case_name == "formfit_rect_candidate" ||
    result.case_name == "binary_region" ||
    result.case_name == "fastmatch_template" ||
    result.case_name == "fast_template_match" ||
    result.case_name == "findobject_region";
  if (!known_case)
    return false;

  if (result.module == "cximage")
    return true;

  const std::string task_id_lower = ToLowerText(result.task_id);
  const std::string script_hint_lower =
    ToLowerText(result.script_path + " " + result.script_name);

  return task_id_lower.find("cximage.") == 0 ||
         script_hint_lower.find("rag_script_cases/cximage/") != std::string::npos ||
         script_hint_lower.find("rag_script_cases\\cximage\\") != std::string::npos ||
         script_hint_lower.find("cximage_") != std::string::npos;
}

std::string NormalizeLoweredPathForTokenMatch(const std::string &text)
{
  std::string normalized;
  normalized.reserve(text.size());
  bool previous_was_slash = false;
  for (size_t i = 0; i < text.size(); ++i)
  {
    char ch = text[i];
    if (ch == '\\')
      ch = '/';
    if (ch == '/')
    {
      if (previous_was_slash)
        continue;
      previous_was_slash = true;
    }
    else
    {
      previous_was_slash = false;
    }
    normalized.push_back(ch);
  }
  return normalized;
}

bool ContainsLoweredPathToken(const std::string &lowered_text,
                              const char *windows_token,
                              const char *unix_token)
{
  if (lowered_text.empty())
    return false;

  const std::string normalized_text = NormalizeLoweredPathForTokenMatch(lowered_text);
  const std::string normalized_windows =
    windows_token != 0 ? NormalizeLoweredPathForTokenMatch(windows_token) : std::string();
  const std::string normalized_unix =
    unix_token != 0 ? NormalizeLoweredPathForTokenMatch(unix_token) : std::string();

  return (!normalized_windows.empty() &&
          normalized_text.find(normalized_windows) != std::string::npos) ||
         (!normalized_unix.empty() &&
          normalized_text.find(normalized_unix) != std::string::npos);
}

std::string BuildTaskContextRef(const TaskContext &task_context)
{
  std::vector<std::string> items;
  PushUniqueText(items, task_context.source_thread);
  PushUniqueText(items, task_context.task_id);
  PushUniqueText(items, task_context.batch_id);
  PushUniqueText(items, task_context.case_name);
  PushUniqueText(items, task_context.stage);
  PushUniqueText(items, task_context.sequence_family);
  PushUniqueText(items, task_context.sequence_stage);
  PushUniqueText(items, task_context.upstream_ref);
  PushUniqueText(items, task_context.downstream_ref);
  return JoinTextItems(items, "|");
}

std::string BuildImageAcquisitionRef(const ImageAcquisitionSpec &image_acquisition_spec)
{
  std::vector<std::string> items;
  PushUniqueText(items, image_acquisition_spec.scope_type);
  PushUniqueText(items, image_acquisition_spec.execution_mode);
  PushUniqueText(items, image_acquisition_spec.crop_identity);
  PushUniqueText(items, image_acquisition_spec.source_image_ref);
  return JoinTextItems(items, "|");
}

std::string DeterminePrimaryGeometrySemantic(const CxScriptExecutionResult &result)
{
  const std::string identity =
    ToLowerText(result.module + " " + result.layer + " " + result.case_name + " " + result.template_test_alignment_status);
  if (identity.find("circle") != std::string::npos)
    return "circle";
  if (identity.find("line") != std::string::npos || identity.find("centerline") != std::string::npos)
    return "line_segment";
  if (identity.find("polyline") != std::string::npos)
    return "open_polyline";
  if (identity.find("arc") != std::string::npos)
    return "arc";
  if (!result.test_rect_overlay_ref.empty() || !result.formfit_selection_overlay_ref.empty())
    return "match_region";
  if (!result.candidate_overlay_ref.empty() || !result.formfit_candidate_overlay_ref.empty() ||
      !result.roi_diff_candidate_ref.empty() || !result.bbox_candidate_list_ref.empty())
    return "candidate_region";
  if (!result.attach_back_ref.empty())
    return "click_point";
  if (!result.template_alignment_ref.empty() || !result.pairs_ref.empty() || !result.template_root.empty())
    return "composite_template";
  if (!result.roi_crop_packet_ref.empty() || !result.published_roi_crop_packet_ref.empty() ||
      !result.published_prior_roi_region_ref.empty() || !result.roi_diff_candidate_ref.empty())
    return "closed_region";
  return "closed_region";
}

void CollectAuxiliaryGeometrySemantics(const CxScriptExecutionResult &result,
                                      const std::string &primary_geometry_semantic,
                                      std::vector<std::string> &auxiliary_geometry_semantics)
{
  if (!result.bbox_candidate_list_ref.empty())
    PushUniqueText(auxiliary_geometry_semantics, "point");
  if (!result.attach_back_ref.empty())
    PushUniqueText(auxiliary_geometry_semantics, "click_point");
  if (!result.bbox_candidate_list_ref.empty() || !result.roi_diff_candidate_ref.empty() ||
      !result.published_bbox_candidate_list_ref.empty() || !result.candidate_overlay_ref.empty() ||
      !result.formfit_candidate_overlay_ref.empty() || !result.region_pattern_overlay_ref.empty())
    PushUniqueText(auxiliary_geometry_semantics, "candidate_region");
  if (!result.test_rect_overlay_ref.empty() || !result.formfit_selection_overlay_ref.empty() ||
      !result.template_alignment_ref.empty() || !result.published_template_alignment_ref.empty())
    PushUniqueText(auxiliary_geometry_semantics, "match_region");
  if (!result.roi_crop_packet_ref.empty() || !result.published_roi_crop_packet_ref.empty() ||
      !result.roi_diff_candidate_ref.empty() || !result.published_prior_roi_region_ref.empty())
    PushUniqueText(auxiliary_geometry_semantics, "closed_region");
  if (!result.template_alignment_ref.empty() || !result.template_root.empty() || !result.pairs_ref.empty())
    PushUniqueText(auxiliary_geometry_semantics, "composite_template");
  if (primary_geometry_semantic != "line_segment" &&
      ToLowerText(result.case_name).find("line") != std::string::npos)
    PushUniqueText(auxiliary_geometry_semantics, "line_segment");

  for (size_t i = 0; i < auxiliary_geometry_semantics.size();)
  {
    if (auxiliary_geometry_semantics[i] == primary_geometry_semantic)
      auxiliary_geometry_semantics.erase(auxiliary_geometry_semantics.begin() + static_cast<long long>(i));
    else
      ++i;
  }
}
std::string DetermineTemplateProvenance(const CxScriptExecutionResult &result)
{
  if (!result.template_alignment_ref.empty())
    return "template_alignment_ref:" + result.template_alignment_ref;
  if (!result.template_root.empty())
    return "template_root:" + result.template_root;
  if (!result.pairs_ref.empty())
    return "pairs_ref:" + result.pairs_ref;
  if (!result.bbox_candidate_list_ref.empty() || !result.roi_crop_packet_ref.empty())
    return "runtime_bridge";
  return result.script_path.empty() ? "cxscript_runtime" : ("script_path:" + result.script_path);
}

std::string DetermineTemplateIdentity(const CxScriptExecutionResult &result)
{
  if (!result.template_alignment_ref.empty())
    return result.template_alignment_ref;
  if (!result.template_root.empty())
    return result.template_root;
  if (!result.pairs_ref.empty())
    return result.pairs_ref;
  if (!result.roi_crop_packet_ref.empty())
    return result.roi_crop_packet_ref;
  if (!result.case_name.empty())
    return result.case_name + ".template";
  return result.task_id.empty() ? "template.unknown" : result.task_id + ".template";
}

std::string DetermineTemplateReviewPriority(const CxScriptExecutionResult &result)
{
  const std::string alignment_status = ToLowerText(result.template_test_alignment_status);
  if (!result.success || result.degraded || !result.error_message.empty())
    return "high";
  if (!result.roi_diff_candidate_ref.empty() ||
      (!result.roi_diff_candidate_count.empty() && result.roi_diff_candidate_count != "0"))
    return "high";
  if (!alignment_status.empty() && alignment_status.find("pass") == std::string::npos)
    return "medium";
  return "normal";
}

std::string DetermineImageScopeType(const CxScriptExecutionResult &result)
{
  if (!result.roi_crop_packet_ref.empty() || !result.published_roi_crop_packet_ref.empty())
    return "template_region_crop";
  if (!result.published_prior_roi_region_ref.empty() || !result.roi_diff_candidate_ref.empty())
    return "focus_region_crop";
  if (!result.bbox_candidate_list_ref.empty())
    return "local_crop";
  return "full_image";
}

std::string DetermineExecutionMode(const CxScriptExecutionResult &result)
{
  if (result.layer == "train" || !result.train_param_summary.empty())
    return "training_sample_build";
  if (result.layer == "scenario")
    return "batch_test";
  if (result.degraded || !result.error_message.empty())
    return "review_recheck";
  return "runtime_single";
}

std::string DetermineImageAcquisitionProvenance(const CxScriptExecutionResult &result)
{
  if (!result.dataset_ref.empty())
    return "dataset_ref:" + result.dataset_ref;
  if (!result.input_dataset.empty())
    return "input_dataset:" + result.input_dataset;
  if (!result.sample_bundle_ref.empty())
    return "sample_bundle_ref:" + result.sample_bundle_ref;
  return result.script_path.empty() ? "cxscript_runtime" : ("script_path:" + result.script_path);
}

std::string DetermineModelRoute(const CxScriptExecutionResult &result)
{
  if (!result.model_name.empty())
    return result.model_name;
  if (!result.selected_method.empty())
    return result.selected_method;
  if (!result.feature_set.empty())
    return result.feature_set;
  if (!result.module.empty() && !result.case_name.empty())
    return result.module + "." + result.case_name;
  return result.module.empty() ? "cxscript_runtime" : result.module;
}

std::string DeterminePhase0TargetObjectRef(const CxScriptExecutionResult &result)
{
  const std::string primary_visual_ref = SelectReviewPrimaryVisualRef(result);
  if (!primary_visual_ref.empty())
    return primary_visual_ref;
  if (!result.result_object.empty())
    return result.result_object;
  return result.task_id;
}

std::string DetermineReviewerActionText(const CxScriptExecutionResult &result)
{
  std::string action = FindNamedResultFieldValue(result, "analysis", "recommended_action");
  if (action.empty())
    action = FindNamedResultFieldValue(result, "analysis", "next_review_action");
  if (action.empty())
    action = result.success ? "consume review boards and unified objects"
                            : "recheck result contract and anomaly focus";
  return action;
}

std::string DetermineFlowbackType(const CxScriptExecutionResult &result)
{
  if (!result.success)
    return "review_recheck";
  if (result.degraded || !result.anomaly_ref.empty() ||
      (!result.roi_diff_candidate_count.empty() && result.roi_diff_candidate_count != "0"))
    return "manual_review";
  if (result.layer == "train")
    return "publish_train_artifacts";
  return "promote_to_next_stage";
}

std::string DetermineFlowbackNextTarget(const CxScriptExecutionResult &result)
{
  if (!result.compare_ref.empty())
    return result.compare_ref;
  if (!result.replay_ref.empty())
    return result.replay_ref;
  if (!result.summary_ref.empty())
    return result.summary_ref;
  if (!result.result_object.empty())
    return result.result_object;
  return result.task_id;
}

std::string BuildEnsmallenSampleCountText(const CxScriptExecutionResult &result)
{
  std::string sample_text = flow_host_runtime_detail::FindAssignmentValue(result.input_artifacts,
                                                                          "sample_id");
  char delimiter = '|';
  if (sample_text.empty())
  {
    sample_text = result.input_sample;
    delimiter = ';';
  }
  const size_t count = CountDelimitedItems(sample_text, delimiter);
  return count == 0 ? std::string() : std::to_string(count);
}

std::string BuildEnsmallenBestParamSetsText(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "circle";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "match";
  if (result.layer == "train")
    return "circle,ellipse,match";
  if (result.layer == "infer")
  {
    const std::string best_params_artifact =
      FindAssignmentValue(result.input_artifacts, "best_params");
    if (!best_params_artifact.empty() || !result.best_params_ref.empty())
      return "circle,ellipse,match";
  }
  return std::string();
}

double ComputeReviewBoardConfidence(const std::string &status,
                                    const std::string &risk_level)
{
  if (status == "abnormal" || risk_level == "high")
    return 0.5;
  if (status == "review_only" || status == "partial" || status == "focused" ||
      risk_level == "medium")
    return 0.75;
  return 1.0;
}

void AddReviewBoardObject(MultimodalSlice &slice,
                          const std::string &object_id,
                          const std::string &object_kind,
                          const std::string &geometry_ref,
                          const std::string &semantic_label,
                          const std::string &summary,
                          double confidence)
{
  if (object_id.empty() && summary.empty())
    return;

  MultimodalSliceObject object;
  object.object_id = object_id.empty() ? summary : object_id;
  object.object_kind = object_kind;
  object.geometry_ref = geometry_ref;
  object.semantic_label = semantic_label;
  object.summary = summary;
  object.confidence = confidence;
  slice.objects.push_back(object);
}

void AddReviewBoardRelation(MultimodalSlice &slice,
                            const std::string &relation_kind,
                            const std::string &source_object_id,
                            const std::string &target_object_id,
                            const std::string &summary)
{
  if (summary.empty())
    return;

  MultimodalSliceRelation relation;
  relation.relation_kind = relation_kind;
  relation.source_object_id = source_object_id;
  relation.target_object_id = target_object_id;
  relation.summary = summary;
  slice.relations.push_back(relation);
}

void AppendReviewBoardSlices(CxScriptExecutionResult &result)
{
  for (size_t i = 0; i < result.unified_image_reviews.size(); ++i)
  {
    const UnifiedImageReviewRecord &image_review = result.unified_image_reviews[i];
    MultimodalSlice image_slice;
    image_slice.slice_id =
      (image_review.batch_id.empty() ? std::string("review.batch") : image_review.batch_id) +
      ".image_detail_board." + std::to_string(i + 1);
    image_slice.source_ref = image_review.input_image_ref;
    image_slice.source_hash = BuildPseudoSourceHash(image_slice.slice_id + "|" + image_review.image_id);
    image_slice.modality = "review_board";
    image_slice.analysis_kind = "image_detail_board";
    image_slice.result_ref = image_review.primary_visual_ref;
    image_slice.evidence_ref = JoinTextItems(image_review.artifact_refs, ";");
    image_slice.log_path = result.script_path;
    image_slice.model_ref = image_review.source_thread;
    image_slice.confidence = ComputeReviewBoardConfidence(image_review.status, std::string());
    image_slice.next_action = image_review.anomaly_flags.empty()
                                ? "inspect image detail evidence board"
                                : "focus anomaly flags and attached image refs";
    image_slice.tags.push_back("image_detail_board");
    image_slice.tags.push_back(image_review.source_thread);
    image_slice.tags.push_back("refresh:" + image_review.refresh_mode);
    image_slice.tags.push_back("priority:" + image_review.refresh_priority);

    const std::string image_object_id =
      image_review.image_id.empty() ? std::string("review.image") : image_review.image_id;
    AddReviewBoardObject(image_slice,
                         image_object_id,
                         "review_image",
                         image_review.stage,
                         image_review.status,
                         image_review.metric_summary_text,
                         image_slice.confidence);
    AddReviewBoardObject(image_slice,
                         image_review.primary_visual_ref,
                         "visual_ref",
                         "visual",
                         "primary_visual",
                         image_review.primary_visual_ref,
                         image_slice.confidence);
    AddReviewBoardObject(image_slice,
                         image_review.input_image_ref,
                         "input_ref",
                         "input",
                         "source_image",
                         image_review.input_image_ref,
                         1.0);

    for (size_t artifact_index = 0; artifact_index < image_review.artifact_refs.size(); ++artifact_index)
    {
      AddReviewBoardObject(image_slice,
                           image_review.artifact_refs[artifact_index],
                           "artifact_ref",
                           "artifact",
                           "review_evidence",
                           image_review.artifact_refs[artifact_index],
                           image_slice.confidence);
    }
    for (size_t element_index = 0; element_index < image_review.detection_elements.size(); ++element_index)
    {
      const UnifiedDetectionElement &element = image_review.detection_elements[element_index];
      AddReviewBoardObject(image_slice,
                           element.element_id,
                           "detection_element",
                           element.element_type,
                           element.consistency_status,
                           element.geometry_payload,
                           image_slice.confidence);
      AddReviewBoardRelation(image_slice,
                             "detection_element",
                             image_object_id,
                             element.element_id,
                             "role=" + element.semantic_role +
                               ";provenance=" + element.provenance +
                               ";status=" + element.consistency_status +
                               ";candidate=" + element.candidate_status +
                               ";match=" + element.match_status +
                               ";manual_review_signal=" + element.manual_review_signal +
                               ";group=" + element.element_group_label +
                               ";confidence=" + element.confidence +
                               ";drift=" + element.drift_summary +
                               ";focus_region_ref=" + element.focus_region_ref +
                               ";local_delta_ref=" + element.local_delta_ref +
                               ";findings=" + element.element_findings +
                               ";focus=" + element.element_level_focus +
                               ";linked_template_element_id=" +
                               element.linked_template_element_id);
    }

    AddReviewBoardRelation(image_slice,
                           "metric_summary",
                           image_object_id,
                           "metrics",
                           image_review.metric_summary_text);
    AddReviewBoardRelation(image_slice,
                           "anomaly_flags",
                           image_object_id,
                           "anomaly_focus",
                           JoinTextItems(image_review.anomaly_flags, ","));
    AddReviewBoardRelation(image_slice,
                           "contract_evidence",
                           image_object_id,
                           "contract",
                           JoinTextItems(image_review.contract_evidence, ";"));
    AddReviewBoardRelation(image_slice,
                           "phenomenon_evidence",
                           image_object_id,
                           "phenomenon",
                           JoinTextItems(image_review.phenomenon_evidence, ";"));
    AddReviewBoardRelation(image_slice,
                           "interaction_evidence",
                           image_object_id,
                           "interaction",
                           JoinTextItems(image_review.interaction_evidence, ";"));
    AddReviewBoardRelation(image_slice,
                           "observation_personality",
                           image_object_id,
                           "review_personality",
                           image_review.observation_personality);
    AddReviewBoardRelation(image_slice,
                           "default_open_chain",
                           image_object_id,
                           "review_chain",
                           image_review.default_open_chain);
    AddReviewBoardRelation(image_slice,
                           "evidence_focus_summary",
                           image_object_id,
                           "evidence_focus",
                           image_review.evidence_focus_summary);
    AddReviewBoardRelation(image_slice,
                           "thread_handoff",
                           image_object_id,
                           "thread_handoff",
                           image_review.thread_handoff);
    AddReviewBoardRelation(image_slice,
                           "refresh_mode",
                           image_object_id,
                           "refresh",
                           image_review.refresh_mode);
    AddReviewBoardRelation(image_slice,
                           "changed_fields",
                           image_object_id,
                           "refresh_fields",
                           JoinTextItems(image_review.changed_fields, ";"));
    AddReviewBoardRelation(image_slice,
                           "changed_element_ids",
                           image_object_id,
                           "refresh_elements",
                           JoinTextItems(image_review.changed_element_ids, ";"));
    AddReviewBoardRelation(image_slice,
                           "changed_chain_keys",
                           image_object_id,
                           "refresh_chains",
                           JoinTextItems(image_review.changed_chain_keys, ";"));
    AddReviewBoardRelation(image_slice,
                           "refresh_priority",
                           image_object_id,
                           "refresh_priority",
                           image_review.refresh_priority);
    AddReviewBoardRelation(image_slice,
                           "primary_detection_semantic",
                           image_object_id,
                           "detection_semantic",
                           image_review.primary_detection_semantic);
    AddReviewBoardRelation(image_slice,
                           "template_alignment_status",
                           image_object_id,
                           "template_alignment",
                           image_review.template_alignment_status);
    AddReviewBoardRelation(image_slice,
                           "detection_element_summary",
                           image_object_id,
                           "detection_elements",
                           BuildElementLevelDiff(image_review));
    AddReviewBoardRelation(image_slice,
                           "element_summary",
                           image_object_id,
                           "element_details",
                           BuildDetectionElementSummary(image_review.detection_elements));
    AddReviewBoardRelation(image_slice,
                           "element_status_summary",
                           image_object_id,
                           "element_status",
                           BuildElementStatusBoardSummary(image_review.detection_elements));
    AddReviewBoardRelation(image_slice,
                           "element_findings",
                           image_object_id,
                           "element_findings",
                           BuildElementFindingsSummary(image_review.detection_elements));
    AddReviewBoardRelation(image_slice,
                           "element_level_focus",
                           image_object_id,
                           "element_focus",
                           BuildElementLevelFocusSummary(image_review.detection_elements));
    AddReviewBoardRelation(image_slice,
                           "candidate_status_summary",
                           image_object_id,
                           "candidate_status",
                           image_review.candidate_status_summary);
    AddReviewBoardRelation(image_slice,
                           "match_status_summary",
                           image_object_id,
                           "match_status",
                           image_review.match_status_summary);
    AddReviewBoardRelation(image_slice,
                           "manual_review_signal_summary",
                           image_object_id,
                           "manual_review_signal",
                           image_review.manual_review_signal_summary);
    AddReviewBoardRelation(image_slice,
                           "element_group_summary",
                           image_object_id,
                           "element_groups",
                           image_review.element_group_summary);
    AddReviewBoardRelation(image_slice,
                           "focus_refresh_targets",
                           image_object_id,
                           "focus_refresh",
                           image_review.focus_refresh_targets);
    AddReviewBoardRelation(image_slice,
                           "local_delta_targets",
                           image_object_id,
                           "local_delta",
                           image_review.local_delta_targets);
    AddReviewBoardRelation(image_slice,
                           "grouped_element_preview",
                           image_object_id,
                           "grouped_preview",
                           image_review.grouped_element_preview);
    AddReviewBoardRelation(image_slice,
                           "focus_element_preview",
                           image_object_id,
                           "focus_preview",
                           image_review.focus_element_preview);
    AddReviewBoardRelation(image_slice,
                           "delta_element_preview",
                           image_object_id,
                           "delta_preview",
                           image_review.delta_element_preview);
    result.multimodal_slices.push_back(image_slice);
  }

  for (size_t i = 0; i < result.unified_task_reviews.size(); ++i)
  {
    const UnifiedTaskReviewBundle &task_review = result.unified_task_reviews[i];
    MultimodalSlice task_slice;
    task_slice.slice_id =
      (task_review.batch_id.empty() ? std::string("review.batch") : task_review.batch_id) +
      ".task_summary_board." + std::to_string(i + 1);
    task_slice.source_ref = task_review.task_id;
    task_slice.source_hash = BuildPseudoSourceHash(task_slice.slice_id + "|" + task_review.task_id);
    task_slice.modality = "review_board";
    task_slice.analysis_kind = "task_summary_board";
    task_slice.result_ref =
      !task_review.primary_visual_ref.empty() ? task_review.primary_visual_ref :
      (!FindFirstNonEmptyText(task_review.supporting_refs).empty()
         ? FindFirstNonEmptyText(task_review.supporting_refs)
         : task_review.task_id);
    task_slice.evidence_ref = JoinTextItems(task_review.supporting_refs, ";");
    task_slice.log_path = result.script_path;
    task_slice.model_ref = task_review.source_thread;
    task_slice.confidence =
      task_review.review_required_count > 0 || task_review.abnormal_images > 0 ? 0.75 : 1.0;
    task_slice.next_action = task_review.next_attention_points;
    task_slice.tags.push_back("task_summary_board");
    task_slice.tags.push_back(task_review.task_type);
    task_slice.tags.push_back("refresh:" + task_review.refresh_mode);
    task_slice.tags.push_back("priority:" + task_review.refresh_priority);

    const std::string task_object_id =
      task_review.task_id.empty() ? std::string("review.task") : task_review.task_id;
    AddReviewBoardObject(task_slice,
                         task_object_id,
                         "review_task",
                         task_review.case_group,
                         "task_summary",
                         task_review.metric_summary,
                         task_slice.confidence);
    for (size_t focus_index = 0; focus_index < task_review.focus_image_ids.size(); ++focus_index)
    {
      AddReviewBoardObject(task_slice,
                           task_review.focus_image_ids[focus_index],
                           "focus_image",
                           "focus",
                           "focus_image",
                           task_review.focus_image_ids[focus_index],
                           task_slice.confidence);
    }
    for (size_t ref_index = 0; ref_index < task_review.supporting_refs.size(); ++ref_index)
    {
      AddReviewBoardObject(task_slice,
                           task_review.supporting_refs[ref_index],
                           "supporting_ref",
                           "ref",
                           "task_supporting_ref",
                           task_review.supporting_refs[ref_index],
                           task_slice.confidence);
    }

    AddReviewBoardRelation(task_slice,
                           "status_distribution",
                           task_object_id,
                           "status",
                           task_review.status_distribution);
    AddReviewBoardRelation(task_slice,
                           "anomaly_distribution",
                           task_object_id,
                           "anomaly",
                           task_review.anomaly_type_distribution);
    AddReviewBoardRelation(task_slice,
                           "baseline_compare",
                           task_object_id,
                           "baseline",
                           task_review.baseline_compare_summary);
    AddReviewBoardRelation(task_slice,
                           "training_evidence",
                           task_object_id,
                           "training",
                           task_review.training_evidence_summary);
    AddReviewBoardRelation(task_slice,
                           "observation_personality",
                           task_object_id,
                           "review_personality",
                           task_review.observation_personality);
    AddReviewBoardRelation(task_slice,
                           "default_open_chain",
                           task_object_id,
                           "review_chain",
                           task_review.default_open_chain);
    AddReviewBoardRelation(task_slice,
                           "evidence_focus_summary",
                           task_object_id,
                           "evidence_focus",
                           task_review.evidence_focus_summary);
    AddReviewBoardRelation(task_slice,
                           "thread_handoff",
                           task_object_id,
                           "thread_handoff",
                           task_review.thread_handoff);
    AddReviewBoardRelation(task_slice,
                           "review_mode",
                           task_object_id,
                           "review_mode",
                           task_review.review_mode);
    AddReviewBoardRelation(task_slice,
                           "default_decision_axis",
                           task_object_id,
                           "decision_axis",
                           task_review.default_decision_axis);
    AddReviewBoardRelation(task_slice,
                           "refresh_mode",
                           task_object_id,
                           "refresh",
                           task_review.refresh_mode);
    AddReviewBoardRelation(task_slice,
                           "changed_fields",
                           task_object_id,
                           "refresh_fields",
                           JoinTextItems(task_review.changed_fields, ";"));
    AddReviewBoardRelation(task_slice,
                           "changed_element_ids",
                           task_object_id,
                           "refresh_elements",
                           JoinTextItems(task_review.changed_element_ids, ";"));
    AddReviewBoardRelation(task_slice,
                           "changed_chain_keys",
                           task_object_id,
                           "refresh_chains",
                           JoinTextItems(task_review.changed_chain_keys, ";"));
    AddReviewBoardRelation(task_slice,
                           "refresh_priority",
                           task_object_id,
                           "refresh_priority",
                           task_review.refresh_priority);
    AddReviewBoardRelation(task_slice,
                           "supporting_refs",
                           task_object_id,
                           "refs",
                           JoinTextItems(task_review.supporting_refs, ";"));
    AddReviewBoardRelation(task_slice,
                           "element_type_summary",
                           task_object_id,
                           "element_summary",
                           task_review.element_type_summary);
    AddReviewBoardRelation(task_slice,
                           "element_summary",
                           task_object_id,
                           "element_details",
                           task_review.element_summary);
    AddReviewBoardRelation(task_slice,
                           "element_status_summary",
                           task_object_id,
                           "element_status",
                           task_review.element_status_summary);
    AddReviewBoardRelation(task_slice,
                           "element_findings",
                           task_object_id,
                           "element_findings",
                           task_review.element_findings);
    AddReviewBoardRelation(task_slice,
                           "element_level_focus",
                           task_object_id,
                           "element_focus",
                           task_review.element_level_focus);
    AddReviewBoardRelation(task_slice,
                           "candidate_status_summary",
                           task_object_id,
                           "candidate_status",
                           task_review.candidate_status_summary);
    AddReviewBoardRelation(task_slice,
                           "match_status_summary",
                           task_object_id,
                           "match_status",
                           task_review.match_status_summary);
    AddReviewBoardRelation(task_slice,
                           "manual_review_signal_summary",
                           task_object_id,
                           "manual_review_signal",
                           task_review.manual_review_signal_summary);
    AddReviewBoardRelation(task_slice,
                           "element_group_summary",
                           task_object_id,
                           "element_groups",
                           task_review.element_group_summary);
    AddReviewBoardRelation(task_slice,
                           "focus_refresh_targets",
                           task_object_id,
                           "focus_refresh",
                           task_review.focus_refresh_targets);
    AddReviewBoardRelation(task_slice,
                           "local_delta_targets",
                           task_object_id,
                           "local_delta",
                           task_review.local_delta_targets);
    AddReviewBoardRelation(task_slice,
                           "grouped_element_preview",
                           task_object_id,
                           "grouped_preview",
                           task_review.grouped_element_preview);
    AddReviewBoardRelation(task_slice,
                           "focus_element_preview",
                           task_object_id,
                           "focus_preview",
                           task_review.focus_element_preview);
    AddReviewBoardRelation(task_slice,
                           "delta_element_preview",
                           task_object_id,
                           "delta_preview",
                           task_review.delta_element_preview);
    AddReviewBoardRelation(task_slice,
                           "missing_element_summary",
                           task_object_id,
                           "missing_elements",
                           task_review.missing_element_summary);
    AddReviewBoardRelation(task_slice,
                           "drifted_element_summary",
                           task_object_id,
                           "drifted_elements",
                           task_review.drifted_element_summary);
    AddReviewBoardRelation(task_slice,
                           "abnormal_element_summary",
                           task_object_id,
                           "abnormal_elements",
                           task_review.abnormal_element_summary);
    AddReviewBoardRelation(task_slice,
                           "current_conclusion",
                           task_object_id,
                           "conclusion",
                           task_review.current_conclusion);
    result.multimodal_slices.push_back(task_slice);
  }

  for (size_t i = 0; i < result.unified_compare_slices.size(); ++i)
  {
    const UnifiedCompareSlice &compare_slice = result.unified_compare_slices[i];
    MultimodalSlice board_slice;
    board_slice.slice_id = compare_slice.compare_id.empty()
                             ? "review.compare.cross_thread_compare_board"
                             : compare_slice.compare_id;
    board_slice.source_ref = compare_slice.left_ref;
    board_slice.source_hash = BuildPseudoSourceHash(board_slice.slice_id + "|" + compare_slice.right_ref);
    board_slice.modality = "review_board";
    board_slice.analysis_kind = "cross_thread_compare_board";
    board_slice.result_ref = compare_slice.right_ref;
    board_slice.evidence_ref = JoinTextItems(compare_slice.supporting_refs, ";");
    board_slice.log_path = result.script_path;
    board_slice.model_ref = result.module;
    board_slice.confidence =
      ComputeReviewBoardConfidence(std::string("ready"), compare_slice.risk_level);
    board_slice.next_action = compare_slice.focus_recommendation;
    board_slice.tags.push_back("cross_thread_compare_board");
    board_slice.tags.push_back(compare_slice.compare_type);
    board_slice.tags.push_back("refresh:" + compare_slice.refresh_mode);
    board_slice.tags.push_back("priority:" + compare_slice.refresh_priority);

    AddReviewBoardObject(board_slice,
                         compare_slice.compare_id,
                         "compare_slice",
                         compare_slice.compare_type,
                         compare_slice.risk_level,
                         compare_slice.delta_summary,
                         board_slice.confidence);
    AddReviewBoardObject(board_slice,
                         compare_slice.left_ref,
                         "compare_left_ref",
                         "left",
                         "baseline_or_current",
                         compare_slice.left_ref,
                         board_slice.confidence);
    AddReviewBoardObject(board_slice,
                         compare_slice.right_ref,
                         "compare_right_ref",
                         "right",
                         "target_or_baseline",
                         compare_slice.right_ref,
                         board_slice.confidence);
    for (size_t ref_index = 0; ref_index < compare_slice.supporting_refs.size(); ++ref_index)
    {
      AddReviewBoardObject(board_slice,
                           compare_slice.supporting_refs[ref_index],
                           "supporting_ref",
                           "ref",
                           "compare_supporting_ref",
                           compare_slice.supporting_refs[ref_index],
                           board_slice.confidence);
    }
    AddReviewBoardRelation(board_slice,
                           "compare_dimensions",
                           compare_slice.compare_id,
                           "dimensions",
                           JoinTextItems(compare_slice.compare_dimensions, ";"));
    AddReviewBoardRelation(board_slice,
                           "delta_summary",
                           compare_slice.compare_id,
                           "delta",
                           compare_slice.delta_summary);
    AddReviewBoardRelation(board_slice,
                           "focus_recommendation",
                           compare_slice.compare_id,
                           "next_action",
                           compare_slice.focus_recommendation);
    AddReviewBoardRelation(board_slice,
                           "observation_personality",
                           compare_slice.compare_id,
                           "review_personality",
                           compare_slice.observation_personality);
    AddReviewBoardRelation(board_slice,
                           "default_open_chain",
                           compare_slice.compare_id,
                           "review_chain",
                           compare_slice.default_open_chain);
    AddReviewBoardRelation(board_slice,
                           "evidence_focus_summary",
                           compare_slice.compare_id,
                           "evidence_focus",
                           compare_slice.evidence_focus_summary);
    AddReviewBoardRelation(board_slice,
                           "thread_handoff",
                           compare_slice.compare_id,
                           "thread_handoff",
                           compare_slice.thread_handoff);
    AddReviewBoardRelation(board_slice,
                           "compare_view_mode",
                           compare_slice.compare_id,
                           "compare_mode",
                           compare_slice.compare_view_mode);
    AddReviewBoardRelation(board_slice,
                           "refresh_mode",
                           compare_slice.compare_id,
                           "refresh",
                           compare_slice.refresh_mode);
    AddReviewBoardRelation(board_slice,
                           "changed_fields",
                           compare_slice.compare_id,
                           "refresh_fields",
                           JoinTextItems(compare_slice.changed_fields, ";"));
    AddReviewBoardRelation(board_slice,
                           "changed_element_ids",
                           compare_slice.compare_id,
                           "refresh_elements",
                           JoinTextItems(compare_slice.changed_element_ids, ";"));
    AddReviewBoardRelation(board_slice,
                           "changed_chain_keys",
                           compare_slice.compare_id,
                           "refresh_chains",
                           JoinTextItems(compare_slice.changed_chain_keys, ";"));
    AddReviewBoardRelation(board_slice,
                           "refresh_priority",
                           compare_slice.compare_id,
                           "refresh_priority",
                           compare_slice.refresh_priority);
    AddReviewBoardRelation(board_slice,
                           "supporting_refs",
                           compare_slice.compare_id,
                           "refs",
                           JoinTextItems(compare_slice.supporting_refs, ";"));
    AddReviewBoardRelation(board_slice,
                           "element_summary",
                           compare_slice.compare_id,
                           "element_details",
                           compare_slice.element_summary);
    AddReviewBoardRelation(board_slice,
                           "element_status_summary",
                           compare_slice.compare_id,
                           "element_status",
                           compare_slice.element_status_summary);
    AddReviewBoardRelation(board_slice,
                           "element_findings",
                           compare_slice.compare_id,
                           "element_findings",
                           compare_slice.element_findings);
    AddReviewBoardRelation(board_slice,
                           "element_level_focus",
                           compare_slice.compare_id,
                           "element_focus",
                           compare_slice.element_level_focus);
    AddReviewBoardRelation(board_slice,
                           "candidate_status_summary",
                           compare_slice.compare_id,
                           "candidate_status",
                           compare_slice.candidate_status_summary);
    AddReviewBoardRelation(board_slice,
                           "match_status_summary",
                           compare_slice.compare_id,
                           "match_status",
                           compare_slice.match_status_summary);
    AddReviewBoardRelation(board_slice,
                           "manual_review_signal_summary",
                           compare_slice.compare_id,
                           "manual_review_signal",
                           compare_slice.manual_review_signal_summary);
    AddReviewBoardRelation(board_slice,
                           "element_group_summary",
                           compare_slice.compare_id,
                           "element_groups",
                           compare_slice.element_group_summary);
    AddReviewBoardRelation(board_slice,
                           "focus_refresh_targets",
                           compare_slice.compare_id,
                           "focus_refresh",
                           compare_slice.focus_refresh_targets);
    AddReviewBoardRelation(board_slice,
                           "local_delta_targets",
                           compare_slice.compare_id,
                           "local_delta",
                           compare_slice.local_delta_targets);
    AddReviewBoardRelation(board_slice,
                           "grouped_element_preview",
                           compare_slice.compare_id,
                           "grouped_preview",
                           compare_slice.grouped_element_preview);
    AddReviewBoardRelation(board_slice,
                           "focus_element_preview",
                           compare_slice.compare_id,
                           "focus_preview",
                           compare_slice.focus_element_preview);
    AddReviewBoardRelation(board_slice,
                           "delta_element_preview",
                           compare_slice.compare_id,
                           "delta_preview",
                           compare_slice.delta_element_preview);
    AddReviewBoardRelation(board_slice,
                           "element_level_diff",
                           compare_slice.compare_id,
                           "element_diff",
                           compare_slice.element_level_diff);
    AddReviewBoardRelation(board_slice,
                           "semantic_diff",
                           compare_slice.compare_id,
                           "semantic_diff",
                           compare_slice.semantic_diff);
    AddReviewBoardRelation(board_slice,
                           "structure_diff",
                           compare_slice.compare_id,
                           "structure_diff",
                           compare_slice.structure_diff);
    result.multimodal_slices.push_back(board_slice);
  }

  for (size_t i = 0; i < result.unified_anomaly_focus_bundles.size(); ++i)
  {
    const UnifiedAnomalyFocusBundle &anomaly_bundle = result.unified_anomaly_focus_bundles[i];
    MultimodalSlice board_slice;
    board_slice.slice_id =
      (anomaly_bundle.batch_id.empty() ? std::string("review.batch") : anomaly_bundle.batch_id) +
      ".anomaly_focus_board." + std::to_string(i + 1);
    board_slice.source_ref = anomaly_bundle.task_id;
    board_slice.source_hash =
      BuildPseudoSourceHash(board_slice.slice_id + "|" + anomaly_bundle.anomaly_type_summary);
    board_slice.modality = "review_board";
    board_slice.analysis_kind = "anomaly_focus_board";
    board_slice.result_ref =
      !FindFirstNonEmptyText(anomaly_bundle.abnormal_image_ids).empty()
        ? FindFirstNonEmptyText(anomaly_bundle.abnormal_image_ids)
        : FindFirstNonEmptyText(anomaly_bundle.top_focus_objects);
    board_slice.evidence_ref = JoinTextItems(anomaly_bundle.supporting_refs, ";");
    board_slice.log_path = result.script_path;
    board_slice.model_ref =
      anomaly_bundle.source_thread.empty() ? result.module : anomaly_bundle.source_thread;
    board_slice.confidence =
      ComputeReviewBoardConfidence(std::string("focused"), anomaly_bundle.risk_level);
    board_slice.next_action = JoinTextItems(anomaly_bundle.analysis_suggestions, ";");
    board_slice.tags.push_back("anomaly_focus_board");
    board_slice.tags.push_back(anomaly_bundle.risk_level);
    board_slice.tags.push_back("refresh:" + anomaly_bundle.refresh_mode);
    board_slice.tags.push_back("priority:" + anomaly_bundle.refresh_priority);

    const std::string anomaly_object_id =
      anomaly_bundle.task_id.empty() ? std::string("review.anomaly") : anomaly_bundle.task_id;
    AddReviewBoardObject(board_slice,
                         anomaly_object_id,
                         "anomaly_focus",
                         "anomaly",
                         anomaly_bundle.risk_level,
                         anomaly_bundle.anomaly_type_summary,
                         board_slice.confidence);
    for (size_t image_index = 0; image_index < anomaly_bundle.abnormal_image_ids.size(); ++image_index)
    {
      AddReviewBoardObject(board_slice,
                           anomaly_bundle.abnormal_image_ids[image_index],
                           "abnormal_image",
                           "image",
                           "need_check",
                           anomaly_bundle.abnormal_image_ids[image_index],
                           board_slice.confidence);
    }
    for (size_t object_index = 0; object_index < anomaly_bundle.top_focus_objects.size(); ++object_index)
    {
      AddReviewBoardObject(board_slice,
                           anomaly_bundle.top_focus_objects[object_index],
                           "focus_object",
                           "focus",
                           "top_focus_object",
                           anomaly_bundle.top_focus_objects[object_index],
                           board_slice.confidence);
    }
    for (size_t ref_index = 0; ref_index < anomaly_bundle.supporting_refs.size(); ++ref_index)
    {
      AddReviewBoardObject(board_slice,
                           anomaly_bundle.supporting_refs[ref_index],
                           "supporting_ref",
                           "ref",
                           "anomaly_supporting_ref",
                           anomaly_bundle.supporting_refs[ref_index],
                           board_slice.confidence);
    }

    AddReviewBoardRelation(board_slice,
                           "anomaly_type_summary",
                           anomaly_object_id,
                           "anomaly_type",
                           anomaly_bundle.anomaly_type_summary);
    AddReviewBoardRelation(board_slice,
                           "analysis_suggestions",
                           anomaly_object_id,
                           "next_analysis",
                           JoinTextItems(anomaly_bundle.analysis_suggestions, ";"));
    AddReviewBoardRelation(board_slice,
                           "observation_personality",
                           anomaly_object_id,
                           "review_personality",
                           anomaly_bundle.observation_personality);
    AddReviewBoardRelation(board_slice,
                           "default_open_chain",
                           anomaly_object_id,
                           "review_chain",
                           anomaly_bundle.default_open_chain);
    AddReviewBoardRelation(board_slice,
                           "evidence_focus_summary",
                           anomaly_object_id,
                           "evidence_focus",
                           anomaly_bundle.evidence_focus_summary);
    AddReviewBoardRelation(board_slice,
                           "thread_handoff",
                           anomaly_object_id,
                           "thread_handoff",
                           anomaly_bundle.thread_handoff);
    AddReviewBoardRelation(board_slice,
                           "anomaly_axis",
                           anomaly_object_id,
                           "anomaly_axis",
                           anomaly_bundle.anomaly_axis);
    AddReviewBoardRelation(board_slice,
                           "refresh_mode",
                           anomaly_object_id,
                           "refresh",
                           anomaly_bundle.refresh_mode);
    AddReviewBoardRelation(board_slice,
                           "changed_fields",
                           anomaly_object_id,
                           "refresh_fields",
                           JoinTextItems(anomaly_bundle.changed_fields, ";"));
    AddReviewBoardRelation(board_slice,
                           "changed_element_ids",
                           anomaly_object_id,
                           "refresh_elements",
                           JoinTextItems(anomaly_bundle.changed_element_ids, ";"));
    AddReviewBoardRelation(board_slice,
                           "changed_chain_keys",
                           anomaly_object_id,
                           "refresh_chains",
                           JoinTextItems(anomaly_bundle.changed_chain_keys, ";"));
    AddReviewBoardRelation(board_slice,
                           "refresh_priority",
                           anomaly_object_id,
                           "refresh_priority",
                           anomaly_bundle.refresh_priority);
    AddReviewBoardRelation(board_slice,
                           "supporting_refs",
                           anomaly_object_id,
                           "refs",
                           JoinTextItems(anomaly_bundle.supporting_refs, ";"));
    AddReviewBoardRelation(board_slice,
                           "anomaly_element_ids",
                           anomaly_object_id,
                           "anomaly_elements",
                           JoinTextItems(anomaly_bundle.anomaly_element_ids, ";"));
    AddReviewBoardRelation(board_slice,
                           "anomaly_element_types",
                           anomaly_object_id,
                           "anomaly_element_types",
                           JoinTextItems(anomaly_bundle.anomaly_element_types, ";"));
    AddReviewBoardRelation(board_slice,
                           "element_summary",
                           anomaly_object_id,
                           "element_details",
                           anomaly_bundle.element_summary);
    AddReviewBoardRelation(board_slice,
                           "element_status_summary",
                           anomaly_object_id,
                           "element_status",
                           anomaly_bundle.element_status_summary);
    AddReviewBoardRelation(board_slice,
                           "element_findings",
                           anomaly_object_id,
                           "element_findings",
                           anomaly_bundle.element_findings);
    AddReviewBoardRelation(board_slice,
                           "element_level_focus",
                           anomaly_object_id,
                           "element_focus",
                           anomaly_bundle.element_level_focus);
    AddReviewBoardRelation(board_slice,
                           "candidate_status_summary",
                           anomaly_object_id,
                           "candidate_status",
                           anomaly_bundle.candidate_status_summary);
    AddReviewBoardRelation(board_slice,
                           "match_status_summary",
                           anomaly_object_id,
                           "match_status",
                           anomaly_bundle.match_status_summary);
    AddReviewBoardRelation(board_slice,
                           "manual_review_signal_summary",
                           anomaly_object_id,
                           "manual_review_signal",
                           anomaly_bundle.manual_review_signal_summary);
    AddReviewBoardRelation(board_slice,
                           "element_group_summary",
                           anomaly_object_id,
                           "element_groups",
                           anomaly_bundle.element_group_summary);
    AddReviewBoardRelation(board_slice,
                           "focus_refresh_targets",
                           anomaly_object_id,
                           "focus_refresh",
                           anomaly_bundle.focus_refresh_targets);
    AddReviewBoardRelation(board_slice,
                           "local_delta_targets",
                           anomaly_object_id,
                           "local_delta",
                           anomaly_bundle.local_delta_targets);
    AddReviewBoardRelation(board_slice,
                           "grouped_element_preview",
                           anomaly_object_id,
                           "grouped_preview",
                           anomaly_bundle.grouped_element_preview);
    AddReviewBoardRelation(board_slice,
                           "focus_element_preview",
                           anomaly_object_id,
                           "focus_preview",
                           anomaly_bundle.focus_element_preview);
    AddReviewBoardRelation(board_slice,
                           "delta_element_preview",
                           anomaly_object_id,
                           "delta_preview",
                           anomaly_bundle.delta_element_preview);
    AddReviewBoardRelation(board_slice,
                           "anomaly_focus_reason",
                           anomaly_object_id,
                           "anomaly_focus_reason",
                           anomaly_bundle.anomaly_focus_reason);
    result.multimodal_slices.push_back(board_slice);
  }
}

bool HasNamedResultFieldEntry(const CxScriptExecutionResult &result,
                              const std::string &result_name,
                              const std::string &field_name)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    if (result.result_fields[i].result_name == result_name &&
        result.result_fields[i].field_name == field_name)
      return true;
  }
  return false;
}

std::string BuildTaskScopedTorchRef(const CxScriptExecutionResult &result,
                                    const char *suffix)
{
  if (suffix == 0 || *suffix == '\0')
    return std::string();

  if (!result.task_id.empty())
    return result.task_id + "." + suffix;

  if (!result.case_name.empty())
    return result.case_name + "." + suffix;

  return std::string();
}

std::string ResolveTorchTrainPublishedRef(const CxScriptExecutionResult &result,
                                          const std::string &field_name)
{
  if (result.module != "torch_module" && result.module != "torch")
    return std::string();

  if (result.case_name == "torch.yolov8.mainline.train")
  {
    if (field_name == "trainer_lifecycle_summary")
      return BuildTaskScopedTorchRef(result, "trainer_lifecycle_summary");
    if (field_name == "trainer_flat_run")
      return BuildTaskScopedTorchRef(result, "trainer_flat_run");
    if (field_name == "unified_mainline_summary")
      return BuildTaskScopedTorchRef(result, "unified_mainline_summary");
    if (field_name == "checkpoint_or_resume_hint")
      return BuildTaskScopedTorchRef(result, "checkpoint_or_resume_hint");
  }

  if (result.case_name == "torch.mobilevit.mainline.train")
  {
    if (field_name == "trainer_lifecycle_summary")
      return BuildTaskScopedTorchRef(result, "trainer_lifecycle_summary");
    if (field_name == "trainer_flat_run")
      return BuildTaskScopedTorchRef(result, "trainer_flat_run");
    if (field_name == "unified_mainline_summary")
      return BuildTaskScopedTorchRef(result, "unified_mainline_summary");
    if (field_name == "roi_train_feedback")
      return BuildTaskScopedTorchRef(result, "roi_train_feedback");
  }

  if (result.case_name == "torch.deeplab.mainline.train")
  {
    if (field_name == "segmentation_trainer_lifecycle_summary")
      return BuildTaskScopedTorchRef(result, "segmentation_trainer_lifecycle_summary");
    if (field_name == "segmentation_trainer_flat_run")
      return BuildTaskScopedTorchRef(result, "segmentation_trainer_flat_run");
    if (field_name == "segmentation_unified_summary")
      return BuildTaskScopedTorchRef(result, "segmentation_unified_summary");
    if (field_name == "foreground_iou_summary")
      return BuildTaskScopedTorchRef(result, "foreground_iou_summary");
  }

  return std::string();
}





