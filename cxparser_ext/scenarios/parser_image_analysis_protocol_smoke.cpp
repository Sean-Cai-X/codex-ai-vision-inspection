#include <iostream>
#include <string>

#include "../pipeline/parser_image_analysis_node.h"

namespace
{
bool Expect(bool condition, const std::string &message)
{
  if (!condition)
  {
    std::cerr << "[FAIL] " << message << "\n";
    return false;
  }
  return true;
}
}

int main()
{
  cxparser_ext::ImageAnalysisRequest request;
  request.task_id = "image_analysis_minimal";
  request.trace_id = "trace.image_analysis_minimal";
  request.module_name = "cxparser";
  request.route_hint = "default";
  request.image.width = 32;
  request.image.height = 24;
  request.image.channels = 1;
  request.image.bytes_per_channel = 1;
  request.image.bytes.assign(32 * 24, 0);
  request.rois.push_back(cxparser_ext::ImageAnalysisRoiInput{"roi_main", cxparser_ext::ImageAnalysisRect{4, 5, 12, 8}, "inspection"});
  request.operations.push_back(cxparser_ext::iao_roi_extract);
  request.operations.push_back(cxparser_ext::iao_boundary_trace);
  request.operations.push_back(cxparser_ext::iao_fit_line);
  request.operations.push_back(cxparser_ext::iao_fit_circle);
  request.operations.push_back(cxparser_ext::iao_fit_ellipse);
  request.operations.push_back(cxparser_ext::iao_template_match);

  cxparser_ext::ParserImageAnalysisNode node;
  cxparser_ext::ImageAnalysisResult result;
  if (!node.Execute(request, result))
  {
    std::cerr << "[FAIL] image analysis node execution failed\n";
    return 1;
  }

  if (!Expect(result.status == "ok", "result status mismatch")) return 1;
  if (!Expect(result.task_id == request.task_id, "task_id mismatch")) return 1;
  if (!Expect(result.trace_id == request.trace_id, "trace_id mismatch")) return 1;
  if (!Expect(result.image_width == 32 && result.image_height == 24, "image size mismatch")) return 1;
  if (!Expect(result.roi_results.size() == 1, "roi result count mismatch")) return 1;
  if (!Expect(result.boundary_results.size() == 1, "boundary result count mismatch")) return 1;
  if (!Expect(result.fit_results.size() == 1, "fit result count mismatch")) return 1;
  if (!Expect(result.circle_results.size() == 1, "circle result count mismatch")) return 1;
  if (!Expect(result.ellipse_results.size() == 1, "ellipse result count mismatch")) return 1;
  if (!Expect(result.match_results.size() == 1, "match result count mismatch")) return 1;
  if (!Expect(result.boundary_results[0].contour.size() == 4, "boundary contour size mismatch")) return 1;
  if (!Expect(result.fit_results[0].fit_kind == "line", "fit kind mismatch")) return 1;
  if (!Expect(result.circle_results[0].radius == 4, "circle radius mismatch")) return 1;
  if (!Expect(result.circle_results[0].center.x == 10, "circle center x mismatch")) return 1;
  if (!Expect(result.circle_results[0].center.y == 9, "circle center y mismatch")) return 1;
  if (!Expect(result.ellipse_results[0].sample_points.size() == 4, "ellipse sample count mismatch")) return 1;
  if (!Expect(result.match_results[0].matched, "match result should be marked matched")) return 1;
  if (!Expect(result.match_results[0].candidates.size() == 1, "match candidate count mismatch")) return 1;
  if (!Expect(result.match_results[0].max_score == result.match_results[0].score, "match max score mismatch")) return 1;
  if (!Expect(result.match_results[0].image_model_score == result.match_results[0].score, "image model score mismatch")) return 1;
  if (!Expect(!result.trace_entries.empty(), "trace should not be empty")) return 1;
  if (!Expect(!result.log_entries.empty(), "log should not be empty")) return 1;
  if (!Expect(result.multimodal_slices.size() >= 3, "multimodal slices should be exported")) return 1;
  if (!Expect(result.operation_atoms.size() >= 1, "operation atoms should be exported")) return 1;
  if (!Expect(result.multimodal_slices[0].modality == "image", "first slice modality mismatch")) return 1;

  std::cout << "[PASS] task=" << result.task_id
            << " roi=" << result.roi_results[0].roi_id
            << " boundary_points=" << result.boundary_results[0].contour.size()
            << " fit_kind=" << result.fit_results[0].fit_kind
            << " circle_radius=" << result.circle_results[0].radius
            << " ellipse_points=" << result.ellipse_results[0].sample_points.size()
            << " match_candidates=" << result.match_results[0].candidates.size()
            << " match_score=" << result.match_results[0].score
            << "\n";
  return 0;
}
