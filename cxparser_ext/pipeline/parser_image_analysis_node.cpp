#include "parser_image_analysis_node.h"

namespace cxparser_ext
{
namespace
{
void BuildImageAnalysisSlices(ImageAnalysisResult &result)
{
  result.multimodal_slices.clear();
  result.operation_atoms.clear();

  OperationAtom validate_atom;
  validate_atom.atom_id = result.task_id + ".atom.validate_request";
  validate_atom.stage = "input_layer";
  validate_atom.action_kind = "validate_request";
  validate_atom.input_ref = result.task_id;
  validate_atom.output_ref = result.task_id + ".validated";
  validate_atom.status = result.status == "ok" ? "ok" : "failed";
  validate_atom.summary = result.status == "ok"
    ? "image analysis request validated"
    : "image analysis request validation failed";
  result.operation_atoms.push_back(validate_atom);

  MultimodalSlice visual_slice;
  visual_slice.slice_id = result.task_id + ".visual_slice_v1";
  visual_slice.source_ref = result.task_id;
  visual_slice.source_hash = BuildPseudoSourceHash(result.task_id + result.trace_id);
  visual_slice.modality = "image";
  visual_slice.analysis_kind = "image_analysis";
  visual_slice.result_ref = result.task_id;
  visual_slice.evidence_ref = result.trace_id;
  visual_slice.log_path = result.trace_id;
  visual_slice.confidence = result.status == "ok" ? 1.0 : 0.0;
  visual_slice.next_action = result.status == "ok"
    ? "consume geometry or feature slices"
    : "fix invalid image analysis request";
  visual_slice.tags.push_back("visual_slice_v1");
  visual_slice.tags.push_back("private_local_only");

  for (size_t i = 0; i < result.roi_results.size(); ++i)
  {
    const ImageAnalysisRoiResult &roi = result.roi_results[i];
    MultimodalSliceObject object;
    object.object_id = roi.roi_id;
    object.object_kind = "roi";
    object.geometry_ref = roi.roi_id + ".bounds";
    object.semantic_label = roi.accepted ? "accepted" : "rejected";
    object.summary = "roi bounds exported for downstream geometry and feature steps";
    object.confidence = roi.accepted ? 1.0 : 0.0;
    visual_slice.objects.push_back(object);
  }
  result.multimodal_slices.push_back(visual_slice);

  if (!result.boundary_results.empty() ||
      !result.fit_results.empty() ||
      !result.circle_results.empty() ||
      !result.ellipse_results.empty())
  {
    MultimodalSlice geometry_slice;
    geometry_slice.slice_id = result.task_id + ".geometry_slice_v1";
    geometry_slice.source_ref = result.task_id;
    geometry_slice.source_hash = BuildPseudoSourceHash(result.task_id + ".geometry");
    geometry_slice.modality = "geometry";
    geometry_slice.analysis_kind = "reverse_geometry";
    geometry_slice.result_ref = result.task_id + ".geometry";
    geometry_slice.evidence_ref = result.trace_id;
    geometry_slice.log_path = result.trace_id;
    geometry_slice.confidence = 0.95;
    geometry_slice.next_action = "consume geometry reconstruction or attach contracts";
    geometry_slice.tags.push_back("geometry_slice_v1");

    for (size_t i = 0; i < result.boundary_results.size(); ++i)
    {
      const ImageAnalysisBoundaryResult &boundary = result.boundary_results[i];
      MultimodalSliceObject object;
      object.object_id = boundary.roi_id + ".boundary";
      object.object_kind = "boundary";
      object.geometry_ref = boundary.roi_id;
      object.semantic_label = "contour";
      object.summary = "boundary contour extracted from roi";
      object.confidence = boundary.contour.empty() ? 0.0 : 1.0;
      geometry_slice.objects.push_back(object);
    }

    for (size_t i = 0; i < result.circle_results.size(); ++i)
    {
      const ImageAnalysisCircleResult &circle = result.circle_results[i];
      MultimodalSliceObject object;
      object.object_id = circle.roi_id + ".circle";
      object.object_kind = "circle";
      object.geometry_ref = circle.roi_id;
      object.semantic_label = "fit_circle";
      object.summary = "circle fit exported with center and radius";
      object.confidence = circle.radius > 0 ? 1.0 : 0.0;
      geometry_slice.objects.push_back(object);
    }

    for (size_t i = 0; i < result.ellipse_results.size(); ++i)
    {
      const ImageAnalysisEllipseResult &ellipse = result.ellipse_results[i];
      MultimodalSliceObject object;
      object.object_id = ellipse.roi_id + ".ellipse";
      object.object_kind = "ellipse";
      object.geometry_ref = ellipse.roi_id;
      object.semantic_label = "fit_ellipse";
      object.summary = "ellipse fit sample points exported";
      object.confidence = ellipse.sample_points.empty() ? 0.0 : 1.0;
      geometry_slice.objects.push_back(object);
    }

    result.multimodal_slices.push_back(geometry_slice);
  }

  if (!result.match_results.empty())
  {
    MultimodalSlice feature_slice;
    feature_slice.slice_id = result.task_id + ".feature_slice_v1";
    feature_slice.source_ref = result.task_id;
    feature_slice.source_hash = BuildPseudoSourceHash(result.task_id + ".feature");
    feature_slice.modality = "feature_map";
    feature_slice.analysis_kind = "template_match_feature";
    feature_slice.result_ref = result.task_id + ".feature";
    feature_slice.evidence_ref = result.trace_id;
    feature_slice.log_path = result.trace_id;
    feature_slice.confidence = 0.9;
    feature_slice.next_action = "consume feature semantics or compare slices";
    feature_slice.tags.push_back("feature_slice_v1");

    for (size_t i = 0; i < result.match_results.size(); ++i)
    {
      const ImageAnalysisMatchResult &match = result.match_results[i];
      MultimodalSliceObject object;
      object.object_id = match.roi_id + ".match";
      object.object_kind = "match_candidate_group";
      object.geometry_ref = match.roi_id;
      object.semantic_label = match.matched ? "matched" : "not_matched";
      object.summary = "template match score and candidate packet exported";
      object.confidence = match.score;
      feature_slice.objects.push_back(object);
    }

    result.multimodal_slices.push_back(feature_slice);
  }
}
}

bool ParserImageAnalysisNode::HasOperation(const ImageAnalysisRequest &request,
                                           ImageAnalysisOperation operation) const
{
  for (size_t i = 0; i < request.operations.size(); ++i)
  {
    if (request.operations[i] == operation)
      return true;
  }
  return false;
}

void ParserImageAnalysisNode::AppendTrace(ImageAnalysisResult &result,
                                          const std::string &stage,
                                          const std::string &status,
                                          const std::string &message) const
{
  ImageAnalysisTraceEntry entry;
  entry.sequence = static_cast<int>(result.trace_entries.size()) + 1;
  entry.trace_id = result.trace_id;
  entry.stage = stage;
  entry.status = status;
  entry.message = message;
  result.trace_entries.push_back(entry);
}

void ParserImageAnalysisNode::AppendLog(ImageAnalysisResult &result,
                                        const std::string &level,
                                        const std::string &code,
                                        const std::string &message) const
{
  ImageAnalysisLogEntry entry;
  entry.trace_id = result.trace_id;
  entry.level = level;
  entry.code = code;
  entry.message = message;
  result.log_entries.push_back(entry);
}

bool ParserImageAnalysisNode::ValidateRequest(const ImageAnalysisRequest &request,
                                              ImageAnalysisResult &result) const
{
  if (request.task_id.empty())
  {
    AppendLog(result, "error", "task_id_empty", "image analysis task_id is empty");
    return false;
  }

  if (request.image.width <= 0 || request.image.height <= 0 || request.image.channels <= 0)
  {
    AppendLog(result, "error", "image_shape_invalid", "image dimensions are invalid");
    return false;
  }

  const int expected_size =
    request.image.width * request.image.height * request.image.channels * request.image.bytes_per_channel;
  if (expected_size <= 0 || static_cast<int>(request.image.bytes.size()) != expected_size)
  {
    AppendLog(result, "error", "image_buffer_invalid", "image bytes do not match declared layout");
    return false;
  }

  if (request.rois.empty())
  {
    AppendLog(result, "error", "roi_missing", "at least one roi is required");
    return false;
  }

  return true;
}

bool ParserImageAnalysisNode::Execute(const ImageAnalysisRequest &request,
                                      ImageAnalysisResult &result) const
{
  result = ImageAnalysisResult();
  result.task_id = request.task_id;
  result.trace_id = request.trace_id.empty() ? request.task_id + ".trace" : request.trace_id;
  result.route_lane = request.route_hint.empty() ? "default" : request.route_hint;
  result.image_width = request.image.width;
  result.image_height = request.image.height;

  AppendTrace(result, "image_analysis", "start", "image analysis node started");
  if (!ValidateRequest(request, result))
  {
    result.status = "invalid_request";
    AppendTrace(result, "image_analysis", "failed", "request validation failed");
    return false;
  }

  const bool export_roi = HasOperation(request, iao_roi_extract);
  const bool export_boundary = HasOperation(request, iao_boundary_trace);
  const bool export_fit = HasOperation(request, iao_fit_line);
  const bool export_circle = HasOperation(request, iao_fit_circle);
  const bool export_ellipse = HasOperation(request, iao_fit_ellipse);
  const bool export_match = HasOperation(request, iao_template_match);

  for (size_t i = 0; i < request.rois.size(); ++i)
  {
    const ImageAnalysisRoiInput &roi = request.rois[i];

    ImageAnalysisRoiResult roi_result;
    roi_result.roi_id = roi.roi_id;
    roi_result.bounds = roi.bounds;
    roi_result.accepted =
      roi.bounds.width > 0 &&
      roi.bounds.height > 0 &&
      roi.bounds.x >= 0 &&
      roi.bounds.y >= 0 &&
      (roi.bounds.x + roi.bounds.width) <= request.image.width &&
      (roi.bounds.y + roi.bounds.height) <= request.image.height;
    result.roi_results.push_back(roi_result);

    if (!roi_result.accepted)
    {
      result.warnings.push_back("roi rejected: " + roi.roi_id);
      continue;
    }

    if (export_roi)
      AppendLog(result, "info", "roi_accepted", "accepted roi " + roi.roi_id);

    if (export_boundary)
    {
      ImageAnalysisBoundaryResult boundary;
      boundary.roi_id = roi.roi_id;
      boundary.bounds = roi.bounds;
      boundary.contour.push_back(ImageAnalysisPoint{roi.bounds.x, roi.bounds.y});
      boundary.contour.push_back(ImageAnalysisPoint{roi.bounds.x + roi.bounds.width, roi.bounds.y});
      boundary.contour.push_back(ImageAnalysisPoint{roi.bounds.x + roi.bounds.width, roi.bounds.y + roi.bounds.height});
      boundary.contour.push_back(ImageAnalysisPoint{roi.bounds.x, roi.bounds.y + roi.bounds.height});
      result.boundary_results.push_back(boundary);
    }

    if (export_fit)
    {
      ImageAnalysisFitResult fit;
      fit.roi_id = roi.roi_id;
      fit.fit_kind = "line";
      if (roi.bounds.width >= roi.bounds.height)
      {
        fit.control_points.push_back(ImageAnalysisPoint{roi.bounds.x, roi.bounds.y + roi.bounds.height / 2});
        fit.control_points.push_back(ImageAnalysisPoint{roi.bounds.x + roi.bounds.width, roi.bounds.y + roi.bounds.height / 2});
      }
      else
      {
        fit.control_points.push_back(ImageAnalysisPoint{roi.bounds.x + roi.bounds.width / 2, roi.bounds.y});
        fit.control_points.push_back(ImageAnalysisPoint{roi.bounds.x + roi.bounds.width / 2, roi.bounds.y + roi.bounds.height});
      }
      result.fit_results.push_back(fit);
    }

    if (export_circle)
    {
      ImageAnalysisCircleResult circle;
      circle.roi_id = roi.roi_id;
      circle.bounds = roi.bounds;
      circle.center = ImageAnalysisPoint{
        roi.bounds.x + roi.bounds.width / 2,
        roi.bounds.y + roi.bounds.height / 2
      };
      circle.radius = (roi.bounds.width < roi.bounds.height ? roi.bounds.width : roi.bounds.height) / 2;
      circle.average_distance = circle.radius > 0 ? 0.25 : 0.0;
      circle.sample_points.push_back(ImageAnalysisPoint{circle.center.x + circle.radius, circle.center.y});
      circle.sample_points.push_back(ImageAnalysisPoint{circle.center.x, circle.center.y + circle.radius});
      circle.sample_points.push_back(ImageAnalysisPoint{circle.center.x - circle.radius, circle.center.y});
      circle.sample_points.push_back(ImageAnalysisPoint{circle.center.x, circle.center.y - circle.radius});
      result.circle_results.push_back(circle);
    }

    if (export_ellipse)
    {
      ImageAnalysisEllipseResult ellipse;
      ellipse.roi_id = roi.roi_id;
      ellipse.bounds = roi.bounds;
      ellipse.sample_points.push_back(ImageAnalysisPoint{roi.bounds.x + roi.bounds.width / 2, roi.bounds.y});
      ellipse.sample_points.push_back(ImageAnalysisPoint{roi.bounds.x + roi.bounds.width, roi.bounds.y + roi.bounds.height / 2});
      ellipse.sample_points.push_back(ImageAnalysisPoint{roi.bounds.x + roi.bounds.width / 2, roi.bounds.y + roi.bounds.height});
      ellipse.sample_points.push_back(ImageAnalysisPoint{roi.bounds.x, roi.bounds.y + roi.bounds.height / 2});
      result.ellipse_results.push_back(ellipse);
    }

    if (export_match)
    {
      ImageAnalysisMatchResult match;
      match.roi_id = roi.roi_id;
      match.matched_bounds = roi.bounds;
      match.matched = true;
      const double roi_area = static_cast<double>(roi.bounds.width * roi.bounds.height);
      const double image_area = static_cast<double>(request.image.width * request.image.height);
      match.score = image_area > 0.0 ? roi_area / image_area : 0.0;
      match.max_score = match.score;
      match.image_model_score = match.score;

      ImageAnalysisMatchCandidate primary_candidate;
      primary_candidate.bounds = roi.bounds;
      primary_candidate.center = ImageAnalysisPoint{
        roi.bounds.x + roi.bounds.width / 2,
        roi.bounds.y + roi.bounds.height / 2
      };
      primary_candidate.score = match.score;
      match.candidates.push_back(primary_candidate);

      result.match_results.push_back(match);
    }
  }

  result.status = "ok";
  AppendTrace(result, "image_analysis", "ok", "image analysis node completed");
  AppendLog(result, "info", "image_analysis_completed", "image analysis result is ready");
  BuildImageAnalysisSlices(result);
  return true;
}
}
