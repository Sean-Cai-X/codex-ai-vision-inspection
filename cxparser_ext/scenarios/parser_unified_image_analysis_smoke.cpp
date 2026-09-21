#include <cmath>
#include <iostream>

#include "../pipeline/parser_cxcore_classical_adapter.h"
#include "../pipeline/parser_unified_entry.h"

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

bool NearlyEqual(double lhs, double rhs, double eps = 1e-9)
{
  return std::fabs(lhs - rhs) <= eps;
}
}

int main()
{
  cxparser_ext::ImageAnalysisRequest request;
  request.task_id = "image_analysis_unified";
  request.trace_id = "trace.image_analysis_unified";
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

  cxparser_ext::ParserUnifiedEntry entry;
  if (!entry.SubmitImageAnalysisTask(request))
  {
    std::cerr << "[FAIL] SubmitImageAnalysisTask failed\n";
    return 1;
  }

  if (!entry.ExecuteMainThreadCycle())
  {
    std::cerr << "[FAIL] ExecuteMainThreadCycle failed for image analysis unified smoke\n";
    return 1;
  }

  const cxparser_ext::ImageAnalysisResult *result = entry.FindImageAnalysisResult(request.task_id);
  if (!result)
  {
    std::cerr << "[FAIL] unified entry could not find image analysis result\n";
    return 1;
  }

  if (!Expect(result->status == "ok", "result status mismatch")) return 1;
  if (!Expect(result->route_lane == "default", "route lane mismatch")) return 1;
  if (!Expect(result->roi_results.size() == 1, "roi result count mismatch")) return 1;
  if (!Expect(result->boundary_results.size() == 1, "boundary result count mismatch")) return 1;
  if (!Expect(result->fit_results.size() == 1, "fit result count mismatch")) return 1;
  if (!Expect(result->circle_results.size() == 1, "circle result count mismatch")) return 1;
  if (!Expect(result->ellipse_results.size() == 1, "ellipse result count mismatch")) return 1;
  if (!Expect(result->match_results.size() == 1, "match result count mismatch")) return 1;
  if (!Expect(result->trace_entries.size() >= 2, "unified trace entries should include main thread trace")) return 1;
  if (!Expect(result->log_entries.size() >= 2, "unified log entries should include main thread log")) return 1;

  const cxcore::BaselineFeatureSampleV1 classical =
    cxparser_ext::ConvertToCxcoreBaselineSample(request, *result);

  if (!Expect(classical.roi_id == "roi_main", "classical roi_id mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.roi_x, 4.0), "classical roi_x mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.roi_y, 5.0), "classical roi_y mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.roi_w, 12.0), "classical roi_w mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.roi_h, 8.0), "classical roi_h mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.line_w_points_count, 2.0), "classical fit point count mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.circle_radius, 4.0), "classical circle radius mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.circle_center_x, 10.0), "classical circle center x mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.circle_center_y, 9.0), "classical circle center y mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.match_candidate_count, 1.0), "classical match candidate count mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.match_best_score, result->match_results[0].score), "classical match score mismatch")) return 1;
  if (!Expect(result->match_results[0].candidates.size() == 1, "unified match candidate count mismatch")) return 1;
  if (!Expect(NearlyEqual(classical.image_model_score, result->match_results[0].image_model_score),
              "classical image model score mismatch")) return 1;

  const cxparser_ext::ParserMainThreadTick &tick = entry.GetLastTick();
  std::cout << "[PASS] task=" << result->task_id
            << " status=" << result->status
            << " route_lane=" << result->route_lane
            << " roi=" << classical.roi_id
            << " boundary_count=" << result->boundary_results.size()
            << " fit_kind=" << result->fit_results[0].fit_kind
            << " circle_radius=" << classical.circle_radius
            << " ellipse_points=" << result->ellipse_results[0].sample_points.size()
            << " match_candidates=" << result->match_results[0].candidates.size()
            << " match_score=" << classical.match_best_score
            << " main_thread=" << tick.thread_name
            << " cycle=" << tick.cycle_index
            << "\n";
  return 0;
}
