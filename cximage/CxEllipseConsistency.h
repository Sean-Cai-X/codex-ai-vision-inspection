#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>
#include <vector>
#include <opencv2/imgproc.hpp>

// Shared by the production tool and native contract probes. No image, GUI,
// logging or project-specific policy belongs in this calculation.
namespace cxvision { namespace ellipse_consistency {
struct Decision {
  double residual = std::numeric_limits<double>::quiet_NaN();
  bool kept = true;
};
enum class State { bypassed, insufficient_points, reference_failed, evaluated };
struct Result {
  State state = State::bypassed;
  std::vector<Decision> points;
};

inline Result Evaluate(const std::vector<cv::Point2d>& points, double threshold) {
  Result result;
  result.points.resize(points.size());
  if (points.size() < 5) {
    result.state = State::insufficient_points;
    return result;
  }
  if (threshold <= 0.0) return result;
  result.state = State::reference_failed;
  std::vector<cv::Point2f> observations;
  observations.reserve(points.size());
  for (const auto& point : points) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return result;
    observations.emplace_back(static_cast<float>(point.x), static_cast<float>(point.y));
  }
  cv::RotatedRect reference;
  try { reference = cv::fitEllipse(observations); }
  catch (const cv::Exception&) { return result; }
  const double rx = reference.size.width * 0.5;
  const double ry = reference.size.height * 0.5;
  if (!std::isfinite(rx) || !std::isfinite(ry) || rx <= 1.0 || ry <= 1.0 ||
      !std::isfinite(reference.center.x) || !std::isfinite(reference.center.y) ||
      !std::isfinite(reference.angle)) return result;
  const double angle = reference.angle * CV_PI / 180.0;
  const double ca = std::cos(angle), sa = std::sin(angle);
  const double scale = std::max(1.0, (std::abs(rx) + std::abs(ry)) * 0.5);
  for (std::size_t i = 0; i < points.size(); ++i) {
    const double dx = points[i].x - reference.center.x;
    const double dy = points[i].y - reference.center.y;
    const double nx = (ca * dx + sa * dy) / rx;
    const double ny = (-sa * dx + ca * dy) / ry;
    const double distance = std::abs(std::sqrt(nx * nx + ny * ny) - 1.0) * scale;
    result.points[i].residual = distance;
    result.points[i].kept = std::isfinite(distance) && distance <= threshold;
  }
  result.state = State::evaluated;
  return result;
}

// Opt-in initialization candidate. Exact deterministic KNN; no RNG or ANN
// approximation. A neighborhood exclusion is evidence, not a final verdict:
// every original point is evaluated against the resulting reference ellipse.
struct NeighborhoodParams {
  int neighbors = 3;
  double isolation_ratio = 2.5;
  double max_excluded_fraction = 0.3;
  std::size_t min_reference_points = 5;
  std::size_t max_input_points = 4096;
};
struct NeighborhoodDecision {
  double radius = 0.0;
  double relative_radius = 0.0;
  bool reference_support = true;
};
struct NeighborhoodResult {
  Result fit;
  std::vector<NeighborhoodDecision> neighborhoods;
  bool applied = false;
  const char* reason = "invalid_parameters";
};
inline NeighborhoodResult EvaluateNeighborhood(
    const std::vector<cv::Point2d>& points, double threshold,
    const NeighborhoodParams& params = NeighborhoodParams{}) {
  NeighborhoodResult out;
  out.fit.points.resize(points.size());
  out.fit.state = State::reference_failed;
  out.neighborhoods.resize(points.size());
  if (params.neighbors < 2 || !std::isfinite(params.isolation_ratio) ||
      params.isolation_ratio <= 1.0 || !std::isfinite(params.max_excluded_fraction) ||
      params.max_excluded_fraction < 0.0 || params.max_excluded_fraction >= 1.0 ||
      params.min_reference_points < 5 || !std::isfinite(threshold) || threshold <= 0)
    return out;
  if (points.size() > params.max_input_points) {
    out.reason = "input_budget_exceeded"; return out;
  }
  if (points.size() < params.min_reference_points ||
      points.size() <= static_cast<std::size_t>(params.neighbors)) {
    out.fit.state = State::insufficient_points;
    out.reason = "insufficient_neighborhood_support"; return out;
  }
  for (const auto& p : points) if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
    out.reason = "nonfinite_observation"; return out;
  }
  std::vector<std::vector<std::pair<double, std::size_t>>> nearest(points.size());
  for (std::size_t i = 0; i < points.size(); ++i) {
    auto& distances = nearest[i];
    for (std::size_t j = 0; j < points.size(); ++j) if (i != j)
      distances.emplace_back(std::hypot(points[i].x - points[j].x,
                                        points[i].y - points[j].y), j);
    std::partial_sort(distances.begin(), distances.begin() + params.neighbors,
                      distances.end()); // index breaks equal-distance ties
    distances.resize(params.neighbors);
    out.neighborhoods[i].radius = distances.back().first;
  }
  std::vector<cv::Point2d> support;
  for (std::size_t i = 0; i < points.size(); ++i) {
    std::vector<double> localRadii;
    for (const auto& neighbor : nearest[i])
      localRadii.push_back(out.neighborhoods[neighbor.second].radius);
    std::sort(localRadii.begin(), localRadii.end());
    const double localScale = localRadii[localRadii.size()/2];
    if (!std::isfinite(localScale) || localScale <= 0) {
      out.reason = "degenerate_neighborhood"; return out;
    }
    auto& decision = out.neighborhoods[i];
    decision.relative_radius = decision.radius / localScale;
    decision.reference_support = decision.relative_radius <= params.isolation_ratio;
    if (decision.reference_support) support.push_back(points[i]);
  }
  if (support.size() < params.min_reference_points ||
      double(points.size() - support.size()) / points.size() > params.max_excluded_fraction) {
    out.reason = "support_guard_rejected"; return out;
  }
  // Reuse the exact production residual calculation on all observations by
  // fitting the selected support, then mapping queries through that reference.
  std::vector<cv::Point2f> fitPoints;
  for (const auto& p : support) fitPoints.emplace_back(float(p.x), float(p.y));
  cv::RotatedRect reference;
  try { reference = cv::fitEllipse(fitPoints); }
  catch (const cv::Exception&) { out.reason = "reference_fit_failed"; return out; }
  const double rx = reference.size.width * 0.5, ry = reference.size.height * 0.5;
  if (!std::isfinite(rx) || !std::isfinite(ry) || rx <= 1 || ry <= 1 ||
      !std::isfinite(reference.center.x) || !std::isfinite(reference.center.y) ||
      !std::isfinite(reference.angle)) { out.reason = "invalid_reference"; return out; }
  const double a = reference.angle * CV_PI / 180, ca = std::cos(a), sa = std::sin(a);
  for (std::size_t i = 0; i < points.size(); ++i) {
    const double dx = points[i].x-reference.center.x, dy = points[i].y-reference.center.y;
    const double nx = (ca*dx+sa*dy)/rx, ny = (-sa*dx+ca*dy)/ry;
    auto& d = out.fit.points[i];
    d.residual = std::abs(std::hypot(nx,ny)-1)*std::max(1.0,(rx+ry)*0.5);
    d.kept = std::isfinite(d.residual) && d.residual <= threshold;
  }
  out.fit.state = State::evaluated;
  out.applied = true;
  out.reason = support.size() == points.size() ? "all_supported" : "isolated_reference_points_excluded";
  return out;
}

struct Source {
  int scan = -1, slot = -1;
  double x = 0.0, y = 0.0;
};
using SourceIndex = std::map<std::pair<double, double>, std::vector<Source>>;
inline SourceIndex IndexSources(const std::vector<Source>& sources) {
  SourceIndex index;
  for (const auto& source : sources)
    if (std::isfinite(source.x) && std::isfinite(source.y))
      index[{source.x, source.y}].push_back(source);
  return index;
}
struct Identity {
  std::size_t candidates = 0;
  int scan = -1, slot = -1;
};
inline Identity Resolve(const SourceIndex& index, double x, double y) {
  Identity identity;
  if (!std::isfinite(x) || !std::isfinite(y)) return identity;
  const auto found = index.find({x, y});
  if (found == index.end()) return identity;
  identity.candidates = found->second.size();
  if (identity.candidates == 1) {
    identity.scan = found->second.front().scan;
    identity.slot = found->second.front().slot;
  }
  return identity;
}
}} // namespace cxvision::ellipse_consistency
