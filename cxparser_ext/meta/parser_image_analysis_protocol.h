#ifndef CXPARSER_EXT_PARSER_IMAGE_ANALYSIS_PROTOCOL_H
#define CXPARSER_EXT_PARSER_IMAGE_ANALYSIS_PROTOCOL_H

#include "parser_multimodal_slice_types.h"

#include <string>
#include <vector>

namespace cxparser_ext
{
struct ImageAnalysisRect
{
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct ImageAnalysisPoint
{
  int x = 0;
  int y = 0;
};

struct ImageAnalysisImageView
{
  int width = 0;
  int height = 0;
  int channels = 0;
  int bytes_per_channel = 1;
  std::vector<unsigned char> bytes;
};

struct ImageAnalysisRoiInput
{
  std::string roi_id;
  ImageAnalysisRect bounds;
  std::string purpose;
};

enum ImageAnalysisOperation
{
  iao_roi_extract,
  iao_boundary_trace,
  iao_fit_line,
  iao_fit_circle,
  iao_fit_ellipse,
  iao_template_match
};

struct ImageAnalysisRequest
{
  std::string task_id;
  std::string trace_id;
  std::string module_name;
  std::string route_hint;
  ImageAnalysisImageView image;
  std::vector<ImageAnalysisRoiInput> rois;
  std::vector<ImageAnalysisOperation> operations;
};

struct ImageAnalysisRoiResult
{
  std::string roi_id;
  ImageAnalysisRect bounds;
  bool accepted = false;
};

struct ImageAnalysisBoundaryResult
{
  std::string roi_id;
  ImageAnalysisRect bounds;
  std::vector<ImageAnalysisPoint> contour;
};

struct ImageAnalysisFitResult
{
  std::string roi_id;
  std::string fit_kind;
  std::vector<ImageAnalysisPoint> control_points;
};

struct ImageAnalysisCircleResult
{
  std::string roi_id;
  ImageAnalysisRect bounds;
  ImageAnalysisPoint center;
  int radius = 0;
  double average_distance = 0.0;
  std::vector<ImageAnalysisPoint> sample_points;
};

struct ImageAnalysisEllipseResult
{
  std::string roi_id;
  ImageAnalysisRect bounds;
  std::vector<ImageAnalysisPoint> sample_points;
};

struct ImageAnalysisMatchCandidate
{
  ImageAnalysisRect bounds;
  ImageAnalysisPoint center;
  double score = 0.0;
};

struct ImageAnalysisMatchResult
{
  std::string roi_id;
  ImageAnalysisRect matched_bounds;
  double score = 0.0;
  double max_score = 0.0;
  double image_model_score = 0.0;
  bool matched = false;
  std::vector<ImageAnalysisMatchCandidate> candidates;
};

struct ImageAnalysisTraceEntry
{
  int sequence = 0;
  std::string trace_id;
  std::string stage;
  std::string status;
  std::string message;
};

struct ImageAnalysisLogEntry
{
  std::string trace_id;
  std::string level;
  std::string code;
  std::string message;
};

struct ImageAnalysisResult
{
  std::string task_id;
  std::string trace_id;
  std::string route_lane;
  std::string status;
  int image_width = 0;
  int image_height = 0;
  std::vector<ImageAnalysisRoiResult> roi_results;
  std::vector<ImageAnalysisBoundaryResult> boundary_results;
  std::vector<ImageAnalysisFitResult> fit_results;
  std::vector<ImageAnalysisCircleResult> circle_results;
  std::vector<ImageAnalysisEllipseResult> ellipse_results;
  std::vector<ImageAnalysisMatchResult> match_results;
  std::vector<std::string> warnings;
  std::vector<ImageAnalysisTraceEntry> trace_entries;
  std::vector<ImageAnalysisLogEntry> log_entries;
  std::vector<MultimodalSlice> multimodal_slices;
  std::vector<OperationAtom> operation_atoms;
};
}

#endif
