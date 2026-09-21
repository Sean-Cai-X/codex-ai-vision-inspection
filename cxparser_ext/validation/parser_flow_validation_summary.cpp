#include "parser_flow_validation_summary.h"

#include "../catalog/parser_cxscript_fragment_catalog.h"

namespace cxparser_ext
{
namespace
{
bool Contains(const std::string &text, const char *token)
{
  return text.find(token) != std::string::npos;
}

void PushUnique(std::vector<std::string> &values, const std::string &value)
{
  for (std::size_t index = 0; index < values.size(); ++index)
  {
    if (values[index] == value)
    {
      return;
    }
  }

  values.push_back(value);
}

void CollectFragments(const FlowScriptPlan &plan,
                      FlowValidationSummary &summary,
                      const std::vector<CxscriptCapabilityFragment> &fragments)
{
  if (plan.script.module == "cxgeom" || plan.script.module == "cxcloud" || plan.script.module == "cxcore")
  {
    PushUnique(summary.fragment_ids, "cxscript.operator.image_prepare_basic_roi");
  }

  for (std::size_t index = 0; index < plan.steps.size(); ++index)
  {
    const std::string &entry = plan.steps[index].entry_name;
    if (Contains(entry, "cxgeom.bulk_"))
    {
      PushUnique(summary.fragment_ids, "cxscript.feature.line_measure_roi");
      PushUnique(summary.fragment_ids, "cxscript.feature.circle_measure_fit");
      PushUnique(summary.fragment_ids, "cxscript.feature.ellipse_measure_roi");
    }

    if (Contains(entry, "cxcloud.bulk_"))
    {
      PushUnique(summary.fragment_ids, "cxscript.operator.image_prepare_basic_roi");
    }

    if (Contains(entry, "refresh_decide") || Contains(entry, "drag_"))
    {
      PushUnique(summary.fragment_ids, "cxscript.feature.line_measure_roi");
    }

    if (Contains(entry, "publish") || Contains(entry, "map_item_to_scene"))
    {
      if (plan.script.module == "cxgeom")
      {
        PushUnique(summary.fragment_ids, "cxscript.feature.line_measure_roi");
        PushUnique(summary.fragment_ids, "cxscript.feature.circle_measure_fit");
        PushUnique(summary.fragment_ids, "cxscript.feature.ellipse_measure_roi");
      }
      if (plan.script.module == "cxcloud")
      {
        PushUnique(summary.fragment_ids, "cxscript.matcher.fast_template_match");
      }
    }
  }

  if (plan.script.layer == "train")
  {
    PushUnique(summary.fragment_ids, "cxscript.embedded_model.mobilevit_mainline");
  }

  if (plan.script.layer == "infer")
  {
    PushUnique(summary.fragment_ids, "cxscript.embedded_model.segmentation_mainline");
  }
}

void DetermineBundles(const FlowScriptPlan &plan,
                      FlowValidationSummary &summary,
                      const std::vector<CxscriptCapabilityFragment> &fragments)
{
  std::vector<CxscriptFlowFragmentBundle> bundles;
  if (!BuildCxscriptFlowFragmentBundles(fragments, bundles))
  {
    return;
  }

  for (std::size_t bundle_index = 0; bundle_index < bundles.size(); ++bundle_index)
  {
    const CxscriptFlowFragmentBundle &bundle = bundles[bundle_index];
    bool satisfied = true;
    for (std::size_t fragment_index = 0;
         fragment_index < bundle.fragment_ids.size();
         ++fragment_index)
    {
      const std::string &fragment_id = bundle.fragment_ids[fragment_index];
      bool found = false;
      for (std::size_t summary_index = 0;
           summary_index < summary.fragment_ids.size();
           ++summary_index)
      {
        if (summary.fragment_ids[summary_index] == fragment_id)
        {
          found = true;
          break;
        }
      }

      if (!found)
      {
        satisfied = false;
        break;
      }
    }

    if (satisfied)
    {
      PushUnique(summary.bundle_ids, bundle.bundle_id);
    }
  }
}

void DetermineWorkflowPathGroups(FlowValidationSummary &summary)
{
  for (std::size_t index = 0; index < summary.bundle_ids.size(); ++index)
  {
    const std::string &bundle_id = summary.bundle_ids[index];
    if (Contains(bundle_id, "image_to_line_feature") ||
        Contains(bundle_id, "image_to_circle_feature") ||
        Contains(bundle_id, "image_to_ellipse_feature"))
    {
      PushUnique(summary.workflow_path_groups, "geometry_feature_path");
    }
    else if (Contains(bundle_id, "image_to_fast_match"))
    {
      PushUnique(summary.workflow_path_groups, "matcher_path");
    }
    else if (Contains(bundle_id, "embedded_model"))
    {
      PushUnique(summary.workflow_path_groups, "embedded_model_path");
    }
    else
    {
      PushUnique(summary.workflow_path_groups, "generic_bundle_path");
    }
  }
}
}

bool BuildFlowValidationSummary(const FlowScriptPlan &plan,
                                FlowValidationSummary &summary)
{
  summary = FlowValidationSummary();
  summary.flow_id = plan.script.flow_id;
  summary.layer = plan.script.layer;
  summary.module = plan.script.module;
  summary.input_count = static_cast<int>(plan.inputs.size());
  summary.step_count = static_cast<int>(plan.steps.size());
  summary.check_count = static_cast<int>(plan.checks.size());
  summary.output_count = static_cast<int>(plan.outputs.size());

  std::vector<CxscriptCapabilityFragment> fragments;
  if (!BuildCxscriptFragmentCatalog(fragments))
  {
    return false;
  }

  for (std::size_t index = 0; index < plan.steps.size(); ++index)
  {
    const std::string &entry = plan.steps[index].entry_name;
    summary.requires_geometry_bulk =
      summary.requires_geometry_bulk || Contains(entry, "cxgeom.bulk_");
    summary.requires_cloud_bulk =
      summary.requires_cloud_bulk || Contains(entry, "cxcloud.bulk_");
    summary.requires_scene_mapping =
      summary.requires_scene_mapping || Contains(entry, "map_item_to_scene");
    summary.requires_refresh_watch =
      summary.requires_refresh_watch || Contains(entry, "refresh_decide") || Contains(entry, "drag_");
    summary.requires_latency_watch =
      summary.requires_latency_watch || Contains(entry, "collect_latency_trace");
    summary.publish_path_involved =
      summary.publish_path_involved || Contains(entry, "publish") || Contains(entry, "map_item_to_scene");
  }

  CollectFragments(plan, summary, fragments);
  DetermineBundles(plan, summary, fragments);
  DetermineWorkflowPathGroups(summary);

  if (summary.input_count == 0)
  {
    summary.blocker_reason = "flow script does not declare inputs";
    summary.ready_for_current_module_validation = false;
    return true;
  }

  if (summary.step_count == 0)
  {
    summary.blocker_reason = "flow script does not declare executable steps";
    summary.ready_for_current_module_validation = false;
    return true;
  }

  if (summary.check_count == 0)
  {
    summary.blocker_reason = "flow script does not declare validation checks";
    summary.ready_for_current_module_validation = false;
    return true;
  }

  summary.ready_for_current_module_validation = true;
  return true;
}
}
