#include "parser_cxscript_fragment_catalog.h"

#include <sstream>

namespace cxparser_ext
{
namespace
{
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

CxscriptCapabilityFragment MakeImagePrepareFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.operator.image_prepare_basic_roi";
  fragment.module_name = "cximage";
  fragment.capability_name = "image_prepare_basic_roi";
  fragment.category = "operator";
  fragment.summary =
      "Prepare an image snapshot for downstream feature, matcher, or model "
      "steps by loading, resizing, grayscale conversion, ROI extraction, "
      "and threshold preprocessing.";
  fragment.source_files.push_back("cxcore/core/Image.cpp");
  fragment.input_contracts.push_back("image_path or image_buffer");
  fragment.input_contracts.push_back("optional roi_rect");
  fragment.input_contracts.push_back("optional resize_wh and threshold");
  fragment.steps.push_back(
      {"input_prepare", "Image.load", "load source image into working mat",
       "loaded_image"});
  fragment.steps.push_back(
      {"input_prepare", "Image.resizeImage", "normalize image size for a "
                                           "stable downstream path",
       "resized_image"});
  fragment.steps.push_back(
      {"input_prepare", "Image.convertToGrayScale",
       "collapse image into a grayscale signal when needed", "gray_image"});
  fragment.steps.push_back(
      {"input_prepare", "Image.getROI", "extract the operator focus area",
       "roi_image"});
  fragment.steps.push_back(
      {"operator_action", "Image.threshold",
       "generate a binary-ready probe image for feature or matcher steps",
       "prepared_image"});
  fragment.steps.push_back(
      {"result_check", "Image.getWidth/getHeight/getType",
       "confirm prepared image shape and type are valid", "image_meta"});
  fragment.checkpoints.push_back("prepared image is non-empty");
  fragment.checkpoints.push_back("roi bounds stay inside the source image");
  fragment.checkpoints.push_back(
      "output width and height match the requested normalize policy");
  fragment.expected_outputs.push_back("prepared_image");
  fragment.expected_outputs.push_back("image_meta");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeLineFeatureFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.feature.line_measure_roi";
  fragment.module_name = "cxvision";
  fragment.capability_name = "line_measure_roi";
  fragment.category = "feature";
  fragment.summary =
      "Measure line evidence inside an ROI and export line points and "
      "measurement bounds for later geometry or validation steps.";
  fragment.source_files.push_back("cxcore/core/Findline.h");
  fragment.source_files.push_back("cxcore/core/Findline.cpp");
  fragment.input_contracts.push_back("prepared_image");
  fragment.input_contracts.push_back("line_roi_rect or line_segment seed");
  fragment.input_contracts.push_back("line_gap, threshold, method");
  fragment.steps.push_back(
      {"input_prepare", "Findline.setrect/setlinesegment",
       "declare the ROI or seed segment used for probing line structure",
       "line_probe_shape"});
  fragment.steps.push_back(
      {"input_prepare", "Findline.setlinegap/setthre/setmethod",
       "configure sampling density, threshold, and search mode",
       "line_probe_config"});
  fragment.steps.push_back(
      {"operator_action", "Findline.measure",
       "run the ROI-based line measurement against the prepared image",
       "line_measure_raw"});
  fragment.steps.push_back(
      {"result_check", "Findline.getresultpointsw/getresultpointsh",
       "verify width/height point sets were produced", "line_feature_points"});
  fragment.steps.push_back(
      {"result_check", "Findline.measurepointsboundingrect",
       "capture the measured bound for downstream geometry publishing",
       "line_feature_bounds"});
  fragment.checkpoints.push_back("at least one line point set is non-empty");
  fragment.checkpoints.push_back("measurement bounds are valid");
  fragment.expected_outputs.push_back("line_feature_points");
  fragment.expected_outputs.push_back("line_feature_bounds");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeCircleFeatureFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.feature.circle_measure_fit";
  fragment.module_name = "cxvision";
  fragment.capability_name = "circle_measure_fit";
  fragment.category = "feature";
  fragment.summary =
      "Probe radial evidence from a seeded circle, fit the result circle, "
      "and expose center/radius measurements for downstream geometry checks.";
  fragment.source_files.push_back("cxcore/core/Findcircle.h");
  fragment.source_files.push_back("cxcore/core/Findcircle.cpp");
  fragment.input_contracts.push_back("prepared_image");
  fragment.input_contracts.push_back("circle_seed(center, axis point)");
  fragment.input_contracts.push_back("gap, threshold, method");
  fragment.steps.push_back(
      {"input_prepare", "Findcircle.setcircle/setcircle2",
       "seed the radial probe area with a center and radius reference",
       "circle_probe_shape"});
  fragment.steps.push_back(
      {"input_prepare", "Findcircle.setlinegap/setthre/setmethod",
       "configure radial sampling and filter policy", "circle_probe_config"});
  fragment.steps.push_back(
      {"operator_action", "Findcircle.measure",
       "run circle evidence extraction on the prepared image",
       "circle_measure_raw"});
  fragment.steps.push_back(
      {"operator_action", "Findcircle.fitcircle",
       "fit a stable circle from the measured evidence", "circle_fit"});
  fragment.steps.push_back(
      {"result_check", "Findcircle.getresultcentx/getresultcenty/getradius",
       "verify the fitted circle parameters are available", "circle_feature"});
  fragment.checkpoints.push_back("fitted radius is positive");
  fragment.checkpoints.push_back("fitted center is finite");
  fragment.expected_outputs.push_back("circle_feature");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeEllipseFeatureFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.feature.ellipse_measure_roi";
  fragment.module_name = "cxvision";
  fragment.capability_name = "ellipse_measure_roi";
  fragment.category = "feature";
  fragment.summary =
      "Probe ellipse evidence from a seeded ellipse and export result points "
      "and bounds for downstream geometry reconstruction or checks.";
  fragment.source_files.push_back("cxcore/core/Findellipse.h");
  fragment.source_files.push_back("cxcore/core/Findellipse.cpp");
  fragment.input_contracts.push_back("prepared_image");
  fragment.input_contracts.push_back("ellipse_seed(center, axis point)");
  fragment.input_contracts.push_back("gap, threshold, method");
  fragment.steps.push_back(
      {"input_prepare", "Findellipse.setellipse/setellipse2",
       "seed the ellipse probe with center and axis references",
       "ellipse_probe_shape"});
  fragment.steps.push_back(
      {"input_prepare", "Findellipse.setlinegap/setthre/setmethod",
       "configure angular sampling and threshold policy",
       "ellipse_probe_config"});
  fragment.steps.push_back(
      {"operator_action", "Findellipse.measure",
       "run ellipse measurement against the prepared image",
       "ellipse_measure_raw"});
  fragment.steps.push_back(
      {"result_check", "Findellipse.getresultpoints",
       "verify ellipse point evidence exists", "ellipse_feature_points"});
  fragment.steps.push_back(
      {"result_check", "Findellipse.measurepointsboundingrect",
       "capture ellipse measurement bounds", "ellipse_feature_bounds"});
  fragment.checkpoints.push_back("ellipse result point set is non-empty");
  fragment.checkpoints.push_back("ellipse measurement bounds are valid");
  fragment.expected_outputs.push_back("ellipse_feature_points");
  fragment.expected_outputs.push_back("ellipse_feature_bounds");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeFastMatchFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.matcher.fast_template_match";
  fragment.module_name = "cxvision";
  fragment.capability_name = "fast_template_match";
  fragment.category = "matcher";
  fragment.summary =
      "Learn or load a fast template model, run ROI/grid-guided matching, "
      "and export match rectangles and scores for downstream checks.";
  fragment.source_files.push_back("cxcore/core/FastMatch.h");
  fragment.source_files.push_back("cxcore/core/FastMatch.cpp");
  fragment.input_contracts.push_back("prepared_image");
  fragment.input_contracts.push_back("model_image or model_file");
  fragment.input_contracts.push_back("match_roi, grid, threshold, min_score");
  fragment.steps.push_back(
      {"input_prepare", "fastmatch.setrect/setgrid/setmatchthre/setminscore",
       "configure the search ROI, grid density, and score gates",
       "match_probe_config"});
  fragment.steps.push_back(
      {"input_prepare", "fastmatch.learn/loadmodelfile",
       "prepare the reusable template representation", "match_model"});
  fragment.steps.push_back(
      {"operator_action", "fastmatch.match/multimatch/rotatematchAB_upgrade",
       "run template matching against the prepared search image",
       "match_results"});
  fragment.steps.push_back(
      {"result_check",
       "fastmatch.getmaxresult/getresultrect/getresultcentx/getresultcenty",
       "verify score and match geometry are available", "match_summary"});
  fragment.checkpoints.push_back("max score satisfies the configured minimum");
  fragment.checkpoints.push_back("at least one result rect is available");
  fragment.expected_outputs.push_back("match_results");
  fragment.expected_outputs.push_back("match_summary");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeMobileViTFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.embedded_model.mobilevit_mainline";
  fragment.module_name = "embedded_model";
  fragment.capability_name = "mobilevit_mainline";
  fragment.category = "embedded_model";
  fragment.summary =
      "Run a compact embedded-model classification flow with mainline "
      "session, trainer analysis, and unified summary generation.";
  fragment.source_files.push_back("libtorch_module/torch_minimal_smoke.cpp");
  fragment.input_contracts.push_back("train_images, train_targets");
  fragment.input_contracts.push_back("eval_images, eval_targets");
  fragment.input_contracts.push_back("runner_config");
  fragment.steps.push_back(
      {"input_prepare", "make_mobilevit_mainline_runner_config",
       "declare device, class count, and image-size policy", "runner_config"});
  fragment.steps.push_back(
      {"operator_action", "run_mobilevit_mainline_session",
       "execute the mainline train/eval embedded-model pass",
       "mobilevit_session"});
  fragment.steps.push_back(
      {"operator_action", "run_mobilevit_trainer_session",
       "execute the trainer-side companion flow", "mobilevit_trainer"});
  fragment.steps.push_back(
      {"result_check", "build_mobilevit_trainer_analysis",
       "summarize stage outcomes and comparison rows", "mobilevit_analysis"});
  fragment.steps.push_back(
      {"result_check", "build_mobilevit_unified_mainline_summary",
       "generate a compact unified summary for publish/report",
       "mobilevit_summary"});
  fragment.checkpoints.push_back("mainline session passes");
  fragment.checkpoints.push_back("trainer lifecycle passes");
  fragment.checkpoints.push_back("unified summary has expected outcomes");
  fragment.expected_outputs.push_back("mobilevit_summary");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeSegmentationFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.embedded_model.segmentation_mainline";
  fragment.module_name = "embedded_model";
  fragment.capability_name = "segmentation_mainline";
  fragment.category = "embedded_model";
  fragment.summary =
      "Run a segmentation-oriented embedded-model flow with configurable "
      "backbone/decoder and emit a checked session/trainer summary.";
  fragment.source_files.push_back("libtorch_module/torch_minimal_smoke.cpp");
  fragment.input_contracts.push_back("segmentation_train_images, masks");
  fragment.input_contracts.push_back("segmentation_eval_images, masks");
  fragment.input_contracts.push_back("segmentation_runner_config");
  fragment.steps.push_back(
      {"input_prepare", "make_segmentation_mainline_runner_config",
       "declare decoder, backbone, class count, and image-size policy",
       "segmentation_runner"});
  fragment.steps.push_back(
      {"operator_action", "run_segmentation_mainline_session",
       "execute the segmentation mainline session", "segmentation_session"});
  fragment.steps.push_back(
      {"operator_action", "run_segmentation_trainer_session",
       "execute the segmentation trainer companion flow",
       "segmentation_trainer"});
  fragment.steps.push_back(
      {"result_check", "build_segmentation_trainer_analysis",
       "verify analysis, comparisons, and recommendation data",
       "segmentation_analysis"});
  fragment.checkpoints.push_back("segmentation session passes");
  fragment.checkpoints.push_back("segmentation trainer lifecycle passes");
  fragment.expected_outputs.push_back("segmentation_analysis");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeTorchGeometryInputPriorFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.integration.torch_geometry.input_prior_min";
  fragment.module_name = "torch_geometry";
  fragment.capability_name = "input_prior_min";
  fragment.category = "integration";
  fragment.summary =
      "Align ROI, line, and point-set geometry into a minimal torch input "
      "prior request and bind one readiness check.";
  fragment.source_files.push_back(
      "cxscript/integration/torch_geometry/feature.input_prior_min.cxsc");
  fragment.input_contracts.push_back("image_path, roi_id");
  fragment.input_contracts.push_back("optional line_id");
  fragment.input_contracts.push_back("optional pointset_id");
  fragment.steps.push_back(
      {"input_prepare", "geom_export_roi",
       "export the ROI object used as the main geometry anchor",
       "roi_object"});
  fragment.steps.push_back(
      {"input_prepare", "geom_export_line/geom_export_pointset",
       "export line and point-set priors when present",
       "geometry_prior_objects"});
  fragment.steps.push_back(
      {"operator_action", "geom_align_input_prior",
       "assemble ROI-centered geometry prior inputs",
       "input_prior_bundle"});
  fragment.steps.push_back(
      {"operator_action", "geom_build_torch_request",
       "materialize the minimal torch request packet",
       "torch_request"});
  fragment.steps.push_back(
      {"result_check", "geom_check_input_ready",
       "verify one ready flag for the aligned input request",
       "input_ready"});
  fragment.checkpoints.push_back("ROI export is available");
  fragment.checkpoints.push_back("input_ready == true");
  fragment.expected_outputs.push_back("input_prior_bundle");
  fragment.expected_outputs.push_back("torch_request");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeTorchGeometryLabelAlignFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.integration.torch_geometry.label_align_min";
  fragment.module_name = "torch_geometry";
  fragment.capability_name = "label_align_min";
  fragment.category = "integration";
  fragment.summary =
      "Align ROI-scoped mask, boundary, and keypoints labels into a minimal "
      "torch training-label packet and bind one readiness check.";
  fragment.source_files.push_back(
      "cxscript/integration/torch_geometry/feature.label_align_min.cxsc");
  fragment.input_contracts.push_back("image_path, roi_id");
  fragment.input_contracts.push_back("mask_id");
  fragment.input_contracts.push_back("boundary_id, keypoints_id");
  fragment.steps.push_back(
      {"input_prepare", "geom_export_roi",
       "export the ROI object used as the label anchor",
       "roi_object"});
  fragment.steps.push_back(
      {"input_prepare",
       "geom_export_mask_label/geom_export_boundary_label/geom_export_keypoints_label",
       "export label-side geometry objects tied to the ROI",
       "geometry_label_objects"});
  fragment.steps.push_back(
      {"operator_action", "geom_align_training_label",
       "assemble aligned mask, boundary, and keypoints labels",
       "label_align_bundle"});
  fragment.steps.push_back(
      {"operator_action", "geom_build_torch_label_packet",
       "materialize the minimal torch label packet",
       "torch_label_packet"});
  fragment.steps.push_back(
      {"result_check", "geom_check_label_ready",
       "verify one ready flag for aligned labels",
       "label_ready"});
  fragment.checkpoints.push_back("mask/boundary/keypoints labels are exported");
  fragment.checkpoints.push_back("label_ready == true");
  fragment.expected_outputs.push_back("label_align_bundle");
  fragment.expected_outputs.push_back("torch_label_packet");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeTorchGeometryAttachFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.integration.torch_geometry.attach_back_min";
  fragment.module_name = "torch_geometry";
  fragment.capability_name = "attach_back_min";
  fragment.category = "integration";
  fragment.summary =
      "Attach torch-side classification, mask, boundary, and keypoints "
      "results back to ROI-centered geometry and bind one readiness check.";
  fragment.source_files.push_back(
      "cxscript/integration/torch_geometry/feature.attach_back_min.cxsc");
  fragment.input_contracts.push_back("roi_id");
  fragment.input_contracts.push_back("optional mask_id");
  fragment.input_contracts.push_back("optional boundary_id, keypoints_id");
  fragment.steps.push_back(
      {"operator_action", "geom_attach_result_to_roi",
       "attach class/score metadata to the ROI anchor",
       "roi_attach_record"});
  fragment.steps.push_back(
      {"operator_action",
       "geom_attach_result_mask/geom_attach_result_boundary/geom_attach_result_keypoints",
       "attach geometry outputs back to mask, boundary, and keypoints layers",
       "geometry_attach_records"});
  fragment.steps.push_back(
      {"result_check", "geom_check_attach_ready",
       "verify one ready flag for the attach packet",
       "attach_ready"});
  fragment.steps.push_back(
      {"result_check", "geom_publish_attach_packet",
       "publish the minimal geometry attach packet",
       "attach_packet"});
  fragment.checkpoints.push_back("ROI attach record exists");
  fragment.checkpoints.push_back("attach_ready == true");
  fragment.expected_outputs.push_back("attach_packet");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeTorchGeometryReplayFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.integration.torch_geometry.replay_min";
  fragment.module_name = "torch_geometry";
  fragment.capability_name = "replay_min";
  fragment.category = "integration";
  fragment.summary =
      "Execute the minimal source-level torch geometry replay chain from "
      "input prior and label alignment through torch run and geometry attach.";
  fragment.source_files.push_back(
      "cxscript/integration/torch_geometry/infer.replay_min.cxsc");
  fragment.input_contracts.push_back("image_path, roi_id");
  fragment.input_contracts.push_back("line_id, pointset_id");
  fragment.input_contracts.push_back("mask_id, boundary_id, keypoints_id");
  fragment.steps.push_back(
      {"input_prepare", "geom_export_roi/geom_export_line/geom_export_pointset",
       "export ROI-centered geometry inputs used to build the replay request",
       "geometry_input_objects"});
  fragment.steps.push_back(
      {"input_prepare",
       "geom_export_mask_label/geom_export_boundary_label/geom_export_keypoints_label",
       "export ROI-scoped label geometry used by replay",
       "geometry_label_objects"});
  fragment.steps.push_back(
      {"operator_action",
       "geom_align_input_prior/geom_build_torch_request/geom_align_training_label/geom_build_torch_label_packet",
       "assemble the minimal replay request and label packets",
       "torch_geometry_replay_inputs"});
  fragment.steps.push_back(
      {"operator_action",
       "torch_run/geom_attach_result_to_roi/geom_attach_result_mask/geom_attach_result_boundary/geom_attach_result_keypoints",
       "execute replay and attach outputs back to geometry",
       "torch_geometry_replay_result"});
  fragment.steps.push_back(
      {"result_check", "geom_check_attach_ready",
       "verify one ready flag for the replay attach packet",
       "replay_ready"});
  fragment.steps.push_back(
      {"result_check", "geom_publish_attach_packet",
       "publish the replay attach packet for downstream validation",
       "torch_geometry_alignment_packet"});
  fragment.checkpoints.push_back("torch replay path emits a result packet");
  fragment.checkpoints.push_back("replay_ready == true");
  fragment.expected_outputs.push_back("torch_result_main");
  fragment.expected_outputs.push_back("torch_geometry_alignment_packet");
  fragment.reusable_for_cxcore = true;
  return fragment;
}

CxscriptCapabilityFragment MakeTorchGeometryReplayContractFragment()
{
  CxscriptCapabilityFragment fragment;
  fragment.fragment_id = "cxscript.integration.torch_geometry.replay_contract";
  fragment.module_name = "torch_geometry";
  fragment.capability_name = "replay_contract";
  fragment.category = "integration";
  fragment.summary =
      "Execute the minimal active no-adapter torch geometry contract chain "
      "from input prior and label alignment through attach readiness.";
  fragment.source_files.push_back(
      "cxscript/integration/torch_geometry/infer.replay_contract.cxsc");
  fragment.input_contracts.push_back("image_path, roi_id");
  fragment.input_contracts.push_back("line_id, pointset_id");
  fragment.input_contracts.push_back("mask_id, boundary_id, keypoints_id");
  fragment.steps.push_back(
      {"input_prepare",
       "geom_export_roi/geom_export_line/geom_export_pointset",
       "export the minimal geometry inputs used by the active contract chain",
       "geometry_input_objects"});
  fragment.steps.push_back(
      {"input_prepare",
       "geom_export_mask_label/geom_export_boundary_label/geom_export_keypoints_label",
       "export the minimal label-side geometry objects",
       "geometry_label_objects"});
  fragment.steps.push_back(
      {"operator_action",
       "geom_align_input_prior/geom_build_torch_request/geom_align_training_label/geom_build_torch_label_packet",
       "assemble active contract request and label state",
       "torch_geometry_contract_inputs"});
  fragment.steps.push_back(
      {"operator_action",
       "geom_attach_result_to_roi/geom_attach_result_mask/geom_attach_result_boundary/geom_attach_result_keypoints/geom_publish_attach_packet",
       "attach active contract outputs back to geometry state",
       "torch_geometry_contract_attach"});
  fragment.steps.push_back(
      {"result_check", "geom_check_replay_ready",
       "verify the full active contract chain is ready",
       "replay_contract_ok"});
  fragment.checkpoints.push_back("active replay contract emits a positive ready flag");
  fragment.expected_outputs.push_back("replay_contract_ok");
  fragment.reusable_for_cxcore = true;
  return fragment;
}
}

bool BuildCxscriptFragmentCatalog(
    std::vector<CxscriptCapabilityFragment> &fragments)
{
  fragments.clear();
  fragments.push_back(MakeImagePrepareFragment());
  fragments.push_back(MakeLineFeatureFragment());
  fragments.push_back(MakeCircleFeatureFragment());
  fragments.push_back(MakeEllipseFeatureFragment());
  fragments.push_back(MakeFastMatchFragment());
  fragments.push_back(MakeMobileViTFragment());
  fragments.push_back(MakeSegmentationFragment());
  fragments.push_back(MakeTorchGeometryInputPriorFragment());
  fragments.push_back(MakeTorchGeometryLabelAlignFragment());
  fragments.push_back(MakeTorchGeometryAttachFragment());
  fragments.push_back(MakeTorchGeometryReplayFragment());
  fragments.push_back(MakeTorchGeometryReplayContractFragment());
  return !fragments.empty();
}

bool FindCxscriptCapabilityFragment(
    const std::vector<CxscriptCapabilityFragment> &fragments,
    const std::string &fragment_id,
    CxscriptCapabilityFragment &fragment)
{
  for (size_t i = 0; i < fragments.size(); ++i)
  {
    if (fragments[i].fragment_id == fragment_id)
    {
      fragment = fragments[i];
      return true;
    }
  }
  return false;
}

bool BuildCxscriptFlowFragmentBundles(
    const std::vector<CxscriptCapabilityFragment> &fragments,
    std::vector<CxscriptFlowFragmentBundle> &bundles)
{
  bundles.clear();
  if (fragments.empty())
  {
    return false;
  }

  CxscriptFlowFragmentBundle line_bundle;
  line_bundle.bundle_id = "cxscript.bundle.image_to_line_feature";
  line_bundle.bundle_name = "image_to_line_feature";
  line_bundle.summary =
      "Reusable low-level flow that prepares an image snapshot, measures line "
      "evidence, and publishes checked line-feature outputs.";
  line_bundle.fragment_ids.push_back("cxscript.operator.image_prepare_basic_roi");
  line_bundle.fragment_ids.push_back("cxscript.feature.line_measure_roi");
  line_bundle.flow_roles.push_back("input_prepare");
  line_bundle.flow_roles.push_back("operator_action");
  line_bundle.flow_roles.push_back("result_check");
  line_bundle.reusable_outputs.push_back("prepared_image");
  line_bundle.reusable_outputs.push_back("line_feature_points");
  line_bundle.reusable_outputs.push_back("line_feature_bounds");
  line_bundle.reusable_for_cxcore = true;
  bundles.push_back(line_bundle);

  CxscriptFlowFragmentBundle circle_bundle;
  circle_bundle.bundle_id = "cxscript.bundle.image_to_circle_feature";
  circle_bundle.bundle_name = "image_to_circle_feature";
  circle_bundle.summary =
      "Reusable low-level flow that prepares an image snapshot, measures "
      "circle evidence, fits the circle, and emits checked center/radius "
      "outputs.";
  circle_bundle.fragment_ids.push_back("cxscript.operator.image_prepare_basic_roi");
  circle_bundle.fragment_ids.push_back("cxscript.feature.circle_measure_fit");
  circle_bundle.flow_roles.push_back("input_prepare");
  circle_bundle.flow_roles.push_back("operator_action");
  circle_bundle.flow_roles.push_back("result_check");
  circle_bundle.reusable_outputs.push_back("prepared_image");
  circle_bundle.reusable_outputs.push_back("circle_feature");
  circle_bundle.reusable_for_cxcore = true;
  bundles.push_back(circle_bundle);

  CxscriptFlowFragmentBundle ellipse_bundle;
  ellipse_bundle.bundle_id = "cxscript.bundle.image_to_ellipse_feature";
  ellipse_bundle.bundle_name = "image_to_ellipse_feature";
  ellipse_bundle.summary =
      "Reusable low-level flow that prepares an image snapshot, measures "
      "ellipse evidence, and exports checked ellipse points and bounds.";
  ellipse_bundle.fragment_ids.push_back("cxscript.operator.image_prepare_basic_roi");
  ellipse_bundle.fragment_ids.push_back("cxscript.feature.ellipse_measure_roi");
  ellipse_bundle.flow_roles.push_back("input_prepare");
  ellipse_bundle.flow_roles.push_back("operator_action");
  ellipse_bundle.flow_roles.push_back("result_check");
  ellipse_bundle.reusable_outputs.push_back("prepared_image");
  ellipse_bundle.reusable_outputs.push_back("ellipse_feature_points");
  ellipse_bundle.reusable_outputs.push_back("ellipse_feature_bounds");
  ellipse_bundle.reusable_for_cxcore = true;
  bundles.push_back(ellipse_bundle);

  CxscriptFlowFragmentBundle matcher_bundle;
  matcher_bundle.bundle_id = "cxscript.bundle.image_to_fast_match";
  matcher_bundle.bundle_name = "image_to_fast_match";
  matcher_bundle.summary =
      "Reusable low-level flow that prepares an image snapshot, prepares or "
      "loads a template model, runs matching, and emits checked match "
      "summary outputs.";
  matcher_bundle.fragment_ids.push_back("cxscript.operator.image_prepare_basic_roi");
  matcher_bundle.fragment_ids.push_back("cxscript.matcher.fast_template_match");
  matcher_bundle.flow_roles.push_back("input_prepare");
  matcher_bundle.flow_roles.push_back("operator_action");
  matcher_bundle.flow_roles.push_back("result_check");
  matcher_bundle.reusable_outputs.push_back("prepared_image");
  matcher_bundle.reusable_outputs.push_back("match_summary");
  matcher_bundle.reusable_for_cxcore = true;
  bundles.push_back(matcher_bundle);

  CxscriptFlowFragmentBundle train_bundle;
  train_bundle.bundle_id = "cxscript.bundle.embedded_model_train_mainline";
  train_bundle.bundle_name = "embedded_model_train_mainline";
  train_bundle.summary =
      "Reusable embedded-model training-oriented low-level flow that prepares "
      "runner inputs, executes a mainline session, and emits checked trainer "
      "analysis outputs.";
  train_bundle.fragment_ids.push_back("cxscript.embedded_model.mobilevit_mainline");
  train_bundle.fragment_ids.push_back("cxscript.embedded_model.segmentation_mainline");
  train_bundle.flow_roles.push_back("input_prepare");
  train_bundle.flow_roles.push_back("operator_action");
  train_bundle.flow_roles.push_back("result_check");
  train_bundle.reusable_outputs.push_back("mobilevit_summary");
  train_bundle.reusable_outputs.push_back("segmentation_analysis");
  train_bundle.reusable_for_cxcore = true;
  bundles.push_back(train_bundle);

  CxscriptFlowFragmentBundle torch_geometry_bundle;
  torch_geometry_bundle.bundle_id = "cxscript.bundle.torch_geometry_alignment_min";
  torch_geometry_bundle.bundle_name = "torch_geometry_alignment_min";
  torch_geometry_bundle.summary =
      "Reusable source-level cxscript flow for torch geometry alignment: "
      "input prior, training label alignment, and result attach.";
  torch_geometry_bundle.fragment_ids.push_back(
      "cxscript.integration.torch_geometry.input_prior_min");
  torch_geometry_bundle.fragment_ids.push_back(
      "cxscript.integration.torch_geometry.label_align_min");
  torch_geometry_bundle.fragment_ids.push_back(
      "cxscript.integration.torch_geometry.attach_back_min");
  torch_geometry_bundle.fragment_ids.push_back(
      "cxscript.integration.torch_geometry.replay_contract");
  torch_geometry_bundle.flow_roles.push_back("input_prepare");
  torch_geometry_bundle.flow_roles.push_back("operator_action");
  torch_geometry_bundle.flow_roles.push_back("result_check");
  torch_geometry_bundle.reusable_outputs.push_back("torch_request");
  torch_geometry_bundle.reusable_outputs.push_back("torch_label_packet");
  torch_geometry_bundle.reusable_outputs.push_back("attach_packet");
  torch_geometry_bundle.reusable_outputs.push_back("replay_contract_ok");
  torch_geometry_bundle.reusable_for_cxcore = true;
  bundles.push_back(torch_geometry_bundle);

  for (std::size_t index = 0; index < bundles.size(); ++index)
  {
    for (std::size_t fragment_index = 0;
         fragment_index < bundles[index].fragment_ids.size();
         ++fragment_index)
    {
      CxscriptCapabilityFragment fragment;
      if (!FindCxscriptCapabilityFragment(
              fragments, bundles[index].fragment_ids[fragment_index], fragment))
      {
        return false;
      }
    }
  }

  return !bundles.empty();
}

bool FindCxscriptFlowFragmentBundle(
    const std::vector<CxscriptFlowFragmentBundle> &bundles,
    const std::string &bundle_id,
    CxscriptFlowFragmentBundle &bundle)
{
  for (std::size_t index = 0; index < bundles.size(); ++index)
  {
    if (bundles[index].bundle_id == bundle_id)
    {
      bundle = bundles[index];
      return true;
    }
  }
  return false;
}

std::string BuildCxscriptFragmentCatalogReport(
    const std::vector<CxscriptCapabilityFragment> &fragments)
{
  std::ostringstream out;
  out << "[CXSCRIPT-FRAGMENTS] count=" << fragments.size() << "\n";
  for (size_t i = 0; i < fragments.size(); ++i)
  {
    const CxscriptCapabilityFragment &fragment = fragments[i];
    out << "[FRAGMENT] id=" << fragment.fragment_id
        << " module=" << fragment.module_name
        << " category=" << fragment.category << "\n";
    out << "[SUMMARY] " << fragment.summary << "\n";
    for (size_t step_index = 0; step_index < fragment.steps.size(); ++step_index)
    {
      const CxscriptFragmentStep &step = fragment.steps[step_index];
      out << "[STEP] stage=" << step.stage_name
          << " entry=" << step.entry_name
          << " output=" << step.output_name << "\n";
    }
  }
  return out.str();
}

std::string BuildCxscriptFlowFragmentBundleReport(
    const std::vector<CxscriptFlowFragmentBundle> &bundles)
{
  std::ostringstream out;
  out << "[CXSCRIPT-BUNDLES] count=" << bundles.size() << "\n";
  for (std::size_t index = 0; index < bundles.size(); ++index)
  {
    const CxscriptFlowFragmentBundle &bundle = bundles[index];
    out << "[BUNDLE] id=" << bundle.bundle_id
        << " name=" << bundle.bundle_name << "\n";
    out << "[SUMMARY] " << bundle.summary << "\n";
    for (std::size_t fragment_index = 0;
         fragment_index < bundle.fragment_ids.size();
         ++fragment_index)
    {
      out << "[FRAGMENT] " << bundle.fragment_ids[fragment_index] << "\n";
    }
    for (std::size_t output_index = 0;
         output_index < bundle.reusable_outputs.size();
         ++output_index)
    {
      out << "[OUTPUT] " << bundle.reusable_outputs[output_index] << "\n";
    }
  }
  return out.str();
}
}
