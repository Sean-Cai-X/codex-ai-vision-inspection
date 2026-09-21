#include "parser_cximage_lowlevel_catalog.h"

#include <sstream>

namespace cxparser_ext
{
namespace
{
void PushUnique(std::vector<std::string> &values, const std::string &value)
{
  for (size_t index = 0; index < values.size(); ++index)
  {
    if (values[index] == value)
      return;
  }
  values.push_back(value);
}

CximageLowLevelCapability MakeImagePrepareCapability()
{
  CximageLowLevelCapability capability;
  capability.capability_id = "cximage.operator.image_prepare_basic_roi";
  capability.category = "operator";
  capability.summary =
      "Load an image, normalize size or channel layout, crop ROI, and prepare "
      "a stable grayscale or threshold-ready working image for downstream use.";
  capability.source_files.push_back("cximage/imagebase.cpp");
  capability.source_files.push_back("cximage/imageroi.cpp");
  capability.lightweight_inputs.push_back("image_path or image_view");
  capability.lightweight_inputs.push_back("optional roi_rect");
  capability.lightweight_inputs.push_back("optional resize_wh and threshold");
  capability.lightweight_outputs.push_back("prepared_image_view");
  capability.lightweight_outputs.push_back("image_meta");
  capability.reusable_fragments.push_back("image_prepare_basic_roi");
  capability.keep_in_low_level_reasons.push_back(
      "pixel layout conversion and OpenCV/Qt image interop should remain hidden");
  capability.keep_in_low_level_reasons.push_back(
      "threshold and resize details are implementation choices, not script concerns");
  capability.script_friendly = true;
  capability.recommended_for_fragment = true;
  return capability;
}

CximageLowLevelCapability MakeLineMeasureCapability()
{
  CximageLowLevelCapability capability;
  capability.capability_id = "cximage.feature.line_measure_roi";
  capability.category = "feature";
  capability.summary =
      "Measure line evidence inside an ROI and return lightweight sampled point "
      "sets plus measurement bounds.";
  capability.source_files.push_back("cximage/findline.cpp");
  capability.source_files.push_back("cximage/shape.cpp");
  capability.lightweight_inputs.push_back("prepared_image_view");
  capability.lightweight_inputs.push_back("line_roi_rect or line_seed");
  capability.lightweight_inputs.push_back("line_gap, threshold, method");
  capability.lightweight_outputs.push_back("line_point_set");
  capability.lightweight_outputs.push_back("line_measure_bounds");
  capability.reusable_fragments.push_back("line_measure_roi");
  capability.keep_in_low_level_reasons.push_back(
      "sampling density, edge polarity, and balanced-measure heuristics should stay internal");
  capability.script_friendly = true;
  capability.recommended_for_fragment = true;
  return capability;
}

CximageLowLevelCapability MakeCircleMeasureCapability()
{
  CximageLowLevelCapability capability;
  capability.capability_id = "cximage.feature.circle_measure_fit";
  capability.category = "feature";
  capability.summary =
      "Measure radial evidence from a seed circle and return fitted center, "
      "radius, average distance, and sample points.";
  capability.source_files.push_back("cximage/findcircle.cpp");
  capability.lightweight_inputs.push_back("prepared_image_view");
  capability.lightweight_inputs.push_back("circle_seed(center, axis_point)");
  capability.lightweight_inputs.push_back("gap, threshold, method");
  capability.lightweight_outputs.push_back("circle_summary");
  capability.lightweight_outputs.push_back("circle_sample_points");
  capability.reusable_fragments.push_back("circle_measure_fit");
  capability.keep_in_low_level_reasons.push_back(
      "fallback probing and fit retry strategy should remain internal");
  capability.script_friendly = true;
  capability.recommended_for_fragment = true;
  return capability;
}

CximageLowLevelCapability MakeEllipseMeasureCapability()
{
  CximageLowLevelCapability capability;
  capability.capability_id = "cximage.feature.ellipse_measure_roi";
  capability.category = "feature";
  capability.summary =
      "Measure ellipse evidence from a seed ellipse and return sampled points "
      "with lightweight bounds for downstream reconstruction.";
  capability.source_files.push_back("cximage/findellipse.cpp");
  capability.lightweight_inputs.push_back("prepared_image_view");
  capability.lightweight_inputs.push_back("ellipse_seed(center, axis_point)");
  capability.lightweight_inputs.push_back("gap, threshold, method");
  capability.lightweight_outputs.push_back("ellipse_point_set");
  capability.lightweight_outputs.push_back("ellipse_measure_bounds");
  capability.reusable_fragments.push_back("ellipse_measure_roi");
  capability.keep_in_low_level_reasons.push_back(
      "ellipse stepping, angular sampling, and local search details should stay hidden");
  capability.script_friendly = true;
  capability.recommended_for_fragment = true;
  return capability;
}

CximageLowLevelCapability MakeTemplateMatchCapability()
{
  CximageLowLevelCapability capability;
  capability.capability_id = "cximage.matcher.fast_template_match";
  capability.category = "matcher";
  capability.summary =
      "Learn or load a template model, run ROI-guided matching, and emit "
      "candidate boxes, centers, and score summary.";
  capability.source_files.push_back("cximage/fastmatch.cpp");
  capability.source_files.push_back("cximage/grid.cpp");
  capability.lightweight_inputs.push_back("prepared_image_view");
  capability.lightweight_inputs.push_back("model_image_view or model_ref");
  capability.lightweight_inputs.push_back("match_roi, grid, thresholds");
  capability.lightweight_outputs.push_back("match_candidates");
  capability.lightweight_outputs.push_back("match_summary");
  capability.reusable_fragments.push_back("fast_template_match");
  capability.keep_in_low_level_reasons.push_back(
      "search grid, compare gap, rotation policy, and fallback matching should remain internal");
  capability.script_friendly = true;
  capability.recommended_for_fragment = true;
  return capability;
}

CximageLowLevelCapability MakeRegionDetectCapability()
{
  CximageLowLevelCapability capability;
  capability.capability_id = "cximage.matcher.findobject_region";
  capability.category = "matcher";
  capability.summary =
      "Detect connected foreground regions or object-like blobs and return "
      "lightweight boxes and component counts.";
  capability.source_files.push_back("cximage/findobject.cpp");
  capability.source_files.push_back("cximage/ObjectAnalysisExA.cpp");
  capability.lightweight_inputs.push_back("prepared_image_view");
  capability.lightweight_inputs.push_back("roi_rect or region_seed");
  capability.lightweight_outputs.push_back("region_boxes");
  capability.lightweight_outputs.push_back("region_summary");
  capability.keep_in_low_level_reasons.push_back(
      "connected-component tuning, morphology, and region filtering should stay internal");
  capability.script_friendly = true;
  capability.recommended_for_fragment = false;
  return capability;
}

CximageLowLevelCapability MakeEmbeddedModelCapability()
{
  CximageLowLevelCapability capability;
  capability.capability_id = "cximage.embedded_model.image_model_runner";
  capability.category = "embedded_model";
  capability.summary =
      "Provide lightweight runner config and image batch handoff material for "
      "small embedded image-model sessions without exposing trainer internals.";
  capability.source_files.push_back("libtorch_module/torch_minimal_smoke.cpp");
  capability.lightweight_inputs.push_back("image_batch_view");
  capability.lightweight_inputs.push_back("runner_config_ref");
  capability.lightweight_outputs.push_back("model_session_summary");
  capability.lightweight_outputs.push_back("embedding_or_mask_packet");
  capability.keep_in_low_level_reasons.push_back(
      "tensor layout normalization, batching, and device/runtime details should remain below scripts");
  capability.script_friendly = false;
  capability.recommended_for_fragment = false;
  return capability;
}
}

bool BuildCximageLowLevelCapabilityCatalog(
    std::vector<CximageLowLevelCapability> &capabilities)
{
  capabilities.clear();
  capabilities.push_back(MakeImagePrepareCapability());
  capabilities.push_back(MakeLineMeasureCapability());
  capabilities.push_back(MakeCircleMeasureCapability());
  capabilities.push_back(MakeEllipseMeasureCapability());
  capabilities.push_back(MakeTemplateMatchCapability());
  capabilities.push_back(MakeRegionDetectCapability());
  capabilities.push_back(MakeEmbeddedModelCapability());
  return !capabilities.empty();
}

bool BuildCximageLowLevelGuidance(CximageLowLevelGuidance &guidance)
{
  guidance = CximageLowLevelGuidance();

  guidance.recommended_script_inputs.push_back(
      {"image_view", "input", "normalized image bytes with explicit width/height/channels"});
  guidance.recommended_script_inputs.push_back(
      {"roi_rect", "input", "small rectangular focus region for low-level operators"});
  guidance.recommended_script_inputs.push_back(
      {"shape_seed", "input", "lightweight line/circle/ellipse seed geometry"});
  guidance.recommended_script_inputs.push_back(
      {"matcher_config", "input", "simple thresholds and roi/grid hints rather than engine internals"});

  guidance.recommended_script_outputs.push_back(
      {"point_set", "output", "sampled point evidence for line/circle/ellipse paths"});
  guidance.recommended_script_outputs.push_back(
      {"measure_bounds", "output", "rectangular measurement bounds for downstream checks"});
  guidance.recommended_script_outputs.push_back(
      {"circle_summary", "output", "center/radius/average-distance tuple"});
  guidance.recommended_script_outputs.push_back(
      {"match_candidates", "output", "candidate boxes, centers, and per-candidate score"});
  guidance.recommended_script_outputs.push_back(
      {"match_summary", "output", "max_score and image_model_score level summary"});

  PushUnique(guidance.keep_in_low_level_capabilities,
             "OpenCV image decode/encode and buffer ownership");
  PushUnique(guidance.keep_in_low_level_capabilities,
             "pixel-level threshold, blur, morphology, and color-space policy");
  PushUnique(guidance.keep_in_low_level_capabilities,
             "line/circle/ellipse sampling heuristics and fallback retries");
  PushUnique(guidance.keep_in_low_level_capabilities,
             "template search grid, compare-gap, and rotation search details");
  PushUnique(guidance.keep_in_low_level_capabilities,
             "tensor layout/device/runtime details for embedded image models");

  PushUnique(guidance.upper_layer_relief_rules,
             "upper layers should call fragment ids or lightweight capability ids, not concrete cximage classes");
  PushUnique(guidance.upper_layer_relief_rules,
             "upper layers should exchange roi_rect, shape_seed, point_set, bounds, and score summaries");
  PushUnique(guidance.upper_layer_relief_rules,
             "upper layers should avoid owning OpenCV Mat, image manager state, or matcher tuning internals");
  PushUnique(guidance.upper_layer_relief_rules,
             "main-thread flow and validation decisions remain outside cximage low-level catalog");

  return true;
}

bool FindCximageLowLevelCapability(
    const std::vector<CximageLowLevelCapability> &capabilities,
    const std::string &capability_id,
    CximageLowLevelCapability &capability)
{
  for (size_t index = 0; index < capabilities.size(); ++index)
  {
    if (capabilities[index].capability_id == capability_id)
    {
      capability = capabilities[index];
      return true;
    }
  }
  return false;
}

std::string BuildCximageLowLevelCapabilityReport(
    const std::vector<CximageLowLevelCapability> &capabilities,
    const CximageLowLevelGuidance &guidance)
{
  std::ostringstream out;
  out << "[CXIMAGE-LOWLEVEL] count=" << capabilities.size() << "\n";
  for (size_t index = 0; index < capabilities.size(); ++index)
  {
    const CximageLowLevelCapability &capability = capabilities[index];
    out << "[CAPABILITY] id=" << capability.capability_id
        << " category=" << capability.category
        << " script_friendly=" << (capability.script_friendly ? "true" : "false")
        << " fragment_ready=" << (capability.recommended_for_fragment ? "true" : "false")
        << "\n";
    out << "[SUMMARY] " << capability.summary << "\n";
    for (size_t io_index = 0; io_index < capability.lightweight_inputs.size(); ++io_index)
    {
      out << "[LIGHT-INPUT] " << capability.lightweight_inputs[io_index] << "\n";
    }
    for (size_t io_index = 0; io_index < capability.lightweight_outputs.size(); ++io_index)
    {
      out << "[LIGHT-OUTPUT] " << capability.lightweight_outputs[io_index] << "\n";
    }
    for (size_t fragment_index = 0; fragment_index < capability.reusable_fragments.size(); ++fragment_index)
    {
      out << "[FRAGMENT] " << capability.reusable_fragments[fragment_index] << "\n";
    }
  }

  out << "[SCRIPT-IO-GUIDANCE]\n";
  for (size_t index = 0; index < guidance.recommended_script_inputs.size(); ++index)
  {
    const CximageLightweightIoShape &shape = guidance.recommended_script_inputs[index];
    out << "[IO] direction=" << shape.direction
        << " name=" << shape.name
        << " summary=" << shape.summary << "\n";
  }
  for (size_t index = 0; index < guidance.recommended_script_outputs.size(); ++index)
  {
    const CximageLightweightIoShape &shape = guidance.recommended_script_outputs[index];
    out << "[IO] direction=" << shape.direction
        << " name=" << shape.name
        << " summary=" << shape.summary << "\n";
  }
  for (size_t index = 0; index < guidance.keep_in_low_level_capabilities.size(); ++index)
  {
    out << "[KEEP-LOWLEVEL] " << guidance.keep_in_low_level_capabilities[index] << "\n";
  }
  for (size_t index = 0; index < guidance.upper_layer_relief_rules.size(); ++index)
  {
    out << "[RELIEF-RULE] " << guidance.upper_layer_relief_rules[index] << "\n";
  }
  return out.str();
}
}
