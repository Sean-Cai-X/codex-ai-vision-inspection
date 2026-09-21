#ifndef CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_FLOW_HOST_HELPERS_H
#define CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_FLOW_HOST_HELPERS_H

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

#include "parser_cxscript_flow.h"
#include "parser_cxscript_types.h"
#include "parser_test_driver.h"

namespace cxparser_ext
{
namespace flow_host_runtime_detail
{
std::string BuildEnsmallenLikelyIssueClass(const CxScriptExecutionResult &result);
std::string BuildEnsmallenRecommendedAction(const CxScriptExecutionResult &result);
std::string BuildEnsmallenComparisonStatus(const CxScriptExecutionResult &result);
std::string BuildEnsmallenComparisonMagnitude(const CxScriptExecutionResult &result);
std::string BuildEnsmallenNextBucketFocus(const CxScriptExecutionResult &result);
std::string BuildEnsmallenObservationMode(const CxScriptExecutionResult &result);
std::string BuildEnsmallenExpansionGate(const CxScriptExecutionResult &result);
std::string BuildEnsmallenBucketCoverage(const CxScriptExecutionResult &result);
std::string BuildEnsmallenRiskAxis(const CxScriptExecutionResult &result);
std::string BuildEnsmallenCoverageGap(const CxScriptExecutionResult &result);
std::string BuildEnsmallenObservationPriority(const CxScriptExecutionResult &result);
std::string BuildEnsmallenPrimaryReviewRef(const CxScriptExecutionResult &result,
                                           const std::string &fallback_review_ref_prefix);
inline std::string FindAssignmentValue(const std::string &text,
                                       const std::string &key);

namespace detail
{
inline bool StartsWithFlowHostHelper(const std::string &text, const char *prefix)
{
  return text.find(prefix) == 0;
}

inline std::string TrimFlowHostHelper(const std::string &text)
{
  size_t begin = 0;
  while (begin < text.size() &&
         (text[begin] == ' ' || text[begin] == '\t' ||
          text[begin] == '\r' || text[begin] == '\n'))
  {
    ++begin;
  }

  size_t end = text.size();
  while (end > begin &&
         (text[end - 1] == ' ' || text[end - 1] == '\t' ||
          text[end - 1] == '\r' || text[end - 1] == '\n'))
  {
    --end;
  }

  return text.substr(begin, end - begin);
}

inline std::string StripQuotesFlowHostHelper(const std::string &value)
{
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    return value.substr(1, value.size() - 2);
  return value;
}

inline std::string StripTrailingSemicolonFlowHostHelper(const std::string &text)
{
  const std::string trimmed = TrimFlowHostHelper(text);
  if (!trimmed.empty() && trimmed[trimmed.size() - 1] == ';')
    return TrimFlowHostHelper(trimmed.substr(0, trimmed.size() - 1));
  return trimmed;
}

inline std::string ToLowerFlowHostHelper(const std::string &text)
{
  std::string lowered = text;
  for (size_t i = 0; i < lowered.size(); ++i)
    lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
  return lowered;
}

inline std::vector<std::string> SplitCallArgumentsFlowHostHelper(const std::string &text)
{
  std::vector<std::string> args;
  std::string current;
  bool in_quotes = false;

  for (size_t i = 0; i < text.size(); ++i)
  {
    const char ch = text[i];
    if (ch == '"')
    {
      in_quotes = !in_quotes;
      current.push_back(ch);
      continue;
    }

    if (ch == ',' && !in_quotes)
    {
      args.push_back(TrimFlowHostHelper(current));
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty())
    args.push_back(TrimFlowHostHelper(current));
  return args;
}

inline std::vector<std::string> SplitSemicolonListFlowHostHelper(const std::string &text)
{
  std::vector<std::string> items;
  std::string current;
  for (size_t i = 0; i < text.size(); ++i)
  {
    const char ch = text[i];
    if (ch == ';')
    {
      if (!current.empty())
        items.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  if (!current.empty())
    items.push_back(current);
  return items;
}

inline bool HasBucketGroupFlowHostHelper(const std::string &bucket_summary,
                                         const char *group_prefix)
{
  return bucket_summary.find(group_prefix) != std::string::npos;
}

inline std::string ResolveBucketGroupLabelFlowHostHelper(const std::string &bucket_summary,
                                                         const char *group_prefix,
                                                         const char *fallback)
{
  std::string current;
  for (size_t i = 0; i < bucket_summary.size(); ++i)
  {
    const char ch = bucket_summary[i];
    if (ch == ',')
    {
      const std::string token = TrimFlowHostHelper(current);
      if (!token.empty() && token.find(group_prefix) == 0)
        return token;
      current.clear();
      continue;
    }
    current.push_back(ch);
  }

  const std::string token = TrimFlowHostHelper(current);
  if (!token.empty() && token.find(group_prefix) == 0)
    return token;
  return fallback ? fallback : std::string();
}
}

inline std::string ClassifyEnsmallenInputSampleBucket(const std::string &sample_name)
{
  if (sample_name == "00041008" ||
      sample_name == "00041012" ||
      sample_name == "00041015" ||
      sample_name == "00041001" ||
      sample_name == "00041003" ||
      sample_name == "00041009")
    return "G0.baseline_stable";
  if (sample_name == "00041000" ||
      sample_name == "00041004" ||
      sample_name == "00041005" ||
      sample_name == "00041010" ||
      sample_name == "00041014" ||
      sample_name == "00041017")
    return "G1.tuning_target";
  if (sample_name == "00041136" ||
      sample_name == "13000017" ||
      sample_name == "13000080" ||
      sample_name == "20085069" ||
      sample_name == "20085083" ||
      sample_name == "20085102")
    return "G2.candidate_competition";
  if (sample_name == "00041126")
    return "G3.stress_boundary";
  if (sample_name == "13000019" ||
      sample_name == "13000020" ||
      sample_name == "13000022" ||
      sample_name == "20085221" ||
      sample_name == "20085225")
    return "G3.stress_boundary";
  if (sample_name == "template_match.template_scene" ||
      sample_name == "fit_circle.circle_double")
    return "G0.baseline_stable";
  if (sample_name == "template_match.template_scene_rotated" ||
      sample_name == "fit_ellipse.ellipse_rotated")
    return "G1.tuning_target";
  if (sample_name.find("template_match") != std::string::npos)
    return "G2.candidate_competition";
  if (sample_name.find("fit_") != std::string::npos)
    return "G3.stress_boundary";

  return "G4.pipeline_bundle";
}

inline std::string SummarizeEnsmallenInputBuckets(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay" &&
      result.layer == "scenario")
  {
    return "G0.halcon_baseline_manual,G1.geometry_fit_tuning,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }

  if (result.case_name == "halcon_screws_cluster_stability" &&
      result.layer == "train")
  {
    return "G0.halcon_baseline_manual,G1.param_tuning,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }

  if (result.case_name == "halcon_universal_joint_match_eval" &&
      result.layer == "infer")
  {
    return "G0.halcon_baseline_manual,G1.pose_variation,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }

  if (result.case_name == "halcon_pcb_focus_interaction_eval" &&
      result.layer == "infer")
  {
    return "G0.halcon_baseline_manual,G1.roi_focus,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }

  const bool is_phase1_train_or_infer_case =
    (result.case_name == "phase1_param_opt" || result.case_name == "phase1_param_eval") &&
    (result.layer == "train" || result.layer == "infer");
  const bool is_real_phase1_dataset =
    result.input_dataset == "dataset.deeppcb.phase1.ensmallen";
  const bool is_real_phase1_train_or_infer =
    is_phase1_train_or_infer_case && is_real_phase1_dataset;
  const std::string phase1_full_bucket_summary =
    "G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle";

  std::vector<std::string> buckets;
  const auto add_bucket = [&buckets](const std::string &bucket)
  {
    if (bucket.empty() || bucket == "G4.pipeline_bundle")
      return;
    for (size_t i = 0; i < buckets.size(); ++i)
    {
      if (buckets[i] == bucket)
        return;
    }
    buckets.push_back(bucket);
  };

  const auto summarize_buckets = [&buckets]() -> std::string
  {
    std::string summary;
    for (size_t i = 0; i < buckets.size(); ++i)
    {
      if (i > 0)
        summary += ",";
      summary += buckets[i];
    }
    return summary;
  };

  const auto append_stage_bucket_if_needed = [&buckets, &result]()
  {
    if (result.layer != "train" && result.layer != "infer")
      return;
    for (size_t i = 0; i < buckets.size(); ++i)
    {
      if (buckets[i] == "G4.pipeline_bundle")
        return;
    }
    buckets.push_back("G4.pipeline_bundle");
  };

  if (result.input_sample.empty())
  {
    const std::string sample_id = FindAssignmentValue(result.input_artifacts, "sample_id");
    if (!sample_id.empty())
    {
      std::string current;
      for (size_t i = 0; i < sample_id.size(); ++i)
      {
        const char ch = sample_id[i];
        if (ch == '|')
        {
          if (!current.empty())
            add_bucket(ClassifyEnsmallenInputSampleBucket(current));
          current.clear();
        }
        else
        {
          current.push_back(ch);
        }
      }
      if (!current.empty())
        add_bucket(ClassifyEnsmallenInputSampleBucket(current));
    }

    const std::string input_image = FindAssignmentValue(result.input_artifacts, "input_image");
    const std::string template_image = FindAssignmentValue(result.input_artifacts, "template_image");
    const std::string explicit_bucket_hints =
      FindAssignmentValue(result.input_artifacts, "bucket_hints");
    const std::string bucket_hints =
      input_image + "|" + template_image + "|" + explicit_bucket_hints;
    if (bucket_hints.find("G0.baseline_stable") != std::string::npos)
      add_bucket("G0.baseline_stable");
    if (bucket_hints.find("G1.tuning_target") != std::string::npos)
      add_bucket("G1.tuning_target");
    if (bucket_hints.find("G2.candidate_competition") != std::string::npos)
      add_bucket("G2.candidate_competition");
    if (bucket_hints.find("G3.stress_boundary") != std::string::npos)
      add_bucket("G3.stress_boundary");
    if (bucket_hints.find("G0.halcon_baseline_manual") != std::string::npos)
      add_bucket("G0.halcon_baseline_manual");
    if (bucket_hints.find("G1.param_tuning") != std::string::npos)
      add_bucket("G1.param_tuning");
    if (bucket_hints.find("G3.boundary_stress") != std::string::npos)
      add_bucket("G3.boundary_stress");

    if (!buckets.empty())
    {
      append_stage_bucket_if_needed();
      if (is_real_phase1_train_or_infer)
        return phase1_full_bucket_summary;
      return summarize_buckets();
    }

    if (result.case_name == "match_score_tuning")
      return "G0.baseline_stable,G1.tuning_target";
    if (result.case_name == "match_score_opt")
      return "G1.tuning_target";
    if (result.case_name == "geometry_fit_tuning")
      return "G0.baseline_stable,G1.tuning_target";
    if (result.case_name == "circle_param_opt" || result.case_name == "ellipse_param_opt")
      return "G1.tuning_target";
    if (is_real_phase1_train_or_infer)
      return phase1_full_bucket_summary;
    return "G4.pipeline_bundle";
  }

  std::vector<std::string> tokens;
  std::string current;
  for (size_t i = 0; i < result.input_sample.size(); ++i)
  {
    const char ch = result.input_sample[i];
    if (ch == ';')
    {
      if (!current.empty())
        tokens.push_back(current);
      current.clear();
    }
    else
    {
      current.push_back(ch);
    }
  }
  if (!current.empty())
    tokens.push_back(current);

  for (size_t i = 0; i < tokens.size(); ++i)
  {
    add_bucket(ClassifyEnsmallenInputSampleBucket(tokens[i]));
  }

  append_stage_bucket_if_needed();
  if (is_real_phase1_train_or_infer)
    return phase1_full_bucket_summary;
  const std::string summary = summarize_buckets();
  if (summary == "G4.pipeline_bundle" && is_real_phase1_train_or_infer)
    return phase1_full_bucket_summary;
  return summary.empty() ? "G4.pipeline_bundle" : summary;
}

inline std::string BuildEnsmallenTestFlowGuide(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "image -> circle candidate fit -> boundary_error compare -> replay_ref";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "multi-object image -> feature distance -> cluster grouping -> summary_ref";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "multi-view image -> roi alignment -> match score compare -> interaction review";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "template/test pair -> roi focus compare -> interaction review -> compare_ref";
  if (result.layer == "scenario")
    return "bucket -> replay_compare -> sample_summaries/pass_fail -> replay_ref";
  if (result.layer == "train")
    return "bucket -> batch_optimize -> best_param_sets/sample_count -> summary_ref";
  if (result.layer == "infer")
    return "bucket -> infer_compare -> baseline_metrics/delta_metrics -> compare_ref";
  if (result.case_name == "match_score_tuning" || result.case_name == "match_score_opt")
    return "bucket -> baseline_eval -> match_score_optimize -> compare -> replay";

  return "bucket -> baseline_eval -> geometry_fit_optimize -> compare -> replay";
}

inline std::string BuildEnsmallenDatasetBridgeTag(const CxScriptExecutionResult &result)
{
  if (result.input_dataset == "dataset.halcon_2605.thread_selection.ensmallen" ||
      result.dataset_ref == "dataset.halcon_2605.thread_selection.ensmallen")
  {
    return "bridge.halcon_2605_thread_selection";
  }

  if (result.input_dataset == "dataset.deeppcb.phase1.ensmallen" ||
      result.dataset_ref == "dataset.deeppcb.phase1.ensmallen")
    return "bridge.deep_pcb_template_match";
  if (result.input_dataset == "dataset.cxcore.phase1.ensmallen" ||
      result.dataset_ref == "dataset.cxcore.phase1.ensmallen")
    return "bridge.synthetic_phase1";

  const std::vector<std::string> items = detail::SplitSemicolonListFlowHostHelper(result.input_artifacts);
  std::string input_image;
  std::string template_image;
  std::string match_gt;
  for (size_t i = 0; i < items.size(); ++i)
  {
    const std::string item = detail::TrimFlowHostHelper(items[i]);
    if (item.find("input_image=") == 0)
      input_image = item.substr(std::string("input_image=").size());
    else if (item.find("template_image=") == 0)
      template_image = item.substr(std::string("template_image=").size());
    else if (item.find("match_gt=") == 0)
      match_gt = item.substr(std::string("match_gt=").size());
  }
  const std::string input_image_lower = detail::ToLowerFlowHostHelper(input_image);
  const std::string template_image_lower = detail::ToLowerFlowHostHelper(template_image);
  if (input_image_lower.find("local_test\\cximage_main_thread\\real_industrial_selection_v4_natural_only") != std::string::npos ||
      input_image_lower.find("local_test/cximage_main_thread/real_industrial_selection_v4_natural_only") != std::string::npos ||
      template_image_lower.find("local_test\\cximage_main_thread\\real_industrial_selection_v4_natural_only") != std::string::npos ||
      template_image_lower.find("local_test/cximage_main_thread/real_industrial_selection_v4_natural_only") != std::string::npos)
  {
    return "bridge.cximage_natural_only";
  }
  if (input_image_lower.find("local_test\\halcon_2605_thread_selection") != std::string::npos ||
      input_image_lower.find("local_test\\halcon_2605_texture_region_selection") != std::string::npos ||
      input_image_lower.find("local_test/halcon_2605_thread_selection") != std::string::npos ||
      input_image_lower.find("local_test/halcon_2605_texture_region_selection") != std::string::npos ||
      template_image_lower.find("local_test\\halcon_2605_thread_selection") != std::string::npos ||
      template_image_lower.find("local_test\\halcon_2605_texture_region_selection") != std::string::npos ||
      template_image_lower.find("local_test/halcon_2605_thread_selection") != std::string::npos ||
      template_image_lower.find("local_test/halcon_2605_texture_region_selection") != std::string::npos)
  {
    return "bridge.halcon_2605_thread_selection";
  }
  if (!input_image.empty() && !template_image.empty() && !match_gt.empty())
    return "bridge.deep_pcb_template_match";

  return "bridge.unknown_dataset";
}

inline std::string FindAssignmentValue(const std::string &text,
                                       const std::string &key)
{
  const std::vector<std::string> items = detail::SplitSemicolonListFlowHostHelper(text);
  const std::string prefix = key + "=";
  for (size_t i = 0; i < items.size(); ++i)
  {
    const std::string item = detail::TrimFlowHostHelper(items[i]);
    if (item.find(prefix) == 0)
      return item.substr(prefix.size());
  }
  return std::string();
}

inline std::string BuildBridgeSampleId(const CxScriptExecutionResult &result)
{
  const std::string sample_id = FindAssignmentValue(result.input_artifacts, "sample_id");
  if (!sample_id.empty())
    return sample_id;
  if (!result.input_sample.empty())
    return result.input_sample;
  return result.case_name;
}

inline std::string BuildEnsmallenConclusionStatus(const CxScriptExecutionResult &result)
{
  const std::string dataset_bridge = BuildEnsmallenDatasetBridgeTag(result);
  if (dataset_bridge == "bridge.deep_pcb_template_match" ||
      dataset_bridge == "bridge.halcon_2605_thread_selection")
    return "chain_ok,export_ok,real_image_pending_human_review";
  return "chain_ok,export_ok,algorithm_pending_human_review";
}

inline std::string BuildEnsmallenBoundaryNote(const CxScriptExecutionResult &result)
{
  const std::string dataset_bridge = BuildEnsmallenDatasetBridgeTag(result);
  if (dataset_bridge == "bridge.deep_pcb_template_match" ||
      dataset_bridge == "bridge.halcon_2605_thread_selection")
    return "real_image_observation_required_before_algorithm_conclusion";
  return "chain_and_export_verified_only_algorithm_not_auto_concluded";
}

inline std::string BuildEnsmallenImageSelectionGuide(const CxScriptExecutionResult &result)
{
  const std::string dataset_bridge = BuildEnsmallenDatasetBridgeTag(result);
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "select circle_plate source pair first then review geometry and boundary evidence";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "select screws source pair first then review roi and cluster evidence";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "select universal_joint multi-view and illumination samples first then review alignment and roi evidence";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "select pcb_focus template/test pair first then review roi focus and interaction evidence";
  if (dataset_bridge == "bridge.deep_pcb_template_match")
    return "select template/test pairs first then representative roi and gt";
  if (result.case_name == "match_score_tuning" || result.case_name == "match_score_opt")
    return "select template_scene and template_scene_rotated before candidate competition images";
  if (result.layer == "scenario" || result.layer == "train" || result.layer == "infer")
    return "select phase1 bundle with geometry and template_match samples";
  return "select baseline_stable and tuning_target geometry images first";
}

inline std::string BuildEnsmallenInteractionRoute(const CxScriptExecutionResult &result)
{
  if (result.case_name == "match_score_tuning" || result.case_name == "match_score_opt")
    return "cxcore.fastmatch -> ensmallen -> rag";
  if (BuildEnsmallenDatasetBridgeTag(result) == "bridge.halcon_2605_thread_selection")
    return "torch.optimization_refs -> cxcore.halcon_review_bundle -> ensmallen -> rag";
  if (result.layer == "scenario" || result.layer == "train" || result.layer == "infer")
    return "torch.optimization_refs -> cxcore.phase1_bundle -> ensmallen -> rag";
  return "cxcore.formfit -> ensmallen -> rag";
}

inline std::string BuildEnsmallenMcpFlow(const CxScriptExecutionResult &result)
{
  if (result.layer == "scenario" || result.layer == "train" || result.layer == "infer")
    return "run-ctest-target exact_name -> task_id -> status/log -> conclusion/evidence";
  return "run-ctest-target exact_name -> task_id -> compare/replay -> conclusion/evidence";
}

inline std::string BuildEnsmallenBatchLabel(const CxScriptExecutionResult &result)
{
  const std::string dataset_bridge = BuildEnsmallenDatasetBridgeTag(result);
  if (dataset_bridge == "bridge.halcon_2605_thread_selection")
    return "halcon_2605_thread_selection";
  if (dataset_bridge == "bridge.deep_pcb_template_match")
    return "real_world_deeppcb";
  if (result.case_name == "match_score_tuning" || result.case_name == "match_score_opt")
    return "template_match";
  if (result.layer == "scenario" || result.layer == "train" || result.layer == "infer")
    return "phase1_bundle";
  return "geometry";
}

inline std::string BuildEnsmallenLikelyIssueClass(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "alignment_interaction_drift";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "roi_focus_interaction_drift";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "candidate_ranking_instability";
  const std::string bucket = SummarizeEnsmallenInputBuckets(result);
  if (detail::HasBucketGroupFlowHostHelper(bucket, "G3."))
    return "boundary_degradation";
  if (detail::HasBucketGroupFlowHostHelper(bucket, "G2."))
    return "candidate_ranking_instability";
  if (detail::HasBucketGroupFlowHostHelper(bucket, "G1."))
    return "parameter_sensitivity";
  if (detail::HasBucketGroupFlowHostHelper(bucket, "G0."))
    return "baseline_regression_guard";
  return "pipeline_bundle_review";
}

inline std::string BuildEnsmallenRecommendedAction(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "inspect boundary_error_ref and geometry_ref then review circle candidate evidence";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "inspect threshold_ref roi_ref and cluster grouping evidence";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "inspect alignment_error_ref roi_ref and multi-view match evidence";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "inspect threshold_ref crop_policy_ref and roi focus interaction evidence";

  const std::string issue_class = BuildEnsmallenLikelyIssueClass(result);
  if (issue_class == "boundary_degradation")
    return "inspect boundary_error_ref and roi_ref then review human evidence";
  if (issue_class == "candidate_ranking_instability")
    return "inspect threshold_ref crop_policy_ref and candidate ordering evidence";
  if (issue_class == "parameter_sensitivity")
    return "compare objective_delta_ref against replay_ref and retune params";
  if (issue_class == "baseline_regression_guard")
    return "guard baseline stability and verify no regression before expansion";
  return "review bundle summaries and compare_ref before expanding dataset";
}

inline std::string BuildEnsmallenComparisonStatus(const CxScriptExecutionResult &result)
{
  if (result.objective_delta < -0.000001)
    return "improved";
  if (result.objective_delta > 0.000001)
    return "regressed";
  return "flat";
}

inline std::string BuildEnsmallenComparisonMagnitude(const CxScriptExecutionResult &result)
{
  const double abs_delta =
    result.objective_delta < 0.0 ? -result.objective_delta : result.objective_delta;
  if (abs_delta >= 0.100000)
    return "major";
  if (abs_delta >= 0.020000)
    return "moderate";
  if (abs_delta > 0.000001)
    return "minor";
  return "none";
}

inline std::string BuildEnsmallenNextBucketFocus(const CxScriptExecutionResult &result)
{
  const std::string issue_class = BuildEnsmallenLikelyIssueClass(result);
  if (issue_class == "alignment_interaction_drift")
    return detail::ResolveBucketGroupLabelFlowHostHelper(SummarizeEnsmallenInputBuckets(result),
                                                         "G3.",
                                                         "G3.boundary_stress");
  if (issue_class == "roi_focus_interaction_drift")
    return detail::ResolveBucketGroupLabelFlowHostHelper(SummarizeEnsmallenInputBuckets(result),
                                                         "G1.",
                                                         "G1.roi_focus");
  if (issue_class == "boundary_degradation")
    return detail::ResolveBucketGroupLabelFlowHostHelper(SummarizeEnsmallenInputBuckets(result),
                                                         "G3.",
                                                         "G3.stress_boundary");
  if (issue_class == "candidate_ranking_instability")
    return detail::ResolveBucketGroupLabelFlowHostHelper(SummarizeEnsmallenInputBuckets(result),
                                                         "G2.",
                                                         "G2.candidate_competition");
  if (issue_class == "parameter_sensitivity")
    return detail::ResolveBucketGroupLabelFlowHostHelper(SummarizeEnsmallenInputBuckets(result),
                                                         "G1.",
                                                         "G1.tuning_target");
  if (issue_class == "baseline_regression_guard")
    return detail::ResolveBucketGroupLabelFlowHostHelper(SummarizeEnsmallenInputBuckets(result),
                                                         "G0.",
                                                         "G0.baseline_stable");
  return "G4.pipeline_bundle";
}

inline std::string BuildEnsmallenObservationMode(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "geometry_fit_stability";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "cluster_stability_only";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "match_score_and_interaction";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "layer_interaction_eval";
  if (result.layer == "scenario")
    return "bundle_replay_compare";
  if (result.layer == "train")
    return "batch_param_stability";
  if (result.layer == "infer")
    return "baseline_vs_optimized_validation";
  return "single_case_tuning";
}

inline std::string BuildEnsmallenExpansionGate(const CxScriptExecutionResult &result)
{
  const std::string comparison_status = BuildEnsmallenComparisonStatus(result);
  const std::string issue_class = BuildEnsmallenLikelyIssueClass(result);
  if (comparison_status == "regressed")
    return "hold_expand_fix_regression";
  if (comparison_status == "flat")
    return "hold_expand_collect_more_evidence";
  if (issue_class == "baseline_regression_guard")
    return "hold_expand_verify_baseline";
  return "expand_next_bucket";
}

inline std::string BuildEnsmallenBucketCoverage(const CxScriptExecutionResult &result)
{
  const std::string bucket = SummarizeEnsmallenInputBuckets(result);
  const bool has_g0 = detail::HasBucketGroupFlowHostHelper(bucket, "G0.");
  const bool has_g1 = detail::HasBucketGroupFlowHostHelper(bucket, "G1.");
  const bool has_g2 = detail::HasBucketGroupFlowHostHelper(bucket, "G2.");
  const bool has_g3 = detail::HasBucketGroupFlowHostHelper(bucket, "G3.");
  const bool has_g4 = detail::HasBucketGroupFlowHostHelper(bucket, "G4.");
  if (result.case_name == "halcon_universal_joint_match_eval" &&
      !(has_g0 && has_g1 && has_g2 && has_g3 && has_g4))
    return "halcon_match_interaction_slice_coverage";
  if (result.case_name == "halcon_pcb_focus_interaction_eval" &&
      !(has_g0 && has_g1 && has_g2 && has_g3 && has_g4))
    return "halcon_interaction_slice_coverage";
  if (has_g0 && has_g1 && has_g2 && has_g3 && has_g4)
    return "full_phase1_bucket_coverage";
  if (has_g0 && has_g1 && has_g3 && !has_g2)
    return "halcon_geometry_fit_slice_coverage";
  if (has_g0 && has_g2 && has_g4 && !has_g1)
    return "halcon_cluster_stability_slice_coverage";
  if (has_g0 && has_g1 && has_g2 && has_g3)
    return "real_match_bucket_coverage";
  if (has_g0 && has_g1)
    return "baseline_tuning_coverage";
  return "single_bucket_coverage";
}

inline std::string BuildEnsmallenRiskAxis(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "boundary_and_geometry";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "cluster_grouping";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "alignment_and_interaction";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "roi_focus_and_interaction";

  const std::string issue_class = BuildEnsmallenLikelyIssueClass(result);
  if (issue_class == "boundary_degradation")
    return "boundary_and_roi";
  if (issue_class == "candidate_ranking_instability")
    return "candidate_ordering";
  if (issue_class == "parameter_sensitivity")
    return "threshold_and_params";
  if (issue_class == "baseline_regression_guard")
    return "baseline_regression";
  return "bundle_aggregation";
}

inline std::string BuildEnsmallenCoverageGap(const CxScriptExecutionResult &result)
{
  const std::string coverage = BuildEnsmallenBucketCoverage(result);
  if (coverage == "halcon_match_interaction_slice_coverage")
    return "missing_G0_manual_guard";
  if (coverage == "halcon_interaction_slice_coverage")
    return "missing_G0_and_G3";
  if (coverage == "full_phase1_bucket_coverage")
    return "no_coverage_gap";
  if (coverage == "halcon_geometry_fit_slice_coverage")
    return "missing_G2_and_G4";
  if (coverage == "halcon_cluster_stability_slice_coverage")
    return "missing_G1_and_G3";
  if (coverage == "real_match_bucket_coverage")
    return "missing_G4_pipeline_bundle";
  if (coverage == "baseline_tuning_coverage")
    return "missing_G2_G3_G4";
  return "limited_bucket_evidence";
}

inline std::string BuildEnsmallenObservationPriority(const CxScriptExecutionResult &result)
{
  const std::string risk_axis = BuildEnsmallenRiskAxis(result);
  if (risk_axis == "alignment_and_interaction")
    return "prioritize_alignment_interaction_review";
  if (risk_axis == "roi_focus_and_interaction")
    return "prioritize_roi_focus_interaction_review";
  if (risk_axis == "boundary_and_geometry")
    return "prioritize_boundary_geometry_review";
  if (risk_axis == "cluster_grouping")
    return "prioritize_cluster_grouping_review";
  if (risk_axis == "boundary_and_roi")
    return "prioritize_boundary_roi_review";
  if (risk_axis == "candidate_ordering")
    return "prioritize_candidate_ordering_review";
  if (risk_axis == "threshold_and_params")
    return "prioritize_threshold_param_review";
  if (risk_axis == "baseline_regression")
    return "prioritize_baseline_guard_review";
  return "prioritize_bundle_review";
}

inline std::string BuildEnsmallenCoverageStatus(const CxScriptExecutionResult &result)
{
  const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
  if (coverage_gap == "missing_G0_manual_guard")
    return "match_interaction_ready_manual_guard_pending";
  if (coverage_gap == "missing_G0_and_G3")
    return "interaction_ready_manual_guard_and_boundary_pending";
  if (coverage_gap == "no_coverage_gap")
    return "coverage_ready_for_deeper_observation";
  if (coverage_gap == "missing_G2_and_G4")
    return "geometry_fit_ready_competition_and_bundle_pending";
  if (coverage_gap == "missing_G1_and_G3")
    return "cluster_ready_tuning_and_boundary_pending";
  if (coverage_gap == "missing_G4_pipeline_bundle")
    return "real_match_ready_pipeline_bundle_pending";
  if (coverage_gap == "missing_G2_G3_G4")
    return "baseline_ready_stress_and_bundle_pending";
  return "coverage_not_ready";
}

inline std::string BuildEnsmallenNextReviewAction(const CxScriptExecutionResult &result)
{
  const std::string risk_axis = BuildEnsmallenRiskAxis(result);
  const std::string next_bucket_focus = BuildEnsmallenNextBucketFocus(result);
  if (risk_axis == "alignment_and_interaction" &&
      next_bucket_focus.find("G3.") == 0)
    return "review_G3_alignment_interaction_cases";
  if (risk_axis == "roi_focus_and_interaction" &&
      next_bucket_focus.find("G1.") == 0)
    return "review_G1_roi_focus_interaction_cases";
  if (risk_axis == "boundary_and_geometry" &&
      next_bucket_focus.find("G3.") == 0)
    return "review_G3_boundary_geometry_cases";
  if (risk_axis == "cluster_grouping" &&
      next_bucket_focus.find("G2.") == 0)
    return "review_G2_cluster_grouping_cases";
  if (risk_axis == "boundary_and_roi" &&
      next_bucket_focus.find("G3.") == 0)
    return "review_G3_boundary_roi_cases";
  if (risk_axis == "candidate_ordering" &&
      next_bucket_focus.find("G2.") == 0)
    return "review_G2_candidate_ordering_cases";
  if (risk_axis == "threshold_and_params" &&
      next_bucket_focus.find("G1.") == 0)
    return "review_G1_threshold_param_cases";
  if (risk_axis == "baseline_regression" &&
      next_bucket_focus.find("G0.") == 0)
    return "review_G0_baseline_guard_cases";
  return "review_G4_pipeline_bundle_cases";
}

inline std::string BuildEnsmallenOptimizationSignal(const CxScriptExecutionResult &result)
{
  const std::string comparison_status = BuildEnsmallenComparisonStatus(result);
  const std::string comparison_magnitude = BuildEnsmallenComparisonMagnitude(result);
  const std::string risk_axis = BuildEnsmallenRiskAxis(result);
  return comparison_status + "_" + comparison_magnitude + "_" + risk_axis;
}

inline std::string BuildEnsmallenBucketReviewTemplate(const CxScriptExecutionResult &)
{
  return "G0=baseline_guard;G1=param_tuning;G2=candidate_ordering;G3=boundary_roi;G4=pipeline_bundle";
}

inline std::string BuildEnsmallenCaseBucketReviewTemplate(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay" ||
      result.case_name == "halcon_universal_joint_match_eval" ||
      result.case_name == "halcon_pcb_focus_interaction_eval")
  {
    return "G0=manual_guard;G1=geometry_fit;G2=candidate_competition;G3=boundary_stress;G4=pipeline_bundle";
  }
  if (result.case_name == "halcon_screws_cluster_stability")
    return "G0=manual_guard;G1=param_tuning;G2=candidate_competition;G3=boundary_stress;G4=pipeline_bundle";
  return "G0=baseline_guard;G1=param_tuning;G2=candidate_ordering;G3=boundary_roi;G4=pipeline_bundle";
}

inline std::string BuildEnsmallenReviewScope(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "scenario_geometry_fit_compare";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "train_cluster_stability_review";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "infer_match_score_compare";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "infer_interaction_compare";
  if (result.layer == "scenario")
    return "scenario_bundle_replay_compare";
  if (result.layer == "train")
    return "train_batch_best_param_review";
  if (result.layer == "infer")
    return "infer_baseline_vs_optimized_review";
  if (result.case_name == "match_score_tuning" || result.case_name == "match_score_opt")
    return "feature_match_score_compare";
  return "feature_geometry_fit_compare";
}

inline std::string BuildEnsmallenConclusionId(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "ensmallen.halcon.circle_plate.geometry_fit";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "ensmallen.halcon.screws.cluster_stability";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "ensmallen.halcon.universal_joint.match_score";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "ensmallen.halcon.pcb_focus.interaction";
  return result.task_id.empty()
           ? (result.module + "." + result.layer + "." + result.case_name)
           : result.task_id;
}

inline std::string BuildEnsmallenShortConclusion(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "circle_plate works as a stable geometry-fit and boundary observation slice.";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "screws works as a multi-candidate cluster-stability observation slice.";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "universal_joint_part works as a cross-view match-score and interaction slice.";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "pcb_focus works as an ROI-focus and interaction observation slice.";
  if (result.layer == "scenario")
    return "scenario replay/compare result is ready for human review.";
  if (result.layer == "train")
    return "train best-param result is ready for human review.";
  if (result.layer == "infer")
    return "infer compare result is ready for human review.";
  return "ensmallen tuning result is ready for review.";
}

inline std::string BuildEnsmallenWhyItMatters(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "Repeated circle structure exposes boundary error and candidate ordering sensitivity.";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "Repeated small-part structures expose clustering and candidate competition stability.";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "Cross-view and illumination drift expose alignment, ROI, and interaction sensitivity together.";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "Focus drift exposes threshold, crop policy, and ROI interaction sensitivity together.";
  return "This result can be reused as a stable review observation for later tuning replay.";
}

inline std::string BuildEnsmallenNextObservation(const CxScriptExecutionResult &result)
{
  if (result.case_name == "halcon_circle_plate_geometry_replay")
    return "Review boundary_error_ref and geometry_ref before expanding to G2 and G4.";
  if (result.case_name == "halcon_screws_cluster_stability")
    return "Review cluster grouping evidence and best_params_ref reuse before adding G1 and G3.";
  if (result.case_name == "halcon_universal_joint_match_eval")
    return "Add G0 manual-guard evidence, then compare alignment_error_ref across camera views.";
  if (result.case_name == "halcon_pcb_focus_interaction_eval")
    return "Add G0 and G3 evidence, then compare threshold_ref and crop_policy_ref under focus drift.";
  return "Continue with the next bucket and keep compare/replay refs stable for RAG writeback.";
}

inline std::string BuildEnsmallenFallbackReviewRefPrefixFromScript(const CxScriptExecutionResult &result)
{
  const std::string script_hint =
    !result.script_name.empty() ? result.script_name : result.script_path;
  if (script_hint.find("halcon_circle_plate_geometry_replay") != std::string::npos)
    return "ensmallen_layer.scenario.halcon_circle_plate_geometry_replay";
  if (script_hint.find("halcon_screws_cluster_stability") != std::string::npos)
    return "ensmallen_layer.train.halcon_screws_cluster_stability";
  if (script_hint.find("halcon_universal_joint_match_eval") != std::string::npos)
    return "ensmallen_layer.infer.halcon_universal_joint_match_eval";
  if (script_hint.find("halcon_pcb_focus_interaction_eval") != std::string::npos)
    return "ensmallen_layer.infer.halcon_pcb_focus_interaction_eval";
  if (script_hint.find("phase1_param_replay") != std::string::npos)
    return "ensmallen_layer.scenario.phase1_param_replay";
  if (script_hint.find("phase1_param_opt") != std::string::npos)
    return "ensmallen_layer.train.phase1_param_opt";
  if (script_hint.find("phase1_param_eval") != std::string::npos)
    return "ensmallen_layer.infer.phase1_param_eval";
  if (script_hint.find("match_score_tuning") != std::string::npos)
    return "ensmallen_layer.feature.match_score_tuning";
  if (script_hint.find("match_score_opt") != std::string::npos)
    return "ensmallen_layer.feature.match_score_opt";
  if (script_hint.find("geometry_fit_tuning") != std::string::npos)
    return "ensmallen_layer.feature.geometry_fit_tuning";
  if (script_hint.find("circle_param_opt") != std::string::npos)
    return "ensmallen_layer.feature.circle_param_opt";
  if (script_hint.find("ellipse_param_opt") != std::string::npos)
    return "ensmallen_layer.feature.ellipse_param_opt";
  return std::string();
}

inline std::string BuildEnsmallenPrimaryReviewRef(const CxScriptExecutionResult &result)
{
  std::string fallback_review_ref_prefix;
  if (!result.module.empty() &&
      !result.layer.empty() &&
      !result.case_name.empty())
  {
    fallback_review_ref_prefix =
      result.module + "." + result.layer + "." + result.case_name;
  }
  else if (!result.task_id.empty())
  {
    fallback_review_ref_prefix = result.task_id;
  }
  else
  {
    fallback_review_ref_prefix =
      BuildEnsmallenFallbackReviewRefPrefixFromScript(result);
    if (fallback_review_ref_prefix.empty())
      fallback_review_ref_prefix = "ensmallen_layer";
  }

  return BuildEnsmallenPrimaryReviewRef(result, fallback_review_ref_prefix);
}

inline std::string BuildEnsmallenPrimaryReviewRef(const CxScriptExecutionResult &result,
                                                  const std::string &fallback_review_ref_prefix)
{
  if (result.layer == "train")
    return result.summary_ref.empty() ? fallback_review_ref_prefix + ".summary" : result.summary_ref;
  if (result.layer == "scenario" || result.layer == "infer")
    return result.compare_ref.empty() ? fallback_review_ref_prefix + ".compare" : result.compare_ref;
  if (!result.objective_delta_ref.empty())
    return result.objective_delta_ref;
  return fallback_review_ref_prefix + ".objective_delta";
}

inline std::string NormalizeFlowHostCaseName(const std::string &value)
{
  std::vector<std::string> parts;
  std::string current;
  for (size_t i = 0; i < value.size(); ++i)
  {
    if (value[i] == '.')
    {
      if (!current.empty())
        parts.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(value[i]);
  }
  if (!current.empty())
    parts.push_back(current);

  if (parts.size() >= 4)
    return parts[2];
  return value;
}

inline bool TryParseFlowHostCall(const std::string &trimmed,
                                 std::string &method_name,
                                 std::vector<std::string> &args)
{
  method_name.clear();
  args.clear();

  const std::string text = detail::StripTrailingSemicolonFlowHostHelper(trimmed);
  bool is_prefixed_flow_call = detail::StartsWithFlowHostHelper(text, "flow.");
  bool is_plain_flow_input =
    detail::StartsWithFlowHostHelper(text, "input_dataset(") ||
    detail::StartsWithFlowHostHelper(text, "input_split(") ||
    detail::StartsWithFlowHostHelper(text, "input_sample(") ||
    detail::StartsWithFlowHostHelper(text, "input_artifact(") ||
    detail::StartsWithFlowHostHelper(text, "input_param(");
  if (!is_prefixed_flow_call && !is_plain_flow_input)
    return false;

  const size_t begin = is_prefixed_flow_call ? text.find('.', 0) : std::string::npos;
  const size_t open = text.find('(', is_prefixed_flow_call ? begin + 1 : 0);
  const size_t close = text.rfind(')');
  if ((is_prefixed_flow_call && begin == std::string::npos) ||
      open == std::string::npos ||
      close == std::string::npos ||
      close <= open)
    return false;

  if (is_prefixed_flow_call)
    method_name = detail::TrimFlowHostHelper(text.substr(begin + 1, open - begin - 1));
  else
    method_name = detail::TrimFlowHostHelper(text.substr(0, open));
  if (method_name.empty())
    return false;

  args = detail::SplitCallArgumentsFlowHostHelper(text.substr(open + 1, close - open - 1));
  for (size_t i = 0; i < args.size(); ++i)
    args[i] = detail::StripQuotesFlowHostHelper(detail::TrimFlowHostHelper(args[i]));
  return true;
}

inline std::string NormalizeEnsmallenDatasetAlias(const std::string &value)
{
  const std::string trimmed = detail::TrimFlowHostHelper(value);
  if (trimmed.empty())
    return std::string();

  if (trimmed == "dataset.deeppcb.phase1.ensmallen" ||
      trimmed == "dataset.deep_pcb.phase1.ensmallen" ||
      trimmed == "deeppcb.phase1")
    return "dataset.deeppcb.phase1.ensmallen";

  if (trimmed == "dataset.cxcore.phase1.ensmallen" ||
      trimmed == "dataset.phase1.ensmallen" ||
      trimmed == "synthetic.phase1")
    return "dataset.cxcore.phase1.ensmallen";

  if (trimmed == "dataset.halcon_2605.thread_selection.ensmallen")
    return "dataset.halcon_2605.thread_selection.ensmallen";

  return trimmed;
}

inline std::string JoinStrings(const std::vector<std::string> &items, const char *separator)
{
  std::string joined;
  for (size_t i = 0; i < items.size(); ++i)
  {
    if (i > 0)
      joined += separator;
    joined += items[i];
  }
  return joined;
}

inline void CollectFlowHostInputs(const CxScriptFlow &flow,
                                  ParserTestRequest &request)
{
  for (size_t i = 0; i < flow.statements.size(); ++i)
  {
    const CxScriptStatement &stmt = flow.statements[i];
    std::string method_name;
    std::vector<std::string> args;
    if (!TryParseFlowHostCall(stmt.text, method_name, args))
      continue;

    if (method_name == "input_dataset" && !args.empty())
    {
      request.input_dataset = NormalizeEnsmallenDatasetAlias(args[0]);
      continue;
    }

    if (method_name == "input_split" && !args.empty())
    {
      request.input_split = args[0];
      continue;
    }

    if (method_name == "input_sample" && !args.empty())
    {
      request.input_samples.push_back(args[0]);
      continue;
    }

    if (method_name == "input_artifact" && args.size() >= 2)
    {
      request.input_artifacts.push_back(args[0] + "=" + args[1]);
      continue;
    }

    if (method_name == "input_param" && args.size() >= 2)
    {
      request.input_params.push_back(args[0] + "=" + args[1]);
      continue;
    }
  }
}

inline bool TryParseLegacyFlowCall(const std::string &trimmed,
                                   std::string &method_name,
                                   std::vector<std::string> &args)
{
  method_name.clear();
  args.clear();

  const std::string text = detail::StripTrailingSemicolonFlowHostHelper(trimmed);
  if (!detail::StartsWithFlowHostHelper(text, "Flow."))
    return false;

  const size_t begin = text.find('.', 0);
  const size_t open = text.find('(', begin + 1);
  const size_t close = text.rfind(')');
  if (begin == std::string::npos || open == std::string::npos ||
      close == std::string::npos || close < open)
    return false;

  method_name = detail::TrimFlowHostHelper(text.substr(begin + 1, open - begin - 1));
  if (method_name.empty())
    return false;

  if (close > open + 1)
  {
    args = detail::SplitCallArgumentsFlowHostHelper(text.substr(open + 1, close - open - 1));
    for (size_t i = 0; i < args.size(); ++i)
      args[i] = detail::StripQuotesFlowHostHelper(detail::TrimFlowHostHelper(args[i]));
  }
  return true;
}
}
}

#endif
