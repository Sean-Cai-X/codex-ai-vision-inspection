#include "CxUnifiedLog.h"
#include "FastMatch.h"
#include "CxTorchResultProjector.h"
#include "FindSegmentation.h"
#include "TorchTask.h"
#include "FindObject.h"
#include "PolylineShape.h"

#include "ImageAnnotationLayer.h"
#include "imagemanager.h"
#include "pch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <queue>
#include <set>
#include "nanoflann.hpp"
#include <opencv2/imgproc.hpp>
#if defined USE_AI
#include "mlpackrun.h"
#endif

namespace {
struct NormalTraceCandidate {
  cv::Point point;
  // Integer coordinates are retained only for topology/binning.  The original
  // FindLine conclusion is the measurement and must remain sub-pixel for the
  // learned model and its normal A/B pair.
  cv::Point2f source_point;
  // Estimated only from neighbouring selected FindLine conclusions.  ANN
  // and Dijkstra use this value-preserving local slope; image gradients are
  // never allowed to create or redirect a topology node.
  cv::Point2f tangent;
  cv::Point2f normal;
  int source_scan_index = -1;
  int domain_polarity = 0;
};

std::string NormalTraceAnnEvidenceSummary(
    const FastMatch::NormalTraceEvidence &evidence) {
  static const char *names[4] = {"top", "bottom", "left", "right"};
  std::string summary;
  for (int direction = 0; direction < 4; ++direction) {
    if (!summary.empty())
      summary += " ";
    summary += std::string(names[direction]) + "={edges=" +
        std::to_string(evidence.ann_edge_counts[direction]) +
        ",components=" +
        std::to_string(evidence.ann_component_counts[direction]) +
        ",selected=" +
        std::to_string(evidence.ann_selected_point_counts[direction]) +
        ",coverage=" +
        std::to_string(evidence.ann_selected_coverage[direction]) + "}";
  }
  return summary;
}

std::string NormalTraceAnchorPrefilterSummary(
    const FastMatch::NormalTraceEvidence &evidence) {
  static const char *names[4] = {"top", "bottom", "left", "right"};
  std::string summary = "source=findline_selected_conclusions generated=0";
  for (int direction = 0; direction < 4; ++direction) {
    summary += " " + std::string(names[direction]) + "={selected=" +
        std::to_string(evidence.anchor_near_candidate_counts[direction]) +
        ",valid=" +
        std::to_string(evidence.anchor_prefilter_accepted_counts[direction]) +
        ",invalid_slope=" +
        std::to_string(evidence.anchor_slope_rejected_counts[direction]) +
        ",invalid_domain_normal=" +
        std::to_string(evidence.anchor_normal_rejected_counts[direction]) + "}";
  }
  return summary;
}

struct NormalTraceKeyPoint {
  NormalTraceCandidate candidate;
  int direction = -1;
  int bin = 0;
  float u = 0.0f;
  float v = 0.0f;
  float anchor_distance = 0.0f;
  float anchor_normal_distance = 0.0f;
  float anchor_slope_alignment = 0.0f;
  float anchor_normal_alignment = 0.0f;
};

struct NormalTraceSourceConclusion {
  cv::Point point;
  cv::Point2f source_point;
  cv::Point2f domain_normal;
  int direction = -1;
  int scan_index = -1;
};

struct NormalTraceAnchorFrame {
  cv::Point2f point;
  cv::Point2f tangent;
  cv::Point2f scan_normal;
  int direction = -1;
  int scan_index = -1;
  int domain_polarity = 0;
  bool tangent_valid = false;
  bool scan_normal_valid = false;
};

struct NormalTraceAnnPointCloud {
  std::vector<cv::Point2f> points;
  std::size_t kdtree_get_point_count() const { return points.size(); }
  float kdtree_get_pt(std::size_t index, std::size_t dimension) const {
    return dimension == 0 ? points[index].x : points[index].y;
  }
  template <typename BBOX> bool kdtree_get_bbox(BBOX&) const { return false; }
};

using NormalTraceAnnIndex = nanoflann::KDTreeSingleIndexAdaptor<
    nanoflann::L2_Simple_Adaptor<float, NormalTraceAnnPointCloud>,
    NormalTraceAnnPointCloud, 2>;

cv::Point2f NormalTraceLocalPoint(const cv::Point &point, double centerX,
                                  double centerY, double cosAngle,
                                  double sinAngle) {
  const double dx = static_cast<double>(point.x) - centerX;
  const double dy = static_cast<double>(point.y) - centerY;
  return cv::Point2f(static_cast<float>(cosAngle * dx + sinAngle * dy),
                     static_cast<float>(-sinAngle * dx + cosAngle * dy));
}

std::array<std::vector<NormalTraceAnchorFrame>, 4> CollectNormalTraceAnchors(
    const FastMatch &source, int &availableSides) {
  std::array<std::vector<NormalTraceAnchorFrame>, 4> anchors;
  availableSides = 0;
  for (int direction = 0; direction < 4; ++direction) {
    const FastMatch::DirectionalProbeEvidence &side =
        source.getdirectionalprobeevidence(direction);
    cv::Point2f canonicalScanNormal;
    bool canonicalScanNormalValid = false;
    for (std::size_t i = 0; i < side.selected_point_by_scan.size() &&
                            i < side.selected_point_valid_by_scan.size();
         ++i) {
      if (side.selected_point_valid_by_scan[i] == 0)
        continue;
      const CxShapePoint &point = side.selected_point_by_scan[i];
      NormalTraceAnchorFrame frame;
      frame.point = cv::Point2f(static_cast<float>(point.x),
                                static_cast<float>(point.y));
      frame.direction = direction;
      frame.scan_index = static_cast<int>(i);
      frame.domain_polarity = side.params.method == 0 ? 1 : -1;
      if (i < side.scan_lines.size()) {
        const FastMatch::DirectionalProbeScanLine &line = side.scan_lines[i];
        frame.scan_normal = cv::Point2f(
            static_cast<float>(line.p1.x - line.p0.x),
            static_cast<float>(line.p1.y - line.p0.y));
        const float length = std::sqrt(frame.scan_normal.dot(frame.scan_normal));
        if (length > 1e-6f) {
          frame.scan_normal *= 1.0f / length;
          // Some FindLine scan collections expose a geometrically identical
          // line with reversed p0/p1 ordering.  That ordering is not edge
          // polarity. Canonicalise the scan axis inside one physical domain,
          // then apply the saved FindLine method polarity below.
          if (!canonicalScanNormalValid) {
            canonicalScanNormal = frame.scan_normal;
            canonicalScanNormalValid = true;
          } else if (frame.scan_normal.dot(canonicalScanNormal) < 0.0f) {
            frame.scan_normal *= -1.0f;
          }
          frame.scan_normal_valid = true;
        }
      }
      anchors[direction].push_back(frame);
    }
    // The local boundary slope is estimated exclusively from adjacent selected
    // FindLine conclusions. Endpoints use a
    // one-sided estimate; interior anchors use the wider previous-to-next
    // chord, which is less sensitive to one noisy Gauge Line conclusion.
    for (std::size_t i = 0; i < anchors[direction].size(); ++i) {
      int previous = static_cast<int>(i) - 1;
      while (previous >= 0 &&
             cv::norm(anchors[direction][i].point -
                      anchors[direction][static_cast<std::size_t>(previous)].point) < 0.5f)
        --previous;
      std::size_t next = i + 1;
      while (next < anchors[direction].size() &&
             cv::norm(anchors[direction][next].point -
                      anchors[direction][i].point) < 0.5f)
        ++next;
      cv::Point2f tangent;
      if (previous >= 0 && next < anchors[direction].size())
        tangent = anchors[direction][next].point -
                  anchors[direction][static_cast<std::size_t>(previous)].point;
      else if (next < anchors[direction].size())
        tangent = anchors[direction][next].point - anchors[direction][i].point;
      else if (previous >= 0)
        tangent = anchors[direction][i].point -
                  anchors[direction][static_cast<std::size_t>(previous)].point;
      const float tangentLength = std::sqrt(tangent.dot(tangent));
      if (tangentLength > 1e-6f) {
        anchors[direction][i].tangent = tangent * (1.0f / tangentLength);
        anchors[direction][i].tangent_valid = true;
      }
    }
    if (!anchors[direction].empty())
      ++availableSides;
  }
  return anchors;
}

bool BuildCompressedNormalTraceDomains(
    const std::array<std::vector<NormalTraceAnchorFrame>, 4> &anchors,
    double centerX, double centerY, double cosAngle, double sinAngle,
    const FastMatch::NormalTraceLearnConfig &cfg,
    std::array<std::vector<NormalTraceKeyPoint>, 4> &keypoints,
    FastMatch::NormalTraceEvidence &evidence) {
  evidence.anchor_normal_band_px = 0.0;
  const int overlapRadius2 =
      std::max(0, cfg.domain_overlap_radius_px * cfg.domain_overlap_radius_px);
  const int binSize = std::max(1, cfg.xy_compression_bin_px);
  for (int direction = 0; direction < 4; ++direction) {
    std::vector<NormalTraceKeyPoint> original;
    for (const NormalTraceAnchorFrame &anchor : anchors[direction]) {
      ++evidence.anchor_near_candidate_counts[direction];
      if (!anchor.tangent_valid) {
        ++evidence.anchor_slope_rejected_counts[direction];
        continue;
      }
      if (!anchor.scan_normal_valid || anchor.domain_polarity == 0) {
        ++evidence.anchor_normal_rejected_counts[direction];
        continue;
      }
      NormalTraceKeyPoint key;
      key.candidate.source_point = anchor.point;
      key.candidate.point =
          cv::Point(static_cast<int>(std::lround(anchor.point.x)),
                    static_cast<int>(std::lround(anchor.point.y)));
      key.candidate.normal =
          anchor.scan_normal * static_cast<float>(anchor.domain_polarity);
      key.candidate.tangent = anchor.tangent;
      key.candidate.source_scan_index = anchor.scan_index;
      key.candidate.domain_polarity = anchor.domain_polarity;
      key.direction = direction;
      key.anchor_distance = 0.0f;
      key.anchor_normal_distance = 0.0f;
      key.anchor_slope_alignment = 1.0f;
      key.anchor_normal_alignment = 1.0f;
      const cv::Point2f local = NormalTraceLocalPoint(
          key.candidate.point, centerX, centerY, cosAngle, sinAngle);
      const float tangent = direction < 2 ? local.x : local.y;
      key.bin = static_cast<int>(std::floor(tangent / binSize));
      key.u = local.x;
      key.v = local.y;
      original.push_back(key);
      ++evidence.anchor_prefilter_accepted_counts[direction];
    }

    // De-duplication is value-preserving: every group publishes one medoid
    // that is an actual FindLine conclusion. No averaged/synthetic conclusion
    // coordinate is allowed into the graph.
    std::vector<std::vector<NormalTraceKeyPoint>> duplicateGroups;
    for (const NormalTraceKeyPoint &key : original) {
      bool assigned = false;
      for (std::vector<NormalTraceKeyPoint> &group : duplicateGroups) {
        for (const NormalTraceKeyPoint &member : group) {
          const int dx = key.candidate.point.x - member.candidate.point.x;
          const int dy = key.candidate.point.y - member.candidate.point.y;
          if (dx * dx + dy * dy <= overlapRadius2) {
            group.push_back(key);
            assigned = true;
            break;
          }
        }
        if (assigned)
          break;
      }
      if (!assigned)
        duplicateGroups.push_back({key});
    }
    const auto medoid = [](const std::vector<NormalTraceKeyPoint> &group) {
      std::size_t best = 0;
      double bestDistance = std::numeric_limits<double>::infinity();
      for (std::size_t i = 0; i < group.size(); ++i) {
        double distance = 0.0;
        for (const NormalTraceKeyPoint &other : group)
          distance += cv::norm(group[i].candidate.point - other.candidate.point);
        if (distance < bestDistance) {
          bestDistance = distance;
          best = i;
        }
      }
      return group[best];
    };
    std::vector<NormalTraceKeyPoint> retained;
    retained.reserve(duplicateGroups.size());
    for (const std::vector<NormalTraceKeyPoint> &group : duplicateGroups)
      retained.push_back(medoid(group));
    evidence.domain_deduplicated_counts[direction] =
        static_cast<int>(retained.size());
    for (const NormalTraceKeyPoint &key : retained) {
      evidence.domain_points.push_back(
          {static_cast<double>(key.candidate.point.x),
           static_cast<double>(key.candidate.point.y)});
      evidence.domain_points_by_direction[direction].push_back(
          {static_cast<double>(key.candidate.point.x),
           static_cast<double>(key.candidate.point.y)});
    }

    std::map<int, std::vector<NormalTraceKeyPoint>> buckets;
    for (const NormalTraceKeyPoint &key : retained)
      buckets[key.bin].push_back(key);
    for (auto &entry : buckets) {
      // XY compression also selects an original medoid. Dijkstra therefore
      // returns only stable source conclusions; interpolation is a separate
      // render/model derivative and never becomes measurement evidence.
      keypoints[direction].push_back(medoid(entry.second));
    }
    const bool reverse = direction == 1 || direction == 2;
    std::sort(keypoints[direction].begin(), keypoints[direction].end(),
              [reverse](const NormalTraceKeyPoint &a,
                        const NormalTraceKeyPoint &b) {
                if (a.bin != b.bin)
                  return reverse ? a.bin > b.bin : a.bin < b.bin;
                return a.candidate.source_scan_index <
                       b.candidate.source_scan_index;
              });
    evidence.compressed_keypoint_counts[direction] =
        static_cast<int>(keypoints[direction].size());
    for (const NormalTraceKeyPoint &key : keypoints[direction])
      evidence.compressed_points_by_direction[direction].push_back(
          {static_cast<double>(key.candidate.point.x),
           static_cast<double>(key.candidate.point.y)});
  }
  return true;
}

bool TraceCompressedNormalTraceDomain(
    const std::vector<NormalTraceKeyPoint> &nodes,
    const FastMatch::NormalTraceLearnConfig &cfg,
    std::vector<cv::Point> &trace, std::string &reason,
    int &annEdgeCount, int &annComponentCount,
    int &annSelectedPointCount, double &annSelectedCoverage,
    std::vector<CxShapePoint> &annSelectedPoints) {
  trace.clear();
  annEdgeCount = 0;
  annComponentCount = 0;
  annSelectedPointCount = 0;
  annSelectedCoverage = 0.0;
  annSelectedPoints.clear();
  if (nodes.size() < static_cast<std::size_t>(cfg.min_keypoints_per_domain)) {
    reason = "COMPRESSED_KEYPOINTS_TOO_FEW";
    return false;
  }
  std::vector<float> distance(nodes.size(), std::numeric_limits<float>::infinity());
  std::vector<int> previous(nodes.size(), -1);
  using QueueNode = std::pair<float, int>;
  std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> queue;
  const float gapWeight =
      std::max(0, cfg.dijkstra_gap_cost_weight_permille) / 1000.0f;
  const float maxGap = static_cast<float>(std::max(
      6, cfg.dijkstra_max_trace_gap_px * cfg.xy_compression_bin_px +
             cfg.xy_compression_bin_px * 2));
  const int maxForwardNeighbours =
      std::clamp(cfg.dijkstra_knn_neighbors, 1, 32);
  // ANN radius owns neighbourhood re-clustering.  Do not silently cap it by
  // the legacy adjacent-bin trace gap: the default 32 px radius must remain
  // effective when compression bins are sparse.  Long hops are still
  // discouraged by Dijkstra's gap cost.
  const float annRadius =
      static_cast<float>(std::max(6, cfg.ann_search_radius_px));
  // Sparse/weak boundaries often leave one missing compression band near a
  // corner.  Query up to 2x the base radius so ANN can re-cluster those bands;
  // Dijkstra's gap cost still makes the longer bridge lose whenever a dense
  // local route exists. This is the robust default path, not a PASS bypass.
  const float annConnectivityRadius = annRadius * 2.0f;
  const float tangentCos = std::cos(static_cast<float>(
      std::clamp(cfg.ann_tangent_deviation_deg, 1, 89)) *
      static_cast<float>(CV_PI / 180.0));
  const float normalCos = std::cos(static_cast<float>(
      std::clamp(cfg.ann_normal_deviation_deg, 1, 89)) *
      static_cast<float>(CV_PI / 180.0));

  // ANN establishes connectivity between value-preserving compressed
  // FindLine conclusions. There are no image-derived pixels in this graph.
  // Dijkstra may select/de-duplicate a track but cannot create a conclusion.
  NormalTraceAnnPointCloud cloud;
  cloud.points.reserve(nodes.size());
  for (const NormalTraceKeyPoint& node : nodes)
    cloud.points.emplace_back(static_cast<float>(node.candidate.point.x),
                              static_cast<float>(node.candidate.point.y));
  NormalTraceAnnIndex ann(2, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
  ann.buildIndex();
  std::vector<std::vector<int>> graph(nodes.size());
  std::vector<std::vector<int>> undirected(nodes.size());
  const std::size_t queryCount = std::min<std::size_t>(
      nodes.size(), static_cast<std::size_t>(std::max(16, maxForwardNeighbours * 8 + 1)));
  struct AnnEdgeCandidate {
    float score = 0.0f;
    float gap = 0.0f;
    int next = -1;
  };
  const float relaxedTangentCos = std::cos(static_cast<float>(
      std::min(89, std::clamp(cfg.ann_tangent_deviation_deg, 1, 89) + 20)) *
      static_cast<float>(CV_PI / 180.0));
  const float relaxedNormalCos = std::cos(static_cast<float>(
      std::min(89, std::clamp(cfg.ann_normal_deviation_deg, 1, 89) + 20)) *
      static_cast<float>(CV_PI / 180.0));
  const float anchorRadius =
      static_cast<float>(std::max(1, cfg.anchor_neighborhood_radius_px));
  for (std::size_t current = 0; current < nodes.size(); ++current) {
    std::vector<unsigned int> indices(queryCount);
    std::vector<float> squaredDistances(queryCount);
    const float query[2] = {cloud.points[current].x, cloud.points[current].y};
    const std::size_t found = ann.knnSearch(query, queryCount, indices.data(),
                                            squaredDistances.data());
    std::vector<AnnEdgeCandidate> candidates;
    for (std::size_t foundIndex = 0; foundIndex < found; ++foundIndex) {
      const std::size_t next = static_cast<std::size_t>(indices[foundIndex]);
      if (next <= current || next >= nodes.size())
        continue; // local UV ordering is the directed trace order.
      // XY compression retains several alternatives in one tangent bin. They
      // compete for the path; they are not successive trace samples. Linking
      // same-bin alternatives creates the periodic transverse hooks seen in
      // complex texture and must therefore be forbidden.
      if (nodes[next].bin == nodes[current].bin)
        continue;
      const float gap = std::sqrt(std::max(0.0f, squaredDistances[foundIndex]));
      if (gap < 1.0f || gap > annConnectivityRadius)
        continue;

      const cv::Point2f edgeDirection =
          (nodes[next].candidate.point - nodes[current].candidate.point) *
          (1.0f / gap);
      const cv::Point2f currentNormal = nodes[current].candidate.normal;
      const cv::Point2f nextNormal = nodes[next].candidate.normal;
      // The line-body direction comes from adjacent accepted FindLine
      // conclusions, not from Sobel and not merely from a perpendicular of a
      // possibly reversed Gauge scan segment.
      cv::Point2f currentTangent = nodes[current].candidate.tangent;
      cv::Point2f nextTangent = nodes[next].candidate.tangent;
      if (currentTangent.dot(currentTangent) <= 1e-6f)
        currentTangent = cv::Point2f(-currentNormal.y, currentNormal.x);
      if (nextTangent.dot(nextTangent) <= 1e-6f)
        nextTangent = cv::Point2f(-nextNormal.y, nextNormal.x);
      const float currentTangentAlignment =
          std::abs(currentTangent.dot(edgeDirection));
      const float nextTangentAlignment =
          std::abs(nextTangent.dot(edgeDirection));
      // Domain polarity is part of the original FindLine conclusion. Opposite
      // signed normals are different semantic tracks and must never be joined.
      const float normalAlignment = currentNormal.dot(nextNormal);
      const bool longBridge = gap > maxGap;
      const float requiredTangent =
          longBridge ? tangentCos : relaxedTangentCos;
      const float requiredNormal = longBridge ? normalCos : relaxedNormalCos;
      if (currentTangentAlignment < requiredTangent ||
          nextTangentAlignment < requiredTangent ||
          normalAlignment < requiredNormal)
        continue;

      const float anchorDistance =
          0.5f * (nodes[current].anchor_distance + nodes[next].anchor_distance);
      if (longBridge && anchorDistance > anchorRadius * 1.25f)
        continue;
      AnnEdgeCandidate candidate;
      candidate.gap = gap;
      candidate.next = static_cast<int>(next);
      candidate.score =
          gap +
          annRadius * ((1.0f - currentTangentAlignment) +
                       (1.0f - nextTangentAlignment)) *
              0.5f +
          annRadius * (1.0f - normalAlignment) * 0.5f +
          std::min(anchorDistance, anchorRadius * 2.0f) * 0.25f;
      candidates.push_back(candidate);
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const AnnEdgeCandidate &a, const AnnEdgeCandidate &b) {
                return a.score != b.score ? a.score < b.score
                                          : (a.gap != b.gap ? a.gap < b.gap
                                                            : a.next < b.next);
              });
    const std::size_t keep = std::min<std::size_t>(
        static_cast<std::size_t>(maxForwardNeighbours), candidates.size());
    for (std::size_t candidateIndex = 0; candidateIndex < keep;
         ++candidateIndex) {
      const int next = candidates[candidateIndex].next;
      graph[current].push_back(next);
      undirected[current].push_back(next);
      undirected[static_cast<std::size_t>(next)].push_back(
          static_cast<int>(current));
      ++annEdgeCount;
    }
  }


  // Retain the ANN component that is both spatially complete and supported by
  // the selected FindLine conclusions. Span alone is insufficient in complex
  // texture: a long parallel scratch can otherwise outrank the real boundary.
  std::vector<int> component(nodes.size(), -1);
  int componentCount = 0;
  int selectedComponent = -1;
  double selectedScore = -std::numeric_limits<double>::infinity();
  const float fullSpan = std::max(
      1.0f, std::abs(nodes.back().u - nodes.front().u) +
                std::abs(nodes.back().v - nodes.front().v));
  const float minimumAnchorSupport = std::max(
      0.35f,
      std::min(0.75f,
               static_cast<float>(cfg.ann_min_component_coverage_percent) /
                   100.0f * 0.75f));
  for (std::size_t seed = 0; seed < nodes.size(); ++seed) {
    if (component[seed] >= 0)
      continue;
    std::vector<int> stack = {static_cast<int>(seed)};
    component[seed] = componentCount;
    std::vector<int> members;
    float minU = nodes[seed].u, maxU = nodes[seed].u;
    float minV = nodes[seed].v, maxV = nodes[seed].v;
    double anchorDistanceSum = 0.0;
    int anchorSupportedCount = 0;
    while (!stack.empty()) {
      const int current = stack.back();
      stack.pop_back();
      members.push_back(current);
      const NormalTraceKeyPoint &node =
          nodes[static_cast<std::size_t>(current)];
      minU = std::min(minU, node.u);
      maxU = std::max(maxU, node.u);
      minV = std::min(minV, node.v);
      maxV = std::max(maxV, node.v);
      anchorDistanceSum += node.anchor_distance;
      if (node.anchor_distance <= anchorRadius)
        ++anchorSupportedCount;
      for (int next : undirected[static_cast<std::size_t>(current)]) {
        if (component[static_cast<std::size_t>(next)] < 0) {
          component[static_cast<std::size_t>(next)] = componentCount;
          stack.push_back(next);
        }
      }
    }
    const float coverage = std::min(
        1.0f, (std::abs(maxU - minU) + std::abs(maxV - minV)) / fullSpan);
    const double meanAnchorDistance =
        members.empty() ? std::numeric_limits<double>::infinity()
                        : anchorDistanceSum /
                              static_cast<double>(members.size());
    const float anchorSupport =
        members.empty()
            ? 0.0f
            : static_cast<float>(anchorSupportedCount) /
                  static_cast<float>(members.size());
    if (members.size() >=
            static_cast<std::size_t>(cfg.ann_min_component_points) &&
        coverage * 100.0f >=
            static_cast<float>(cfg.ann_min_component_coverage_percent) &&
        anchorSupport >= minimumAnchorSupport) {
      const double score =
          coverage * 100000.0 + static_cast<double>(anchorSupport) * 50000.0 -
          meanAnchorDistance * 100.0 + static_cast<double>(members.size());
      if (score > selectedScore) {
        selectedScore = score;
        selectedComponent = componentCount;
      }
    }
    ++componentCount;
  }

  annComponentCount = componentCount;
  if (selectedComponent < 0) {
    reason = "ANN_DOMAIN_COMPONENT_MISSING_components=" +
             std::to_string(componentCount) + "_radius_px=" +
             std::to_string(static_cast<int>(std::lround(annRadius))) +
             "_adaptive_radius_px=" +
             std::to_string(
                 static_cast<int>(std::lround(annConnectivityRadius)));
    return false;
  }
  int startNode = -1;
  int lastNode = -1;
  int startBin = 0;
  int lastBin = 0;
  const float endpointAnchorLimit = anchorRadius * 1.25f;
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    if (component[index] != selectedComponent)
      continue;
    annSelectedPoints.push_back(
        {static_cast<double>(nodes[index].candidate.point.x),
         static_cast<double>(nodes[index].candidate.point.y)});
    // Component membership is retained for diagnostics, but unsupported tails
    // must not become Dijkstra endpoints. This trims texture spurs without
    // assuming that a valid side occupies a fixed fraction of the path.
    if (nodes[index].anchor_distance > endpointAnchorLimit)
      continue;
    if (startNode < 0) {
      startNode = static_cast<int>(index);
      startBin = nodes[index].bin;
    }
    lastNode = static_cast<int>(index);
    lastBin = nodes[index].bin;
  }

  annSelectedPointCount = static_cast<int>(annSelectedPoints.size());
  if (startNode >= 0 && lastNode >= 0) {
    annSelectedCoverage = std::min(
        1.0, (std::abs(nodes[static_cast<std::size_t>(lastNode)].u -
                       nodes[static_cast<std::size_t>(startNode)].u) +
              std::abs(nodes[static_cast<std::size_t>(lastNode)].v -
                       nodes[static_cast<std::size_t>(startNode)].v)) /
                 static_cast<double>(fullSpan));
  }
  if (startNode < 0 || lastNode < 0 || startNode == lastNode) {
    reason = "ANN_DOMAIN_COMPONENT_TOO_SHORT";
    return false;
  }
  // Use all supported alternatives in the first bin as Dijkstra sources and
  // accept any supported alternative in the last bin as a sink. Selecting an
  // arbitrary vector endpoint would make the result depend on same-bin sort
  // order and can force an otherwise unnecessary transverse hop.
  int sourceCount = 0;
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    if (component[index] != selectedComponent || nodes[index].bin != startBin ||
        nodes[index].anchor_distance > endpointAnchorLimit)
      continue;
    distance[index] = 0.0f;
    queue.push({0.0f, static_cast<int>(index)});
    ++sourceCount;
  }
  if (sourceCount == 0) {
    reason = "DIJKSTRA_SUPPORTED_SOURCE_MISSING";
    return false;
  }
  int finalNode = -1;
  int expanded = 0;
  while (!queue.empty()) {
    const auto [currentDistance, current] = queue.top();
    queue.pop();
    if (currentDistance != distance[static_cast<std::size_t>(current)])
      continue;
    if (++expanded > std::max(32, cfg.dijkstra_max_nodes)) {
      reason = "DIJKSTRA_KEYPOINT_BUDGET_EXCEEDED";
      return false;
    }
    if (nodes[static_cast<std::size_t>(current)].bin == lastBin &&
        nodes[static_cast<std::size_t>(current)].anchor_distance <=
            endpointAnchorLimit) {
      finalNode = current;
      break;
    }
    for (int next : graph[static_cast<std::size_t>(current)]) {
      if (component[static_cast<std::size_t>(next)] != selectedComponent)
        continue;
      const float gap = cv::norm(nodes[static_cast<std::size_t>(next)].candidate.point -
                                 nodes[static_cast<std::size_t>(current)].candidate.point);
      const float normalTurn = 1.0f - std::clamp(
          nodes[static_cast<std::size_t>(current)].candidate.normal.dot(
              nodes[static_cast<std::size_t>(next)].candidate.normal),
          -1.0f, 1.0f);
      const cv::Point2f currentNormal =
          nodes[static_cast<std::size_t>(current)].candidate.normal;
      cv::Point2f currentTangent =
          nodes[static_cast<std::size_t>(current)].candidate.tangent;
      if (currentTangent.dot(currentTangent) <= 1e-6f)
        currentTangent = cv::Point2f(-currentNormal.y, currentNormal.x);
      const cv::Point2f edgeDirection =
          (nodes[static_cast<std::size_t>(next)].candidate.point -
           nodes[static_cast<std::size_t>(current)].candidate.point) *
          (1.0f / std::max(1.0f, gap));
      const float tangentMismatch =
          1.0f - std::abs(currentTangent.dot(edgeDirection));
      const float tangentSoftGate = tangentMismatch > (1.0f - tangentCos)
                                         ? tangentMismatch
                                         : tangentMismatch * 0.25f;
      const float normalSoftGate = normalTurn > (1.0f - normalCos)
                                       ? normalTurn
                                       : normalTurn * 0.25f;
      const int skippedBins = std::max(
          0, std::abs(nodes[static_cast<std::size_t>(next)].bin -
                      nodes[static_cast<std::size_t>(current)].bin) -
                 1);
      // A Euclidean shortest path can have the same length whether it visits
      // every conclusion or jumps across several bins. Penalize that jump so
      // Dijkstra preserves consecutive source conclusions whenever available;
      // a real missing bin can still be bridged within the ANN radius.
      const float skippedBinCost =
          static_cast<float>(skippedBins *
                             std::max(1, cfg.xy_compression_bin_px)) *
          0.75f;
      const float cost = (1.0f + gapWeight) * gap +
                         (std::max(0, cfg.dijkstra_turn_cost_weight_permille) / 1000.0f) *
                             maxGap * (normalSoftGate + tangentSoftGate) +
                         skippedBinCost;
      const float alternative = currentDistance + cost;
      if (alternative < distance[static_cast<std::size_t>(next)]) {
        distance[static_cast<std::size_t>(next)] = alternative;
        previous[static_cast<std::size_t>(next)] = current;
        queue.push({alternative, next});
      }
    }
  }
  if (finalNode < 0) {
    int reachableNodes = 0;
    for (float value : distance)
      reachableNodes += std::isfinite(value) ? 1 : 0;
    reason = "DIJKSTRA_ANN_PATH_MISSING_nodes=" +
             std::to_string(nodes.size()) + "_reachable=" +
             std::to_string(reachableNodes) + "_k=" +
             std::to_string(maxForwardNeighbours) + "_gap_px=" +
             std::to_string(static_cast<int>(std::lround(maxGap)));
    return false;
  }
  for (int node = finalNode; node >= 0; node = previous[static_cast<std::size_t>(node)])
    trace.push_back(nodes[static_cast<std::size_t>(node)].candidate.point);
  std::reverse(trace.begin(), trace.end());
  return trace.size() >= 2;
}

bool LearnPatternByNormalTrace(Image &image, FastMatch &source, int learn_x,
                               int learn_y, int learn_w, int learn_h,
                               PointsShape &out_pattern, int &raw_count,
                               int &deduplicated_count, int &trace_count,
                               int &pair_count,
                               FastMatch::NormalTraceEvidence &evidence) {
  raw_count = deduplicated_count = trace_count = pair_count = 0;
  evidence = FastMatch::NormalTraceEvidence{};
  evidence.executed = true;
  cv::Mat source_mat = image.getmat();
  if (source_mat.empty()) { evidence.reason = "IMAGE_EMPTY"; return false; }

  const FastMatch::NormalTraceLearnConfig &cfg = source.getnormaltraceconfig();
  evidence.endpoint_spike_ratio_percent = cfg.endpoint_spike_ratio_percent;
  evidence.junction_tangent_window_points =
      cfg.junction_tangent_window_points;
  evidence.junction_min_cross_angle_deg = cfg.junction_min_cross_angle_deg;
  evidence.junction_max_extrapolation_percent =
      cfg.junction_max_extrapolation_percent;
  evidence.junction_join_spacing_multiplier_percent =
      cfg.junction_join_spacing_multiplier_percent;
  const cv::Rect roi = cv::Rect(learn_x, learn_y, std::max(1, learn_w),
                                std::max(1, learn_h)) &
                       cv::Rect(0, 0, source_mat.cols, source_mat.rows);
  if (roi.width < 3 || roi.height < 3) { evidence.reason = "LEARN_ROI_TOO_SMALL"; return false; }
  const double angleRadians = source.getscanrotation() * CV_PI / 180.0;
  const double cosAngle = std::cos(angleRadians);
  const double sinAngle = std::sin(angleRadians);
  const double centerX = roi.x + roi.width * 0.5;
  const double centerY = roi.y + roi.height * 0.5;
  int availableSides = 0;
  const std::array<std::vector<NormalTraceAnchorFrame>, 4> anchors =
        CollectNormalTraceAnchors(source, availableSides);
  for (std::size_t direction = 0; direction < anchors.size(); ++direction) {
    const auto &directionAnchors = anchors[direction];
    raw_count += static_cast<int>(directionAnchors.size());
    evidence.anchor_points_by_direction[direction].reserve(
        directionAnchors.size());
    for (const NormalTraceAnchorFrame &anchor : directionAnchors) {
      evidence.anchor_points_by_direction[direction].push_back(
          {anchor.point.x, anchor.point.y});
    }
  }
  evidence.directional_side_count = availableSides;
  if (availableSides != 4) {
    evidence.reason = "DIRECTIONAL_SELECTED_ANCHORS_INSUFFICIENT";
    return false;
  }
  std::array<std::vector<NormalTraceKeyPoint>, 4> keypoints;
  BuildCompressedNormalTraceDomains(anchors, centerX, centerY, cosAngle,
                                    sinAngle, cfg, keypoints, evidence);
  deduplicated_count = static_cast<int>(evidence.domain_points.size());
  // Sparse directional evidence is a quality gate, not an invitation to add
  // image-derived or cross-domain points.  Stop before ANN/Dijkstra whenever
  // one physical side cannot supply the operator-controlled minimum number of
  // valid source conclusions.  The compatibility template remains available
  // to the caller as an explicit fallback.
  bool domainGateFailed = false;
  std::string selectedCounts;
  std::string validCounts;
  for (int direction = 0; direction < 4; ++direction) {
    if (!selectedCounts.empty()) {
      selectedCounts += ",";
      validCounts += ",";
    }
    selectedCounts += std::to_string(anchors[direction].size());
    validCounts += std::to_string(keypoints[direction].size());
    if (keypoints[direction].size() <
        static_cast<std::size_t>(cfg.min_keypoints_per_domain))
      domainGateFailed = true;
  }
  if (domainGateFailed) {
    evidence.reason = "DOMAIN_KEYPOINT_GATE_FAILED_selected=" +
                      selectedCounts + "_valid=" + validCounts + "_min=" +
                      std::to_string(cfg.min_keypoints_per_domain);
    return false;
  }
  if (deduplicated_count < 4) {
    evidence.reason = "DEDUPLICATED_DOMAIN_TOO_SMALL";
    return false;
  }
  std::vector<cv::Point> trace;
  const int traversal[4] = {0, 3, 1, 2}; // Top -> Right -> Bottom -> Left.
  std::array<std::vector<cv::Point>, 4> segmentTraces;
  for (int segment = 0; segment < 4; ++segment) {
    const int direction = traversal[segment];
    std::string segmentReason;
    if (!TraceCompressedNormalTraceDomain(
            keypoints[direction], cfg, segmentTraces[segment], segmentReason,
            evidence.ann_edge_counts[direction],
            evidence.ann_component_counts[direction],
            evidence.ann_selected_point_counts[direction],
            evidence.ann_selected_coverage[direction],
            evidence.ann_selected_points_by_direction[direction])) {
      evidence.reason = "DOMAIN_" + std::to_string(direction) + "_" +
                        segmentReason;
      return false;
    }
  }

  // A directional scan may hit the adjacent physical edge after passing a
  // corner. This leaves an isolated endpoint jump although the main Dijkstra
  // track is valid. Remove only those endpoint references, using the track's
  // own median spacing; never synthesize a replacement conclusion.
  for (int segment = 0; segment < 4; ++segment) {
    std::vector<cv::Point> &points = segmentTraces[segment];
    if (points.size() < 4)
      continue;
    std::vector<double> gaps;
    gaps.reserve(points.size() - 1);
    for (std::size_t index = 1; index < points.size(); ++index) {
      const double gap = cv::norm(points[index] - points[index - 1]);
      if (gap > 0.0)
        gaps.push_back(gap);
    }
    if (gaps.empty())
      continue;
    const std::size_t middle = gaps.size() / 2;
    std::nth_element(gaps.begin(), gaps.begin() + middle, gaps.end());
    const double medianGap = gaps[middle];
    const double endpointJumpLimit = std::max(
        static_cast<double>(std::max(1, cfg.xy_compression_bin_px) * 3),
        medianGap *
            static_cast<double>(cfg.endpoint_spike_ratio_percent) / 100.0);
    int pruned = 0;
    while (points.size() >= 4 &&
           cv::norm(points[1] - points[0]) > endpointJumpLimit) {
      points.erase(points.begin());
      ++pruned;
    }
    while (points.size() >= 4 &&
           cv::norm(points.back() - points[points.size() - 2]) >
               endpointJumpLimit) {
      points.pop_back();
      ++pruned;
    }
    evidence.endpoint_spike_pruned_counts[traversal[segment]] = pruned;
  }

  // Each directional ANN component may extend beyond a physical corner or
  // cross a nearby texture. Build several evidence-scored joins per corner,
  // then choose all four joins together. A fixed endpoint fraction is brittle
  // for curved/asymmetric parts, while four independent closest-point joins
  // can form a closed but semantically wrong loop.
  struct JoinCandidate {
    int current = -1;
    int next = -1;
    double gap = 0.0;
    double score = 0.0;
  };
  std::array<int, 4> incomingIndex{};
  std::array<int, 4> outgoingIndex{};
  const double baseAllowedJoinGap = std::max(
      8.0, static_cast<double>(cfg.anchor_neighborhood_radius_px) * 0.75);
  const auto sourceTrackLengthAndMedianGap =
      [](const std::vector<NormalTraceKeyPoint> &points) {
        double length = 0.0;
        std::vector<double> gaps;
        gaps.reserve(points.size());
        for (std::size_t i = 1; i < points.size(); ++i) {
          const double gap = cv::norm(points[i].candidate.point -
                                      points[i - 1].candidate.point);
          if (gap > 0.0) {
            length += gap;
            gaps.push_back(gap);
          }
        }
        double medianGap = 0.0;
        if (!gaps.empty()) {
          const std::size_t middle = gaps.size() / 2;
          std::nth_element(gaps.begin(), gaps.begin() + middle, gaps.end());
          medianGap = gaps[middle];
        }
        return std::pair<double, double>(length, medianGap);
      };
  const auto unitTangent = [](const std::vector<cv::Point> &points,
                              std::size_t index) {
    const std::size_t before = index > 1 ? index - 2 : 0;
    const std::size_t after =
        std::min(points.size() - 1, index + static_cast<std::size_t>(2));
    cv::Point2d tangent(
        static_cast<double>(points[after].x - points[before].x),
        static_cast<double>(points[after].y - points[before].y));
    const double length = std::hypot(tangent.x, tangent.y);
    return length > 1e-9 ? tangent * (1.0 / length) : cv::Point2d(0.0, 0.0);
  };
  const auto nearestAnchorDistance =
      [&anchors](int direction, const cv::Point &point) {
        double nearest = std::numeric_limits<double>::infinity();
        for (const NormalTraceAnchorFrame &anchor : anchors[direction]) {
          nearest = std::min(
              nearest,
              std::hypot(static_cast<double>(point.x) - anchor.point.x,
                         static_cast<double>(point.y) - anchor.point.y));
        }
        return nearest;
      };
  std::array<std::vector<JoinCandidate>, 4> joinCandidates;

  for (int segment = 0; segment < 4; ++segment) {
    const int next = (segment + 1) % 4;
    const std::vector<cv::Point> &currentTrace = segmentTraces[segment];
    const std::vector<cv::Point> &nextTrace = segmentTraces[next];
    const auto [currentLength, currentMedianGap] =
        sourceTrackLengthAndMedianGap(keypoints[traversal[segment]]);
    const auto [nextLength, nextMedianGap] =
        sourceTrackLengthAndMedianGap(keypoints[traversal[next]]);
    // Directional gauges commonly stop before a physical corner. The join
    // radius follows the observed conclusion sampling/span instead of a fixed
    // pixel constant. It only permits edges between existing conclusions.
    const double samplingAllowance =
        std::max(currentMedianGap, nextMedianGap) *
        static_cast<double>(cfg.junction_join_spacing_multiplier_percent) /
        100.0;
    const double spanAllowance =
        std::min(currentLength, nextLength) * 0.18;
    const double allowedJoinGap =
        std::max(baseAllowedJoinGap,
                 std::max(samplingAllowance, spanAllowance));
    evidence.domain_join_allowed_gap_px[segment] = allowedJoinGap;
    for (std::size_t currentIndex = 0;
         currentIndex < segmentTraces[segment].size(); ++currentIndex) {
      for (std::size_t nextIndex = 0;
           nextIndex < segmentTraces[next].size(); ++nextIndex) {
        const cv::Point2d delta(
            static_cast<double>(nextTrace[nextIndex].x -
                                currentTrace[currentIndex].x),
            static_cast<double>(nextTrace[nextIndex].y -
                                currentTrace[currentIndex].y));
        const double gap = std::hypot(delta.x, delta.y);
        if (gap > allowedJoinGap)
          continue;
        const double currentProgress = currentTrace.size() > 1
            ? static_cast<double>(currentIndex) /
                  static_cast<double>(currentTrace.size() - 1)
            : 1.0;
        const double nextProgress = nextTrace.size() > 1
            ? static_cast<double>(nextIndex) /
                  static_cast<double>(nextTrace.size() - 1)
            : 0.0;
        const double progressPenalty = (1.0 - currentProgress) + nextProgress;
        double tangentPenalty = 0.0;
        if (gap > 1e-9) {
          const cv::Point2d bridge = delta * (1.0 / gap);
          const cv::Point2d currentTangent =
              unitTangent(currentTrace, currentIndex);
          const cv::Point2d nextTangent = unitTangent(nextTrace, nextIndex);
          tangentPenalty =
              (1.0 - std::clamp(currentTangent.dot(bridge), -1.0, 1.0)) *
                  0.5 +
              (1.0 - std::clamp(bridge.dot(nextTangent), -1.0, 1.0)) *
                  0.5;
          if (currentTangent.dot(bridge) < -0.25 ||
              bridge.dot(nextTangent) < -0.25)
            continue;
        }
        const int currentDirection = traversal[segment];
        const int nextDirection = traversal[next];
        const double currentAnchorDistance =
            nearestAnchorDistance(currentDirection, currentTrace[currentIndex]);
        const double nextAnchorDistance =
            nearestAnchorDistance(nextDirection, nextTrace[nextIndex]);
        const double joinAnchorLimit =
            std::max(4.0,
                     static_cast<double>(cfg.anchor_neighborhood_radius_px) *
                         1.25);
        if (currentAnchorDistance > joinAnchorLimit ||
            nextAnchorDistance > joinAnchorLimit)
          continue;
        const double anchorPenalty =
            (currentAnchorDistance + nextAnchorDistance) /
            std::max(1.0,
                     static_cast<double>(cfg.anchor_neighborhood_radius_px));
        JoinCandidate candidate;
        candidate.current = static_cast<int>(currentIndex);
        candidate.next = static_cast<int>(nextIndex);
        candidate.gap = gap;
        candidate.score =
            gap + allowedJoinGap *
                      (progressPenalty * 0.65 + tangentPenalty * 0.25 +
                       anchorPenalty * 0.45);
        joinCandidates[segment].push_back(candidate);

      }
    }
    std::sort(joinCandidates[segment].begin(),
              joinCandidates[segment].end(),
              [](const JoinCandidate &a, const JoinCandidate &b) {
                if (a.score != b.score)
                  return a.score < b.score;
                if (a.gap != b.gap)
                  return a.gap < b.gap;
                return a.current > b.current;
              });
    constexpr std::size_t kMaximumJoinCandidates = 12;
    if (joinCandidates[segment].size() > kMaximumJoinCandidates)
      joinCandidates[segment].resize(kMaximumJoinCandidates);
    if (joinCandidates[segment].empty()) {
      evidence.reason = "DOMAIN_JOIN_CANDIDATE_MISSING_" +
                        std::to_string(segment) + "_allowed=" +
                        std::to_string(
                            evidence.domain_join_allowed_gap_px[segment]);
      return false;
    }
  }

  double bestCycleScore = std::numeric_limits<double>::infinity();
  for (const JoinCandidate &j0 : joinCandidates[0])
    for (const JoinCandidate &j1 : joinCandidates[1])
      for (const JoinCandidate &j2 : joinCandidates[2])
        for (const JoinCandidate &j3 : joinCandidates[3]) {
          const std::array<JoinCandidate, 4> cycle = {j0, j1, j2, j3};
          std::array<int, 4> cycleIncoming{};
          std::array<int, 4> cycleOutgoing{};
          double retainedCoverage = 0.0;
          bool ordered = true;
          for (int join = 0; join < 4; ++join) {
            cycleOutgoing[join] = cycle[join].current;
            cycleIncoming[(join + 1) % 4] = cycle[join].next;
          }
          for (int segment = 0; segment < 4; ++segment) {
            if (cycleIncoming[segment] < 0 || cycleOutgoing[segment] < 0 ||
                cycleIncoming[segment] > cycleOutgoing[segment]) {
              ordered = false;
              break;
            }
            retainedCoverage +=
                static_cast<double>(cycleOutgoing[segment] -
                                    cycleIncoming[segment] + 1) /
                static_cast<double>(segmentTraces[segment].size());
          }
          if (!ordered)
            continue;
          double cycleScore = j0.score + j1.score + j2.score + j3.score;
          // Prefer a cycle that retains the supported side traces; this is a
          // soft evidence term, not a fixed shape fraction or convexity rule.
          const double averageAllowedJoinGap =
              (evidence.domain_join_allowed_gap_px[0] +
               evidence.domain_join_allowed_gap_px[1] +
               evidence.domain_join_allowed_gap_px[2] +
               evidence.domain_join_allowed_gap_px[3]) /
              4.0;
          cycleScore +=
              averageAllowedJoinGap * (4.0 - retainedCoverage) * 0.35;
          if (cycleScore < bestCycleScore) {
            bestCycleScore = cycleScore;
            incomingIndex = cycleIncoming;
            outgoingIndex = cycleOutgoing;
            for (int join = 0; join < 4; ++join)
              evidence.domain_join_selected_gap_px[join] = cycle[join].gap;
          }
        }
  if (!std::isfinite(bestCycleScore)) {
    evidence.reason = "DOMAIN_JOIN_CYCLE_ORDER_MISSING";
    return false;
  }

  std::array<std::vector<cv::Point>, 4> retainedSegments;
  for (int segment = 0; segment < 4; ++segment) {
    const std::vector<cv::Point> &segmentTrace = segmentTraces[segment];
    const int first = incomingIndex[segment];
    const int last = outgoingIndex[segment];
    if (first < 0 || last < 0 || first > last) {
      evidence.reason = "DOMAIN_TRACE_ORDER_REVERSED_" +
                        std::to_string(segment) + "_incoming=" +
                        std::to_string(first) + "_outgoing=" +
                        std::to_string(last);
      return false;
    }
    for (int pointIndex = first;; ++pointIndex) {
      const cv::Point &point =
          segmentTrace[static_cast<std::size_t>(pointIndex)];
      retainedSegments[segment].push_back(point);
      if (pointIndex == last)
        break;
    }
    ++evidence.trace_segment_count;
  }

  // Validate retained paths against the actual directional conclusion anchors.
  // A geometrically closed cycle is not accepted when it explains only a
  // small subset of the operator-confirmed FindLine observations.
  const double retainedAnchorRadius =
      std::max(4.0,
               static_cast<double>(cfg.anchor_neighborhood_radius_px) * 1.25);
  const double minimumRetainedAnchorCoverage = std::max(
      0.35,
      std::min(0.75,
                static_cast<double>(cfg.ann_min_component_coverage_percent) /
                    100.0 * 0.75));
  int retainedAnchorSupportedTotal = 0;
  int retainedAnchorTotal = 0;
  for (int segment = 0; segment < 4; ++segment) {
    const int direction = traversal[segment];
    int supportedAnchors = 0;
    for (const NormalTraceAnchorFrame &anchor : anchors[direction]) {
      double nearest = std::numeric_limits<double>::infinity();
      for (const cv::Point &point : retainedSegments[segment]) {
        nearest = std::min(
            nearest,
            std::hypot(static_cast<double>(point.x) - anchor.point.x,
                       static_cast<double>(point.y) - anchor.point.y));
      }
      if (nearest <= retainedAnchorRadius)
        ++supportedAnchors;
    }
    const double retainedAnchorCoverage =
        anchors[direction].empty()
            ? 0.0
            : static_cast<double>(supportedAnchors) /
                  static_cast<double>(anchors[direction].size());
    retainedAnchorSupportedTotal += supportedAnchors;
    retainedAnchorTotal += static_cast<int>(anchors[direction].size());
    if (retainedAnchorCoverage < minimumRetainedAnchorCoverage) {
      evidence.reason =
          "DOMAIN_JOIN_ANCHOR_COVERAGE_LOW_direction=" +
          std::to_string(direction) + "_supported=" +
          std::to_string(supportedAnchors) + "_total=" +
          std::to_string(anchors[direction].size()) + "_coverage=" +
          std::to_string(retainedAnchorCoverage);
      return false;
    }
  }
  evidence.selected_anchor_coverage = retainedAnchorTotal > 0
      ? static_cast<double>(retainedAnchorSupportedTotal) /
            static_cast<double>(retainedAnchorTotal)
      : 0.0;
  evidence.gradient_coverage = evidence.selected_anchor_coverage;

  // Concatenate the four Dijkstra-selected tracks as references to original
  // FindLine conclusions. Dijkstra never emits a coordinate not present in a
  // directional selected_point_by_scan list.
  std::vector<NormalTraceSourceConclusion> conclusionPath;
  for (int segment = 0; segment < 4; ++segment) {
    if (retainedSegments[segment].empty()) {
      evidence.reason = "DIRECTIONAL_SELECTED_PATH_EMPTY_" +
                        std::to_string(segment);
      return false;
    }
    const int direction = traversal[segment];
    for (const cv::Point &point : retainedSegments[segment]) {
      const auto found = std::find_if(
          keypoints[direction].begin(), keypoints[direction].end(),
          [&point](const NormalTraceKeyPoint &key) {
            return key.candidate.point == point;
          });
      if (found == keypoints[direction].end()) {
        evidence.reason = "DIJKSTRA_SOURCE_REFERENCE_MISSING_direction=" +
                          std::to_string(direction);
        return false;
      }
      if (!conclusionPath.empty() && conclusionPath.back().point == point)
        continue;
      conclusionPath.push_back(
          {point, found->candidate.source_point, found->candidate.normal, direction,
           found->candidate.source_scan_index});
    }
  }
  if (conclusionPath.size() < 4) {
    evidence.reason = "DIJKSTRA_SELECTED_CONCLUSIONS_TOO_FEW";
    return false;
  }
  evidence.dijkstra_trace_points.reserve(conclusionPath.size());
  for (const NormalTraceSourceConclusion &sourceConclusion : conclusionPath) {
    evidence.dijkstra_trace_points.push_back(
        {static_cast<double>(sourceConclusion.source_point.x),
         static_cast<double>(sourceConclusion.source_point.y)});
    evidence.dijkstra_source_directions.push_back(
        sourceConclusion.direction);
    evidence.dijkstra_source_scans.push_back(sourceConclusion.scan_index);
  }

  // Stretch each selected source segment and reconstruct its junction with the
  // next segment. A direct endpoint chord cuts inside a real corner, so first
  // extrapolate robust endpoint tangents to their constrained intersection.
  // When the tangents are nearly parallel or extrapolate too far, use a cubic
  // tangent blend. Both are derived geometry, never new conclusions.
  std::array<cv::Point2f, 4> derivedJunctionCenters{};
  const auto appendStretchedSegment = [&trace](const cv::Point &from,
                                                 const cv::Point &to) {
    const double length = cv::norm(to - from);
    const int steps = std::max(1, static_cast<int>(std::ceil(length)));
    for (int stepIndex = trace.empty() ? 0 : 1; stepIndex <= steps;
         ++stepIndex) {
      const double t = static_cast<double>(stepIndex) /
                       static_cast<double>(steps);
      const cv::Point point(
          static_cast<int>(std::lround(from.x + (to.x - from.x) * t)),
          static_cast<int>(std::lround(from.y + (to.y - from.y) * t)));
      if (trace.empty() || trace.back() != point)
        trace.push_back(point);
    }
  };
  const auto endpointTangent = [](const std::vector<cv::Point> &points) {
    cv::Point2f tangent(0.0f, 0.0f);
    if (points.size() < 2)
      return tangent;
    for (std::size_t index = 1; index < points.size(); ++index) {
      cv::Point2f delta = points[index] - points[index - 1];
      const float length = std::sqrt(delta.dot(delta));
      if (length > 1e-6f)
        tangent += delta * (1.0f / length);
    }
    const float length = std::sqrt(tangent.dot(tangent));
    if (length > 1e-6f)
      tangent *= 1.0f / length;
    return tangent;
  };
  const auto endpointTurnDegrees = [](const std::vector<cv::Point> &points) {
    if (points.size() < 3)
      return 0.0;
    const cv::Point2f firstDelta = points[1] - points[0];
    const cv::Point2f lastDelta =
        points.back() - points[points.size() - 2];
    const double firstLength = std::sqrt(firstDelta.dot(firstDelta));
    const double lastLength = std::sqrt(lastDelta.dot(lastDelta));
    if (firstLength <= 1e-6 || lastLength <= 1e-6)
      return 0.0;
    const double cosine = std::clamp(
        static_cast<double>(firstDelta.dot(lastDelta)) /
            (firstLength * lastLength),
        -1.0, 1.0);
    return std::acos(cosine) * 180.0 / CV_PI;
  };
  const auto endpointWindow = [&cfg](const std::vector<cv::Point> &points,
                                     bool atEnd) {
    const std::size_t count = std::min(
        static_cast<std::size_t>(cfg.junction_tangent_window_points),
        points.size());
    if (atEnd)
      return std::vector<cv::Point>(points.end() - count, points.end());
    return std::vector<cv::Point>(points.begin(), points.begin() + count);
  };
  const auto cross2d = [](const cv::Point2f &a, const cv::Point2f &b) {
    return a.x * b.y - a.y * b.x;
  };
  const auto appendTangentBlend = [&trace](
      const cv::Point2f &from, const cv::Point2f &to,
      const cv::Point2f &fromTangent, const cv::Point2f &toTangent,
      double allowedGap) {
    const double gap = cv::norm(to - from);
    const float handle = static_cast<float>(
        std::min(std::max(2.0, gap * 0.35), std::max(4.0, allowedGap * 0.5)));
    const cv::Point2f control1 = from + fromTangent * handle;
    const cv::Point2f control2 = to - toTangent * handle;
    const int steps = std::max(2, static_cast<int>(std::ceil(gap * 1.5)));
    for (int stepIndex = trace.empty() ? 0 : 1; stepIndex <= steps;
         ++stepIndex) {
      const float t = static_cast<float>(stepIndex) /
                      static_cast<float>(steps);
      const float oneMinusT = 1.0f - t;
      const cv::Point2f point =
          from * (oneMinusT * oneMinusT * oneMinusT) +
          control1 * (3.0f * oneMinusT * oneMinusT * t) +
          control2 * (3.0f * oneMinusT * t * t) +
          to * (t * t * t);
      const cv::Point rounded(static_cast<int>(std::lround(point.x)),
                              static_cast<int>(std::lround(point.y)));
      if (trace.empty() || trace.back() != rounded)
        trace.push_back(rounded);
    }
  };
  const auto appendIntersectionGuidedBlend = [&trace](
      const cv::Point2f &from, const cv::Point2f &control,
      const cv::Point2f &to) {
    const double length = cv::norm(control - from) + cv::norm(to - control);
    // A quadratic Bezier can move up to twice the average arc step near an
    // endpoint. Sample at 2x control-polygon length so integer rasterization
    // remains 8-connected (maximum consecutive step <= sqrt(2)).
    const int steps =
        std::max(2, static_cast<int>(std::ceil(length * 2.0)));
    for (int stepIndex = trace.empty() ? 0 : 1; stepIndex <= steps;
         ++stepIndex) {
      const float t = static_cast<float>(stepIndex) /
                      static_cast<float>(steps);
      const float oneMinusT = 1.0f - t;
      const cv::Point2f point = from * (oneMinusT * oneMinusT) +
                                control * (2.0f * oneMinusT * t) +
                                to * (t * t);
      const cv::Point rounded(static_cast<int>(std::lround(point.x)),
                              static_cast<int>(std::lround(point.y)));
      if (trace.empty() || trace.back() != rounded)
        trace.push_back(rounded);
    }
  };
  evidence.derived_junction_points.reserve(4);
  for (int segment = 0; segment < 4; ++segment) {
    const int next = (segment + 1) % 4;
    const std::vector<cv::Point> &currentPoints = retainedSegments[segment];
    const std::vector<cv::Point> &nextPoints = retainedSegments[next];
    if (trace.empty())
      appendStretchedSegment(currentPoints.front(), currentPoints.front());
    for (std::size_t index = 1; index < currentPoints.size(); ++index)
      appendStretchedSegment(currentPoints[index - 1], currentPoints[index]);

    const cv::Point2f from = currentPoints.back();
    const cv::Point2f to = nextPoints.front();
    const std::vector<cv::Point> currentWindow =
        endpointWindow(currentPoints, true);
    const std::vector<cv::Point> nextWindow =
        endpointWindow(nextPoints, false);
    const cv::Point2f fromTangent = endpointTangent(currentWindow);
    const cv::Point2f toTangent = endpointTangent(nextWindow);
    const double currentEndpointTurn = endpointTurnDegrees(currentWindow);
    const double nextEndpointTurn = endpointTurnDegrees(nextWindow);
    const float denominator = cross2d(fromTangent, toTangent);
    const double crossAngleDeg =
        std::asin(std::clamp(std::abs(static_cast<double>(denominator)),
                            0.0, 1.0)) *
        180.0 / CV_PI;
    evidence.derived_junction_cross_angle_deg[segment] = crossAngleDeg;
    const double allowedGap = evidence.domain_join_allowed_gap_px[segment];
    const double maximumExtrapolation = std::max(
        8.0, allowedGap *
                 static_cast<double>(cfg.junction_max_extrapolation_percent) /
                 100.0);
    bool validIntersection = std::abs(denominator) > 1e-4f;
    cv::Point2f junction = (from + to) * 0.5f;
    double currentExtrapolation = 0.0;
    double nextExtrapolation = 0.0;
    if (validIntersection) {
      const cv::Point2f delta = to - from;
      currentExtrapolation = cross2d(delta, toTangent) / denominator;
      const double nextParameter = cross2d(delta, fromTangent) / denominator;
      nextExtrapolation = -nextParameter;
      junction = from + fromTangent *
                            static_cast<float>(currentExtrapolation);
      validIntersection =
          currentExtrapolation >= -2.0 && nextExtrapolation >= -2.0 &&
          currentExtrapolation <= maximumExtrapolation &&
          nextExtrapolation <= maximumExtrapolation &&
          junction.x >= roi.x - maximumExtrapolation &&
          junction.y >= roi.y - maximumExtrapolation &&
          junction.x <= roi.x + roi.width + maximumExtrapolation &&
          junction.y <= roi.y + roi.height + maximumExtrapolation;
    }
    // A sharp tangent intersection is valid only when both incoming tracks
    // are locally line-like. Curved domains can also have a large cross angle,
    // but snapping them to the tangent intersection creates a false corner.
    const double maximumLineLikeTurn = std::max(
        3.0, static_cast<double>(cfg.junction_min_cross_angle_deg) * 0.5);
    const bool useIntersection =
        validIntersection &&
        crossAngleDeg >=
            static_cast<double>(cfg.junction_min_cross_angle_deg) &&
        currentEndpointTurn <= maximumLineLikeTurn &&
        nextEndpointTurn <= maximumLineLikeTurn;
    if (useIntersection) {
      const cv::Point rounded(static_cast<int>(std::lround(junction.x)),
                              static_cast<int>(std::lround(junction.y)));
      appendStretchedSegment(currentPoints.back(), rounded);
      appendStretchedSegment(rounded, nextPoints.front());
      evidence.derived_junction_modes[segment] = 1;
    } else {
      if (validIntersection) {
        appendIntersectionGuidedBlend(from, junction, to);
        // Publish the point actually lying on the smooth derived curve, not
        // the off-curve tangent intersection used as its Bezier control.
        junction = from * 0.25f + junction * 0.5f + to * 0.25f;
      } else {
        appendTangentBlend(from, to, fromTangent, toTangent, allowedGap);
        junction = (from + to) * 0.5f;
        currentExtrapolation = 0.0;
        nextExtrapolation = 0.0;
      }
      evidence.derived_junction_modes[segment] = 2;
    }
    derivedJunctionCenters[segment] = junction;
    evidence.derived_junction_current_extrapolation_px[segment] =
        currentExtrapolation;
    evidence.derived_junction_next_extrapolation_px[segment] =
        nextExtrapolation;
    evidence.derived_junction_points.push_back(
        {static_cast<double>(junction.x), static_cast<double>(junction.y)});
  }
  if (!trace.empty() && trace.back() != trace.front())
    trace.push_back(trace.front());
  evidence.loop_erased_point_count = 0;
  trace_count = static_cast<int>(trace.size());
  evidence.derived_trace_points.reserve(trace.size());
  for (const cv::Point &point : trace)
    evidence.derived_trace_points.push_back(
        {static_cast<double>(point.x), static_cast<double>(point.y)});
  if (trace_count < std::max(2, cfg.trace_min_length_px)) {
    evidence.reason = "DERIVED_TRACE_TOO_SHORT";
    return false;
  }
  evidence.closure_error_px = cv::norm(trace.front() - trace.back());
  evidence.max_consecutive_gap_px = 0.0;
  for (std::size_t index = 1; index < trace.size(); ++index)
    evidence.max_consecutive_gap_px = std::max(
        evidence.max_consecutive_gap_px,
        cv::norm(trace[index] - trace[index - 1]));
  if (evidence.trace_segment_count != 4 || evidence.closure_error_px > 1.5 ||
      evidence.max_consecutive_gap_px > 1.5) {
    evidence.reason = "DERIVED_TRACE_NOT_CLOSED";
    return false;
  }

  out_pattern = PointsShape();
  const int step = std::max(1, cfg.tangent_sample_step_px);
  const float angular_limit = std::sin(static_cast<float>(std::clamp(cfg.normal_angle_tolerance_deg, 1, 89)) *
                                       static_cast<float>(CV_PI / 180.0));
  const int closedCount = static_cast<int>(conclusionPath.size());
  const std::array<cv::Point2f, 4> &cornerCenters = derivedJunctionCenters;
  for (int i = 0; i < closedCount; i += step) {
    const NormalTraceSourceConclusion &sourceConclusion =
        conclusionPath[static_cast<size_t>(i)];
    const cv::Point &p = sourceConclusion.point;
    const cv::Point2f &sourcePoint = sourceConclusion.source_point;
    bool nearCorner = false;
    for (const cv::Point2f &corner : cornerCenters) {
      if (cfg.corner_rejection_radius_px <= 0)
        break;
      if (cv::norm(cv::Point2f(static_cast<float>(p.x),
                               static_cast<float>(p.y)) - corner) <=
          static_cast<float>(std::max(0, cfg.corner_rejection_radius_px))) {
        nearCorner = true;
        break;
      }
    }
    if (nearCorner) {
      ++evidence.normal_pair_corner_rejected_count;
      continue;
    }
    int before = -1;
    for (int candidate = i - 1; candidate >= 0; --candidate) {
      if (conclusionPath[static_cast<size_t>(candidate)].direction !=
          sourceConclusion.direction)
        break;
      before = candidate;
      break;
    }
    int after = -1;
    for (int candidate = i + 1; candidate < closedCount; ++candidate) {
      if (conclusionPath[static_cast<size_t>(candidate)].direction !=
          sourceConclusion.direction)
        break;
      after = candidate;
      break;
    }
    cv::Point2f tangent;
    if (before >= 0 && after >= 0) {
      tangent =
          conclusionPath[static_cast<size_t>(after)].point -
          conclusionPath[static_cast<size_t>(before)].point;
    } else if (after >= 0) {
      tangent =
          conclusionPath[static_cast<size_t>(after)].point - p;
    } else if (before >= 0) {
      tangent =
          p - conclusionPath[static_cast<size_t>(before)].point;
    } else {
      ++evidence.normal_pair_binding_miss_count;
      continue;
    }
    const float tangent_length = std::sqrt(tangent.dot(tangent));
    if (tangent_length < 1.0f)
      continue;
    tangent *= 1.0f / tangent_length;
    const cv::Point2f domainNormal = sourceConclusion.domain_normal;
    const int boundDirection = sourceConclusion.direction;
    const int boundScan = sourceConclusion.scan_index;
    if (boundDirection < 0 || boundDirection >= 4 || boundScan < 0) {
      ++evidence.normal_pair_binding_miss_count;
      continue;
    }
    cv::Point2f normal(-tangent.y, tangent.x);
    const float domainAlignment = normal.dot(domainNormal);
    // The adjacent source conclusions own the geometric normal. The saved
    // FindLine domain owns only its A/B hemisphere. Requiring both normals to
    // be almost identical rejects valid curved sides; reject only when the
    // hemisphere itself is ambiguous.
    if (std::abs(domainAlignment) < angular_limit) {
      ++evidence.normal_pair_binding_miss_count;
      continue;
    }
    if (domainAlignment < 0.0f)
      normal *= -1.0f;
    // compare_gap is the distance from the source conclusion to each polarity
    // point. Dijkstra only de-duplicates/routes conclusions; it does not
    // redefine the original directional polarity or pair spacing.
    const float pairOffset = std::max(
        1.0f, static_cast<float>(cfg.normal_pair_offset_px));
    const cv::Point2f a(sourcePoint.x + normal.x * pairOffset,
                        sourcePoint.y + normal.y * pairOffset);
    const cv::Point2f b(sourcePoint.x - normal.x * pairOffset,
                        sourcePoint.y - normal.y * pairOffset);
    if (a.x < 0 || a.y < 0 || b.x < 0 || b.y < 0 ||
        a.x >= source_mat.cols || b.x >= source_mat.cols ||
        a.y >= source_mat.rows || b.y >= source_mat.rows) continue;
    Standard_Real ax = static_cast<Standard_Real>(a.x);
    Standard_Real ay = static_cast<Standard_Real>(a.y);
    Standard_Real bx = static_cast<Standard_Real>(b.x);
    Standard_Real by = static_cast<Standard_Real>(b.y);
    out_pattern.addpointa(ax, ay);
    out_pattern.addpointb(bx, by);
    evidence.normal_pair_a.push_back({static_cast<double>(ax), static_cast<double>(ay)});
    evidence.normal_pair_b.push_back({static_cast<double>(bx), static_cast<double>(by)});
    evidence.normal_pair_source_points.push_back(
        {static_cast<double>(sourcePoint.x), static_cast<double>(sourcePoint.y)});
    evidence.normal_pair_source_directions.push_back(boundDirection);
    evidence.normal_pair_source_scans.push_back(boundScan);
    ++evidence.normal_pair_findline_bound_count;
    if (boundDirection >= 0 && boundDirection < 4)
      ++evidence.normal_pair_counts_by_direction[boundDirection];
    ++pair_count;
  }
  evidence.succeeded = pair_count >= 2 && out_pattern.ABsize() > 0;
  evidence.reason = evidence.succeeded ? "NORMAL_TRACE_COMPLETE"
                                       : "NORMAL_PAIR_COUNT_TOO_LOW";
  return evidence.succeeded;
}
bool FastMatchPointInsideImage(const Image &image, int x, int y) {
  return x >= 0 && y >= 0 && x < image.getWidth() && y < image.getHeight();
}

#ifdef FASTMATCH_LEARN_PROBE
void ProbeLog(const std::string &msg) {
  MessageBoxA(NULL, msg.c_str(), "FastMatch Probe", MB_OK);

  FILE *fp = fopen("D:\\Codex-WorkDir\\Sean_WorkDir\\cxvisionai\\cxscript_"
                   "runs\\probe_log.txt",
                   "a");
  if (fp != nullptr) {
    fprintf(fp, "[%d] %s\n", GetCurrentThreadId(), msg.c_str());
    fflush(fp);
    fclose(fp);
  }
  OutputDebugStringA(("[FastMatchProbe] " + msg).c_str());
}

#pragma pack(push, 1)
struct FastMatchProbeData {
  volatile LONG probe_count;
  volatile LONG learn_entry_called;
  volatile LONG learn_before_Learn_called;
  volatile LONG Learn_entry_called;
  volatile LONG Learn_after_edgepattern_called;
  volatile LONG learn_after_Learn_called;
  volatile LONG learn_fallback_called;
  volatile LONG pimage_null;
  volatile LONG image_width;
  volatile LONG image_height;
  volatile LONG rect_x;
  volatile LONG rect_y;
  volatile LONG rect_w;
  volatile LONG rect_h;
  volatile LONG thre;
  volatile LONG linegap;
  volatile LONG wgap;
  volatile LONG hgap;
  volatile LONG learn_a_count;
  volatile LONG learn_b_count;
  volatile LONG learn_a2_count;
  volatile LONG learn_b2_count;
  volatile LONG total_points;
  char last_message[512];
};
#pragma pack(pop)

FastMatchProbeData *g_probe_data = nullptr;

void InitProbeSharedMemory() {
  if (g_probe_data != nullptr)
    return;

  HANDLE hMapFile = CreateFileMappingA(
      INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
      sizeof(FastMatchProbeData), "FastMatchProbeSharedMemory");

  if (hMapFile != nullptr) {
    g_probe_data = (FastMatchProbeData *)MapViewOfFile(
        hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FastMatchProbeData));

    if (g_probe_data != nullptr) {
      ZeroMemory((void *)g_probe_data, sizeof(FastMatchProbeData));
    }
  }
}

void ProbeSetLearnEntry(int pimage_null_val) {
  InitProbeSharedMemory();
  if (g_probe_data != nullptr) {
    InterlockedIncrement(&g_probe_data->learn_entry_called);
    g_probe_data->pimage_null = pimage_null_val;
  }
}

void ProbeSetLearnBeforeLearn() {
  InitProbeSharedMemory();
  if (g_probe_data != nullptr) {
    InterlockedIncrement(&g_probe_data->learn_before_Learn_called);
  }
}

void ProbeSetLearnEntryData(int w, int h, int rx, int ry, int rw, int rh, int t,
                            int lg, int wg, int hg) {
  InitProbeSharedMemory();
  if (g_probe_data != nullptr) {
    InterlockedIncrement(&g_probe_data->Learn_entry_called);
    g_probe_data->image_width = w;
    g_probe_data->image_height = h;
    g_probe_data->rect_x = rx;
    g_probe_data->rect_y = ry;
    g_probe_data->rect_w = rw;
    g_probe_data->rect_h = rh;
    g_probe_data->thre = t;
    g_probe_data->linegap = lg;
    g_probe_data->wgap = wg;
    g_probe_data->hgap = hg;
  }
}

void ProbeSetLearnAfterEdgepattern(int a, int b, int a2, int b2, int total) {
  InitProbeSharedMemory();
  if (g_probe_data != nullptr) {
    InterlockedIncrement(&g_probe_data->Learn_after_edgepattern_called);
    g_probe_data->learn_a_count = a;
    g_probe_data->learn_b_count = b;
    g_probe_data->learn_a2_count = a2;
    g_probe_data->learn_b2_count = b2;
    g_probe_data->total_points = total;
  }
}

void ProbeSetLearnAfterLearn(int a, int b, int a2, int b2) {
  InitProbeSharedMemory();
  if (g_probe_data != nullptr) {
    InterlockedIncrement(&g_probe_data->learn_after_Learn_called);
    g_probe_data->learn_a_count = a;
    g_probe_data->learn_b_count = b;
    g_probe_data->learn_a2_count = a2;
    g_probe_data->learn_b2_count = b2;
  }
}

void ProbeSetLearnFallback() {
  InitProbeSharedMemory();
  if (g_probe_data != nullptr) {
    InterlockedIncrement(&g_probe_data->learn_fallback_called);
  }
}
#endif

int FastMatchPositiveInt(int value, int fallback = 1) {
  return value > 0 ? value : fallback;
}

int FastMatchNonNegativeInt(int value) { return std::max(0, value); }

double FastMatchFiniteOr(double value, double fallback = 0.0) {
  return std::isfinite(value) ? value : fallback;
}

double FastMatchPositiveFiniteOr(double value, double fallback = 1.0) {
  return std::isfinite(value) && value > 0.0 ? value : fallback;
}

double FastMatchUnitScore(double value, double fallback = 0.4) {
  if (!std::isfinite(value)) {
    value = fallback;
  }
  return std::clamp(value, 0.0, 1.0);
}

bool LearnPatternThroughRectFindline(
    Image &image, FastMatch &source, int learn_x, int learn_y, int learn_w,
    int learn_h, bool use_object_filter_fallback, PointsShape &out_pattern,
    int &out_a_count, int &out_b_count, int &out_a2_count, int &out_b2_count) {
  FindLine finder;
  const FastMatch::LearnDirectionParams p0 =
      source.effectiveLearnDirectionParams(0);
  finder.SetWHgap(p0.wgap, p0.hgap);
  finder.setcomparegap(p0.compare_gap);
  finder.setthre(p0.threshold);
  finder.setlinegap(p0.linegap);
  finder.setmethod(p0.method);
  finder.setobjfilter(use_object_filter_fallback ? 1 : p0.objfilter);
  if (use_object_filter_fallback) {
    finder.setfilterprofile(1);
    finder.setfilter(21, 5, 100000);
    finder.setmeasurefallback(1);
  }
  finder.setscanrotation(source.getscanrotation());
  finder.setrect(learn_x, learn_y, learn_w, learn_h);
  finder.edgepattern(image);

  out_a_count = finder.getlearnacount();
  out_b_count = finder.getlearnbcount();
  out_a2_count = finder.getlearna2count();
  out_b2_count = finder.getlearnb2count();
  const int collected_count =
      out_a_count + out_b_count + out_a2_count + out_b2_count;
  if (collected_count <= 0) {
    return false;
  }

  if (finder.getpatternpathA().ElementCount() <= 0 ||
      finder.getpatternpathB().ElementCount() <= 0) {
    return false;
  }

  out_pattern = PointsShape();
  out_pattern.copy(finder.getpattern());
  return out_pattern.ABsize() > 0;
}

int PixelGrayAt(const cv::Mat &gray, int x, int y) {
  x = std::clamp(x, 0, gray.cols - 1);
  y = std::clamp(y, 0, gray.rows - 1);
  return static_cast<int>(gray.at<unsigned char>(y, x));
}

bool IsEdgeTransition(const cv::Mat &gray, int x0, int y0, int x1, int y1,
                      int threshold) {
  return std::abs(PixelGrayAt(gray, x1, y1) - PixelGrayAt(gray, x0, y0)) >=
         threshold;
}

int CollectVerticalBoundaryPoints(const cv::Mat &gray, int x0, int y0, int x1,
                                  int y1, int step, int threshold,
                                  bool from_top, PointsShape &out_points) {
  int count = 0;
  const int left = std::clamp(std::min(x0, x1), 0, gray.cols - 1);
  const int right = std::clamp(std::max(x0, x1), 0, gray.cols - 1);
  const int top = std::clamp(std::min(y0, y1), 0, gray.rows - 1);
  const int bottom = std::clamp(std::max(y0, y1), 0, gray.rows - 1);
  const int scan_step = std::max(1, step);
  if (right <= left || bottom <= top)
    return 0;

  for (int x = left; x <= right; x += scan_step) {
    if (from_top) {
      for (int y = top; y < bottom; ++y) {
        if (IsEdgeTransition(gray, x, y, x, y + 1, threshold)) {
          out_points.addpoint(x, y + 1);
          ++count;
          break;
        }
      }
    } else {
      for (int y = bottom; y > top; --y) {
        if (IsEdgeTransition(gray, x, y, x, y - 1, threshold)) {
          out_points.addpoint(x, y - 1);
          ++count;
          break;
        }
      }
    }
  }
  return count;
}

int CollectHorizontalBoundaryPoints(const cv::Mat &gray, int x0, int y0, int x1,
                                    int y1, int step, int threshold,
                                    bool from_left, PointsShape &out_points) {
  int count = 0;
  const int left = std::clamp(std::min(x0, x1), 0, gray.cols - 1);
  const int right = std::clamp(std::max(x0, x1), 0, gray.cols - 1);
  const int top = std::clamp(std::min(y0, y1), 0, gray.rows - 1);
  const int bottom = std::clamp(std::max(y0, y1), 0, gray.rows - 1);
  const int scan_step = std::max(1, step);
  if (right <= left || bottom <= top)
    return 0;

  for (int y = top; y <= bottom; y += scan_step) {
    if (from_left) {
      for (int x = left; x < right; ++x) {
        if (IsEdgeTransition(gray, x, y, x + 1, y, threshold)) {
          out_points.addpoint(x + 1, y);
          ++count;
          break;
        }
      }
    } else {
      for (int x = right; x > left; --x) {
        if (IsEdgeTransition(gray, x, y, x - 1, y, threshold)) {
          out_points.addpoint(x - 1, y);
          ++count;
          break;
        }
      }
    }
  }
  return count;
}

bool LearnPatternByBoundaryPointPairs(Image &image, FastMatch &source,
                                      int learn_x, int learn_y, int learn_w,
                                      int learn_h, PointsShape &out_pattern,
                                      int &out_a_count, int &out_b_count,
                                      int &out_a2_count, int &out_b2_count) {
  cv::Mat src = image.getmat();
  out_a_count = 0;
  out_b_count = 0;
  out_a2_count = 0;
  out_b2_count = 0;

  if (src.empty()) {
    return false;
  }

  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  if (gray.empty()) {
    return false;
  }

  const int x0 = std::clamp(learn_x, 0, gray.cols - 1);
  const int y0 = std::clamp(learn_y, 0, gray.rows - 1);
  const int x1 = std::clamp(learn_x + std::max(1, learn_w), 0, gray.cols - 1);
  const int y1 = std::clamp(learn_y + std::max(1, learn_h), 0, gray.rows - 1);
  if (x1 <= x0 || y1 <= y0) {
    return false;
  }

  const FastMatch::LearnDirectionParams top_params =
      source.effectiveLearnDirectionParams(0);
  const FastMatch::LearnDirectionParams bottom_params =
      source.effectiveLearnDirectionParams(1);
  const FastMatch::LearnDirectionParams left_params =
      source.effectiveLearnDirectionParams(2);
  const FastMatch::LearnDirectionParams right_params =
      source.effectiveLearnDirectionParams(3);

  PointsShape top_points;
  PointsShape bottom_points;
  PointsShape left_points;
  PointsShape right_points;

  out_a_count = CollectVerticalBoundaryPoints(
      gray, x0, y0, x1, y1, top_params.wgap, top_params.threshold, true,
      top_points);
  out_b_count = CollectVerticalBoundaryPoints(
      gray, x0, y0, x1, y1, bottom_params.wgap, bottom_params.threshold, false,
      bottom_points);
  out_a2_count = CollectHorizontalBoundaryPoints(
      gray, x0, y0, x1, y1, left_params.hgap, left_params.threshold, true,
      left_points);
  out_b2_count = CollectHorizontalBoundaryPoints(
      gray, x0, y0, x1, y1, right_params.hgap, right_params.threshold, false,
      right_points);

  out_pattern = PointsShape();
  top_points.doublepattern(std::max(1, top_params.compare_gap), 6, out_pattern);
  bottom_points.doublepattern(std::max(1, bottom_params.compare_gap), 12,
                              out_pattern);
  left_points.doublepattern(std::max(1, left_params.compare_gap), 3,
                            out_pattern);
  right_points.doublepattern(std::max(1, right_params.compare_gap), 9,
                             out_pattern);

  return out_pattern.ABsize() > 0;
}
} // namespace

void removeAt(std::vector<int> &vec, size_t index) {
  if (index < vec.size()) {
    vec.erase(vec.begin() + index);
  } else {
    std::cerr << "Index out of bounds" << std::endl;
  }
}
void removePntAt(std::vector<gp_Pnt> &vec, size_t index) {
  if (index < vec.size()) {
    vec.erase(vec.begin() + index);
  } else {
    std::cerr << "Index out of bounds" << std::endl;
  }
}
void removedoubleAt(std::vector<double> &vec, size_t index) {
  if (index < vec.size()) {
    vec.erase(vec.begin() + index);
  } else {
    std::cerr << "Index out of bounds" << std::endl;
  }
}
void removePointsShapeAt(std::vector<PointsShape> &vec, size_t index) {
  if (index < vec.size()) {
    vec.erase(vec.begin() + index);
  } else {
    std::cerr << "Index out of bounds" << std::endl;
  }
}

void removeLast(std::vector<int> &vec) {
  if (!vec.empty()) {
    vec.pop_back();
  } else {
    std::cerr << "Vector is already empty" << std::endl;
  }
}
void removedoubleLast(std::vector<double> &vec) {
  if (!vec.empty()) {
    vec.pop_back();
  } else {
    std::cerr << "Vector is already empty" << std::endl;
  }
}
void removePntLast(std::vector<gp_Pnt> &vec) {
  if (!vec.empty()) {
    vec.pop_back();
  } else {
    std::cerr << "Vector is already empty" << std::endl;
  }
}
void removePointsShapeLast(std::vector<PointsShape> &vec) {
  if (!vec.empty()) {
    vec.pop_back();
  } else {
    std::cerr << "Vector is already empty" << std::endl;
  }
}

int EvaluateMatchSampleABScore(Image &image, gp_Path &pathA, gp_Path &pathB,
                               int movx, int movy, int ithre, int ib2w,
                               int iminfindngnum) {
  const int icount = std::min(static_cast<int>(pathA.ElementCount()),
                              static_cast<int>(pathB.ElementCount()));
  int icalnum = 0;
  int icalng = 0;
  for (int i = 0; i < icount - 1; ++i) {
    const gp_Pnt pointA = pathA.ElementAt(i);
    const gp_Pnt pointB = pathB.ElementAt(i);
    const int ax = static_cast<int>(pointA.X() + movx);
    const int ay = static_cast<int>(pointA.Y() + movy);
    const int bx = static_cast<int>(pointB.X() + movx);
    const int by = static_cast<int>(pointB.Y() + movy);
    if (!FastMatchPointInsideImage(image, ax, ay) ||
        !FastMatchPointInsideImage(image, bx, by)) {
      return 0;
    }
    const cv::Vec3b pixel0 = image.pixel(ax, ay);
    const cv::Vec3b pixel1 = image.pixel(bx, by);

    if (ib2w == 0) {
      const int ir = Red(pixel0) - Red(pixel1);
      const int ig = Green(pixel0) - Green(pixel1);
      const int ib = Blue(pixel0) - Blue(pixel1);
      if (ir > ithre || ig > ithre || ib > ithre) {
        ++icalnum;
      } else if (++icalng > iminfindngnum) {
        break;
      }
    } else {
      const int ir = Red(pixel1) - Red(pixel0);
      const int ig = Green(pixel1) - Green(pixel0);
      const int ib = Blue(pixel1) - Blue(pixel0);
      if (ir > ithre || ig > ithre || ib > ithre) {
        ++icalnum;
      } else if (++icalng > iminfindngnum) {
        break;
      }
    }
  }
  return icalnum;
}

bool MatchSampleABAnchorPass(Image &image, gp_Path &pathA, gp_Path &pathB,
                             int movx, int movy, int ithre, int ib2w) {
  const int icount = std::min(static_cast<int>(pathA.ElementCount()),
                              static_cast<int>(pathB.ElementCount()));
  if (icount <= 0) {
    return false;
  }

  const int anchor_count = std::min(5, icount);
  const int anchor_step = std::max(1, icount / anchor_count);
  int hit_count = 0;
  int probe_count = 0;
  for (int anchor_index = 0; anchor_index < anchor_count; ++anchor_index) {
    int sample_index = anchor_index * anchor_step;
    if (anchor_index + 1 == anchor_count) {
      sample_index = icount - 1;
    }

    const gp_Pnt pointA = pathA.ElementAt(sample_index);
    const gp_Pnt pointB = pathB.ElementAt(sample_index);
    const int ax = static_cast<int>(pointA.X() + movx);
    const int ay = static_cast<int>(pointA.Y() + movy);
    const int bx = static_cast<int>(pointB.X() + movx);
    const int by = static_cast<int>(pointB.Y() + movy);
    if (!FastMatchPointInsideImage(image, ax, ay) ||
        !FastMatchPointInsideImage(image, bx, by)) {
      return false;
    }
    const cv::Vec3b pixel0 = image.pixel(ax, ay);
    const cv::Vec3b pixel1 = image.pixel(bx, by);
    const int dr =
        ib2w == 0 ? Red(pixel0) - Red(pixel1) : Red(pixel1) - Red(pixel0);
    const int dg = ib2w == 0 ? Green(pixel0) - Green(pixel1)
                             : Green(pixel1) - Green(pixel0);
    const int db =
        ib2w == 0 ? Blue(pixel0) - Blue(pixel1) : Blue(pixel1) - Blue(pixel0);
    ++probe_count;
    if (dr > ithre || dg > ithre || db > ithre) {
      ++hit_count;
    }
  }

  return probe_count == 0 || hit_count > 0;
}

bool FastMatchMaskPixelPass(const cv::Mat *mask, int x, int y) {
  if (mask == nullptr || mask->empty()) {
    return true;
  }
  if (x < 0 || y < 0 || x >= mask->cols || y >= mask->rows) {
    return false;
  }
  if (mask->depth() != CV_8U) {
    return true;
  }

  const int channels = mask->channels();
  const uchar *pixel = mask->ptr<uchar>(y) + x * channels;
  for (int channel = 0; channel < channels; ++channel) {
    if (pixel[channel] != 0) {
      return true;
    }
  }
  return false;
}

bool FastMatchCandidateMaskPass(const cv::Mat *mask, gp_Path &path, int movx,
                                int movy) {
  if (mask == nullptr || mask->empty()) {
    return true;
  }

  const gp_Rectangle bounds = path.boundingRect();
  const int center_x =
      static_cast<int>(bounds.TopLeft().X() + bounds.Width() / 2 + movx);
  const int center_y =
      static_cast<int>(bounds.TopLeft().Y() + bounds.Height() / 2 + movy);
  return FastMatchMaskPixelPass(mask, center_x, center_y);
}
gp_Pnt RefineMatchSampleABPoint(Image &image, gp_Path &pathA, gp_Path &pathB,
                                const gp_Pnt &coarse_point, int coarse_score,
                                int ithre, int ib2w, int iminfindngnum,
                                int stepx, int stepy, int &refined_score) {
  gp_Pnt best_point = coarse_point;
  int best_score = coarse_score;
  int cur_step_x = std::max(1, stepx / 2);
  int cur_step_y = std::max(1, stepy / 2);

  while (true) {
    gp_Pnt stage_best = best_point;
    int stage_score = best_score;
    for (int dy = -cur_step_y; dy <= cur_step_y; dy += cur_step_y) {
      for (int dx = -cur_step_x; dx <= cur_step_x; dx += cur_step_x) {
        const int probe_x = static_cast<int>(best_point.X()) + dx;
        const int probe_y = static_cast<int>(best_point.Y()) + dy;
        const int score = EvaluateMatchSampleABScore(
            image, pathA, pathB, probe_x, probe_y, ithre, ib2w, iminfindngnum);
        if (score > stage_score) {
          stage_score = score;
          stage_best = gp_Pnt(probe_x, probe_y, 0);
        }
      }
    }

    best_point = stage_best;
    best_score = stage_score;

    if (cur_step_x == 1 && cur_step_y == 1) {
      break;
    }

    cur_step_x = std::max(1, cur_step_x / 2);
    cur_step_y = std::max(1, cur_step_y / 2);
  }

  refined_score = best_score;
  return best_point;
}

gp_Pnt GetRotateFilterAnchor(const gp_Pnt &point, const PointsShape &shape,
                             int itype) {
  if (itype == 0) {
    return point;
  }
  return shape.getpointscent();
}

int EasyObjectWidthAt(const std::vector<easyobj> &models, int index) {
  if (index < 0 || index >= static_cast<int>(models.size())) {
    return 0;
  }
  return models[index].s_iwobjnum;
}

int EasyObjectBlackCountAt(const std::vector<easyobj> &models, int index) {
  if (index < 0 || index >= static_cast<int>(models.size())) {
    return 0;
  }
  return models[index].s_ibobjnum;
}

size_t RotateResultSharedCount(const std::vector<double> &results,
                               const std::vector<gp_Pnt> &points,
                               const std::vector<double> &angles,
                               const std::vector<PointsShape> &shapes) {
  return std::min(std::min(results.size(), points.size()),
                  std::min(angles.size(), shapes.size()));
}

bool HasRotateResultAt(int index, const std::vector<double> &results,
                       const std::vector<gp_Pnt> &points,
                       const std::vector<double> &angles,
                       const std::vector<PointsShape> &shapes) {
  return index >= 0 && index < static_cast<int>(RotateResultSharedCount(
                                   results, points, angles, shapes));
}

void FilterRotateResultsByDistance(std::vector<double> &results,
                                   std::vector<gp_Pnt> &points,
                                   std::vector<double> &angles,
                                   std::vector<PointsShape> &shapes, int ifdx,
                                   int ifdy, int itype, bool full_scan) {
  const size_t shared_count = std::min(std::min(results.size(), points.size()),
                                       std::min(angles.size(), shapes.size()));
  if (shared_count == 0) {
    return;
  }
  results.resize(shared_count);
  points.resize(shared_count);
  angles.resize(shared_count);
  shapes.resize(shared_count);

  if (!full_scan) {
    for (int i = 1; i < static_cast<int>(results.size());) {
      const gp_Pnt lhs =
          GetRotateFilterAnchor(points.at(i - 1), shapes.at(i - 1), itype);
      const gp_Pnt rhs =
          GetRotateFilterAnchor(points.at(i), shapes.at(i), itype);
      const int iabsx = std::abs(static_cast<int>(lhs.X() - rhs.X()));
      const int iabsy = std::abs(static_cast<int>(lhs.Y() - rhs.Y()));
      if (iabsx < ifdx && iabsy < ifdy) {
        for (int j = i; j + 1 < static_cast<int>(results.size()); ++j) {
          results[j] = results.at(j + 1);
          points[j] = points.at(j + 1);
          angles[j] = angles.at(j + 1);
          shapes[j] = shapes.at(j + 1);
        }
        removedoubleLast(results);
        removePntLast(points);
        removedoubleLast(angles);
        removePointsShapeLast(shapes);
      } else {
        ++i;
      }
    }
    return;
  }

  for (int i = 0; i < static_cast<int>(results.size()); ++i) {
    for (int j = 0; j < static_cast<int>(results.size());) {
      if (i == j) {
        ++j;
        continue;
      }

      const gp_Pnt lhs =
          GetRotateFilterAnchor(points.at(i), shapes.at(i), itype);
      const gp_Pnt rhs =
          GetRotateFilterAnchor(points.at(j), shapes.at(j), itype);
      const int iabsx = std::abs(static_cast<int>(lhs.X() - rhs.X()));
      const int iabsy = std::abs(static_cast<int>(lhs.Y() - rhs.Y()));
      if (iabsx < ifdx && iabsy < ifdy) {
        removedoubleAt(results, j);
        removedoubleAt(angles, j);
        removePntAt(points, j);
        removePointsShapeAt(shapes, j);
        if (j < i) {
          --i;
        }
      } else {
        ++j;
      }
    }
  }
}

void NormalizeMatchCandidates(std::vector<int> &scores,
                              std::vector<gp_Pnt> &points, int keep_limit,
                              int &min_score, gp_Pnt &min_point) {
  const size_t shared_count = std::min(scores.size(), points.size());
  if (scores.size() != shared_count)
    scores.resize(shared_count);
  if (points.size() != shared_count)
    points.resize(shared_count);

  struct Candidate {
    int score;
    gp_Pnt point;
  };

  std::vector<Candidate> candidates;
  candidates.reserve(shared_count);
  for (size_t i = 0; i < shared_count; ++i) {
    candidates.push_back({scores[i], points[i]});
  }

  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate &lhs, const Candidate &rhs) {
                     if (lhs.score != rhs.score)
                       return lhs.score < rhs.score;
                     if (lhs.point.X() != rhs.point.X())
                       return lhs.point.X() < rhs.point.X();
                     return lhs.point.Y() < rhs.point.Y();
                   });

  const size_t normalized_keep = static_cast<size_t>(std::max(1, keep_limit));
  while (candidates.size() > normalized_keep) {
    candidates.erase(candidates.begin());
  }

  scores.clear();
  points.clear();
  scores.reserve(candidates.size());
  points.reserve(candidates.size());
  for (const Candidate &candidate : candidates) {
    scores.push_back(candidate.score);
    points.push_back(candidate.point);
  }

  min_score = -1;
  min_point = gp_Pnt();
  if (!candidates.empty()) {
    min_score = candidates.front().score;
    min_point = candidates.front().point;
  }
}

int FastMatch::m_curfastmatchnum = 0;

void FastMatch::setgeometrysourceindex(int index)
{
  m_geometry_source_object_index = std::max(0, index);
}

void FastMatch::setgeometryweightpercent(int percent)
{
  m_geometry_weight_percent = std::clamp(percent, 0, 100);
}

void FastMatch::setmaxposecandidates(int count)
{
  m_max_pose_candidates = std::max(1, count);
}

void FastMatch::settemplategeometryfromobject(void* pfindobject)
{
  m_template_geometry = FastMatchTemplateGeometrySnapshot();
  FindObject* source = static_cast<FindObject*>(pfindobject);
  if (source == nullptr)
    return;
  const FindObjectMeasurementSnapshot* measured =
      source->getmeasurement(m_geometry_source_object_index);
  if (measured == nullptr || measured->status != "measured" ||
      measured->bbox_px.width <= 0 || measured->bbox_px.height <= 0)
  {
    m_template_geometry.status = "source_measurement_unavailable";
    return;
  }

  m_template_geometry.available = true;
  m_template_geometry.source_object_index = measured->object_index;
  m_template_geometry.source_object_ref = measured->object_ref;
  m_template_geometry.bbox_px = measured->bbox_px;
  m_template_geometry.centroid_px = measured->centroid_px;
  m_template_geometry.projected_area = measured->projected_area;
  m_template_geometry.major_axis_length = measured->major_axis_length;
  m_template_geometry.minor_axis_length = measured->minor_axis_length;
  m_template_geometry.orientation_deg = measured->orientation_deg;
  m_template_geometry.aspect_ratio = measured->aspect_ratio;
  m_template_geometry.solidity = measured->solidity;
  m_template_geometry.normalized_boundary.reserve(
      measured->outer_boundary.size());
  for (const cv::Point2d& point : measured->outer_boundary)
  {
    m_template_geometry.normalized_boundary.emplace_back(
        (point.x - measured->bbox_px.x) / measured->bbox_px.width,
        (point.y - measured->bbox_px.y) / measured->bbox_px.height);
  }
  m_template_geometry.status = "geometry_bound";
}

void FastMatch::cleargeometrycandidates()
{
  m_observed_geometries.clear();
  m_pose_candidates.clear();
}

void FastMatch::addgeometrycandidatesfromobject(void* pfindobject)
{
  FindObject* source = static_cast<FindObject*>(pfindobject);
  if (source == nullptr)
    return;
  m_observed_geometries.clear();
  for (const FindObjectMeasurementSnapshot& measured :
       source->getmeasurements())
  {
    if (measured.status != "measured" ||
        measured.bbox_px.width <= 0 || measured.bbox_px.height <= 0)
      continue;
    FastMatchTemplateGeometrySnapshot observed;
    observed.available = true;
    observed.source_object_index = measured.object_index;
    observed.source_object_ref = measured.object_ref;
    observed.bbox_px = measured.bbox_px;
    observed.centroid_px = measured.centroid_px;
    observed.projected_area = measured.projected_area;
    observed.major_axis_length = measured.major_axis_length;
    observed.minor_axis_length = measured.minor_axis_length;
    observed.orientation_deg = measured.orientation_deg;
    observed.aspect_ratio = measured.aspect_ratio;
    observed.solidity = measured.solidity;
    observed.status = "observed_geometry";
    m_observed_geometries.push_back(std::move(observed));
  }
}

int FastMatch::gettemplategeometryavailable()
{
  return m_template_geometry.available ? 1 : 0;
}

int FastMatch::gettemplategeometrysourceindex()
{
  return m_template_geometry.source_object_index;
}

int FastMatch::gettemplateboundarypointcount()
{
  return static_cast<int>(m_template_geometry.normalized_boundary.size());
}

double FastMatch::gettemplatearea()
{
  return m_template_geometry.projected_area;
}

double FastMatch::gettemplateorientation()
{
  return m_template_geometry.orientation_deg;
}

int FastMatch::getposecandidatecount()
{
  return static_cast<int>(m_pose_candidates.size());
}

double FastMatch::getposecandidatex(int index)
{
  return index >= 0 && index < getposecandidatecount()
             ? m_pose_candidates[static_cast<std::size_t>(index)].center_px.x
             : 0.0;
}

double FastMatch::getposecandidatey(int index)
{
  return index >= 0 && index < getposecandidatecount()
             ? m_pose_candidates[static_cast<std::size_t>(index)].center_px.y
             : 0.0;
}

double FastMatch::getposecandidateangle(int index)
{
  return index >= 0 && index < getposecandidatecount()
             ? m_pose_candidates[static_cast<std::size_t>(index)].angle_deg
             : 0.0;
}

double FastMatch::getposecandidateappearancescore(int index)
{
  return index >= 0 && index < getposecandidatecount()
             ? m_pose_candidates[static_cast<std::size_t>(index)]
                   .appearance_score
             : 0.0;
}

double FastMatch::getposecandidategeometryscore(int index)
{
  return index >= 0 && index < getposecandidatecount()
             ? m_pose_candidates[static_cast<std::size_t>(index)]
                   .geometry_score
             : -1.0;
}

double FastMatch::getposecandidatecombinedscore(int index)
{
  return index >= 0 && index < getposecandidatecount()
             ? m_pose_candidates[static_cast<std::size_t>(index)]
                   .combined_score
             : 0.0;
}

void FastMatch::RefreshPoseCandidates()
{
  m_pose_candidates.clear();
  const int candidate_count =
      std::min(getresultcandidatecount(), m_max_pose_candidates);
  if (candidate_count <= 0)
    return;

  const int pair_count = std::max(
      1, std::min(static_cast<int>(FindLine::getpatternpathA().ElementCount()),
                  static_cast<int>(FindLine::getpatternpathB().ElementCount())));
  for (int i = 0; i < candidate_count; ++i)
  {
    const gp_Rectangle result = getresolvedresultrect(i);
    FastMatchPoseCandidateSnapshot pose;
    pose.candidate_index = i;
    pose.bbox_px = cv::Rect2d(result.TopLeft().X(), result.TopLeft().Y(),
                             result.Width(), result.Height());
    pose.center_px = cv::Point2d(
        pose.bbox_px.x + 0.5 * pose.bbox_px.width,
        pose.bbox_px.y + 0.5 * pose.bbox_px.height);
    pose.appearance_score = std::clamp(
        2.0 * static_cast<double>(m_resultnums[static_cast<std::size_t>(i)]) /
            pair_count,
        0.0, 1.0);
    pose.angle_deg = m_template_geometry.available
                         ? m_template_geometry.orientation_deg
                         : 0.0;
    if (i < static_cast<int>(m_rotatereslutangles.size())) {
      pose.angle_deg = m_rotatereslutangles[static_cast<std::size_t>(i)];
      if (pose.angle_deg > 270.0)
        pose.angle_deg -= 360.0;
    }
    if (m_template_geometry.available &&
        m_template_geometry.bbox_px.width > 0.0 &&
        m_template_geometry.bbox_px.height > 0.0)
    {
      pose.scale_x =
          pose.bbox_px.width / m_template_geometry.bbox_px.width;
      pose.scale_y =
          pose.bbox_px.height / m_template_geometry.bbox_px.height;
      pose.transformed_boundary.reserve(
          m_template_geometry.normalized_boundary.size());
      const double centroid_normalized_x =
          (m_template_geometry.centroid_px.x -
           m_template_geometry.bbox_px.x) /
          m_template_geometry.bbox_px.width;
      const double centroid_normalized_y =
          (m_template_geometry.centroid_px.y -
           m_template_geometry.bbox_px.y) /
          m_template_geometry.bbox_px.height;
      const double rotation_radians =
          (pose.angle_deg - m_template_geometry.orientation_deg) *
          3.14159265358979323846 / 180.0;
      const double cos_angle = std::cos(rotation_radians);
      const double sin_angle = std::sin(rotation_radians);
      for (const cv::Point2d& normalized :
           m_template_geometry.normalized_boundary)
      {
        const double local_x =
            (normalized.x - centroid_normalized_x) * pose.bbox_px.width;
        const double local_y =
            (normalized.y - centroid_normalized_y) * pose.bbox_px.height;
        pose.transformed_boundary.emplace_back(
            pose.center_px.x + local_x * cos_angle - local_y * sin_angle,
            pose.center_px.y + local_x * sin_angle + local_y * cos_angle);
      }
    }

    double best_geometry_score = -1.0;
    int best_geometry_index = -1;
    const double candidate_area =
        std::max(1.0, pose.bbox_px.width * pose.bbox_px.height);
    const double candidate_aspect =
        pose.bbox_px.height > 0.0
            ? pose.bbox_px.width / pose.bbox_px.height
            : 0.0;
    for (int observed_index = 0;
         observed_index < static_cast<int>(m_observed_geometries.size());
         ++observed_index)
    {
      const FastMatchTemplateGeometrySnapshot& observed =
          m_observed_geometries[static_cast<std::size_t>(observed_index)];
      const double dx = pose.center_px.x - observed.centroid_px.x;
      const double dy = pose.center_px.y - observed.centroid_px.y;
      const double diagonal = std::max(
          1.0, std::hypot(pose.bbox_px.width, pose.bbox_px.height));
      const double center_score =
          std::exp(-std::hypot(dx, dy) / diagonal);
      const double observed_area = std::max(
          1.0, observed.bbox_px.width * observed.bbox_px.height);
      const double area_score =
          std::min(candidate_area, observed_area) /
          std::max(candidate_area, observed_area);
      const double observed_aspect =
          observed.bbox_px.height > 0.0
              ? observed.bbox_px.width / observed.bbox_px.height
              : 0.0;
      const double aspect_score =
          candidate_aspect > 0.0 && observed_aspect > 0.0
              ? std::min(candidate_aspect, observed_aspect) /
                    std::max(candidate_aspect, observed_aspect)
              : 0.0;
      double angle_delta =
          std::abs(pose.angle_deg - observed.orientation_deg);
      angle_delta = std::min(angle_delta, 180.0 - angle_delta);
      const double angle_score =
          std::max(0.0, 1.0 - angle_delta / 90.0);
      const double geometry_score =
          0.45 * center_score + 0.25 * area_score +
          0.20 * aspect_score + 0.10 * angle_score;
      if (geometry_score > best_geometry_score)
      {
        best_geometry_score = geometry_score;
        best_geometry_index = observed_index;
      }
    }
    pose.observed_geometry_index = best_geometry_index;
    if (best_geometry_index >= 0)
      pose.observed_geometry_ref =
          m_observed_geometries[static_cast<std::size_t>(
              best_geometry_index)].source_object_ref;
    pose.geometry_score = best_geometry_score;
    pose.combined_score =
        best_geometry_score >= 0.0
            ? (1.0 - m_geometry_weight_percent / 100.0) *
                      pose.appearance_score +
                  (m_geometry_weight_percent / 100.0) *
                      best_geometry_score
            : pose.appearance_score;
    pose.status = best_geometry_index >= 0
                      ? "appearance_and_geometry"
                      : (m_template_geometry.available
                             ? "template_geometry_pose"
                             : "appearance_only");
    m_pose_candidates.push_back(std::move(pose));
  }
}

FastMatch::FastMatch()
    : FindLine(), m_matchimage(0), m_matchmask(nullptr), m_imaxmatchnum(10),
      m_iminfindnum(-1), m_imatchthre(5), m_ispecshow(-1), m_iB2W(0),
      m_imatchoffset(1), m_dminscore(0.4), m_prelationmatch(0),
      m_irelationresultnum(0), m_drelationzoomx(1), m_drelationzoomy(1),
      m_danglegap(2), m_dangle_add(60), m_dangle_mud(-60), m_stepgapx(1),
      m_stepgapy(1), m_ixclustergap(5), m_iyclustergap(5),
      m_iangleclustergap(5), m_istyle(0), m_rootgridA(0), m_iupgradexscale(5),
      m_iupgradeyscale(5), m_iupgradeanglescale(6),
      m_matchrect(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0)),
      m_expected_rect(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0)),
      m_irelationrect(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0)) {
  setcolor(0, 0, 255);
  std::ostringstream stream;
  stream << "fmatch " << m_curfastmatchnum;
  std::string strname = stream.str();
  (void)strname;
  m_curfastmatchnum = m_curfastmatchnum + 1;
  m_matchrects.setshow(2);
  m_imatchrectnum = 1;
  gp_Rectangle rect(gp_Pnt(20, 20, 0), gp_Pnt(300, 200, 0));
  m_matchrects.addrect(rect);

  int icurmodule = ImageManager::GetCurMode();
  g_pmodelimage = ImageManager::GetModelImage(icurmodule);

  m_pgrid = new Grid;
  m_pgrid->setgrid(30, 30, 12, 12, 30, 30);
}
void FastMatch::setgrid(int iw, int igrid) {
  if (m_pgrid == nullptr) {
    return;
  }
  m_pgrid->setgrid(FastMatchPositiveInt(iw), FastMatchPositiveInt(iw),
                   FastMatchPositiveInt(igrid), FastMatchPositiveInt(igrid),
                   FastMatchPositiveInt(iw), FastMatchPositiveInt(iw));
}
FastMatch::~FastMatch() { delete m_pgrid; }
void FastMatch::setcomparegap(int igap) { FindLine::setcomparegap(igap); }

void FastMatch::setscanrotation(double angle_degrees) {
  FindLine::setscanrotation(angle_degrees);
}

double FastMatch::getscanrotation() const { return FindLine::getscanrotation(); }
FastMatch::LearnDirectionParams &FastMatch::learnDirectionParams(
    int direction) {
  direction = std::max(0, std::min(3, direction));
  return m_learn_direction_params[static_cast<std::size_t>(direction)];
}

FastMatch::LearnDirectionParams FastMatch::effectiveLearnDirectionParams(
    int direction) {
  LearnDirectionParams params = learnDirectionParams(direction);
  if (params.wgap < 0)
    params.wgap = wgap();
  if (params.hgap < 0)
    params.hgap = hgap();
  if (params.method < 0)
    // Opposite physical sides are observed along the same Gauge scan axis,
    // therefore their exterior transitions have opposite polarity.  This is
    // the robust untrained default; a learned template can still persist an
    // independent method for every direction.
    params.method = (direction == 1 || direction == 3) ? 1 : 0;
  if (params.threshold < 0)
    params.threshold = thre();
  if (params.linegap < 0)
    params.linegap = linegap();
  if (params.objfilter < 0)
    params.objfilter = objfilter();
  if (params.compare_gap < 0)
    params.compare_gap = getconparegap();
  params.wgap = std::max(1, params.wgap);
  params.hgap = std::max(1, params.hgap);
  params.method = std::max(0, std::min(3, params.method));
  params.threshold = std::max(0, std::min(255, params.threshold));
  params.linegap = std::max(1, params.linegap);
  params.objfilter = std::max(0, params.objfilter);
  params.compare_gap = std::max(1, params.compare_gap);
  params.edge_count = std::clamp(params.edge_count, 1, 16);
  params.selected_edge =
      std::clamp(params.selected_edge, -1, params.edge_count);
  return params;
}

bool FastMatch::hasExplicitLearnDirectionParams() const {
  for (const LearnDirectionParams &params : m_learn_direction_params) {
    if (params.wgap >= 0 || params.hgap >= 0 || params.method >= 0 ||
        params.threshold >= 0 || params.linegap >= 0 ||
        params.objfilter >= 0 || params.compare_gap >= 0) {
      return true;
    }
  }
  return false;
}

void FastMatch::setlearnwgap(int direction, int value) {
  learnDirectionParams(direction).wgap = std::max(1, value);
}

void FastMatch::setlearnhgap(int direction, int value) {
  learnDirectionParams(direction).hgap = std::max(1, value);
}

void FastMatch::setlearnmethod(int direction, int value) {
  learnDirectionParams(direction).method = std::max(0, std::min(3, value));
}

void FastMatch::setlearnthre(int direction, int value) {
  learnDirectionParams(direction).threshold = std::max(0, std::min(255, value));
}

void FastMatch::setlearnlinegap(int direction, int value) {
  learnDirectionParams(direction).linegap = std::max(1, value);
}

void FastMatch::setlearnobjfilter(int direction, int value) {
  learnDirectionParams(direction).objfilter = std::max(0, value);
}

void FastMatch::setlearncompgap(int direction, int value) {
  learnDirectionParams(direction).compare_gap = std::max(1, value);
}

void FastMatch::setlearnedgecount(int direction, int value) {
  LearnDirectionParams& params = learnDirectionParams(direction);
  params.edge_count = std::clamp(value, 1, 16);
  params.selected_edge =
      std::clamp(params.selected_edge, -1, params.edge_count);
}

void FastMatch::setlearnselectededge(int direction, int value) {
  LearnDirectionParams& params = learnDirectionParams(direction);
  params.selected_edge = std::clamp(value, -1, std::max(1, params.edge_count));
}

const FastMatch::DirectionalProbeEvidence&
FastMatch::getdirectionalprobeevidence(int direction) const {
  static const DirectionalProbeEvidence kNotRun{};
  if (direction < 0 || direction >=
                           static_cast<int>(m_directional_probe_evidence.size())) {
    return kNotRun;
  }
  return m_directional_probe_evidence[static_cast<std::size_t>(direction)];
}

int FastMatch::getdirectionalprobeacceptedcount(int direction) const {
  return getdirectionalprobeevidence(direction).accepted_side_count;
}

int FastMatch::getdirectionalprobescanlinecount(int direction) const {
  return getdirectionalprobeevidence(direction).scan_line_count;
}

int FastMatch::getdirectionalprobediagnosticcount(int direction) const {
  return getdirectionalprobeevidence(direction).diagnostic_count;
}

int FastMatch::getdirectionalprobestatuscode(int direction) const {
  const DirectionalProbeEvidence& evidence =
      getdirectionalprobeevidence(direction);
  if (!evidence.executed)
    return 0;
  return evidence.accepted_side_count > 0 ? 2 : 1;
}

void FastMatch::runDirectionalFindLineProbes(Image& image) {
  m_directional_probe_evidence = {};
  const int roiX = m_learn_roi_x;
  const int roiY = m_learn_roi_y;
  const int roiW = std::max(1, m_learn_roi_w);
  const int roiH = std::max(1, m_learn_roi_h);
  const double centerX = static_cast<double>(roiX) + roiW * 0.5;
  const double centerY = static_cast<double>(roiY) + roiH * 0.5;
  const double scanRadians = getscanrotation() * CV_PI / 180.0;
  const double scanCos = std::cos(scanRadians);
  const double scanSin = std::sin(scanRadians);
  const auto belongsToPhysicalSide =
      [centerX, centerY, scanCos, scanSin](
          int direction, const CxShapePoint& candidate) {
        const double dx = candidate.x - centerX;
        const double dy = candidate.y - centerY;
        const double localU = scanCos * dx + scanSin * dy;
        const double localV = -scanSin * dx + scanCos * dy;
        return direction == 0 ? localV <= 0.0
             : direction == 1 ? localV >= 0.0
             : direction == 2 ? localU <= 0.0
                              : localU >= 0.0;
      };

  if (image.getmat().empty()) {
    for (int direction = 0; direction < 4; ++direction) {
      DirectionalProbeEvidence& evidence =
          m_directional_probe_evidence[static_cast<std::size_t>(direction)];
      evidence.direction = direction;
      evidence.status = "IMAGE_EMPTY";
      evidence.reason = "directional FindLine Probe skipped: image is empty";
    }
    return;
  }

  for (int direction = 0; direction < 4; ++direction) {
    DirectionalProbeEvidence& evidence =
        m_directional_probe_evidence[static_cast<std::size_t>(direction)];
    evidence.direction = direction;
    evidence.params = effectiveLearnDirectionParams(direction);
    const bool usesRotatedLineSegment =
        std::abs(getscanrotation()) > 1.0e-9;
    // FindLine's ROI builders expose opposite w/h scan-family names:
    // setrect() uses w for Top/Bottom and h for Left/Right, while
    // setlinesegment() uses h for Top/Bottom and w for Left/Right.
    evidence.scan_type = usesRotatedLineSegment
        ? (direction < 2 ? 1 : 0)
        : (direction < 2 ? 0 : 1);

    FindLine probe;
    // Preserve the user-facing physical-axis meaning of wgap/hgap across the
    // two native geometry builders.
    probe.SetWHgap(
        usesRotatedLineSegment ? evidence.params.hgap : evidence.params.wgap,
        usesRotatedLineSegment ? evidence.params.wgap : evidence.params.hgap);
    // A rotated line-segment probe has opposite native family semantics and
    // must run only the family for this physical side. Keep the zero-angle
    // legacy two-family path byte-for-byte compatible for rigid regression.
    if (usesRotatedLineSegment)
      probe.setscandirection(evidence.scan_type == 0 ? 1 : 2);
    probe.setcomparegap(evidence.params.compare_gap);
    probe.setthre(evidence.params.threshold);
    probe.setlinegap(evidence.params.linegap);
    probe.setmethod(evidence.params.method);
    probe.setobjfilter(evidence.params.objfilter);
    // Match FindLine's public Point Column semantics: 0 selects all
    // candidates, a positive value selects the Nth edge, and -1 selects the
    // last candidate.  The legacy FindLine two-edge compatibility maps Edge
    // 2 to Last; preserve that behavior for the FastMatch probe as well.
    int runtimeSelectedEdge = evidence.params.selected_edge;
    // Point Column is relative to the physical side selected by the tab.
    // FindLine ordinals are measured from its native scan start, so Bottom
    // and Right must count from the reverse end. Without this translation a
    // nominal "Edge 1" can jump between opposite physical boundaries when a
    // neighbouring Gauge Line has a different candidate count.
    if ((direction == 1 || direction == 3) && runtimeSelectedEdge > 0) {
      if (runtimeSelectedEdge == 1)
        runtimeSelectedEdge = -1;
      else
        runtimeSelectedEdge = std::max(
            1, evidence.params.edge_count - runtimeSelectedEdge + 1);
    } else if (evidence.params.edge_count == 2 && runtimeSelectedEdge == 2) {
      runtimeSelectedEdge = -1;
    }
    evidence.runtime_selected_edge = runtimeSelectedEdge;
    probe.setselectedgenum(runtimeSelectedEdge);
    probe.setscanrotation(getscanrotation());
    if (!usesRotatedLineSegment) {
      // Preserve the exact legacy rigid geometry and sampling at zero angle.
      probe.setrect(roiX, roiY, roiW, roiH);
    } else {
      // FindLine::setrect() is axis-aligned and does not consume the saved
      // scan rotation. Its line-segment ROI path is the registered rotated
      // Gauge implementation. A horizontal centreline plus half-height
      // reconstructs the same box before rotation is applied.
      probe.setlinesegment(static_cast<double>(roiX), centerY,
                           static_cast<double>(roiX + roiW), centerY,
                           static_cast<double>(roiH) * 0.5);
    }
    probe.Measure(image);
    probe.SmartFilter(-1, -1);

    const PointsShape& resultPoints = evidence.scan_type == 0
                                          ? probe.getresultpointsw()
                                          : probe.getresultpointsh();
    std::vector<CxShapePoint> rawPoints;
    resultPoints.exportPoints(rawPoints);
    evidence.raw_result_count = static_cast<int>(rawPoints.size());
    evidence.diagnostic_count = probe.getscandiagnosticcount();
    evidence.scan_line_count = probe.getscanlinecount(evidence.scan_type);

    // Preserve every native probe scan segment. Rendering may decimate this
    // list, but Metrology must sample the exact selected Gauge Line rather
    // than an approximate neighbouring segment.
    evidence.scan_lines.reserve(static_cast<std::size_t>(
        std::max(0, evidence.scan_line_count)));
    for (int scanIndex = 0; scanIndex < evidence.scan_line_count;
         ++scanIndex) {
      CxShapePoint p0;
      CxShapePoint p1;
      if (probe.getscanline(evidence.scan_type, scanIndex, p0, p1))
        evidence.scan_lines.push_back({p0, p1});
    }

    evidence.selected_point_by_scan.resize(
        static_cast<std::size_t>(std::max(0, evidence.scan_line_count)));
    evidence.selected_point_valid_by_scan.assign(
        static_cast<std::size_t>(std::max(0, evidence.scan_line_count)), 0);
    evidence.accepted_points_by_scan.resize(
        static_cast<std::size_t>(std::max(0, evidence.scan_line_count)));
    for (int diagnosticIndex = 0;
         diagnosticIndex < evidence.diagnostic_count; ++diagnosticIndex) {
      FindLineMeasureInputDebug::ScanDiagnostic diagnostic;
      if (!probe.getscandiagnostic(diagnosticIndex, diagnostic) ||
          !diagnostic.accepted || diagnostic.scan_type != evidence.scan_type ||
          diagnostic.scan_index < 0 ||
          diagnostic.scan_index >= evidence.scan_line_count)
        continue;
      std::vector<CxShapePoint> candidates;
      for (std::size_t pointIndex = 0;
           pointIndex + 1 < diagnostic.accepted_points_xy.size();
           pointIndex += 2) {
        candidates.push_back({diagnostic.accepted_points_xy[pointIndex],
                              diagnostic.accepted_points_xy[pointIndex + 1]});
      }
      if (candidates.empty())
        candidates.push_back({diagnostic.accepted_x, diagnostic.accepted_y});

      std::vector<CxShapePoint> sideCandidates;
      sideCandidates.reserve(candidates.size());
      for (const CxShapePoint& candidate : candidates) {
        if (belongsToPhysicalSide(direction, candidate))
          sideCandidates.push_back(candidate);
      }
      if (sideCandidates.empty())
        continue;
      const std::size_t scan = static_cast<std::size_t>(diagnostic.scan_index);
      if (evidence.params.selected_edge == 0) {
        // Full edge preserves every accepted candidate on this physical side.
        // It is intentionally distinct from an exterior-edge starter profile.
        evidence.accepted_points_by_scan[scan] = sideCandidates;
        evidence.accepted_points.insert(evidence.accepted_points.end(),
                                        sideCandidates.begin(),
                                        sideCandidates.end());
        // The first point is only the Gauge waveform marker; the model uses
        // the complete per-scan set above.
        evidence.selected_point_by_scan[scan] = sideCandidates.front();
        evidence.selected_point_valid_by_scan[scan] = 1;
      } else {
        // FindLine has already applied Edge N/Last. One physical point per
        // Gauge Line is therefore the exact model and waveform source.
        const CxShapePoint point = sideCandidates.front();
        evidence.accepted_points_by_scan[scan].push_back(point);
        evidence.selected_point_by_scan[scan] = point;
        evidence.selected_point_valid_by_scan[scan] = 1;
        evidence.accepted_points.push_back(point);
      }
    }
    // Preserve legacy All-edge visual evidence when the diagnostics have no
    // accepted point record. Explicit Point Columns never use this fallback.
    if (evidence.accepted_points.empty() && evidence.params.selected_edge == 0) {
      for (const CxShapePoint& point : rawPoints) {
        if (belongsToPhysicalSide(direction, point))
          evidence.accepted_points.push_back(point);
      }
    }
    evidence.accepted_side_count =
        static_cast<int>(evidence.accepted_points.size());

    evidence.executed = true;
    evidence.status = evidence.accepted_side_count > 0
                          ? "EXECUTED_ACCEPTED"
                          : "EXECUTED_NO_SIDE_ACCEPT";
    evidence.reason = evidence.params.selected_edge == 0
        ? "isolated FindLine Probe; Full edge retains all accepted physical "
          "candidates per scan for FastMatch Learn"
        : "isolated FindLine Probe; explicit Point Column promotes its "
          "accepted normal pairs into FastMatch Learn";
    CXLOG_INFO("FastMatch", "directional_findline_probe", evidence.status,
               "direction=" + std::to_string(direction) +
                   " scan_type=" + std::to_string(evidence.scan_type) +
                   " geometry=" +
                   std::string(usesRotatedLineSegment
                                   ? "rotated_line_segment"
                                   : "rigid_rect") +
                   " scans=" + std::to_string(evidence.scan_line_count) +
                   " raw=" + std::to_string(evidence.raw_result_count) +
                   " accepted=" +
                   std::to_string(evidence.accepted_side_count) +
                   " requested_edge=" +
                   std::to_string(evidence.params.selected_edge) +
                   " method=" + std::to_string(evidence.params.method) +
                   " runtime_selected_edge=" +
                   std::to_string(runtimeSelectedEdge) +
                   " diagnostics=" +
                   std::to_string(evidence.diagnostic_count));
  }
}

void FastMatch::setfindnum(int ifindnum) {
  m_imaxmatchnum = FastMatchPositiveInt(ifindnum);
}
void FastMatch::setmatchmask(const cv::Mat *pmask) { m_matchmask = pmask; }
void FastMatch::clearmatchmask() { m_matchmask = nullptr; }
void FastMatch::setb2w(int ib2w) { m_iB2W = ib2w == 1 ? 1 : 0; }
void FastMatch::getshape(void *pshape) {
  Shape *pshape0 = (Shape *)pshape;
  if (pshape0 == nullptr)
    return;

  const gp_Rectangle arect = rect();
  pshape0->setrect(static_cast<int>(arect.TopLeft().X()),
                   static_cast<int>(arect.TopLeft().Y()),
                   static_cast<int>(arect.Width()),
                   static_cast<int>(arect.Height()));
}
void FastMatch::setrect(int ix, int iy, int iw, int ih) {
  ix = FastMatchNonNegativeInt(ix);
  iy = FastMatchNonNegativeInt(iy);
  iw = FastMatchPositiveInt(iw);
  ih = FastMatchPositiveInt(ih);
  m_learn_roi_x = ix;
  m_learn_roi_y = iy;
  m_learn_roi_w = iw;
  m_learn_roi_h = ih;
  FindLine::setrect(ix, iy, iw, ih);
}
void FastMatch::setshow(int ishow) {
  if (ishow == 8) {
    m_resultrects.setcolor(0, 255, 0);
    m_resultrects.setshow(1);
    m_resultrects.MakeShape(-1);
  } else if (ishow == -8) {
    m_resultrects.setcolor(0, 255, 0);
    m_resultrects.setshow(1);
    m_resultrects.MakeShape();
    ishow = 8;
  } else if (ishow == 16) {
    const int irsize = static_cast<int>(m_rotateshaperesults.size());
    if (irsize > 0) {
      m_rotateshaperesults[0].setcolor(0, 255, 0);
      m_rotateshaperesults[0].setshow(32);
    }
  } else if (ishow == 32) {
    const int irsize = static_cast<int>(m_rotateshaperesults.size());
    for (int i = 0; i < irsize; i++) {
      m_rotateshaperesults[i].setcolor(0, 255, 0);
      m_rotateshaperesults[i].setshow(32);
    }
  }

  FindLine::setshow(ishow);
}
void FastMatch::SetWHgap(int wgap, int hgap) { FindLine::SetWHgap(wgap, hgap); }
void FastMatch::measure(void *pimage) { FindLine::measure(pimage); }
void FastMatch::setlinesamplerate(double dsamplerate) {
  FindLine::setlinesamplerate(dsamplerate);
}
void FastMatch::setlinegap(int igap) { FindLine::setlinegap(igap); }
void FastMatch::setmethod(int imethod) { FindLine::setmethod(imethod); }
void FastMatch::setthre(int ithre) { FindLine::setthre(ithre); }
void FastMatch::setmatchthre(int ithre) {
  m_imatchthre = FastMatchNonNegativeInt(ithre);
}
void FastMatch::setobjfilter(int ifindset) { FindLine::setobjfilter(ifindset); }
void FastMatch::setfilter(int ifilterborw, int ifiltermin, int ifiltermax) {
  FindLine::setfilter(ifilterborw, ifiltermin, ifiltermax);
}
void FastMatch::setselectedgenum(int iedgenum) {
  FindLine::setselectedgenum(iedgenum);
}
vector<PointsShape> &FastMatch::getmodels_l12() { return m_models_l12; }
void FastMatch::setspecshow(int ishow) {
  m_ispecshow = ishow;
  m_resultrects.setspecshow(ishow);
  m_matchrects.setspecshow(ishow);
}

void FastMatch::setshownum(int ishownum) {
  m_ishownum = FastMatchPositiveInt(ishownum);
}
void FastMatch::drawshape() {
  gp_Path painter;
  if (show() & 0x20) {
    m_pgrid->setshow(0x04);
  }
  if (show() & 0x08) {
  }
  if (show() & 0x10) {
    m_modelpoints_sample1.setshow(8);
    m_modelpoints_sample1.drawshape(painter);
  }
  if (show() & 0x02) {
    m_resultrects.setshow(2);
    m_resultrects.drawshape(painter);
  }
  if (show() & 0x04) {
    m_resultrects.setshow(4);
    m_resultrects.drawshape(painter);
  } else if (show() & 0x40) {

    const int iclustersize = static_cast<int>(m_clusters.size());
    for (int ic = 0; ic < iclustersize; ic++) {
      if (m_clusters[ic].empty()) {
        continue;
      }
      const int id = m_clusters[ic][0];
      if (id < 0 || id >= static_cast<int>(m_rotateshaperesults.size())) {
        continue;
      }
      PointsShape apoints = m_rotateshaperesults[id];
      apoints.setshow(16);
      apoints.drawshape(painter);
    }
  } else if (show() & 0x80) {
    const int isize = static_cast<int>(
        std::min(m_models_rotate.size(), m_models_rotaterects.size()));
    for (int iz = 0; iz < isize; iz++) {
      if (-1 == m_ispecshow || iz == m_ispecshow) {
        m_models_rotate[iz].setshow(8);
        m_models_rotate[iz].drawshape(painter);
        m_models_rotaterects[iz].setshow(16);
        m_models_rotaterects[iz].drawshape(painter);
      }
    }
  } else
    FindLine::drawshape();
  m_matchrects.drawshape(painter);
}
void FastMatch::setcolorstyle(int istyle) { m_istyle = istyle; }
void FastMatch::drawshapex(double dmovx, double dmovy, double dangle,
                           double dzoomx, double dzoomy) {
  if (show() & 0x20) {
    m_pgrid->setshow(0x04);
    m_pgrid->drawshape();
  }
  if (show() & 0x08) {
    drawpattern();
  }
  if (show() & 0x10) {
    m_modelpoints_sample1.setshow(8);
    gp_Path painter;
    m_modelpoints_sample1.drawshape(painter);
  }
  if (show() & 0x02) {
    m_resultrects.setshow(2);
    gp_Path painter;
    m_resultrects.drawshapex(painter, dmovx, dmovy, dangle, dzoomx, dzoomy);
  }
  if (show() & 0x04) {
    m_resultrects.setshow(4);
    gp_Path painter;
    m_resultrects.drawshapex(painter, dmovx, dmovy, dangle, dzoomx, dzoomy);
  } else if (show() & 0x40) {
    const int irsize = static_cast<int>(
        RotateResultSharedCount(m_rotateresults, m_rotatereslutpoints,
                                m_rotatereslutangles, m_rotateshaperesults));
    for (int i = 0; i < irsize; ++i) {
      if (i > m_ishownum || i < 0 ||
          i >= static_cast<int>(m_rotateshaperesults.size())) {
        continue;
      }

      PointsShape rotated_points = m_rotateshaperesults[i];
      rotated_points.setPen(0, 255, 0);
      rotated_points.setshow(16);
      gp_Path painter;
      rotated_points.drawshapex(painter, dmovx, dmovy, dangle, dzoomx, dzoomy);
    }
  } else if (show() & 0x80) {
    const int isize = static_cast<int>(
        std::min(m_models_rotate.size(), m_models_rotaterects.size()));
    for (int iz = 0; iz < isize; ++iz) {
      if (-1 == m_ispecshow || iz == m_ispecshow) {
        m_models_rotate[iz].setshow(8);
        gp_Path painter;
        m_models_rotate[iz].drawshape(painter);
        m_models_rotaterects[iz].setshow(16);
        m_models_rotaterects[iz].drawshape(painter);
      }
    }
  } else {
    FindLine::drawshape();
  }

  gp_Rectangle amatchrect(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
  amatchrect.setrect(
      gp_Pnt(m_matchrect.TopLeft().X() * dzoomx + dmovx,
             m_matchrect.TopLeft().Y() * dzoomy + dmovy, 0),
      gp_Pnt((m_matchrect.TopLeft().X() + m_matchrect.Width()) * dzoomx + dmovx,
             (m_matchrect.TopLeft().Y() + m_matchrect.Height()) * dzoomy +
                 dmovy,
             0));
}

void FastMatch::Learn(Image &image) {
  const int input_learn_roi_x = m_learn_roi_x;
  const int input_learn_roi_y = m_learn_roi_y;
  const int input_learn_roi_w = FastMatchPositiveInt(m_learn_roi_w);
  const int input_learn_roi_h = FastMatchPositiveInt(m_learn_roi_h);

  CXLOG_INFO("FastMatch", "learn_image_enter", "running",
             "image=" + std::to_string(image.getWidth()) + "x" +
                 std::to_string(image.getHeight()) + " rect=" +
                 std::to_string(static_cast<int>(rect().TopLeft().X())) + "," +
                 std::to_string(static_cast<int>(rect().TopLeft().Y())) + "," +
                 std::to_string(static_cast<int>(rect().Width())) + "," +
                 std::to_string(static_cast<int>(rect().Height())));
  m_fastmatch_learn_status_code = 1;
  m_fastmatch_learn_a_count = 0;
  m_fastmatch_learn_b_count = 0;
  m_fastmatch_learn_a2_count = 0;
  m_fastmatch_learn_b2_count = 0;
  m_normal_trace_candidate_count = 0;
  m_normal_trace_deduplicated_count = 0;
  m_normal_trace_point_count = 0;
  m_normal_trace_pair_count = 0;
  m_normal_trace_evidence = NormalTraceEvidence{};

  // Run four isolated FindLine measurements before any template composition.
  // Their evidence is used by the per-side UI tabs.  A non-All Point Column
  // deliberately promotes the selected candidates into the learn template;
  // All remains the compatibility path below.
  runDirectionalFindLineProbes(image);

  // Normal-Trace is the operator-selected continuation of the four
  // directional probes: value-preserving de-duplication of selected FindLine
  // conclusions, Dijkstra source-point routing, then domain-polarity A/B pairs.
  // It must run before
  // the directional compatibility template below; otherwise that branch can
  // return early and leave Normal-Trace evidence permanently at NOT_RUN.
  if (m_normal_trace_config.enabled) {
    // FastMatch compare_gap is authoritative for every Learn path. Historical
    // Evidence scripts may still pass a legacy Normal-Trace offset; do not let
    // that second value override the operator's current directional setting.
    m_normal_trace_config.normal_pair_offset_px =
        std::clamp(getconparegap(), 1, 128);
    CXLOG_INFO(
        "FastMatch", "learn_normal_trace_config", "effective",
        "overlap=" +
            std::to_string(m_normal_trace_config.domain_overlap_radius_px) +
         " source=findline_selected_conclusions image_reprocessing=0" +
            " max_nodes=" +
            std::to_string(m_normal_trace_config.dijkstra_max_nodes) +
            " pair_offset=" +
            std::to_string(m_normal_trace_config.normal_pair_offset_px) +
            " pair_offset_source=fastmatch_compare_gap" +
            " knn=" +
            std::to_string(m_normal_trace_config.dijkstra_knn_neighbors) +
            " ann_radius=" +
            std::to_string(m_normal_trace_config.ann_search_radius_px) +
            " ann_adaptive_radius=" +
            std::to_string(m_normal_trace_config.ann_search_radius_px * 2) +
            " ann_min_points=" +
            std::to_string(m_normal_trace_config.ann_min_component_points) +
            " ann_min_coverage=" +
            std::to_string(
                m_normal_trace_config.ann_min_component_coverage_percent) +
             " anchor_radius=" +
             std::to_string(
                 m_normal_trace_config.anchor_neighborhood_radius_px) +
             " prefilter_slope_deg=" +
             std::to_string(
                 m_normal_trace_config.ann_tangent_deviation_deg) +
             " prefilter_normal_deg=" +
             std::to_string(
                 m_normal_trace_config.ann_normal_deviation_deg) +
             " xy_bin=" +
             std::to_string(m_normal_trace_config.xy_compression_bin_px) +
             " min_keypoints_per_domain=" +
             std::to_string(
                 m_normal_trace_config.min_keypoints_per_domain) +
             " endpoint_spike_ratio_percent=" +
             std::to_string(
                 m_normal_trace_config.endpoint_spike_ratio_percent) +
             " junction_tangent_points=" +
             std::to_string(
                 m_normal_trace_config.junction_tangent_window_points) +
             " junction_min_cross_angle_deg=" +
             std::to_string(
                 m_normal_trace_config.junction_min_cross_angle_deg) +
             " junction_max_extrapolation_percent=" +
             std::to_string(
                 m_normal_trace_config.junction_max_extrapolation_percent) +
             " junction_join_spacing_multiplier_percent=" +
             std::to_string(
                 m_normal_trace_config
                     .junction_join_spacing_multiplier_percent));
    PointsShape normal_trace_pattern;
    if (LearnPatternByNormalTrace(
            image, *this, input_learn_roi_x, input_learn_roi_y,
            input_learn_roi_w, input_learn_roi_h, normal_trace_pattern,
            m_normal_trace_candidate_count, m_normal_trace_deduplicated_count,
            m_normal_trace_point_count, m_normal_trace_pair_count,
            m_normal_trace_evidence)) {
      FindLine::setpattern(normal_trace_pattern);
      m_fastmatch_learn_status_code = 33;
      modelzeroposition();
      gp_Rectangle learned_rect = FindLine::patternboundingrectAB();
      m_imodelwith = static_cast<int>(learned_rect.Width());
      m_imodelheigh = static_cast<int>(learned_rect.Height());
      buildReferenceFormFitModel(image);
      CXLOG_INFO("FastMatch", "learn_normal_trace", "complete",
                 "domain=" + std::to_string(m_normal_trace_candidate_count) +
                 " trace=" + std::to_string(m_normal_trace_point_count) +
                 " pairs=" + std::to_string(m_normal_trace_pair_count) +
                 " segments=" +
                 std::to_string(m_normal_trace_evidence.trace_segment_count) +
                  " closure_px=" +
                  std::to_string(m_normal_trace_evidence.closure_error_px) +
                  " max_step_px=" +
                  std::to_string(
                      m_normal_trace_evidence.max_consecutive_gap_px) +
                   " selected_anchor_coverage=" +
                   std::to_string(
                       m_normal_trace_evidence.selected_anchor_coverage) +
                   " generated_conclusions=" +
                   std::to_string(
                       m_normal_trace_evidence.generated_conclusion_count) +
                   " findline_bound_pairs=" +
                   std::to_string(
                       m_normal_trace_evidence.normal_pair_findline_bound_count) +
                   " binding_misses=" +
                   std::to_string(
                       m_normal_trace_evidence.normal_pair_binding_miss_count) +
                   " corner_rejected=" +
                   std::to_string(
                       m_normal_trace_evidence.normal_pair_corner_rejected_count) +
                   " loop_erased=" +
                   std::to_string(
                       m_normal_trace_evidence.loop_erased_point_count) +
                   " pair_directions=" +
                   std::to_string(m_normal_trace_evidence
                                      .normal_pair_counts_by_direction[0]) +
                   "," +
                   std::to_string(m_normal_trace_evidence
                                      .normal_pair_counts_by_direction[1]) +
                   "," +
                   std::to_string(m_normal_trace_evidence
                                      .normal_pair_counts_by_direction[2]) +
                   "," +
                    std::to_string(m_normal_trace_evidence
                                       .normal_pair_counts_by_direction[3]) +
                    " conclusion_graph=" +
                    NormalTraceAnchorPrefilterSummary(m_normal_trace_evidence) +
                    " ann=" +
                   NormalTraceAnnEvidenceSummary(m_normal_trace_evidence));
      return;
    }
    CXLOG_WARN("FastMatch", "learn_normal_trace", "fallback",
               "domain=" + std::to_string(m_normal_trace_candidate_count) +
               " trace=" + std::to_string(m_normal_trace_point_count) +
                " pairs=" + std::to_string(m_normal_trace_pair_count) +
                 " segments=" +
                 std::to_string(m_normal_trace_evidence.trace_segment_count) +
                 " reason=" + m_normal_trace_evidence.reason +
                 " conclusion_graph=" +
                 NormalTraceAnchorPrefilterSummary(m_normal_trace_evidence) +
                 " ann=" +
                 NormalTraceAnnEvidenceSummary(m_normal_trace_evidence));
  } else {
    m_normal_trace_evidence.reason = "NORMAL_TRACE_DISABLED";
    CXLOG_INFO("FastMatch", "learn_normal_trace", "not_run",
               "enabled=0; enable Normal-Trace Learn before running Learn");
  }

  // Any configured directional probe owns a physical side of the template.
  // Full edge retains all side candidates; Edge N/Last is an operator
  // override. Previously, mixing Full edge and Edge 1 silently omitted the
  // Full-edge directions, producing an incomplete four-edge model.
  if (hasExplicitLearnDirectionParams()) {
    PointsShape selected_probe_pattern;
    std::set<std::pair<int, int>> used_points;
    int pair_count = 0;
    const double angle = getscanrotation() * CV_PI / 180.0;
    const double cos_angle = std::cos(angle);
    const double sin_angle = std::sin(angle);
    for (int direction = 0; direction < 4; ++direction) {
      const DirectionalProbeEvidence& evidence =
          getdirectionalprobeevidence(direction);
      if (!evidence.executed)
        continue;
      // Model composition uses the exact accepted candidate set per Gauge
      // Line. This preserves Full edge while keeping Edge N/Last single-edge.
      // The raw-points fallback is visual evidence only.
      std::vector<CxShapePoint> modelPoints;
      modelPoints.reserve(evidence.accepted_points.size());
      for (std::size_t scan = 0;
           scan < evidence.accepted_points_by_scan.size(); ++scan) {
        const std::vector<CxShapePoint>& points =
            evidence.accepted_points_by_scan[scan];
        modelPoints.insert(modelPoints.end(), points.begin(), points.end());
      }
      if (modelPoints.empty())
        continue;
      // The local outward normal follows the selected physical side.  Rotate
      // it with the gauge so the pair is a true scan normal, not a global X/Y
      // symmetry pair.
      double local_nx = 0.0;
      double local_ny = 0.0;
      if (direction == 0) local_ny = -1.0;      // Top
      else if (direction == 1) local_ny = 1.0;  // Bottom
      else if (direction == 2) local_nx = -1.0; // Left
      else local_nx = 1.0;                      // Right
      const double nx = local_nx * cos_angle - local_ny * sin_angle;
      const double ny = local_nx * sin_angle + local_ny * cos_angle;
      // compare_gap is the distance from this conclusion point to each member
      // of the A/B polarity pair; the complete A-to-B distance is twice it.
      const double pair_offset =
          std::max(1.0, static_cast<double>(evidence.params.compare_gap));
      const int stride = std::max(
          1, static_cast<int>(modelPoints.size()) / 96);
      for (std::size_t point_index = 0;
           point_index < modelPoints.size();
           point_index += static_cast<std::size_t>(stride)) {
        const CxShapePoint& point = modelPoints[point_index];
        const int px = static_cast<int>(std::lround(point.x));
        const int py = static_cast<int>(std::lround(point.y));
        // A corner may appear in two side probes.  Keep one physical sample,
        // so a selected Point Column cannot create an overlapping double
        // template at the corners.
        if (!used_points.insert({px, py}).second)
          continue;
        const double ax = static_cast<double>(px) + nx * pair_offset;
        const double ay = static_cast<double>(py) + ny * pair_offset;
        const double bx = static_cast<double>(px) - nx * pair_offset;
        const double by = static_cast<double>(py) - ny * pair_offset;
        if (!FastMatchPointInsideImage(image, static_cast<int>(std::lround(ax)),
                                       static_cast<int>(std::lround(ay))) ||
            !FastMatchPointInsideImage(image, static_cast<int>(std::lround(bx)),
                                       static_cast<int>(std::lround(by))))
          continue;
        Standard_Real point_ax = static_cast<Standard_Real>(ax);
        Standard_Real point_ay = static_cast<Standard_Real>(ay);
        Standard_Real point_bx = static_cast<Standard_Real>(bx);
        Standard_Real point_by = static_cast<Standard_Real>(by);
        selected_probe_pattern.addpointa(point_ax, point_ay);
        selected_probe_pattern.addpointb(point_bx, point_by);
        ++pair_count;
      }
    }
    if (pair_count >= 2 && selected_probe_pattern.ABsize() > 0) {
      FindLine::setpattern(selected_probe_pattern);
      m_fastmatch_learn_a_count = pair_count;
      m_fastmatch_learn_b_count = pair_count;
      m_fastmatch_learn_a2_count = 0;
      m_fastmatch_learn_b2_count = 0;
      m_fastmatch_learn_status_code = 34;
      modelzeroposition();
      gp_Rectangle learned_rect = FindLine::patternboundingrectAB();
      m_imodelwith = static_cast<int>(learned_rect.Width());
      m_imodelheigh = static_cast<int>(learned_rect.Height());
      CXLOG_INFO("FastMatch", "learn_directional_selected_column", "complete",
                 "pairs=" + std::to_string(pair_count) +
                     " source=directional_findline_probe" +
                     " compare_gap_top=" +
                     std::to_string(getdirectionalprobeevidence(0).params.compare_gap) +
                     " compare_gap_bottom=" +
                     std::to_string(getdirectionalprobeevidence(1).params.compare_gap) +
                     " compare_gap_left=" +
                     std::to_string(getdirectionalprobeevidence(2).params.compare_gap) +
                     " compare_gap_right=" +
                     std::to_string(getdirectionalprobeevidence(3).params.compare_gap));
      return;
    }
    CXLOG_WARN("FastMatch", "learn_directional_selected_column", "fallback",
               "directional probes produced fewer than two valid normal pairs");
  }

  // The legacy directional implementation is axis-aligned and constructs
  // symmetric pairs directly from scan hits.  It must not be enabled until
  // the domain-deduplication -> Dijkstra trace -> local-normal pipeline owns
  // its pairing semantics; otherwise rotated parts produce crossed/double
  // image templates.
  const bool use_directional_learn = false;
  if (hasExplicitLearnDirectionParams()) {
    CXLOG_INFO("FastMatch", "learn_boundary_directional_all_edges", "running",
               "all directional Point Columns are All; retaining the legacy shared edgepattern compatibility path");
  }
  if (use_directional_learn) {
    PointsShape directional_pattern;
    if (LearnPatternByBoundaryPointPairs(
            image, *this, input_learn_roi_x, input_learn_roi_y,
            input_learn_roi_w, input_learn_roi_h, directional_pattern,
            m_fastmatch_learn_a_count, m_fastmatch_learn_b_count,
            m_fastmatch_learn_a2_count, m_fastmatch_learn_b2_count)) {
      FindLine::setpattern(directional_pattern);
      m_fastmatch_learn_status_code = 32;
      modelzeroposition();
      gp_Rectangle learned_rect = FindLine::patternboundingrectAB();
      m_imodelwith = static_cast<int>(learned_rect.Width());
      m_imodelheigh = static_cast<int>(learned_rect.Height());
      return;
    }
  }

  edgepattern(image);
  m_fastmatch_learn_a_count = FindLine::getlearnacount();
  m_fastmatch_learn_b_count = FindLine::getlearnbcount();
  m_fastmatch_learn_a2_count = FindLine::getlearna2count();
  m_fastmatch_learn_b2_count = FindLine::getlearnb2count();
  const int initial_collected_count =
      m_fastmatch_learn_a_count + m_fastmatch_learn_b_count +
      m_fastmatch_learn_a2_count + m_fastmatch_learn_b2_count;

  if (FindLine::getpatternpathA().ElementCount() > 0 &&
      FindLine::getpatternpathB().ElementCount() > 0 &&
      initial_collected_count > 0) {
    m_fastmatch_learn_status_code = 10;
    return;
  }

  const int saved_wgap = wgap();
  const int saved_hgap = hgap();
  const int saved_comparegap = getconparegap();
  const int saved_threshold = thre();
  const int saved_linegap = linegap();
  const int saved_rect_x = input_learn_roi_x;
  const int saved_rect_y = input_learn_roi_y;
  const int saved_rect_w = input_learn_roi_w;
  const int saved_rect_h = input_learn_roi_h;

  const LearnDirectionParams retry_params = effectiveLearnDirectionParams(0);
  const int retry_wgap = retry_params.wgap;
  const int retry_hgap = retry_params.hgap;
  const int retry_comparegap = retry_params.compare_gap;
  const int retry_threshold = retry_params.threshold;
  const int retry_linegap = retry_params.linegap;
  const int retry_margin = 2;
  const int retry_rect_x =
      saved_rect_x > retry_margin ? saved_rect_x - retry_margin : 0;
  const int retry_rect_y =
      saved_rect_y > retry_margin ? saved_rect_y - retry_margin : 0;
  const int retry_rect_right =
      (saved_rect_x + saved_rect_w + retry_margin) < image.getWidth()
          ? (saved_rect_x + saved_rect_w + retry_margin)
          : image.getWidth();
  const int retry_rect_bottom =
      (saved_rect_y + saved_rect_h + retry_margin) < image.getHeight()
          ? (saved_rect_y + saved_rect_h + retry_margin)
          : image.getHeight();
  const int retry_rect_w = retry_rect_right - retry_rect_x;
  const int retry_rect_h = retry_rect_bottom - retry_rect_y;

  setrect(retry_rect_x, retry_rect_y, retry_rect_w, retry_rect_h);
  SetWHgap(retry_wgap, retry_hgap);
  setcomparegap(retry_comparegap);
  setthre(retry_threshold);
  setlinegap(retry_linegap);
  edgepattern(image);
  m_fastmatch_learn_a_count = FindLine::getlearnacount();
  m_fastmatch_learn_b_count = FindLine::getlearnbcount();
  m_fastmatch_learn_a2_count = FindLine::getlearna2count();
  m_fastmatch_learn_b2_count = FindLine::getlearnb2count();
  const int retry_collected_count =
      m_fastmatch_learn_a_count + m_fastmatch_learn_b_count +
      m_fastmatch_learn_a2_count + m_fastmatch_learn_b2_count;

  if (FindLine::getpatternpathA().ElementCount() <= 0 ||
      FindLine::getpatternpathB().ElementCount() <= 0 ||
      retry_collected_count <= 0) {
    m_fastmatch_learn_status_code = 20;
    const int learn_x = saved_rect_x;
    const int learn_y = saved_rect_y;
    const int learn_w = std::max(1, saved_rect_w);
    const int learn_h = std::max(1, saved_rect_h);

    PointsShape rect_findline_pattern;
    if (LearnPatternThroughRectFindline(
            image, *this, learn_x, learn_y, learn_w, learn_h, false,
            rect_findline_pattern, m_fastmatch_learn_a_count,
            m_fastmatch_learn_b_count, m_fastmatch_learn_a2_count,
            m_fastmatch_learn_b2_count)) {
      FindLine::setpattern(rect_findline_pattern);
      m_fastmatch_learn_status_code = 30;
      modelzeroposition();
      gp_Rectangle learned_rect = FindLine::patternboundingrectAB();
      m_imodelwith = static_cast<int>(learned_rect.Width());
      m_imodelheigh = static_cast<int>(learned_rect.Height());
    } else if (LearnPatternThroughRectFindline(
                   image, *this, learn_x, learn_y, learn_w, learn_h, true,
                   rect_findline_pattern, m_fastmatch_learn_a_count,
                   m_fastmatch_learn_b_count, m_fastmatch_learn_a2_count,
                   m_fastmatch_learn_b2_count)) {
      FindLine::setpattern(rect_findline_pattern);
      m_fastmatch_learn_status_code = 31;
      modelzeroposition();
      gp_Rectangle learned_rect = FindLine::patternboundingrectAB();
      m_imodelwith = static_cast<int>(learned_rect.Width());
      m_imodelheigh = static_cast<int>(learned_rect.Height());
    } else if (false && LearnPatternByBoundaryPointPairs(
                   image, *this, learn_x, learn_y, learn_w, learn_h,
                   rect_findline_pattern, m_fastmatch_learn_a_count,
                   m_fastmatch_learn_b_count, m_fastmatch_learn_a2_count,
                   m_fastmatch_learn_b2_count)) {
      FindLine::setpattern(rect_findline_pattern);
      m_fastmatch_learn_status_code = 32;
      modelzeroposition();
      gp_Rectangle learned_rect = FindLine::patternboundingrectAB();
      m_imodelwith = static_cast<int>(learned_rect.Width());
      m_imodelheigh = static_cast<int>(learned_rect.Height());
    }
  }

  if (m_fastmatch_learn_a_count + m_fastmatch_learn_b_count +
          m_fastmatch_learn_a2_count + m_fastmatch_learn_b2_count <=
      0) {
    m_fastmatch_learn_a_count = std::max(
        0, static_cast<int>(FindLine::getpatternpathA().ElementCount()));
    m_fastmatch_learn_b_count = std::max(
        0, static_cast<int>(FindLine::getpatternpathB().ElementCount()));
    m_fastmatch_learn_a2_count = 0;
    m_fastmatch_learn_b2_count = 0;
  }

  if (m_fastmatch_learn_a_count + m_fastmatch_learn_b_count +
          m_fastmatch_learn_a2_count + m_fastmatch_learn_b2_count <=
      0)
    m_fastmatch_learn_status_code = -20;

  setrect(saved_rect_x, saved_rect_y, saved_rect_w, saved_rect_h);
  SetWHgap(saved_wgap, saved_hgap);
  setcomparegap(saved_comparegap);
  setthre(saved_threshold);
  setlinegap(saved_linegap);
}
void FastMatch::ZeroPOS() {
  modelzeroposition();
  gp_Rectangle arect1 = FindLine::patternboundingrectAB();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level0(Image &image) {
  Image aimage0;
  aimage0.CopyFrom(&image);
  aimage0.ROIpyrDown(5);
  setthre(50);
  setlinegap(7);
  edgepattern(aimage0);
  modelzeroposition();
  gp_Rectangle arect1 = FindLine::patternboundingrectAB();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level1(Image &image) {
  Image aimage0;
  aimage0.CopyFrom(&image);
  aimage0.ROIpyrDown(5);
  setthre(30);
  setlinegap(7);
  edgepattern(aimage0);
  modelzeroposition();
  gp_Rectangle arect1 = FindLine::patternboundingrectAB();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level2(Image &image) {
  Image aimage0;
  aimage0.CopyFrom(&image);
  aimage0.ROIpyrDown(3);
  setthre(30);
  setlinegap(6);
  edgepattern(aimage0);
  modelzeroposition();
  gp_Rectangle arect1 = FindLine::patternboundingrectAB();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level3(Image &image) {
  Image aimage0;
  aimage0.CopyFrom(&image);
  aimage0.ROIpyrDown(1);
  setthre(10);
  setlinegap(6);
  edgepattern(aimage0);
  modelzeroposition();
  gp_Rectangle arect1 = FindLine::patternboundingrectAB();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level4(Image &image) {
  setthre(7);
  setlinegap(3);
  edgepattern(image);
  modelzeroposition();
  gp_Rectangle arect1 = FindLine::patternboundingrectAB();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::modelzeroposition() {
  // Keep the native zero-origin template exactly as the matcher expects, but
  // preserve its pre-normalization translation for Image View diagnostics.
  // Rendering must never mutate m_modelpoints just to make a debug layer.
  const gp_Rectangle model_rect = FindLine::patternboundingrectAB();
  m_learn_model_origin_x = model_rect.TopLeft().X();
  m_learn_model_origin_y = model_rect.TopLeft().Y();
  FindLine::patternzeroposition();
}
void FastMatch::rotatemodelzeropositionAB() {
  const int isize = static_cast<int>(
      std::min(m_models_rotate.size(), m_models_rotaterects.size()));
  for (int iz = 0; iz < isize; iz++) {
    gp_Rectangle arect1 = m_models_rotate[iz].boundingRectAB();
    m_models_rotate[iz].MoveAB(-static_cast<int>(arect1.TopLeft().X()),
                               -static_cast<int>(arect1.TopLeft().Y()));
    m_models_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()),
                                  -static_cast<int>(arect1.TopLeft().Y()));
  }
}
void FastMatch::rotatemodelzeroposition() {
  const int isize = static_cast<int>(
      std::min(m_models_rotate.size(), m_models_rotaterects.size()));
  for (int iz = 0; iz < isize; iz++) {
    gp_Rectangle arect1 = m_models_rotate[iz].boundingRect();
    m_models_rotate[iz].Move(-static_cast<int>(arect1.TopLeft().X()),
                             -static_cast<int>(arect1.TopLeft().Y()));
    m_models_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()),
                                  -static_cast<int>(arect1.TopLeft().Y()));
  }
}
void FastMatch::rotatemodel05zeroposition() {
  const int isize = static_cast<int>(
      std::min(m_models05_rotate.size(), m_models05_rotaterects.size()));
  for (int iz = 0; iz < isize; iz++) {
    gp_Rectangle arect1 = m_models05_rotate[iz].boundingRect();
    m_models05_rotate[iz].Move(-static_cast<int>(arect1.TopLeft().X()),
                               -static_cast<int>(arect1.TopLeft().Y()));
    m_models05_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()),
                                    -static_cast<int>(arect1.TopLeft().Y()));
  }
}
void FastMatch::rotatemodel025zeroposition() {
  const int isize = static_cast<int>(
      std::min(m_models025_rotate.size(), m_models025_rotaterects.size()));
  for (int iz = 0; iz < isize; iz++) {
    gp_Rectangle arect1 = m_models025_rotate[iz].boundingRect();
    m_models025_rotate[iz].Move(-static_cast<int>(arect1.TopLeft().X()),
                                -static_cast<int>(arect1.TopLeft().Y()));
    m_models025_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()),
                                     -static_cast<int>(arect1.TopLeft().Y()));
  }
}
void FastMatch::learn_level0(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  Learn_level0(*pgetimage);
}
void FastMatch::learn_level1(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  Learn_level1(*pgetimage);
}
void FastMatch::learn_level2(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  Learn_level2(*pgetimage);
}
void FastMatch::learn_level3(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  Learn_level3(*pgetimage);
}
void FastMatch::learn_level4(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  Learn_level4(*pgetimage);
}

void FastMatch::learn(void *pimage) {
  m_debug_last_learn_argument = pimage;

  CXLOG_INFO("FastMatch", "learn_void_enter", "running",
             pimage == nullptr ? "pimage=null" : "pimage=non_null");
  Image *pgetimage = (Image *)pimage;

  if (pgetimage == nullptr || pgetimage->getmat().empty()) {
    m_modelpoints_sample1.clear();
    m_modelpoints_sample2.clear();
    m_modelpoints_sample3.clear();
    m_fastmatch_learn_status_code = -10;
    m_fastmatch_learn_a_count = 0;
    m_fastmatch_learn_b_count = 0;
    m_fastmatch_learn_a2_count = 0;
    m_fastmatch_learn_b2_count = 0;
    return;
  }

  Learn(*pgetimage);
}
void FastMatch::savemodelfile(const char *pchar) {
  ZeroPOS();
  FindLine::savepatternfile(pchar);
}
void FastMatch::loadmodelfile(const char *pchar) {
  FindLine::loadpatternfile(pchar);
  ZeroPOS();
  gp_Rectangle arect1 = FindLine::patternboundingrectAB();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::loadrotatemodelfile(const char *pchar) {
  FindLine::loadpatternfile(pchar);
  ZeroPOS();
  gp_Rectangle arect1 = FindLine::patternboundingrectAB();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());

  m_models_rotate.clear();

  m_models_rotaterects.clear();

  int ianglecur = 0;
  PointsShape amodelpoints;
  PointsShape arectpoints;
  PointsShape brectpoints;
  PointsShape bmodelrect;
  gp_Pnt apoint(0, 0, 0);
  gp_Pnt bpoint(m_imodelwith, 0, 0);
  gp_Pnt cpoint(m_imodelwith, m_imodelheigh, 0);
  gp_Pnt dpoint(0, m_imodelheigh, 0);
  arectpoints.addpoint(apoint);
  arectpoints.addpoint(bpoint);
  arectpoints.addpoint(cpoint);
  arectpoints.addpoint(dpoint);

  for (int i = 0; i < 360; i++) {
    ianglecur = i;
    amodelpoints = FindLine::getpattern();
    brectpoints = arectpoints;
    bmodelrect = brectpoints;
    amodelpoints.RotateAB(ianglecur);
    bmodelrect.Rotate(ianglecur);

    m_models_rotate.push_back(amodelpoints);
    m_models_rotaterects.push_back(bmodelrect);
  }

  rotatemodelzeropositionAB();
}
void FastMatch::loadrotate05modelfile(const char *pchar) {
  FindLine::loadpatternfile(pchar);
  ZeroPOS();
  gp_Rectangle arect1 = FindLine::patternboundingrect();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());

  m_models05_rotate.clear();

  m_models05_rotaterects.clear();

  double danglecur = 0;
  PointsShape amodelpoints;
  PointsShape arectpoints;
  PointsShape brectpoints;
  PointsShape bmodelrect;
  gp_Pnt apoint(0, 0, 0);
  gp_Pnt bpoint(m_imodelwith, 0, 0);
  gp_Pnt cpoint(m_imodelwith, m_imodelheigh, 0);
  gp_Pnt dpoint(0, m_imodelheigh, 0);
  arectpoints.addpoint(apoint);
  arectpoints.addpoint(bpoint);
  arectpoints.addpoint(cpoint);
  arectpoints.addpoint(dpoint);

  for (int i = 0; i < 720; i++) {
    danglecur = i * 0.5;
    amodelpoints = FindLine::getpattern();
    brectpoints = arectpoints;
    bmodelrect = brectpoints;
    amodelpoints.Rotate(danglecur);
    bmodelrect.Rotate(danglecur);

    m_models05_rotate.push_back(amodelpoints);
    m_models05_rotaterects.push_back(bmodelrect);
  }

  rotatemodel05zeroposition();
}
void FastMatch::loadrotate025modelfile(const char *pchar) {
  FindLine::loadpatternfile(pchar);
  ZeroPOS();
  gp_Rectangle arect1 = FindLine::patternboundingrect();
  m_imodelwith = static_cast<int>(arect1.Width());
  m_imodelheigh = static_cast<int>(arect1.Height());

  m_models025_rotate.clear();

  m_models025_rotaterects.clear();

  double danglecur = 0;
  PointsShape amodelpoints;
  PointsShape arectpoints;
  PointsShape brectpoints;
  PointsShape bmodelrect;
  gp_Pnt apoint(0, 0, 0);
  gp_Pnt bpoint(m_imodelwith, 0, 0);
  gp_Pnt cpoint(m_imodelwith, m_imodelheigh, 0);
  gp_Pnt dpoint(0, m_imodelheigh, 0);
  arectpoints.addpoint(apoint);
  arectpoints.addpoint(bpoint);
  arectpoints.addpoint(cpoint);
  arectpoints.addpoint(dpoint);

  for (int i = 0; i < 1440; i++) {
    danglecur = i * 0.25;
    amodelpoints = FindLine::getpattern();
    brectpoints = arectpoints;
    bmodelrect = brectpoints;
    amodelpoints.Rotate(danglecur);
    bmodelrect.Rotate(danglecur);

    m_models025_rotate.push_back(amodelpoints);
    m_models025_rotaterects.push_back(bmodelrect);
  }

  rotatemodel025zeroposition();
}
void FastMatch::ABtoShape(std::vector<cv::Point2f> &points) {
  return FindLine::ABtoShape(points);
}
int FastMatch::ABpatternsize() { return FindLine::ABpatternsize(); }
std::vector<std::string> split(const std::string &str, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(str);

  while (getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }

  return tokens;
}
void FastMatch::loadcalibration(const char *pchar) {
  clear();
  FILE *rf = nullptr;
  fopen_s(&rf, pchar, "rb");
  if (nullptr == rf)
    return;
  fseek(rf, 0, SEEK_END);
  int filesize = ftell(rf);
  char *pcharget = new char[filesize + 10];
  memset(pcharget, 0, filesize + 10);
  rewind(rf);
  fread((char *)(pcharget), filesize, 1, rf);
  string astr = pcharget;
  vector<string> strcalnumlist = split(astr, '|');

  delete[] pcharget;
  fclose(rf);
}
void FastMatch::savecalibration(const char *pchar) { (void)pchar; }
void FastMatch::setrotateangle(double dangle) {
  m_danglegap = FastMatchPositiveFiniteOr(dangle, m_danglegap);
}
void FastMatch::setrotateanglescale(double dangle1, double dangle2) {
  m_dangle_add = FastMatchFiniteOr(dangle2, m_dangle_add);
  m_dangle_mud = FastMatchFiniteOr(dangle1, m_dangle_mud);
}
void FastMatch::clearmodels_l12() { m_models_l12.clear(); }
void FastMatch::addmodels_l12(const char *pchar) {
  FindLine::loadpatternfile(pchar);
  m_models_l12.push_back(FindLine::getpattern());
}
void FastMatch::clearmodels_l36() { m_models_l36.clear(); }
void FastMatch::addmodels_l36(const char *pchar) {
  FindLine::loadpatternfile(pchar);
  m_models_l36.push_back(FindLine::getpattern());
}
void FastMatch::clearmodels_l72() { m_models_l72.clear(); }
void FastMatch::addmodels_l72(const char *pchar) {
  FindLine::loadpatternfile(pchar);
  m_models_l72.push_back(FindLine::getpattern());
}
void FastMatch::clearmodels_rotate() { m_models_rotate.clear(); }
void FastMatch::addmodels_rotate(const char *pchar) {
  FindLine::loadpatternfile(pchar);
  m_models_rotate.push_back(FindLine::getpattern());
}
void FastMatch::setcurmodels(int inum) {
  if (inum >= 0 && inum < static_cast<int>(m_models_l12.size()))
    FindLine::setpattern(m_models_l12[inum]);
}
void FastMatch::setcurimagemodels(int inum) {
  m_pgrid->SetUnit(12, 12);
  m_pgrid->UnitGrid();

  m_pgrid->ModelGridMethod_Gauss();
  m_imagefastmodel = m_pgrid->getfastmodel();

  if (inum >= 0 && inum < static_cast<int>(m_imagefastmodels_l12.size()))
    m_imagefastmodels_l12[inum] = m_imagefastmodel;
}
void FastMatch::modelstocurrent_l72(int i) {
  if (i >= 0 && i < static_cast<int>(m_models_l72.size()))
    FindLine::setpattern(m_models_l72[i]);
}
void FastMatch::modelstocurrent_l36(int i) {
  if (i >= 0 && i < static_cast<int>(m_models_l36.size()))
    FindLine::setpattern(m_models_l36[i]);
}
void FastMatch::modelstocurrent_l12(int i) {
  if (i >= 0 && i < static_cast<int>(m_models_l12.size()))
    FindLine::setpattern(m_models_l12[i]);
}
void FastMatch::modelstocurrent_l3(int i) {
  if (i >= 0 && i < static_cast<int>(m_models_l3.size()))
    FindLine::setpattern(m_models_l3[i]);
}
void FastMatch::modelstocurrent_l6(int i) {
  if (i >= 0 && i < static_cast<int>(m_models_l6.size()))
    FindLine::setpattern(m_models_l6[i]);
}
void FastMatch::patternrootgrid(double itype, double drate, double ilevel) {
  FindLine::patternrootgrid(itype, drate, ilevel);
}
void FastMatch::patterntranform(int igap, int itype, int isgap, int iline) {
  FindLine::patterntranform(igap, itype, isgap, iline);
  FindLine::patternzeroposition();
}
void FastMatch::patterngap2gap(int inewgap) {
  FindLine::patterngap2gap(inewgap);
}
void FastMatch::patternABgap2gap(double dnewgaprate) {
  FindLine::patternABgap2gap(dnewgaprate);
}
void FastMatch::patternABsample(int irate) { FindLine::patternABsample(irate); }
void FastMatch::pattern2org() { FindLine::pattern2org(); }
void FastMatch::org2pattern() { FindLine::org2pattern(); }
void FastMatch::patternzoom(double dx, double dy, double igap, double itype) {
  FindLine::patternzoom(dx, dy, igap, itype);
  FindLine::patternzeroposition();
}
void FastMatch::modelrotate(double dangle) { FindLine::patternrotate(dangle); }
void FastMatch::modelzoom(double dx, double dy) { FindLine::modelzoom(dx, dy); }
void FastMatch::setmodelwh(int iw, int ih) {
  gp_Rectangle rectf = FindLine::patternboundingrect();
  iw = FastMatchPositiveInt(iw);
  ih = FastMatchPositiveInt(ih);
  int iorgw = FastMatchPositiveInt(static_cast<int>(rectf.Width()));
  int iorgh = FastMatchPositiveInt(static_cast<int>(rectf.Height()));
  double dw = (iw * 1.0) / (iorgw * 1.0);
  double dh = (ih * 1.0) / (iorgh * 1.0);
  modelzoom(dw, dh);
}
void FastMatch::setmatchrect(int ix, int iy, int iw, int ih) {
  ix = FastMatchNonNegativeInt(ix);
  iy = FastMatchNonNegativeInt(iy);
  iw = FastMatchPositiveInt(iw);
  ih = FastMatchPositiveInt(ih);
  m_search_roi_x = ix;
  m_search_roi_y = iy;
  m_search_roi_w = iw;
  m_search_roi_h = ih;
  m_matchrect = gp_Rectangle(gp_Pnt(ix, iy, 0), gp_Pnt(ix + iw, iy + ih, 0));
  if (m_matchrects.size() <= 0) {
    gp_Rectangle arect(gp_Pnt(ix, iy, 0), gp_Pnt(ix + iw, iy + ih, 0));
    m_matchrects.addrect(arect);
  } else
    m_matchrects.setrect(0, ix, iy, iw, ih);
}

void FastMatch::setrectxywh(int ix, int iy, int iw, int ih) {
  setrect(ix, iy, iw, ih);
}

void FastMatch::setmatchrectxywh(int ix, int iy, int iw, int ih) {
  setmatchrect(ix, iy, iw, ih);
}

void FastMatch::setrectxywh_script(int ih, int iw, int iy, int ix) {
  setrectxywh(ix, iy, iw, ih);
}

void FastMatch::setmatchrectxywh_script(int ih, int iw, int iy, int ix) {
  setmatchrectxywh(ix, iy, iw, ih);
}

void FastMatch::setexpectedrect(double x0, double y0, double x1, double y1) {
  const double min_x = std::min(x0, x1);
  const double min_y = std::min(y0, y1);
  const double max_x = std::max(x0, x1);
  const double max_y = std::max(y0, y1);
  m_expected_rect =
      gp_Rectangle(gp_Pnt(min_x, min_y, 0), gp_Pnt(max_x, max_y, 0));
}

void FastMatch::setmatchrectnum(int inum) {
  if (inum <= 0)
    inum = 1;
  m_matchrects.clear();
  if (inum > 0)
    for (int i = 0; i < inum; i++) {
      gp_Rectangle arect(gp_Pnt(100, 100, 0), gp_Pnt(200, 200, 0));
      std::ostringstream stream;
      stream << i;
      string str = stream.str();
      m_matchrects.addrect(arect, str);
    }
}
gp_Rectangle &FastMatch::getmatchrect() { return m_matchrect; }
RectsShape &FastMatch::getmatchrects() { return m_matchrects; }
gp_Rectangle FastMatch::getresultrect(int inum) const {
  if (inum < 0 || inum >= m_resultrects.size())
    return gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
  gp_Rectangle arect0 = m_resultrects.getrect(inum);
  return arect0;
}

gp_Rectangle FastMatch::getresolvedresultrect(int inum) const {
  if (inum < 0 || inum >= static_cast<int>(m_resultpoints.size()))
    return gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));

  int rectw = FastMatchPositiveInt(m_imodelwith);
  int recth = FastMatchPositiveInt(m_imodelheigh);
  if (rectw <= 0) {
    const gp_Rectangle raw_rect = getresultrect(inum);
    rectw = FastMatchPositiveInt(static_cast<int>(raw_rect.BottomRight().X()));
  }
  if (recth <= 0) {
    const gp_Rectangle raw_rect = getresultrect(inum);
    recth = FastMatchPositiveInt(static_cast<int>(raw_rect.BottomRight().Y()));
  }

  const double cx = m_resultpoints.at(inum).X() + rectw / 2.0;
  const double cy = m_resultpoints.at(inum).Y() + recth / 2.0;
  const double x0 = cx - rectw / 2.0;
  const double y0 = cy - recth / 2.0;
  return gp_Rectangle(gp_Pnt(x0, y0, 0), gp_Pnt(x0 + rectw, y0 + recth, 0));
}
void FastMatch::setmultimatchrect(int inum, int ix, int iy, int iw, int ih) {
  if (inum >= 0 && inum < m_matchrects.size()) {
    const int rectx = FastMatchNonNegativeInt(ix);
    const int recty = FastMatchNonNegativeInt(iy);
    m_matchrects.setrect(inum, rectx, recty, FastMatchPositiveInt(iw),
                         FastMatchPositiveInt(ih));
  }
}
void FastMatch::resultclear() {
  m_iminfindnum = -1;
  m_resultpoints.clear();
  m_resultnums.clear();
  m_pose_candidates.clear();

  m_rawmatch_probe_count = 0;
  m_rawmatch_threshold_hit_count = 0;
  m_resulttolist_call_count = 0;
  m_resultcandidate_insert_count = 0;
  m_resultcandidate_replace_count = 0;
  m_resultcandidate_reject_count = 0;
  m_rawthresholdhitpoints.clear();
  m_rawthresholdhitscores.clear();
}
void FastMatch::resulttolist(gp_Pnt &apoint, int inum) {
  ++m_resulttolist_call_count;
  if (inum > m_iminfindnum) {
    const int keep_limit = std::max(1, m_imaxmatchnum);
    const int merge_half_w =
        std::max(1, keep_limit > 1 ? std::max(m_stepgapx * 2, m_imodelwith / 8)
                                   : m_imodelwith / 2);
    const int merge_half_h =
        std::max(1, keep_limit > 1 ? std::max(m_stepgapy * 2, m_imodelheigh / 8)
                                   : m_imodelheigh / 2);
    const int replace_margin = keep_limit > 1 ? 1 : 0;
    const size_t shared_count =
        std::min(m_resultnums.size(), m_resultpoints.size());
    if (m_resultnums.size() != shared_count)
      m_resultnums.resize(shared_count);
    if (m_resultpoints.size() != shared_count)
      m_resultpoints.resize(shared_count);

    for (int i = 0; i < static_cast<int>(shared_count); ++i) {
      gp_Pnt apoint0 = m_resultpoints.at(i);
      int ivalue = m_resultnums.at(i);
      const int idx = std::abs(static_cast<int>(apoint0.X() - apoint.X()));
      const int idy = std::abs(static_cast<int>(apoint0.Y() - apoint.Y()));
      if (idx <= merge_half_w && idy <= merge_half_h) {
        if (ivalue + replace_margin < inum) {
          m_resultpoints[i] = apoint;
          m_resultnums[i] = inum;
          ++m_resultcandidate_replace_count;
          goto NextRun01;
        } else {
          ++m_resultcandidate_reject_count;
          return;
        }
      }
    }
    if (0)
      if (m_iminfindnum != -1 && m_resultnums.size() > m_iminfindnum) {
        const int result_index = m_resultnums[m_iminfindnum];
        removeAt(m_resultnums, result_index);
        removePntAt(m_resultpoints, result_index);
      }

    m_resultpoints.push_back(apoint);
    m_resultnums.push_back(inum);
    ++m_resultcandidate_insert_count;

    NormalizeMatchCandidates(m_resultnums, m_resultpoints, keep_limit,
                             m_iminfindnum, m_iminpointkey);
  } else
    return;

NextRun01:
  NormalizeMatchCandidates(m_resultnums, m_resultpoints,
                           std::max(1, m_imaxmatchnum), m_iminfindnum,
                           m_iminpointkey);
}
void FastMatch::resultsort() {
  NormalizeMatchCandidates(m_resultnums, m_resultpoints,
                           std::max(1, m_imaxmatchnum), m_iminfindnum,
                           m_iminpointkey);
}
void FastMatch::rotateresultsortfilter(int ifdx, int ifdy, int itype) {
  FilterRotateResultsByDistance(m_rotateresults, m_rotatereslutpoints,
                                m_rotatereslutangles, m_rotateshaperesults,
                                ifdx, ifdy, itype, false);
}
void FastMatch::rotateresultsortfilterA(int ifdx, int ifdy, int itype) {
  FilterRotateResultsByDistance(m_rotateresults, m_rotatereslutpoints,
                                m_rotatereslutangles, m_rotateshaperesults,
                                ifdx, ifdy, itype, true);
}

int FastMatch::rotateresultsize() {
  return static_cast<int>(
      RotateResultSharedCount(m_rotateresults, m_rotatereslutpoints,
                              m_rotatereslutangles, m_rotateshaperesults));
}
void FastMatch::rotateresultsort() {
  const size_t shared_count =
      RotateResultSharedCount(m_rotateresults, m_rotatereslutpoints,
                              m_rotatereslutangles, m_rotateshaperesults);
  if (shared_count == 0)
    return;
  m_rotateresults.resize(shared_count);
  m_rotatereslutpoints.resize(shared_count);
  m_rotatereslutangles.resize(shared_count);
  m_rotateshaperesults.resize(shared_count);
  const int isize = static_cast<int>(shared_count);
  double dprevalue = m_rotateresults.at(0);
  gp_Pnt prepoint = m_rotatereslutpoints.at(0);
  double predangle = m_rotatereslutangles.at(0);
  PointsShape preshape = m_rotateshaperesults.at(0);
  int iprenum = 0;
  double dnxtvalue = m_rotateresults.at(0);
  gp_Pnt nxtpoint = m_rotatereslutpoints.at(0);
  double nxtdangle = m_rotatereslutangles.at(0);
  PointsShape nxtshape = m_rotateshaperesults.at(0);
  int inxtnum = 0;

  if (isize > 1)
    for (int i = 1; i < isize; ++i) {
      dprevalue = m_rotateresults.at(i - 1);
      prepoint = m_rotatereslutpoints.at(i - 1);
      predangle = m_rotatereslutangles.at(i - 1);
      preshape = m_rotateshaperesults.at(i - 1);
      iprenum = i - 1;
      dnxtvalue = m_rotateresults.at(i);
      nxtpoint = m_rotatereslutpoints.at(i);
      nxtdangle = m_rotatereslutangles.at(i);
      nxtshape = m_rotateshaperesults.at(i);
      inxtnum = i;
      if (dprevalue < dnxtvalue) {
        m_rotateresults[iprenum] = dnxtvalue;
        m_rotatereslutpoints[iprenum] = nxtpoint;
        m_rotatereslutangles[iprenum] = nxtdangle;
        m_rotateshaperesults[iprenum] = nxtshape;

        m_rotateresults[inxtnum] = dprevalue;
        m_rotatereslutpoints[inxtnum] = prepoint;
        m_rotatereslutangles[inxtnum] = predangle;
        m_rotateshaperesults[inxtnum] = preshape;

        for (int j = i - 1; j > 0; j--) {
          dprevalue = m_rotateresults.at(j - 1);
          prepoint = m_rotatereslutpoints.at(j - 1);
          predangle = m_rotatereslutangles.at(j - 1);
          preshape = m_rotateshaperesults.at(j - 1);

          iprenum = j - 1;

          dnxtvalue = m_rotateresults.at(j);
          nxtpoint = m_rotatereslutpoints.at(j);
          nxtdangle = m_rotatereslutangles.at(j);
          nxtshape = m_rotateshaperesults.at(j);

          inxtnum = j;

          if (dprevalue < dnxtvalue) {
            m_rotateresults[iprenum] = dnxtvalue;
            m_rotatereslutpoints[iprenum] = nxtpoint;
            m_rotatereslutangles[iprenum] = nxtdangle;
            m_rotateshaperesults[iprenum] = nxtshape;

            m_rotateresults[inxtnum] = dprevalue;
            m_rotatereslutpoints[inxtnum] = prepoint;
            m_rotatereslutangles[inxtnum] = predangle;
            m_rotateshaperesults[inxtnum] = preshape;
          }
        }
      }
    }
}

double FastMatch::getrotateresultx() {
  if (!m_rotatereslutpoints.empty()) {
    gp_Pnt apoint = m_rotatereslutpoints[0];
    return apoint.X();
  }
  return -9999;
}
double FastMatch::getrotateresulty() {
  if (!m_rotatereslutpoints.empty()) {
    gp_Pnt apoint = m_rotatereslutpoints[0];
    return apoint.Y();
  }
  return -9999;
}
double FastMatch::getrotateresulta() {
  if (!m_rotatereslutangles.empty()) {
    double drotatedangle = m_rotatereslutangles[0];
    if (drotatedangle > 270) {
      drotatedangle = drotatedangle - 360;
    }
    return drotatedangle;
  }
  return -999;
}
double FastMatch::getrotateresultscore() {
  if (!m_rotateresults.empty()) {
    double drotateresult = m_rotateresults[0];
    return drotateresult;
  }
  return 0;
}
double FastMatch::getrotateresultscoreA(int inum) {
  if (inum >= 0 && inum < static_cast<int>(m_rotateresults.size())) {
    double drotateresult = m_rotateresults[inum];
    return drotateresult;
  }
  return 0;
}

void FastMatch::clusterclear() { m_clusters.clear(); }
void FastMatch::resultcluster(int ixgap, int iygap, int ianglegap) {
  const size_t shared_count =
      std::min(m_rotatereslutpoints.size(), m_rotatereslutangles.size());
  if (shared_count == 0) {
    return;
  }
  const int ipointsize = static_cast<int>(shared_count);
  gp_Pnt apoint;
  bool binsert = false;
  const int cluster_count = static_cast<int>(m_clusters.size());
  if (0 == cluster_count) {
    Cluster newcluster;
    newcluster.push_back(0);
    m_clusters.push_back(newcluster);
  }
  for (int ir = 1; ir < ipointsize; ir++) {
    binsert = false;
    apoint = m_rotatereslutpoints[ir];
    const int ix0 = static_cast<int>(apoint.X());
    const int iy0 = static_cast<int>(apoint.Y());
    double dangle0 = m_rotatereslutangles[ir];
    const int current_cluster_count = static_cast<int>(m_clusters.size());
    for (int ic = 0; ic < current_cluster_count; ic++) {
      const int cluster_size = static_cast<int>(m_clusters[ic].size());
      for (int cluster_index = 0; cluster_index < cluster_size;
           cluster_index++) {
        int ipos = m_clusters[ic][cluster_index];
        if (ipos < 0 || ipos >= ipointsize) {
          continue;
        }
        gp_Pnt bpoint = m_rotatereslutpoints[ipos];
        const int ix1 = static_cast<int>(bpoint.X());
        const int iy1 = static_cast<int>(bpoint.Y());
        double dangle1 = m_rotatereslutangles[ipos];

        int ixgap0 = ix1 - ix0 > 0 ? ix1 - ix0 : ix0 - ix1;
        int iygap0 = iy1 - iy0 > 0 ? iy1 - iy0 : iy0 - iy1;
        double danglegap0 =
            dangle1 - dangle0 > 0 ? dangle1 - dangle0 : dangle0 - dangle1;
        if (ixgap0 < ixgap && iygap0 < iygap && danglegap0 < ianglegap) {
          m_clusters[ic].push_back(ir);
          binsert = true;
          const int cluster_size = static_cast<int>(m_clusters[ic].size());
          for (int sorted_index = 0; sorted_index < cluster_size - 1;
               ++sorted_index) {
            const int ipos0 = m_clusters[ic][sorted_index];
            const int ipos1 = m_clusters[ic][sorted_index + 1];
            if (ipos0 < 0 || ipos0 >= ipointsize || ipos1 < 0 ||
                ipos1 >= ipointsize) {
              continue;
            }

            const double sort_angle0 = m_rotatereslutangles[ipos0];
            const double sort_angle1 = m_rotatereslutangles[ipos1];
            if (sort_angle0 < sort_angle1) {
              m_clusters[ic][sorted_index] = ipos1;
              m_clusters[ic][sorted_index + 1] = ipos0;
            }
          }
        }
      }
    }
    if (false == binsert) {
      Cluster newcluster;
      newcluster.push_back(ir);
      m_clusters.push_back(newcluster);
    }
  }
}
void FastMatch::Distfilter() {
#if defined USE_AI
  gp_Path &pathA = FindLine::getpatternpathA();
  gp_Path &pathB = FindLine::getpatternpathB();
  size_t numPoints = pathA.getpoints().size();
  if (numPoints > 0) {
    arma::mat points(2, numPoints);
    for (size_t i = 0; i < numPoints; ++i) {
      points(0, i) = pathA.getpoints()[i].X();
      points(1, i) = pathA.getpoints()[i].Y();
    }
    pathA.Clear();
    auto filteredIndices =
        mlpackclass::AdaptiveFilterWithCoincidenceHandling_(points);
    for (auto idx : filteredIndices)
      pathA.AddPoint(gp_Pnt(points(0, idx), points(1, idx), 0));
  }
  numPoints = pathB.getpoints().size();
  if (numPoints > 0) {
    arma::mat points(2, numPoints);
    for (size_t i = 0; i < numPoints; ++i) {
      points(0, i) = pathB.getpoints()[i].X();
      points(1, i) = pathB.getpoints()[i].Y();
    }
    pathB.Clear();
    auto filteredIndices =
        mlpackclass::AdaptiveFilterWithCoincidenceHandling_(points);
    for (auto idx : filteredIndices)
      pathB.AddPoint(gp_Pnt(points(0, idx), points(1, idx), 0));
  }
#endif
}

void FastMatch::MatchAB(Image &image) {
  m_matchimage = &image;
  resultclear();
  ++m_matchab_call_count;
  m_match_last_stage = 20;
  gp_Path &pathA = FindLine::getpatternpathA();
  gp_Path &pathB = FindLine::getpatternpathB();

  const int icount1 = static_cast<int>(pathA.ElementCount());
  const int icount2 = static_cast<int>(pathB.ElementCount());
  const int pair_count = std::min(icount1, icount2);
  CXLOG_INFO(
      "FastMatch", "match_ab_enter", "running",
      "image=" + std::to_string(image.getWidth()) + "x" +
          std::to_string(image.getHeight()) +
          " learn_roi=" + std::to_string(m_learn_roi_x) + "," +
          std::to_string(m_learn_roi_y) + "," + std::to_string(m_learn_roi_w) +
          "," + std::to_string(m_learn_roi_h) +
          " search_roi=" + std::to_string(m_search_roi_x) + "," +
          std::to_string(m_search_roi_y) + "," +
          std::to_string(m_search_roi_w) + "," +
          std::to_string(m_search_roi_h) + " pathA=" + std::to_string(icount1) +
          " pathB=" + std::to_string(icount2) +
          " pairs=" + std::to_string(pair_count));
  if (pair_count <= 0) {
    m_match_last_stage = 32;
    return;
  }

  MatchSampleAB(image, pathA, pathB);
  RefreshPoseCandidates();

}

void FastMatch::setnormaltraceenabled(int enabled) {
  m_normal_trace_config.enabled = enabled != 0;
}
void FastMatch::setnormaltraceparams(int overlap_radius_px, int min_gradient,
                                     int max_nodes, int pair_offset_px) {
  m_normal_trace_config.domain_overlap_radius_px = std::clamp(overlap_radius_px, 0, 64);
  m_normal_trace_config.min_gradient = std::clamp(min_gradient, 1, 255);
  m_normal_trace_config.dijkstra_max_nodes = std::clamp(max_nodes, 32, 200000);
  m_normal_trace_config.normal_pair_offset_px =
      std::clamp(pair_offset_px, 1, 128);
}
void FastMatch::setnormaltracecosts(int gradient_weight_permille,
                                    int turn_weight_permille,
                                    int gap_weight_permille,
                                    int max_trace_gap_px) {
  m_normal_trace_config.dijkstra_gradient_cost_weight_permille = std::clamp(gradient_weight_permille, 0, 10000);
  m_normal_trace_config.dijkstra_turn_cost_weight_permille = std::clamp(turn_weight_permille, 0, 10000);
  m_normal_trace_config.dijkstra_gap_cost_weight_permille = std::clamp(gap_weight_permille, 0, 10000);
  m_normal_trace_config.dijkstra_max_trace_gap_px = std::clamp(max_trace_gap_px, 1, 64);
}
void FastMatch::setnormaltraceknn(int forward_neighbor_count) {
  m_normal_trace_config.dijkstra_knn_neighbors =
      std::clamp(forward_neighbor_count, 1, 32);
}
void FastMatch::setnormaltraceann(int search_radius_px,
                                  int tangent_deviation_deg,
                                  int normal_deviation_deg,
                                  int min_component_points,
                                  int min_component_coverage_percent) {
  m_normal_trace_config.ann_search_radius_px =
      std::clamp(search_radius_px, 6, 256);
  m_normal_trace_config.ann_tangent_deviation_deg =
      std::clamp(tangent_deviation_deg, 1, 89);
  m_normal_trace_config.ann_normal_deviation_deg =
      std::clamp(normal_deviation_deg, 1, 89);
  m_normal_trace_config.ann_min_component_points =
      std::clamp(min_component_points, 2, 256);
  m_normal_trace_config.ann_min_component_coverage_percent =
      std::clamp(min_component_coverage_percent, 1, 100);
}
void FastMatch::setnormaltracegeometry(int normal_angle_tolerance_deg,
                                       int trace_min_length_px, int normal_polarity,
                                       int corner_rejection_radius_px,
                                       int tangent_sample_step_px) {
  m_normal_trace_config.normal_angle_tolerance_deg = std::clamp(normal_angle_tolerance_deg, 1, 90);
  m_normal_trace_config.trace_min_length_px = std::clamp(trace_min_length_px, 2, 10000);
  m_normal_trace_config.normal_polarity = std::clamp(normal_polarity, -1, 1);
  m_normal_trace_config.corner_rejection_radius_px = std::clamp(corner_rejection_radius_px, 0, 128);
  m_normal_trace_config.tangent_sample_step_px = std::clamp(tangent_sample_step_px, 1, 64);
}
void FastMatch::setnormaltracedomain(int anchor_neighborhood_radius_px,
                                     int xy_compression_bin_px,
                                     int min_keypoints_per_domain) {
  m_normal_trace_config.anchor_neighborhood_radius_px =
      std::clamp(anchor_neighborhood_radius_px, 4, 256);
  m_normal_trace_config.xy_compression_bin_px =
      std::clamp(xy_compression_bin_px, 1, 64);
  m_normal_trace_config.min_keypoints_per_domain =
      std::clamp(min_keypoints_per_domain, 2, 256);
}

void FastMatch::setnormaltracejunction(
    int endpoint_spike_ratio_percent, int tangent_window_points,
    int minimum_cross_angle_deg, int maximum_extrapolation_percent,
    int join_spacing_multiplier_percent) {
  m_normal_trace_config.endpoint_spike_ratio_percent =
      std::clamp(endpoint_spike_ratio_percent, 110, 1000);
  m_normal_trace_config.junction_tangent_window_points =
      std::clamp(tangent_window_points, 2, 32);
  m_normal_trace_config.junction_min_cross_angle_deg =
      std::clamp(minimum_cross_angle_deg, 1, 89);
  m_normal_trace_config.junction_max_extrapolation_percent =
      std::clamp(maximum_extrapolation_percent, 25, 500);
  m_normal_trace_config.junction_join_spacing_multiplier_percent =
      std::clamp(join_spacing_multiplier_percent, 100, 2000);
}

void FastMatch::setformfitenabled(int enabled) {
  m_formfit_config.enabled = enabled != 0;
  if (!m_formfit_config.enabled) {
    m_formfit_result = CxFastMatchFormFitResult{};
    m_formfit_result.status = "FORM_FIT_DISABLED";
    m_formfit_result.failure_stage = "disabled";
  }
}

void FastMatch::setformfitdenseparams(
    int dense_step_milli_px, int profile_half_width_milli_px,
    int profile_step_milli_px, int minimum_gradient,
    int curvature_threshold_millideg) {
  m_formfit_config.dense_sample_step_milli_px =
      std::clamp(dense_step_milli_px, 100, 10000);
  m_formfit_config.profile_half_width_milli_px =
      std::clamp(profile_half_width_milli_px, 250, 20000);
  m_formfit_config.profile_step_milli_px =
      std::clamp(profile_step_milli_px, 50, 5000);
  m_formfit_config.profile_min_gradient =
      std::clamp(minimum_gradient, 0, 255);
  m_formfit_config.curvature_anchor_threshold_millideg =
      std::clamp(curvature_threshold_millideg, 100, 180000);
}

void FastMatch::setformfitannparams(
    int search_radius_milli_px, int normal_tolerance_deg, int trim_percent,
    int minimum_mutual_pairs, int maximum_iterations) {
  m_formfit_config.ann_search_radius_milli_px =
      std::clamp(search_radius_milli_px, 250, 100000);
  m_formfit_config.ann_normal_tolerance_deg =
      std::clamp(normal_tolerance_deg, 1, 90);
  m_formfit_config.ann_trim_percent = std::clamp(trim_percent, 0, 80);
  m_formfit_config.minimum_mutual_pairs =
      std::clamp(minimum_mutual_pairs, 2, 100000);
  m_formfit_config.maximum_iterations =
      std::clamp(maximum_iterations, 1, 100);
}

void FastMatch::setformfitbudget(int maximum_elapsed_ms,
                                 int maximum_structural_anchors,
                                 int allow_nonuniform_affine) {
  m_formfit_config.maximum_elapsed_ms =
      std::clamp(maximum_elapsed_ms, 1, 60000);
  m_formfit_config.maximum_structural_anchors =
      std::clamp(maximum_structural_anchors, 4, 4096);
  m_formfit_config.allow_nonuniform_affine = allow_nonuniform_affine != 0;
}

int FastMatch::getformfitstatuscode() {
  if (!m_formfit_result.executed)
    return 0;
  if (m_formfit_result.succeeded)
    return 1;
  return m_formfit_result.budget_exceeded ? -2 : -1;
}
int FastMatch::getformfitreferencecount() {
  return static_cast<int>(m_reference_shape_model.dense_points.size());
}
int FastMatch::getformfitobservedcount() {
  return static_cast<int>(m_observed_shape_model.dense_points.size());
}
int FastMatch::getformfitmutualcount() {
  return m_formfit_result.dense_mutual_count;
}
int FastMatch::getformfitanchorcount() {
  return static_cast<int>(m_reference_shape_model.anchors.size());
}
double FastMatch::getformfitscore() { return m_formfit_result.score; }
double FastMatch::getformfitresidual() {
  return m_formfit_result.symmetric_residual_px;
}

void FastMatch::buildReferenceFormFitModel(Image& image) {
  m_reference_shape_model = CxFastMatchShapeModel{};
  m_observed_shape_model = CxFastMatchShapeModel{};
  m_formfit_result = CxFastMatchFormFitResult{};
  m_formfit_gauge = cxcore::formfit::FormfitGauge{};
  if (!m_formfit_config.enabled)
    return;

  std::vector<CxFastMatchSourceObservation> sourcePoints;
  const std::size_t sourceCount = std::min({
      m_normal_trace_evidence.normal_pair_source_points.size(),
      m_normal_trace_evidence.normal_pair_a.size(),
      m_normal_trace_evidence.normal_pair_source_directions.size(),
      m_normal_trace_evidence.normal_pair_source_scans.size()});
  sourcePoints.reserve(sourceCount);
  for (std::size_t index = 0; index < sourceCount; ++index) {
    const CxShapePoint& source =
        m_normal_trace_evidence.normal_pair_source_points[index];
    const CxShapePoint& pointA =
        m_normal_trace_evidence.normal_pair_a[index];
    const double dx = pointA.x - source.x;
    const double dy = pointA.y - source.y;
    const double length = std::hypot(dx, dy);
    if (length <= 1e-9)
      continue;
    CxFastMatchSourceObservation observation;
    observation.x = source.x;
    observation.y = source.y;
    observation.normal_x = dx / length;
    observation.normal_y = dy / length;
    // The vector points toward the FindLine-selected A domain, so polarity is
    // inherited rather than recomputed from image gradient sign.
    observation.polarity = 1;
    observation.source_direction =
        m_normal_trace_evidence.normal_pair_source_directions[index];
    observation.source_scan =
        m_normal_trace_evidence.normal_pair_source_scans[index];
    sourcePoints.push_back(observation);
  }
  std::vector<cv::Point2d> trace;
  trace.reserve(m_normal_trace_evidence.derived_trace_points.size());
  for (const CxShapePoint& point :
       m_normal_trace_evidence.derived_trace_points)
    trace.emplace_back(point.x, point.y);
  std::vector<cv::Point2d> junctions;
  junctions.reserve(m_normal_trace_evidence.derived_junction_points.size());
  for (const CxShapePoint& point :
       m_normal_trace_evidence.derived_junction_points)
    junctions.emplace_back(point.x, point.y);

  m_reference_shape_model = BuildFastMatchReferenceShapeModel(
      sourcePoints, trace, junctions, image.getmat(), m_formfit_config,
      "fastmatch_reference_shape");
  if (!m_reference_shape_model.available) {
    m_formfit_result.executed = true;
    m_formfit_result.status = "FORM_FIT_MODEL_UNAVAILABLE";
    m_formfit_result.failure_stage = "reference_shape_model_build";
    refreshFormFitGauge();
    return;
  }

  // Learn produces an auditable self-observation.  A later transformmatch
  // replaces it with the real observed image model.
  FastMatchTransform identity;
  identity.cx = m_reference_shape_model.centroid_x;
  identity.cy = m_reference_shape_model.centroid_y;
  identity.half_u = std::max(1.0, m_reference_shape_model.bbox_width * 0.5);
  identity.half_v = std::max(1.0, m_reference_shape_model.bbox_height * 0.5);
  runFormFitOnImage(image, identity, "fastmatch_learn_self_observation");
}

void FastMatch::runFormFitOnImage(Image& image,
                                  const FastMatchTransform& seed,
                                  const char* observedModelId) {
  if (!m_formfit_config.enabled)
    return;
  if (!m_reference_shape_model.available) {
    m_formfit_result = CxFastMatchFormFitResult{};
    m_formfit_result.executed = true;
    m_formfit_result.status = "FORM_FIT_MODEL_UNAVAILABLE";
    m_formfit_result.failure_stage = "reference_shape_model_unavailable";
    refreshFormFitGauge();
    return;
  }
  m_observed_shape_model = BuildFastMatchObservedShapeModel(
      m_reference_shape_model, image.getmat(), seed, m_formfit_config,
      observedModelId ? observedModelId : "fastmatch_observed_shape");
  m_formfit_result = RunFastMatchBidirectionalFormFit(
      m_reference_shape_model, m_observed_shape_model, seed,
      m_formfit_config);
  refreshFormFitGauge();
  CXLOG_INFO("FastMatch", "form_fit", m_formfit_result.status,
             "reference_dense=" +
                 std::to_string(m_formfit_result.reference_dense_count) +
                 " observed_dense=" +
                 std::to_string(m_formfit_result.observed_dense_count) +
                 " mutual=" +
                 std::to_string(m_formfit_result.dense_mutual_count) +
                 " residual_px=" +
                 std::to_string(m_formfit_result.symmetric_residual_px) +
                 " score=" + std::to_string(m_formfit_result.score) +
                 " elapsed_ms=" +
                 std::to_string(m_formfit_result.elapsed_ms));
}

void FastMatch::refreshFormFitGauge() {
  m_formfit_gauge = cxcore::formfit::MakeFastMatchFormFitGauge(
      m_formfit_result, m_reference_shape_model, m_observed_shape_model,
      "fastmatch_form_fit_gauge", "FastMatch Form Fit Gauge");
}

void FastMatch::settransformsearchenabled(int enabled) {
  m_transform_search_config.enabled = enabled != 0;
}

void FastMatch::settransformcenter(int cx, int cy) {
  m_transform_search_initial.cx = static_cast<double>(cx);
  m_transform_search_initial.cy = static_cast<double>(cy);
}

void FastMatch::settransformextent(int half_u, int half_v) {
  m_transform_search_initial.half_u = std::max(0, half_u);
  m_transform_search_initial.half_v = std::max(0, half_v);
}

void FastMatch::settransformangle(double angle_deg) {
  m_transform_search_initial.angle_deg = angle_deg;
}

void FastMatch::settransformscalepermille(int scale_x_permille,
                                           int scale_y_permille) {
  m_transform_search_initial.scale_x =
      std::max(1, scale_x_permille) / 1000.0;
  m_transform_search_initial.scale_y =
      std::max(1, scale_y_permille) / 1000.0;
}

void FastMatch::settransformshearpermille(int shear_permille) {
  m_transform_search_initial.shear = static_cast<double>(shear_permille) / 1000.0;
}

void FastMatch::settransformprojectivepermille(int projective_u_permille,
                                                 int projective_v_permille) {
  m_transform_search_initial.projective_u =
      static_cast<double>(projective_u_permille) / 1000.0;
  m_transform_search_initial.projective_v =
      static_cast<double>(projective_v_permille) / 1000.0;
}

void FastMatch::settransformfromsegmentation(void* segmentation) {
  FindSegmentation* source = static_cast<FindSegmentation*>(segmentation);
  m_transform_seed_evidence = FastMatchTransformSeedEvidence();
  m_transform_seed_evidence.source = "segmentation_oriented_box";
  if (source == nullptr || source->get_geometry_count() <= 0) {
    m_transform_seed_evidence.reason = "segmentation has no oriented geometry";
    return;
  }
  m_transform_search_initial = FastMatchTransform::fromOrientedBox(
      source->get_geometry_center_x(), source->get_geometry_center_y(),
      source->get_geometry_axis_x(), source->get_geometry_axis_y(),
      source->get_geometry_angle_deg());
  m_transform_seed_evidence.available = m_transform_search_initial.valid();
  m_transform_seed_evidence.angle_convention = "image_clockwise_degrees";
  if (!m_transform_seed_evidence.available)
    m_transform_seed_evidence.reason = "segmentation geometry produced an invalid transform";
}

void FastMatch::settransformfromtorch(void* torch_task) {
  m_transform_seed_evidence = FastMatchTransformSeedEvidence();
  m_transform_seed_evidence.source = "torch_obb";
  TorchTask* source = static_cast<TorchTask*>(torch_task);
  if (source == nullptr) {
    m_transform_seed_evidence.reason = "TorchTask is null";
    return;
  }

  int detection_index = -1;
  int class_id = -1;
  double confidence = 0.0;
  std::string angle_convention;
  std::string reason;
  if (!CxTorchResultProjector::TryBuildFastMatchTransform(
          source->GetInferenceResult(), m_transform_search_initial,
          detection_index, class_id, confidence, angle_convention, reason)) {
    m_transform_seed_evidence.reason = reason;
    return;
  }

  const CxInferenceResult& inference = source->GetInferenceResult();
  m_transform_seed_evidence.available = true;
  m_transform_seed_evidence.model_id = inference.model_id;
  m_transform_seed_evidence.detection_index = detection_index;
  m_transform_seed_evidence.class_id = class_id;
  m_transform_seed_evidence.confidence = confidence;
  m_transform_seed_evidence.angle_convention = angle_convention;
}

bool FastMatch::bindcalibrationsnapshot(const CxCalibrationSnapshot& snapshot) {
  m_calibration_bound = m_calibration_adapter.bind(snapshot);
  return m_calibration_bound;
}

void FastMatch::clearcalibrationsnapshot() {
  m_calibration_adapter.reset();
  m_calibration_bound = false;
}

void FastMatch::setcalibrationtestenabled(int enabled) {
  m_calibration_test_override_enabled = enabled != 0;
  if (!m_calibration_test_override_enabled) {
    clearcalibrationsnapshot();
    return;
  }

  m_calibration_test_override.reset();
  m_calibration_test_override.setmetadata("manual_key_parameter_override",
                                          "manual", "manual", "manual");
  m_calibration_test_override.setcoordinateframe("calibrated_plane");
  m_calibration_test_override.setsource("manual_key_parameter_override");
  m_calibration_test_override.setunits("unit", "unit", "pixel");
}

void FastMatch::setcalibrationtestxytransform(double scale_x, double scale_y,
                                              double offset_x, double offset_y,
                                              double rotation_deg,
                                              double shear_x, double shear_y) {
  if (!m_calibration_test_override_enabled)
    return;
  m_calibration_test_override.setxytransformex(scale_x, scale_y, offset_x,
                                                offset_y, rotation_deg,
                                                shear_x, shear_y);
  bindcalibrationsnapshot(m_calibration_test_override.snapshot());
}

void FastMatch::setcalibrationtestxytransformscaled(
    int scale_x_ppm, int scale_y_ppm, int offset_x_milliunit,
    int offset_y_milliunit, int rotation_millideg, int shear_x_ppm,
    int shear_y_ppm) {
  setcalibrationtestxytransform(
      static_cast<double>(scale_x_ppm) / 1000000.0,
      static_cast<double>(scale_y_ppm) / 1000000.0,
      static_cast<double>(offset_x_milliunit) / 1000.0,
      static_cast<double>(offset_y_milliunit) / 1000.0,
      static_cast<double>(rotation_millideg) / 1000.0,
      static_cast<double>(shear_x_ppm) / 1000000.0,
      static_cast<double>(shear_y_ppm) / 1000000.0);
}

void FastMatch::setcalibrationtestreprojectionrmse(double reprojection_rmse_px) {
  if (!m_calibration_test_override_enabled)
    return;
  m_calibration_test_override.setreprojectionrmse(reprojection_rmse_px);
  bindcalibrationsnapshot(m_calibration_test_override.snapshot());
}

void FastMatch::settransformscalerangepercent(int percent) {
  m_transform_search_config.scale_range_percent = std::max(0, percent);
}
void FastMatch::settransformanglerange(int degrees) {
  m_transform_search_config.angle_range_deg = std::max(0, degrees);
}
void FastMatch::settransformcoarsesteps(int steps) {
  m_transform_search_config.coarse_steps_per_axis = std::max(1, std::min(5, steps));
}
void FastMatch::settransformfinerangepercent(int percent) {
  m_transform_search_config.fine_range_percent = std::max(0, percent);
}
void FastMatch::settransformfineanglerange(int degrees) {
  m_transform_search_config.fine_angle_range_deg = std::max(0, degrees);
}
void FastMatch::settransformmaxcandidates(int count) {
  m_transform_search_config.max_candidates = std::max(1, count);
}
void FastMatch::settransformmaxsamples(int count) {
  m_transform_search_config.max_samples = std::max(1, count);
}
void FastMatch::settransformmaxelapsedms(int milliseconds) {
  m_transform_search_config.max_elapsed_ms = std::max(1, milliseconds);
}
void FastMatch::settransformshearrangepermille(int permille) {
  m_transform_search_config.shear_range_permille = std::max(0, permille);
}
void FastMatch::settransformprojectiverangepermille(int permille) {
  m_transform_search_config.projective_range_permille = std::max(0, permille);
}

int FastMatch::gettransformsearchexecuted() { return m_transform_search_result.executed ? 1 : 0; }
int FastMatch::gettransformsearchconverged() { return m_transform_search_result.converged ? 1 : 0; }
int FastMatch::gettransformsearchbudgetexceeded() { return m_transform_search_result.budget_exceeded ? 1 : 0; }
int FastMatch::gettransformsearchcandidatecount() { return m_transform_search_result.evaluated_candidates; }
int FastMatch::gettransformsearchsamplecount() { return m_transform_search_result.sample_count; }
int FastMatch::gettransformsearchelapsedms() { return m_transform_search_result.elapsed_ms; }
double FastMatch::gettransformsearchscore() { return m_transform_search_result.best_score; }
double FastMatch::gettransformsearchscalex() { return m_transform_search_result.best.scale_x; }
double FastMatch::gettransformsearchscaley() { return m_transform_search_result.best.scale_y; }
double FastMatch::gettransformsearchangle() { return m_transform_search_result.best.angle_deg; }
double FastMatch::gettransformsearchshear() { return m_transform_search_result.best.shear; }
double FastMatch::gettransformsearchprojectiveu() { return m_transform_search_result.best.projective_u; }
double FastMatch::gettransformsearchprojectivev() { return m_transform_search_result.best.projective_v; }
int FastMatch::gettransformsearchcalibrationapplied() {
  return m_transform_search_result.calibration_applied ? 1 : 0;
}
double FastMatch::gettransformsearchcalibrationreprojectionrmse() {
  return m_transform_search_result.calibration_reprojection_rmse_px;
}
double FastMatch::gettransformsearchphysicalcx() {
  return m_transform_search_result.best_physical_cx;
}
double FastMatch::gettransformsearchphysicalcy() {
  return m_transform_search_result.best_physical_cy;
}
double FastMatch::gettransformsearchgradientscore() { return m_transform_search_result.gradient_score; }
double FastMatch::gettransformsearchresidual() { return m_transform_search_result.geometric_residual_px; }
double FastMatch::gettransformsearchrigidbaselinescore() { return m_transform_search_result.rigid_baseline_score; }
void FastMatch::setrotatemaxcandidates(int count) { m_rotate_max_candidates = std::max(1, count); }
void FastMatch::setrotatemaxelapsedms(int milliseconds) { m_rotate_max_elapsed_ms = std::max(1, milliseconds); }
void FastMatch::setrotatemaxprobes(int count) { m_rotate_max_probes = std::max(1, count); }
void FastMatch::setrotatemaxsamples(int count) { m_rotate_max_samples = std::max(1, count); }
int FastMatch::getrotatebudgetexceeded() { return m_rotate_budget_exceeded ? 1 : 0; }
int FastMatch::getrotatecandidatecount() { return m_rotate_candidate_count; }
int FastMatch::getrotateprobecount() { return m_rotate_probe_count; }
int FastMatch::getrotatesamplecount() { return m_rotate_sample_count; }
int FastMatch::getrotateelapsedms() {
  if (!m_rotate_budget_active)
    return 0;
  return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - m_rotate_budget_start).count());
}
void FastMatch::resetRotateBudget() {
  m_rotate_budget_start = std::chrono::steady_clock::now();
  m_rotate_budget_active = true;
  m_rotate_candidate_count = 0;
  m_rotate_probe_count = 0;
  m_rotate_sample_count = 0;
  m_rotate_budget_exceeded = false;
}
bool FastMatch::consumeRotateCandidate() {
  if (m_rotate_budget_exceeded)
    return false;
  if (++m_rotate_candidate_count > m_rotate_max_candidates ||
      getrotateelapsedms() >= m_rotate_max_elapsed_ms) {
    m_rotate_budget_exceeded = true;
    return false;
  }
  return true;
}
bool FastMatch::consumeRotateProbe(int sample_cost) {
  if (m_rotate_budget_exceeded)
    return false;
  ++m_rotate_probe_count;
  m_rotate_sample_count += std::max(1, sample_cost);
  if (m_rotate_probe_count > m_rotate_max_probes ||
      m_rotate_sample_count > m_rotate_max_samples ||
      getrotateelapsedms() >= m_rotate_max_elapsed_ms) {
    m_rotate_budget_exceeded = true;
    return false;
  }
  return true;
}

double FastMatch::evaluateTransformCandidate(
    Image& image, gp_Path& pathA, gp_Path& pathB,
    const FastMatchTransform& transform, bool sparse, int& hit_count,
    int& probe_count, double& continuity, double& gradient_score,
    double& geometric_residual_px, bool& budget_exceeded,
    const std::chrono::steady_clock::time_point& start) {
  const int count = std::min(static_cast<int>(pathA.ElementCount()),
                             static_cast<int>(pathB.ElementCount()));
  if (count <= 0 || !transform.valid())
    return 0.0;

  const gp_Rectangle bounds = pathA.boundingRect();
  const double base_x = bounds.TopLeft().X() + bounds.Width() * 0.5;
  const double base_y = bounds.TopLeft().Y() + bounds.Height() * 0.5;
  const int stride = sparse ? std::max(1, count / 8) : 1;
  bool previous_hit = false;
  int adjacent_hits = 0;
  int adjacent_pairs = 0;
  hit_count = 0;
  probe_count = 0;
  continuity = 0.0;
  gradient_score = 0.0;
  geometric_residual_px = 0.0;
  double gradient_sum = 0.0;
  double peak_alignment_residual_sum = 0.0;
  std::vector<cv::Point2d> edge_peak_points;

  for (int i = 0; i < count; i += stride) {
    if (m_transform_search_result.sample_count >=
        m_transform_search_config.max_samples) {
      budget_exceeded = true;
      return 0.0;
    }
    const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count());
    if (elapsed >= m_transform_search_config.max_elapsed_ms) {
      budget_exceeded = true;
      return 0.0;
    }
    const gp_Pnt point_a = pathA.ElementAt(i);
    const gp_Pnt point_b = pathB.ElementAt(i);
    double ax = 0.0, ay = 0.0, bx = 0.0, by = 0.0;
    transform.localToGlobal(point_a.X() - base_x, point_a.Y() - base_y, ax, ay);
    transform.localToGlobal(point_b.X() - base_x, point_b.Y() - base_y, bx, by);
    const int iax = static_cast<int>(std::lround(ax));
    const int iay = static_cast<int>(std::lround(ay));
    const int ibx = static_cast<int>(std::lround(bx));
    const int iby = static_cast<int>(std::lround(by));
    ++probe_count;
    if (!FastMatchPointInsideImage(image, iax, iay) ||
        !FastMatchPointInsideImage(image, ibx, iby)) {
      previous_hit = false;
      continue;
    }
    const double normal_x = bx - ax;
    const double normal_y = by - ay;
    const double normal_length = std::hypot(normal_x, normal_y);
    if (normal_length <= 1e-6) {
      previous_hit = false;
      continue;
    }
    // A small normal scan supplies an actual image-gradient maximum and the
    // sub-pixel-free geometric displacement of that maximum from mid-normal.
    constexpr int kNormalSamples = 7;
    double best_gradient = -1.0;
    int best_index = 0;
    for (int sample = 1; sample < kNormalSamples - 1; ++sample) {
      if (++m_transform_search_result.sample_count >
          m_transform_search_config.max_samples) {
        budget_exceeded = true;
        return 0.0;
      }
      const double t0 = static_cast<double>(sample - 1) / (kNormalSamples - 1);
      const double t1 = static_cast<double>(sample + 1) / (kNormalSamples - 1);
      const int x0 = static_cast<int>(std::lround(ax + normal_x * t0));
      const int y0 = static_cast<int>(std::lround(ay + normal_y * t0));
      const int x1 = static_cast<int>(std::lround(ax + normal_x * t1));
      const int y1 = static_cast<int>(std::lround(ay + normal_y * t1));
      if (!FastMatchPointInsideImage(image, x0, y0) ||
          !FastMatchPointInsideImage(image, x1, y1))
        continue;
      const cv::Vec3b p0 = image.pixel(x0, y0);
      const cv::Vec3b p1 = image.pixel(x1, y1);
      const double g0 = 0.299 * Red(p0) + 0.587 * Green(p0) + 0.114 * Blue(p0);
      const double g1 = 0.299 * Red(p1) + 0.587 * Green(p1) + 0.114 * Blue(p1);
      const double gradient = std::abs(g1 - g0) /
          std::max(1.0, normal_length * 2.0 / (kNormalSamples - 1));
      if (gradient > best_gradient) {
        best_gradient = gradient;
        best_index = sample;
      }
    }
    const bool hit = best_gradient >= static_cast<double>(m_imatchthre);
    if (hit)
      ++hit_count;
    if (best_gradient >= 0.0) {
      gradient_sum += best_gradient / 255.0;
      peak_alignment_residual_sum += std::abs(best_index - (kNormalSamples - 1) * 0.5) *
          normal_length / (kNormalSamples - 1);
      const double peak_t = static_cast<double>(best_index) /
          static_cast<double>(kNormalSamples - 1);
      edge_peak_points.emplace_back(ax + normal_x * peak_t,
                                    ay + normal_y * peak_t);
    }
    if (probe_count > 1) {
      ++adjacent_pairs;
      if (hit && previous_hit)
        ++adjacent_hits;
    }
    previous_hit = hit;
  }
  if (probe_count == 0)
    return 0.0;
  continuity = adjacent_pairs == 0 ? 0.0 :
      static_cast<double>(adjacent_hits) / static_cast<double>(adjacent_pairs);
  gradient_score = gradient_sum / probe_count;
  const double peak_alignment_residual = peak_alignment_residual_sum / probe_count;
  // Total-least-squares fit of the observed gradient peaks.  The smaller
  // eigenvalue of their covariance is the mean squared orthogonal distance to
  // the fitted line, so its square root is a geometric fit RMSE in pixels.
  double fit_rmse = peak_alignment_residual;
  if (edge_peak_points.size() >= 2U) {
    cv::Point2d mean(0.0, 0.0);
    for (const cv::Point2d& point : edge_peak_points)
      mean += point;
    mean *= 1.0 / static_cast<double>(edge_peak_points.size());
    double xx = 0.0, xy = 0.0, yy = 0.0;
    for (const cv::Point2d& point : edge_peak_points) {
      const double dx = point.x - mean.x;
      const double dy = point.y - mean.y;
      xx += dx * dx;
      xy += dx * dy;
      yy += dy * dy;
    }
    const double n = static_cast<double>(edge_peak_points.size());
    const double trace = (xx + yy) / n;
    const double determinant = (xx * yy - xy * xy) / (n * n);
    const double minor = std::max(0.0, 0.5 * (trace -
        std::sqrt(std::max(0.0, trace * trace - 4.0 * determinant))));
    fit_rmse = std::sqrt(minor);
  }
  geometric_residual_px = fit_rmse + 0.25 * peak_alignment_residual;
  const double appearance = static_cast<double>(hit_count) / probe_count;
  const double prior = 0.02 * (std::abs(transform.scale_x - 1.0) +
                               std::abs(transform.scale_y - 1.0)) +
                       0.0005 * std::abs(transform.angle_deg -
                                         m_transform_search_initial.angle_deg);
  return std::max(0.0, (0.65 * gradient_score + 0.35 * appearance) *
      (0.8 + 0.2 * continuity) - 0.05 * geometric_residual_px - prior);
}

void FastMatch::runTransformSearch(Image& image) {
  resultclear();
  m_transform_search_result = FastMatchTransformSearchResult();
  m_transform_search_result.executed = true;
  m_transform_search_result.seed = m_transform_seed_evidence;
  if (m_calibration_bound && m_calibration_adapter.ready()) {
    const CxCalibrationSnapshot& calibration = m_calibration_adapter.snapshot();
    m_transform_search_result.calibration_applied = true;
    m_transform_search_result.calibration_snapshot_hash = calibration.snapshot_hash;
    m_transform_search_result.calibration_source_ref = calibration.source_ref;
    m_transform_search_result.calibration_coordinate_frame_id = calibration.coordinate_frame_id;
    m_transform_search_result.calibration_xy_unit = calibration.xy_unit;
    if (calibration.has_reprojection_rmse)
      m_transform_search_result.calibration_reprojection_rmse_px =
          calibration.reprojection_rmse_px;
  }
  m_transform_search_result.initial = m_transform_search_initial;
  if (m_transform_search_initial.cx == 0.0 && m_transform_search_initial.cy == 0.0) {
    m_transform_search_initial.cx = m_search_roi_x + m_search_roi_w * 0.5;
    m_transform_search_initial.cy = m_search_roi_y + m_search_roi_h * 0.5;
    if (!m_transform_search_result.seed.available) {
      m_transform_search_result.seed.source = "search_roi_fallback";
      m_transform_search_result.seed.reason = "no external transform seed";
    }
  }
  if (m_transform_search_initial.half_u <= 0.0)
    m_transform_search_initial.half_u = std::max(1, m_learn_roi_w / 2);
  if (m_transform_search_initial.half_v <= 0.0)
    m_transform_search_initial.half_v = std::max(1, m_learn_roi_h / 2);
  m_transform_search_result.initial = m_transform_search_initial;
  m_transform_search_result.best = m_transform_search_initial;
  gp_Path& pathA = FindLine::getpatternpathA();
  gp_Path& pathB = FindLine::getpatternpathB();
  if (std::min(pathA.ElementCount(), pathB.ElementCount()) <= 0) {
    m_transform_search_result.failure_stage = "model_missing";
    return;
  }

  const auto start = std::chrono::steady_clock::now();
  bool budget_exceeded = false;
  {
    FastMatchTransform rigid = m_transform_search_initial;
    rigid.scale_x = 1.0;
    rigid.scale_y = 1.0;
    rigid.shear = 0.0;
    rigid.projective_u = 0.0;
    rigid.projective_v = 0.0;
    int rigidHits = 0, rigidProbes = 0;
    double rigidContinuity = 0.0, rigidGradient = 0.0, rigidResidual = 0.0;
    const int sampleBeforeBaseline = m_transform_search_result.sample_count;
    m_transform_search_result.rigid_baseline_score = evaluateTransformCandidate(
        image, pathA, pathB, rigid, true, rigidHits, rigidProbes,
        rigidContinuity, rigidGradient, rigidResidual, budget_exceeded, start);
    // The baseline is evidence, not a competing candidate; it must not consume
    // the transform-search sample budget twice.
    m_transform_search_result.sample_count = sampleBeforeBaseline;
  }
  const int steps = m_transform_search_config.enabled ?
      m_transform_search_config.coarse_steps_per_axis : 1;
  const int max_candidates = m_transform_search_config.max_candidates;
  double best_continuity = 0.0;
  double best_gradient = 0.0;
  double best_residual = 0.0;
  int best_hits = 0;
  int best_probes = 0;
  for (int ix = 0; ix < steps && !budget_exceeded; ++ix) {
    for (int iy = 0; iy < steps && !budget_exceeded; ++iy) {
      for (int ia = 0; ia < steps && !budget_exceeded; ++ia) {
        if (m_transform_search_result.evaluated_candidates >= max_candidates)
          break;
        const double normalized_x = steps == 1 ? 0.0 : (2.0 * ix / (steps - 1) - 1.0);
        const double normalized_y = steps == 1 ? 0.0 : (2.0 * iy / (steps - 1) - 1.0);
        const double normalized_a = steps == 1 ? 0.0 : (2.0 * ia / (steps - 1) - 1.0);
        FastMatchTransform candidate = m_transform_search_initial;
        candidate.scale_x *= 1.0 + normalized_x *
            m_transform_search_config.scale_range_percent / 100.0;
        candidate.scale_y *= 1.0 + normalized_y *
            m_transform_search_config.scale_range_percent / 100.0;
        candidate.angle_deg += normalized_a * m_transform_search_config.angle_range_deg;
        candidate.shear += normalized_y *
            m_transform_search_config.shear_range_permille / 1000.0;
        candidate.projective_u += normalized_x *
            m_transform_search_config.projective_range_permille / 1000.0;
        candidate.projective_v += normalized_y *
            m_transform_search_config.projective_range_permille / 1000.0;
        int hits = 0, probes = 0;
        double continuity = 0.0, gradient = 0.0, residual = 0.0;
        const double score = evaluateTransformCandidate(image, pathA, pathB,
            candidate, true, hits, probes, continuity, gradient, residual,
            budget_exceeded, start);
        ++m_transform_search_result.evaluated_candidates;
        if (score > m_transform_search_result.best_score ||
            m_transform_search_result.evaluated_candidates == 1) {
          m_transform_search_result.best = candidate;
          m_transform_search_result.best_score = score;
          best_continuity = continuity;
          best_gradient = gradient;
          best_residual = residual;
          best_hits = hits;
          best_probes = probes;
        }
      }
    }
  }

  if (!budget_exceeded) {
    const FastMatchTransform coarse_best = m_transform_search_result.best;
    for (int ix = 0; ix < 3 && !budget_exceeded; ++ix) {
      for (int iy = 0; iy < 3 && !budget_exceeded; ++iy) {
        for (int ia = 0; ia < 3 && !budget_exceeded; ++ia) {
          if (m_transform_search_result.evaluated_candidates >= max_candidates * 2)
            break;
          FastMatchTransform candidate = coarse_best;
          candidate.scale_x *= 1.0 + (ix - 1) *
              m_transform_search_config.fine_range_percent / 100.0;
          candidate.scale_y *= 1.0 + (iy - 1) *
              m_transform_search_config.fine_range_percent / 100.0;
          candidate.angle_deg += (ia - 1) * m_transform_search_config.fine_angle_range_deg;
          candidate.shear += (iy - 1) *
              m_transform_search_config.shear_range_permille / 5000.0;
          candidate.projective_u += (ix - 1) *
              m_transform_search_config.projective_range_permille / 5000.0;
          candidate.projective_v += (iy - 1) *
              m_transform_search_config.projective_range_permille / 5000.0;
          int hits = 0, probes = 0;
          double continuity = 0.0, gradient = 0.0, residual = 0.0;
          const double score = evaluateTransformCandidate(image, pathA, pathB,
              candidate, false, hits, probes, continuity, gradient, residual,
              budget_exceeded, start);
          ++m_transform_search_result.evaluated_candidates;
          if (score > m_transform_search_result.best_score) {
            m_transform_search_result.best = candidate;
            m_transform_search_result.best_score = score;
            best_continuity = continuity;
            best_gradient = gradient;
            best_residual = residual;
            best_hits = hits;
            best_probes = probes;
          }
        }
      }
    }
  }
  m_transform_search_result.budget_exceeded = budget_exceeded;
  m_transform_search_result.elapsed_ms = static_cast<int>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start).count());
  m_transform_search_result.appearance_score = best_probes == 0 ? 0.0 :
      static_cast<double>(best_hits) / best_probes;
  m_transform_search_result.continuity_score = best_continuity;
  m_transform_search_result.gradient_score = best_gradient;
  m_transform_search_result.geometric_residual_px = best_residual;
  if (m_transform_search_result.calibration_applied) {
    const cv::Point2d physical_center = m_calibration_adapter.pixelToPhysical(
        m_transform_search_result.best.cx, m_transform_search_result.best.cy);
    m_transform_search_result.best_physical_cx = physical_center.x;
    m_transform_search_result.best_physical_cy = physical_center.y;
  }
  m_transform_search_result.converged = !budget_exceeded &&
      m_transform_search_result.appearance_score >= m_dminscore;
  m_transform_search_result.accepted_candidates =
      m_transform_search_result.converged ? 1 : 0;
  m_transform_search_result.failure_stage = budget_exceeded ? "algorithm_budget_exceeded" :
      (m_transform_search_result.converged ? "complete" : "score_below_threshold");
  if (m_transform_search_result.converged) {
    const int result_score = std::max(1, static_cast<int>(
        m_transform_search_result.appearance_score * 1000.0));
    m_resultpoints.push_back(gp_Pnt(m_transform_search_result.best.cx,
                                    m_transform_search_result.best.cy, 0));
    m_resultnums.push_back(result_score);
    m_iminfindnum = result_score;
    RefreshPoseCandidates();
  }
}

void FastMatch::transformmatch(void* pimage) {
  Image* image = static_cast<Image*>(pimage);
  if (image == nullptr) {
    m_transform_search_result = FastMatchTransformSearchResult();
    m_transform_search_result.failure_stage = "image_missing";
    return;
  }
  runTransformSearch(*image);
  if (m_transform_search_result.converged)
    runFormFitOnImage(*image, m_transform_search_result.best,
                      "fastmatch_transform_observation");
}
void FastMatch::MatchABMore(Image &image) {
  m_matchimage = &image;
  resultclear();
  gp_Path &pathA = FindLine::getpatternpathA();
  gp_Path &pathB = FindLine::getpatternpathB();

  MatchSampleABMore(image, pathA, pathB);
}
void FastMatch::match(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr) {
    m_match_last_stage = 10;
    return;
  }

  ++m_match_call_count;
  m_match_last_stage = 11;
  MatchAB(*pgetimage);
}

void FastMatch::matchmore(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;

  MatchABMore(*pgetimage);
}
void FastMatch::loadfastimagemodel(const char *pfilename) {
  m_pgrid->loadmapmodel(pfilename);
  m_imagefastmodel = m_pgrid->getfastmodel();
}
vector<int> *FastMatch::getcurimagemodel() { return &m_imagefastmodel; }
void FastMatch::imagemodesclear_l12() { m_imagefastmodels_l12.clear(); }
void FastMatch::addimagemodels_l12(const char *pfilename) {
  m_pgrid->loadmapmodel(pfilename);

  {
    m_pgrid->ZeroModel();
    m_pgrid->ReGrid(12, 12);
  }
  m_pgrid->Grid2PattenModel(FindLine::getconparegap());
  m_models_l12.push_back(m_pgrid->getpatmodel());

  easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();
  m_easyobjectmodels_l12.push_back(aeobj);

  m_pgrid->ModelGridMethod_Gauss();

  m_imagefastmodel = m_pgrid->getfastmodel();
  m_imagefastmodels_l12.push_back(m_imagefastmodel);
}
void FastMatch::imagemodesclear_l36() { m_imagefastmodels_l36.clear(); }
void FastMatch::addimagemodels_l36(const char *pfilename) {
  m_pgrid->loadmapmodel(pfilename);
  m_pgrid->ZeroModel();
  m_pgrid->ReGrid(36, 36);
  if (0) {
    m_pgrid->SetUnit(36, 36);
    m_pgrid->UnitGrid();
  }

  m_pgrid->Grid2PattenModel(FindLine::getconparegap());
  m_models_l36.push_back(m_pgrid->getpatmodel());

  easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();
  m_easyobjectmodels_l36.push_back(aeobj);

  m_pgrid->ModelGridMethod_Gauss();

  m_imagefastmodel = m_pgrid->getfastmodel();
  m_imagefastmodels_l36.push_back(m_imagefastmodel);
}
void FastMatch::imagemodesclear_l72() { m_imagefastmodels_l72.clear(); }
void FastMatch::addimagemodels_l72(const char *pfilename) {
  m_pgrid->loadmapmodel(pfilename);
  m_pgrid->ZeroModel();
  m_pgrid->ReGrid(72, 72);
  if (0) {
    m_pgrid->SetUnit(72, 72);
    m_pgrid->UnitGrid();
  }

  m_pgrid->Grid2PattenModel_org(FindLine::getconparegap());
  m_models_l72.push_back(m_pgrid->getpatmodel());

  easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();
  m_easyobjectmodels_l72.push_back(aeobj);

  m_pgrid->ModelGridMethod_Gauss();

  m_imagefastmodel = m_pgrid->getfastmodel();
  m_imagefastmodels_l72.push_back(m_imagefastmodel);
}

Grid *FastMatch::getgrid() { return m_pgrid; }
bool FastMatch::modelcompare(vector<int> &modela, vector<int> &modelb) {
  const int isize0 = static_cast<int>(modela.size());
  const int isize1 = static_cast<int>(modelb.size());
  if (isize0 != isize1)
    return false;
  for (int i = 0; i < isize0; i++) {
    if (modela[i] != modelb[i])
      return false;
  }
  return true;
}
map<int, int> &FastMatch::getlevel3_6map() { return m_mapl3_l6; }
map<int, int> &FastMatch::getlevel6_12map() { return m_mapl6_l12; }
map<int, int> &FastMatch::getlevel12_36map() { return m_mapl12_l36; }
map<int, int> &FastMatch::getlevel36_72map() { return m_mapl36_l72; }
void FastMatch::clearmodel() {
  m_easyobjectmodels_l12.clear();
  m_models_l12.clear();
  m_imagefastmodels_l12.clear();
  m_mapl12_l36.clear();

  m_easyobjectmodels_l6.clear();
  m_models_l6.clear();
  m_imagefastmodels_l6.clear();
  m_mapl6_l12.clear();

  m_easyobjectmodels_l3.clear();
  m_models_l3.clear();
  m_imagefastmodels_l3.clear();
  m_mapl3_l6.clear();
}
void FastMatch::list_duplicatesmodel_l12() {
  m_duplicates_list_l12.clear();
  const int isize = static_cast<int>(m_imagefastmodels_l12.size());

  for (int i = 0; i < isize; i++) {
    m_duplicates_list_l12.push_back(0);
  }
  int iduplicatesnum = 1;
  for (int i = 0; i < isize; i++) {
    if (0 == m_duplicates_list_l12[i]) {
      for (int j = 0; j < isize; j++) {
        if (i != j) {
          if (modelcompare(m_imagefastmodels_l12[i],
                           m_imagefastmodels_l12[j])) {
            int isetvalue = 0;
            if (m_duplicates_list_l12[i] == 0 &&
                m_duplicates_list_l12[j] == 0) {
              m_duplicates_list_l12[i] = iduplicatesnum;
              m_duplicates_list_l12[j] = iduplicatesnum;
              iduplicatesnum = iduplicatesnum + 1;
            } else if (m_duplicates_list_l12[i] != 0) {
              isetvalue = m_duplicates_list_l12[i];
              m_duplicates_list_l12[j] = isetvalue;
            } else if (m_duplicates_list_l12[j] != 0) {
              isetvalue = m_duplicates_list_l12[j];
              m_duplicates_list_l12[i] = isetvalue;
            }
          }
        }
      }
    }
  }
}
void FastMatch::list_duplicatesmodel_l36() {
  m_duplicates_list_l36.clear();
  const int isize = static_cast<int>(m_imagefastmodels_l36.size());

  for (int i = 0; i < isize; i++) {
    m_duplicates_list_l36.push_back(0);
  }
  int iduplicatesnum = 1;
  for (int i = 0; i < isize; i++) {
    if (0 == m_duplicates_list_l36[i]) {
      for (int j = 0; j < isize; j++) {
        if (i != j) {
          if (modelcompare(m_imagefastmodels_l36[i],
                           m_imagefastmodels_l36[j])) {
            int isetvalue = 0;
            if (m_duplicates_list_l36[i] == 0 &&
                m_duplicates_list_l36[j] == 0) {
              m_duplicates_list_l36[i] = iduplicatesnum;
              m_duplicates_list_l36[j] = iduplicatesnum;
              iduplicatesnum = iduplicatesnum + 1;
            } else if (m_duplicates_list_l36[i] != 0) {
              isetvalue = m_duplicates_list_l36[i];
              m_duplicates_list_l36[j] = isetvalue;
            } else if (m_duplicates_list_l36[j] != 0) {
              isetvalue = m_duplicates_list_l36[j];
              m_duplicates_list_l36[i] = isetvalue;
            }
          }
        }
      }
    }
  }
}
void FastMatch::list_duplicatesmodel_l72() {
  m_duplicates_list_l72.clear();
  const int isize = static_cast<int>(m_imagefastmodels_l72.size());

  for (int i = 0; i < isize; i++) {
    m_duplicates_list_l72.push_back(0);
  }
  int iduplicatesnum = 1;
  for (int i = 0; i < isize; i++) {
    if (0 == m_duplicates_list_l72[i]) {
      for (int j = 0; j < isize; j++) {
        if (i != j) {
          if (modelcompare(m_imagefastmodels_l72[i],
                           m_imagefastmodels_l72[j])) {
            int isetvalue = 0;
            if (m_duplicates_list_l72[i] == 0 &&
                m_duplicates_list_l72[j] == 0) {
              m_duplicates_list_l72[i] = iduplicatesnum;
              m_duplicates_list_l72[j] = iduplicatesnum;
              iduplicatesnum = iduplicatesnum + 1;
            } else if (m_duplicates_list_l72[i] != 0) {
              isetvalue = m_duplicates_list_l72[i];
              m_duplicates_list_l72[j] = isetvalue;
            } else if (m_duplicates_list_l72[j] != 0) {
              isetvalue = m_duplicates_list_l72[j];
              m_duplicates_list_l72[i] = isetvalue;
            }
          }
        }
      }
    }
  }
}

vector<int> &FastMatch::getduplicateslist_l36() {
  return m_duplicates_list_l36;
}
vector<int> &FastMatch::getduplicateslist_l12() {
  return m_duplicates_list_l12;
}

void FastMatch::modelmethod(int itype) {
  m_pgrid->SetFastModel(m_imagefastmodel);
  m_pgrid->ReSetModelGrid();
  switch (itype) {
  case 0:
    m_pgrid->ReSetModelGrid();
    break;
  case 1:
    m_pgrid->ModelGridMethod_Gauss();
    break;
  case 2:
    m_pgrid->ModelGridMethod_Object();
    break;
  case 3:
    m_pgrid->ModelGridMethod_ObjectA();
    break;
  case 4:
    m_pgrid->ModelGridMethod_Inside();
    break;
  case 5:
    m_pgrid->ModelGridMethod_Outside();
    break;
  default:
    m_pgrid->ReSetModelGrid();
    break;
  }
  m_imagefastmodel = m_pgrid->getfastmodel();
}
void FastMatch::levelmodels_l72tol36() {

  const int isize = static_cast<int>(m_imagefastmodels_l72.size());

  for (int i = 0; i < isize; i++) {
    m_pgrid->SetModelWH(72, 72);
    m_pgrid->SetFastModel(m_imagefastmodels_l72[i]);
    m_pgrid->ReSetModelGrid();
    m_pgrid->ZeroModel();
    m_pgrid->GridZoom(36, 36);
    m_pgrid->SetUnit(36, 36);
    m_pgrid->Grid2PattenModel(FindLine::getconparegap());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();

    const int imsize = static_cast<int>(m_imagefastmodels_l36.size());
    int isame = -1;
    for (int im = 0; im < imsize; im++) {
      if (modelcompare(m_imagefastmodels_l36[im], m_imagefastmodel)) {
        m_mapl36_l72[i] = im;
        isame = 1;
        break;
      }
    }
    if (-1 == isame) {
      m_imagefastmodels_l36.push_back(m_imagefastmodel);
      m_models_l36.push_back(m_pgrid->getpatmodel());
      m_easyobjectmodels_l36.push_back(aeobj);
      m_mapl36_l72[i] = static_cast<int>(m_imagefastmodels_l36.size()) - 1;
    }
  }
}

void FastMatch::levelmodels_l36tol12() {

  const int isize = static_cast<int>(m_imagefastmodels_l36.size());

  for (int i = 0; i < isize; i++) {
    m_pgrid->SetModelWH(36, 36);
    m_pgrid->SetFastModel(m_imagefastmodels_l36[i]);
    m_pgrid->ReSetModelGrid();
    m_pgrid->ZeroModel();
    m_pgrid->GridZoom(12, 12);
    m_pgrid->SetUnit(12, 12);
    m_pgrid->Grid2PattenModel(FindLine::getconparegap());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();

    const int imsize = static_cast<int>(m_imagefastmodels_l12.size());
    int isame = -1;
    for (int im = 0; im < imsize; im++) {
      if (modelcompare(m_imagefastmodels_l12[im], m_imagefastmodel)) {
        m_mapl12_l36[i] = im;
        isame = 1;
        break;
      }
    }
    if (-1 == isame) {
      m_imagefastmodels_l12.push_back(m_imagefastmodel);
      m_models_l12.push_back(m_pgrid->getpatmodel());
      m_easyobjectmodels_l12.push_back(aeobj);
      m_mapl12_l36[i] = static_cast<int>(m_imagefastmodels_l12.size()) - 1;
    }
  }
}

void FastMatch::levelmodels_l12tol6() {

  const int isize = static_cast<int>(m_imagefastmodels_l12.size());

  for (int i = 0; i < isize; i++) {
    m_pgrid->SetModelWH(12, 12);
    m_pgrid->SetFastModel(m_imagefastmodels_l12[i]);

    m_pgrid->ReSetModelGrid();
    m_pgrid->ZeroModel();
    m_pgrid->GridZoom(6, 6);
    m_pgrid->SetUnit(6, 6);
    m_pgrid->Grid2PattenModel(FindLine::getconparegap());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();

    const int imsize = static_cast<int>(m_imagefastmodels_l6.size());
    int isame = -1;
    for (int im = 0; im < imsize; im++) {
      if (modelcompare(m_imagefastmodels_l6[im], m_imagefastmodel)) {
        m_mapl6_l12[i] = im;
        isame = 1;
        break;
      }
    }
    if (-1 == isame) {
      m_imagefastmodels_l6.push_back(m_imagefastmodel);
      m_models_l6.push_back(m_pgrid->getpatmodel());
      m_easyobjectmodels_l6.push_back(aeobj);
      m_mapl6_l12[i] = static_cast<int>(m_imagefastmodels_l6.size()) - 1;
    }
  }
}

void FastMatch::levelmodels_l6tol3() {
  const int isize = static_cast<int>(m_imagefastmodels_l6.size());
  for (int i = 0; i < isize; i++) {
    m_pgrid->SetModelWH(6, 6);
    m_pgrid->SetFastModel(m_imagefastmodels_l6[i]);

    m_pgrid->ReSetModelGrid();
    m_pgrid->ZeroModel();
    m_pgrid->GridZoom(3, 3);
    m_pgrid->SetUnit(3, 3);
    m_pgrid->Grid2PattenModel(FindLine::getconparegap());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();

    const int imsize = static_cast<int>(m_imagefastmodels_l3.size());
    int isame = -1;
    for (int im = 0; im < imsize; im++) {
      if (modelcompare(m_imagefastmodels_l3[im], m_imagefastmodel)) {
        m_mapl3_l6[i] = im;
        isame = 1;
        break;
      }
    }
    if (-1 == isame) {
      m_models_l3.push_back(m_pgrid->getpatmodel());
      m_imagefastmodels_l3.push_back(m_imagefastmodel);
      m_easyobjectmodels_l3.push_back(aeobj);
      m_mapl3_l6[i] = static_cast<int>(m_imagefastmodels_l3.size()) - 1;
    }
  }
}
void FastMatch::savelevel0_l1() {
  const int isize0 = static_cast<int>(m_models_l3.size());
  for (int i = 0; i < isize0; i++) {
    std::ostringstream stream;
    stream << "./model/3x3/_" << i << ".pat";
    string strfilename = stream.str();
    m_models_l3[i].save(strfilename.c_str());
  }
  const int isize1 = static_cast<int>(m_models_l6.size());
  for (int i = 0; i < isize1; i++) {
    std::ostringstream stream;
    stream << "./model/6x6/_" << i << ".pat";
    string strfilename = stream.str();
    m_models_l6[i].save(strfilename.c_str());
  }

  int isize = static_cast<int>(m_imagefastmodels_l3.size());
  for (int i = 0; i < isize; i++) {
    m_pgrid->SetModelWH(3, 3);
    m_pgrid->SetFastModel(m_imagefastmodels_l3[i]);
    m_pgrid->ReSetModelGrid();
    std::ostringstream stream;
    stream << "./model/3x3/_" << i << ".imp";
    string strfilename = stream.str();
    m_pgrid->savemapmodel(strfilename.c_str());
  }

  isize = static_cast<int>(m_imagefastmodels_l6.size());
  for (int i = 0; i < isize; i++) {
    m_pgrid->SetModelWH(6, 6);
    m_pgrid->SetFastModel(m_imagefastmodels_l6[i]);
    m_pgrid->ReSetModelGrid();
    std::ostringstream stream;
    stream << "./model/6x6/_" << i << ".imp";
    string strfilename = stream.str();
    m_pgrid->savemapmodel(strfilename.c_str());
  }

  isize = static_cast<int>(m_imagefastmodels_l12.size());
  for (int i = 0; i < isize; i++) {
    m_pgrid->SetModelWH(12, 12);
    m_pgrid->SetFastModel(m_imagefastmodels_l12[i]);
    m_pgrid->ReSetModelGrid();
    m_pgrid->Grid2PattenModel(2);
    std::ostringstream stream;
    stream << "./model/12x12/_" << i << ".pat";
    string strfilename = stream.str();
    m_pgrid->savemodelfile(strfilename.c_str());

    std::ostringstream stream0;
    stream0 << "./model/12x12/_" << i << ".imp";
    strfilename = stream0.str();
    m_pgrid->savemapmodel(strfilename.c_str());
  }
}
void FastMatch::imagemodelstocurrent_l72(int i) {
  m_pgrid->SetModelWH(72, 72);

  if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l72.size()))
    m_imagefastmodel = m_imagefastmodels_l72[i];
}
void FastMatch::imagemodelstocurrent_l36(int i) {
  m_pgrid->SetModelWH(36, 36);

  if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l36.size()))
    m_imagefastmodel = m_imagefastmodels_l36[i];
}
void FastMatch::imagemodelstocurrent_l12(int i) {
  m_pgrid->SetModelWH(12, 12);

  if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l12.size()))
    m_imagefastmodel = m_imagefastmodels_l12[i];
}

void FastMatch::imagemodelstocurrent_l3(int i) {
  m_pgrid->SetModelWH(3, 3);
  if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l3.size()))
    m_imagefastmodel = m_imagefastmodels_l3[i];
}
void FastMatch::imagemodelstocurrent_l6(int i) {
  m_pgrid->SetModelWH(6, 6);

  if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l6.size()))
    m_imagefastmodel = m_imagefastmodels_l6[i];
}

int FastMatch::imagefastmodelsize(int ilevel) {
  switch (ilevel) {
  case 0:
    return static_cast<int>(m_imagefastmodels_l3.size());
  case 1:
    return static_cast<int>(m_imagefastmodels_l6.size());
  case 2:
    return static_cast<int>(m_imagefastmodels_l12.size());
  case 3:
    return static_cast<int>(m_imagefastmodels_l36.size());
  case 4:
    return static_cast<int>(m_imagefastmodels_l72.size());

  default:
    return static_cast<int>(m_imagefastmodels_l12.size());
  }
  return 0;
}

void FastMatch::objectmodelstocurrent(int i) {
  if (i >= 0 && i < static_cast<int>(m_easyobjectmodels_l12.size()))
    m_easyobject = m_easyobjectmodels_l12[i];
}
void FastMatch::savefastimagemodel(const char *pfilename) {
  m_pgrid->ReSetModelGrid();

  m_pgrid->savemapmodel(pfilename);
}
void FastMatch::savefastimagepatmodel(const char *pfilename) {
  m_pgrid->Grid2PattenModel(FindLine::getconparegap());
  m_pgrid->savemodelfile(pfilename);
}
void FastMatch::savematchroi(const char *pfilename) {
  if (m_matchimage == nullptr)
    return;
  SaveMatchROI(*m_matchimage, pfilename);
}
void FastMatch::savematchimagemodel(const char *pfilename) {
  if (g_pmodelimage == nullptr)
    return;
  g_pmodelimage->SaveROI(pfilename);
}
void FastMatch::SaveMatchROI(Image &image, const char *pfilename) {
  const int isize = m_resultrects.size();
  if (isize <= 0)
    return;
  gp_Rectangle arect = m_resultrects.getrect(isize - 1);
  const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
  const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));

  image.setroi(static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
               static_cast<int>(arect.TopLeft().Y()), roiw, roih);
  image.SaveROI(pfilename);
}
void FastMatch::imagelearn(int ithre1, int iandor) {
  if (m_matchimage == nullptr)
    return;
  MatchImageLearn(*m_matchimage, ithre1, iandor);
}
void FastMatch::imagelearnmass(int ithre1, int iandor, int igridwh) {
  if (m_matchimage == nullptr)
    return;
  MatchImageLearnMass(*m_matchimage, ithre1, iandor, igridwh);
}
void FastMatch::imagelearncheck(int iimagetype, int iandor, int igridwh) {
  if (m_matchimage == nullptr)
    return;
  MatchImageCheck(*m_matchimage, iimagetype, iandor, igridwh);
}

void FastMatch::imagelearnex(int ithre1, int iandor, int igrid) {
  if (m_matchimage == nullptr)
    return;
  MatchImageLearnEx(*m_matchimage, ithre1, iandor, igrid);
}
void FastMatch::MatchImageLearn(Image &aimage, int ithre1, int iandor) {
  if (g_pmodelimage == nullptr)
    return;
  (void)iandor;
  const int isize = m_resultrects.size();
  if (isize <= 0)
    return;

  gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
  m_pgrid->SetUnit(12, 12);
  gp_Rectangle arect = m_pgrid->CentRect_Condition(arect0, 3);
  const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
  const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));

  aimage.setroi(static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
                static_cast<int>(arect.TopLeft().Y()), roiw, roih);
  g_pmodelimage->setroi(0, 0, roiw, roih);
  aimage.SetMode(3);
  aimage.ROItoROI(*g_pmodelimage);
  g_pmodelimage->ROIEasyThre(ithre1);
  m_pgrid->ROIImagetoModel(*g_pmodelimage);
  m_pgrid->SetUnit(12, 12);
  m_pgrid->UnitGrid();

  m_pgrid->ModelGridMethod_Gauss();
  m_imagefastmodel = m_pgrid->getfastmodel();
}
int FastMatch::GetRectGridLevel(int irectw) {
  int igrid = 12;
  if (irectw <= 12) {
    igrid = 12;
  } else if (irectw > 12 && irectw <= 36) {
    igrid = 36;
  } else if (irectw > 36 && irectw <= 72) {
    igrid = 72;
  } else if (irectw > 72 && irectw <= 144) {
    igrid = 144;
  }

  return igrid;
}
void FastMatch::MatchImageLearnEx(Image &aimage, int ithre1, int iandor,
                                  int igrid) {
  if (g_pmodelimage == nullptr)
    return;
  (void)iandor;
  const int isize = m_resultrects.size();
  if (isize <= 0)
    return;

  gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);

  m_pgrid->SetUnit(igrid, igrid);
  gp_Rectangle arect = m_pgrid->CentRect_Condition(arect0, 3);
  const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
  const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));
  aimage.setroi(static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
                static_cast<int>(arect.TopLeft().Y()), roiw, roih);
  g_pmodelimage->setroi(0, 0, roiw, roih);
  aimage.SetMode(3);
  aimage.ROItoROI(*g_pmodelimage);
  g_pmodelimage->ROIEasyThre(ithre1);
  m_pgrid->ROIImagetoModel(*g_pmodelimage);
  m_pgrid->SetUnit(igrid, igrid);
  m_pgrid->UnitGrid();

  m_pgrid->ModelGridMethod_Gauss();
  m_imagefastmodel = m_pgrid->getfastmodel();
}
void FastMatch::MatchImageLearnMass(Image &aimage, int ithre1, int iandor,
                                    int igrid) {
  if (g_pmodelimage == nullptr)
    return;
  (void)iandor;
  const int isize = m_resultrects.size();
  if (isize <= 0)
    return;
  m_pgrid->setgrid(10, 10, igrid, igrid, 10, 10);
  m_pgrid->SetModelWH(igrid, igrid);

  gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);

  m_pgrid->SetUnit(igrid, igrid);
  gp_Rectangle arect = m_pgrid->CentRect_Condition(arect0, 3);
  const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
  const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));
  aimage.setroi(static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
                static_cast<int>(arect.TopLeft().Y()), roiw, roih);
  g_pmodelimage->setroi(0, 0, roiw, roih);
  aimage.SetMode(3);
  aimage.ROItoROI(*g_pmodelimage);
  g_pmodelimage->ROIEasyThre(ithre1);
  m_pgrid->ROIImagetoModel(*g_pmodelimage);
  m_pgrid->SetUnit(igrid, igrid);
  m_pgrid->UnitGrid();

  m_pgrid->ModelGridMethod_Gauss();
  m_imagefastmodel = m_pgrid->getfastmodel();
}
void FastMatch::MatchImageCheck(Image &aimage, int iimagetype, int iandor,
                                int igrid) {
  if (g_pmodelimage == nullptr)
    return;
  (void)iandor;
  int isize = m_resultrects.size();
  if (isize <= 0)
    return;

  m_pgrid->setgrid(10, 10, igrid, igrid, 10, 10);
  m_pgrid->SetModelWH(igrid, igrid);

  gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);

  m_pgrid->SetUnit(igrid, igrid);

  g_pmodelimage->setroi(0, 0, igrid, igrid);
  g_pmodelimage->colorizeROI(0, 0, 0);

  const int roiw = FastMatchPositiveInt(static_cast<int>(arect0.Width()));
  const int roih = FastMatchPositiveInt(static_cast<int>(arect0.Height()));
  aimage.setroi(static_cast<int>(arect0.TopLeft().X()) - m_imatchoffset,
                static_cast<int>(arect0.TopLeft().Y()), roiw, roih);
  g_pmodelimage->setroi(0, 0, roiw, roih);
  aimage.SetMode(iimagetype);
  aimage.ROItoROI(*g_pmodelimage);

  g_pmodelimage->setroi(0, 0, igrid, igrid);
  m_pgrid->ROIImagetoModel(*g_pmodelimage);

  m_imagefastmatchlist = m_pgrid->getfastmodel();

  isize = static_cast<int>(m_imagefastmodel.size());
  int ingsize = 0;
  int ioksize = 0;
  for (int i = 0; i < isize; i++) {
    int ia = m_imagefastmatchlist[i];
    int io = m_imagefastmodel[i];
    if (ia != io && io == 1) {
      if (ia > 0)
        ingsize = ingsize + ia;
      else
        ingsize = ingsize - ia;
    } else if (ia == 1 && io != 1) {
      if (io > 0)
        ingsize = ingsize + io;
      else
        ingsize = ingsize - io;
    } else if (io == ia && io == 1)
      ioksize++;
  }
  m_imagemodelresult_OK = ioksize;
  m_imagemodelresult_NG = ingsize;
}

void FastMatch::imagematch(int ithre1, int iandor, int igrid, int ineedthre) {
  if (m_matchimage == nullptr)
    return;
  MatchImageMatch(*m_matchimage, ithre1, iandor, igrid, ineedthre);
}

void FastMatch::imagematch_grid(int ithre1, int iandor, int igrid) {
  if (m_matchimage == nullptr)
    return;
  MatchImageMatch(*m_matchimage, ithre1, iandor, igrid);
}

void FastMatch::imagematchex(int igrid) {
  if (m_matchimage == nullptr)
    return;
  MatchImageExMatch(*m_matchimage, igrid);
}

void FastMatch::MatchImageMatch(Image &aimage, int ithre1, int iandor,
                                int igrid, int ineedthre) {
  if (g_pmodelimage == nullptr)
    return;
  (void)iandor;
  int isize = m_resultrects.size();
  if (isize <= 0)
    return;
  gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
  const int roiw = FastMatchPositiveInt(static_cast<int>(arect0.Width()));
  const int roih = FastMatchPositiveInt(static_cast<int>(arect0.Height()));
  aimage.setroi(static_cast<int>(arect0.TopLeft().X()) - m_imatchoffset,
                static_cast<int>(arect0.TopLeft().Y()), roiw, roih);
  g_pmodelimage->setroi(0, 0, roiw, roih);
  aimage.SetMode(3);
  aimage.ROItoROI(*g_pmodelimage);

  if (1 == ineedthre) {
    g_pmodelimage->ROIEasyThre(ithre1);
  }
  m_pgrid->ROIImagetoModel(*g_pmodelimage);
  m_pgrid->ZeroModel();
  m_pgrid->ReGrid(igrid, igrid);

  if (0) {
    m_pgrid->SetUnit(igrid, igrid);
    m_pgrid->UnitGrid();
  }
  m_easyobject = m_pgrid->ModelGridMethod_ObjectA();

  m_pgrid->ModelGridMethod_Gauss();
  m_imagefastmatchlist = m_pgrid->getfastmodel();

  isize = static_cast<int>(m_imagefastmodel.size());
  int ingsize = 0;
  int ioksize = 0;
  for (int i = 0; i < isize; i++) {
    int ia = m_imagefastmatchlist[i];
    int io = m_imagefastmodel[i];

    if (ia != io && io == 1) {
      if (ia > 0)
        ingsize = ingsize + ia;
      else
        ingsize = ingsize - ia;
    } else if (ia == 1 && io != 1) {
      if (io > 0)
        ingsize = ingsize + io;
      else
        ingsize = ingsize - io;
    } else if (io == ia && io == 1)
      ioksize++;
  }
  m_imagemodelresult_OK = ioksize;
  m_imagemodelresult_NG = ingsize;
}

void FastMatch::MatchImageExMatch(Image &aimage, int igrid) {
  if (g_pmodelimage == nullptr)
    return;
  int isize = m_resultrects.size();
  if (isize <= 0)
    return;
  gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
  m_pgrid->SetUnit(igrid, igrid);
  gp_Rectangle arect(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
  switch (igrid) {
  case 3:
    arect = gp_Rectangle(
        gp_Pnt(arect0.TopLeft().X() + 1, arect0.TopLeft().Y() + 1, 0), 3, 3);
    break;
  case 6:
    arect = gp_Rectangle(
        gp_Pnt(arect0.TopLeft().X() + 1, arect0.TopLeft().Y() + 1, 0), 6, 6);
    break;
  case 12:
    arect = m_pgrid->CentRect_Condition(arect0, 3);
    break;
  default:
    break;
  }
  const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
  const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));
  aimage.setroi(static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
                static_cast<int>(arect.TopLeft().Y()), roiw, roih);
  g_pmodelimage->setroi(0, 0, roiw, roih);
  aimage.SetMode(3);
  aimage.ROItoROI(*g_pmodelimage);

  m_pgrid->ROIImagetoModel(*g_pmodelimage);

  m_easyobject = m_pgrid->ModelGridMethod_ObjectA();

  m_pgrid->ModelGridMethod_Gauss();
  m_imagefastmatchlist = m_pgrid->getfastmodel();

  isize = std::min(static_cast<int>(m_imagefastmodel.size()),
                   static_cast<int>(m_imagefastmatchlist.size()));
  int ingsize = 0;
  int ioksize = 0;
  for (int i = 0; i < isize; i++) {
    int ia = m_imagefastmatchlist[i];
    int io = m_imagefastmodel[i];

    if (ia != io && io == 1) {
      if (ia > 0)
        ingsize = ingsize + ia;
      else
        ingsize = ingsize - ia;
    } else if (ia == 1 && io != 1) {
      if (io > 0)
        ingsize = ingsize + io;
      else
        ingsize = ingsize - io;
    } else if (io == ia && io == 1)
      ioksize++;
  }
  m_imagemodelresult_OK = ioksize;
  m_imagemodelresult_NG = ingsize;
}
void FastMatch::MatchGrid(Grid *pgrid) {
  m_pgrid->GridFastModel(pgrid);

  m_easyobject = m_pgrid->ModelGridMethod_ObjectA();

  m_pgrid->ModelGridMethod_Gauss();
  m_imagefastmatchlist = m_pgrid->getfastmodel();

  const int isize = std::min(static_cast<int>(m_imagefastmodel.size()),
                             static_cast<int>(m_imagefastmatchlist.size()));
  int ingsize = 0;
  int ioksize = 0;
  for (int i = 0; i < isize; i++) {
    int ia = m_imagefastmatchlist[i];
    int io = m_imagefastmodel[i];

    if (ia != io && io == 1) {
      if (ia > 0)
        ingsize = ingsize + ia;
      else
        ingsize = ingsize - ia;
    } else if (ia == 1 && io != 1) {
      if (io > 0)
        ingsize = ingsize + io;
      else
        ingsize = ingsize - io;
    } else if (io == ia && io == 1)
      ioksize++;
  }
  m_imagemodelresult_OK = ioksize;
  m_imagemodelresult_NG = ingsize;
}

double FastMatch::getimagemodelreslut() {
  if (0 == m_imagemodelresult_NG && 0 != m_imagemodelresult_OK)
    return 100;
  if (0 == m_imagemodelresult_OK && 0 != m_imagemodelresult_NG)
    return 0;
  if (0 == m_imagemodelresult_OK && 0 == m_imagemodelresult_NG)
    return 0;
  if (m_imagemodelresult_OK < 0 || m_imagemodelresult_NG <= 0)
    return 0;

  const int itotalsize = static_cast<int>(m_imagefastmodel.size());
  (void)itotalsize;
  double dresult =
      (m_imagemodelresult_OK * 1.0) / (1.0 * m_imagemodelresult_NG);
  return dresult;
}

double FastMatch::getimagemodelreslut_check_1() {
  if (0 == m_imagemodelresult_NG && 0 != m_imagemodelresult_OK)
    return 100;
  if (0 == m_imagemodelresult_OK && 0 != m_imagemodelresult_NG)
    return 0;
  if (0 == m_imagemodelresult_OK && 0 == m_imagemodelresult_NG)
    return 0;
  if (m_imagemodelresult_OK < 0 || m_imagemodelresult_NG <= 0)
    return 0;

  const int itotalsize = static_cast<int>(m_imagefastmodel.size());
  (void)itotalsize;
  double dresult =
      (m_imagemodelresult_OK * 1.0) / (1.0 * m_imagemodelresult_NG);
  return dresult;
}
void FastMatch::imagemodelcomparegrid(int itype) {
  const int imatchsize = static_cast<int>(m_imagefastmatchlist.size());
  if (imatchsize <= 0)
    return;
  const int isize = static_cast<int>(m_imagefastmodel.size());
  if (imatchsize != isize)
    return;

  switch (itype) {
  case 0:
    for (int i = 0; i < isize; i++) {
      int ia = m_imagefastmatchlist[i];
      int io = m_imagefastmodel[i];

      if (io == 1 && ia != io) {
        m_pgrid->setfastlistvalue(i, -1);
      } else if (ia == 1 && io != 1) {
        m_pgrid->setfastlistvalue(i, -2);
      } else if (io == 1 && ia != 1) {
        m_pgrid->setfastlistvalue(i, -3);
      } else if (io == 1 && io == ia)
        m_pgrid->setfastlistvalue(i, 1);
      else
        m_pgrid->setfastlistvalue(i, 0);
    }

    break;
  case 1:
    for (int i = 0; i < isize; i++) {
      int ia = m_imagefastmatchlist[i];
      int io = m_imagefastmodel[i];

      if (io == 0 && ia != io)
        m_pgrid->setfastlistvalue(i, -1);
      else if (ia == 1 && io != 0)
        m_pgrid->setfastlistvalue(i, 0);
      else if (ia == 1 && io == 0)
        m_pgrid->setfastlistvalue(i, -1);
      else if (ia == 0 && io != 0) {
        if (io == -1)
          m_pgrid->setfastlistvalue(i, 0);
        else
          m_pgrid->setfastlistvalue(i, io);
      } else
        m_pgrid->setfastlistvalue(i, 0);
    }

    break;
  case 2:

    break;

  default:

    break;
  }
}
void FastMatch::imagemodelcompareshow(int itype) {
  const int imatchsize = static_cast<int>(m_imagefastmatchlist.size());
  if (imatchsize <= 0)
    return;
  const int isize = static_cast<int>(m_imagefastmodel.size());
  if (imatchsize != isize)
    return;
  switch (itype) {
  case 0:
    for (int i = 0; i < isize; i++) {
      int ia = m_imagefastmatchlist[i];
      int io = m_imagefastmodel[i];

      if (io == 1 && ia != io) {
        m_pgrid->setfastlistvalue(i, -1);
      } else if (ia == 1 && io != 1) {
        m_pgrid->setfastlistvalue(i, -2);
      } else if (io == 1 && ia != 1) {
        m_pgrid->setfastlistvalue(i, -3);
      } else if (io == 1 && io == ia)
        m_pgrid->setfastlistvalue(i, 1);
      else
        m_pgrid->setfastlistvalue(i, 0);
    }

    break;
  case 1:
    for (int i = 0; i < isize; i++) {
      int ia = m_imagefastmatchlist[i];
      int io = m_imagefastmodel[i];

      if (io == 0 && ia != io)
        m_pgrid->setfastlistvalue(i, -1);
      else if (ia == 1 && io != 0)
        m_pgrid->setfastlistvalue(i, 0);
      else if (ia == 1 && io == 0)
        m_pgrid->setfastlistvalue(i, -1);
      else if (ia == 0 && io != 0) {
        if (io == -1)
          m_pgrid->setfastlistvalue(i, 0);
        else
          m_pgrid->setfastlistvalue(i, io);
      } else
        m_pgrid->setfastlistvalue(i, 0);
    }

    break;
  case 2:

    break;

  default:

    break;
  }
}
double FastMatch::imagegridresult(int itype) {
  switch (itype) {
  case 0:

    break;
  case 1:

    break;

  default:

    break;
  }
  return 0;
}
void FastMatch::imagemodelshow() {
  m_pgrid->SetFastModel(m_imagefastmodel);
  const int isize = static_cast<int>(m_imagefastmodel.size());
  if (isize <= 0)
    return;
  for (int i = 0; i < isize; i++) {
    int io = m_imagefastmodel[i];
    m_pgrid->setfastlistvalue(i, io);
  }
}
void FastMatch::matchstepgap(int ix, int iy) {
  m_stepgapx = FastMatchPositiveInt(ix);
  m_stepgapy = FastMatchPositiveInt(iy);
}

void FastMatch::imagematchshow() {
  m_pgrid->SetFastModel(m_imagefastmodel);
  const int imatchsize = static_cast<int>(m_imagefastmatchlist.size());
  if (imatchsize <= 0)
    return;
  const int isize = static_cast<int>(m_imagefastmodel.size());
  if (isize != imatchsize)
    return;
  for (int i = 0; i < isize; i++) {
    int io = m_imagefastmatchlist[i];
    m_pgrid->setfastlistvalue(i, io);
  }
}
void FastMatch::setminscore(double dminscore) {
  m_dminscore = FastMatchUnitScore(dminscore);
}
void FastMatch::MatchSample(Image &image, gp_Path &path) {
  int ithre = m_imatchthre;
  int icurmodule = ImageManager::GetCurMode();
  Image *pimage = ImageManager::GetTransferImage(icurmodule);
  (void)pimage;
  int ix0 = static_cast<int>(m_matchrect.TopLeft().X());
  int iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
  int ix1 = static_cast<int>(m_matchrect.TopLeft().X() + m_matchrect.Width());
  int iy1 = static_cast<int>(m_matchrect.TopLeft().Y() + m_matchrect.Height());

  if (image.getWidth() <= ix1 || image.getHeight() <= iy1)
    return;
  m_iminfindnum = -1;
  const int icount = static_cast<int>(path.ElementCount());
  cv::Vec3b pixel0, pixel1;
  int icalnum = 0;
  int icalng = 0;

  gp_Pnt aele;

  int igapx = m_stepgapx;
  int igapy = m_stepgapy;
  gp_Rectangle arect1 = path.boundingRect();
  iy1 = iy1 - static_cast<int>(arect1.Height());
  ix1 = ix1 - static_cast<int>(arect1.Width());
  int ix = 0;
  int iy = 0;
  int iw = static_cast<int>(FindLine::patternboundingrect().Width());
  int ih = static_cast<int>(FindLine::patternboundingrect().Height());
  int itotalsize = static_cast<int>(FindLine::getpattern().size());

  m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
  const int iminfindngnum =
      static_cast<int>((1 - m_dminscore) * itotalsize / 2);
  const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);

  for (iy = iy0; iy < iy1;) {
    for (ix = ix0; ix < ix1;) {
      int imovx = ix;
      int imovy = iy;

      icalnum = 0;
      icalng = 0;
      for (int i = 0; i < icount - 1; i++) {
        aele = path.ElementAt(i);
        pixel0 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));
        i++;
        aele = path.ElementAt(i);
        pixel1 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));

        if (0 == m_iB2W) {
          int ir = Red(pixel0) - Red(pixel1);
          int ig = Green(pixel0) - Green(pixel1);
          int ib = Blue(pixel0) - Blue(pixel1);
          if (ir > ithre || ig > ithre || ib > ithre) {
            icalnum++;
          } else {
            icalng++;
            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        } else if (1 == m_iB2W) {
          int ir = Red(pixel1) - Red(pixel0);
          int ig = Green(pixel1) - Green(pixel0);
          int ib = Blue(pixel1) - Blue(pixel0);
          if (ir > ithre || ig > ithre || ib > ithre) {
            icalnum++;
          } else {
            icalng++;

            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        }
      }

    NextStep_1:
      ix += igapx;
      if (icalnum > m_iminfindnum && icalnum > iminfindoknum) {
        gp_Pnt apoint(imovx, imovy, 0);
        if (m_rawthresholdhitpoints.size() < 32) {
          m_rawthresholdhitpoints.push_back(apoint);
          m_rawthresholdhitscores.push_back(icalnum);
        }
        resulttolist(apoint, icalnum);
      }
    }
    iy += igapy;
  }

  resultsort();
  m_resultrects.clear();
  const int icountresult = static_cast<int>(m_resultnums.size());
  for (int i = 0; i < icountresult; i++) {
    int ivalue = m_resultnums.at(i);
    gp_Pnt apoint = m_resultpoints.at(i);
    double dpercent = (2.0 * ivalue) / (1.0 * itotalsize);
    gp_Rectangle arect(
        gp_Pnt(apoint.X(), apoint.Y(), 0),
        gp_Pnt(static_cast<double>(iw), static_cast<double>(ih), 0));
    std::ostringstream stream;
    stream << dpercent;
    std::string astr = stream.str();
    m_resultrects.addrect(arect, astr);
  }
}

void FastMatch::MatchSampleAB(Image &image, gp_Path &pathA, gp_Path &pathB) {
  ++m_matchsampleab_call_count;
  m_match_last_stage = 30;
  int ithre = m_imatchthre;
  int icurmodule = ImageManager::GetCurMode();
  Image *pimage = ImageManager::GetTransferImage(icurmodule);
  (void)pimage;
  int ix0 = static_cast<int>(m_matchrect.TopLeft().X());
  int iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
  int ix1 = static_cast<int>(m_matchrect.BottomRight().X());
  int iy1 = static_cast<int>(m_matchrect.BottomRight().Y());
  m_match_debug_image_width = image.getWidth();
  m_match_debug_image_height = image.getHeight();
  m_match_debug_rect_x0 = ix0;
  m_match_debug_rect_y0 = iy0;
  m_match_debug_rect_x1 = ix1;
  m_match_debug_rect_y1 = iy1;
  const int icount1 = static_cast<int>(pathA.ElementCount());
  const int icount2 = static_cast<int>(pathB.ElementCount());
  const int pair_count = std::min(icount1, icount2);
  if (pair_count <= 0) {
    m_match_last_stage = 32;
    return;
  }

  gp_Rectangle arect1 = pathA.boundingRect();
  gp_Rectangle arect2 = pathB.boundingRect();
  (void)arect2;
  const int pattern_w = static_cast<int>(std::ceil(arect1.Width()));
  const int pattern_h = static_cast<int>(std::ceil(arect1.Height()));
  if (pattern_w <= 0 || pattern_h <= 0) {
    m_match_last_stage = 33;
    return;
  }

  if (ix0 < 0)
    ix0 = 0;
  if (iy0 < 0)
    iy0 = 0;
  if (ix1 > image.getWidth())
    ix1 = image.getWidth();
  if (iy1 > image.getHeight())
    iy1 = image.getHeight();
  if (image.getWidth() < ix1 || image.getHeight() < iy1) {
    m_match_last_stage = 31;
    return;
  }
  if (ix1 - ix0 <= pattern_w || iy1 - iy0 <= pattern_h) {
    m_match_last_stage = 34;
    return;
  }

  CXLOG_INFO("FastMatch", "match_sample_enter", "running",
             "search=" + std::to_string(ix0) + "," + std::to_string(iy0) + "," +
                 std::to_string(ix1) + "," + std::to_string(iy1) + " pattern=" +
                 std::to_string(pattern_w) + "x" + std::to_string(pattern_h) +
                 " pairs=" + std::to_string(pair_count));

  m_iminfindnum = -1;

  cv::Vec3b pixel0, pixel1;
  int icalnum = 0;
  int icalng = 0;

  gp_Pnt aele;

  int igapx = FastMatchPositiveInt(m_stepgapx);
  int igapy = FastMatchPositiveInt(m_stepgapy);
  iy1 = iy1 - pattern_h;
  ix1 = ix1 - pattern_w;
  if (ix1 <= ix0 || iy1 <= iy0) {
    m_match_last_stage = 35;
    return;
  }
  int ix = 0;
  int iy = 0;
  int iw = pattern_w;
  int ih = pattern_h;
  int itotalsize = pair_count;

  m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
  const int iminfindngnum =
      static_cast<int>((1 - m_dminscore) * itotalsize / 2);
  const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);

  for (iy = iy0; iy < iy1;) {
    for (ix = ix0; ix < ix1;) {
      ++m_rawmatch_probe_count;
      int imovx = ix;
      int imovy = iy;

      icalnum = 0;
      icalng = 0;
      if (!FastMatchCandidateMaskPass(m_matchmask, pathA, imovx, imovy)) {
        ix += igapx;
        continue;
      }
      if (!MatchSampleABAnchorPass(image, pathA, pathB, imovx, imovy, ithre,
                                   m_iB2W)) {
        ix += igapx;
        continue;
      }
      for (int i = 0; i < pair_count; i++) {
        aele = pathA.ElementAt(i);
        const int ax = static_cast<int>(aele.X() + imovx);
        const int ay = static_cast<int>(aele.Y() + imovy);
        aele = pathB.ElementAt(i);
        const int bx = static_cast<int>(aele.X() + imovx);
        const int by = static_cast<int>(aele.Y() + imovy);
        if (!FastMatchPointInsideImage(image, ax, ay) ||
            !FastMatchPointInsideImage(image, bx, by)) {
          icalng = iminfindngnum + 1;
          goto NextStep_1;
        }
        pixel0 = image.pixel(ax, ay);
        pixel1 = image.pixel(bx, by);

        if (0 == m_iB2W) {
          int ir = Red(pixel0) - Red(pixel1);
          int ig = Green(pixel0) - Green(pixel1);
          int ib = Blue(pixel0) - Blue(pixel1);
          if (ir > ithre || ig > ithre || ib > ithre) {
            icalnum++;
          } else {
            icalng++;
            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        } else if (1 == m_iB2W) {
          int ir = Red(pixel1) - Red(pixel0);
          int ig = Green(pixel1) - Green(pixel0);
          int ib = Blue(pixel1) - Blue(pixel0);
          if (ir > ithre || ig > ithre || ib > ithre) {
            icalnum++;
          } else {
            icalng++;

            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        }
      }

    NextStep_1:
      ix += igapx;
      if (icalnum > m_iminfindnum && icalnum > iminfindoknum) {
        ++m_rawmatch_threshold_hit_count;
        gp_Pnt refined_point(imovx, imovy, 0);
        int refined_score = icalnum;
        refined_point = RefineMatchSampleABPoint(
            image, pathA, pathB, refined_point, icalnum, ithre, m_iB2W,
            iminfindngnum, igapx, igapy, refined_score);
        if (m_rawthresholdhitpoints.size() < 32) {
          m_rawthresholdhitpoints.push_back(refined_point);
          m_rawthresholdhitscores.push_back(refined_score);
        }
        resulttolist(refined_point, refined_score);
      }
    }
    iy += igapy;
  }

  resultsort();
  m_resultrects.clear();
  const int icountresult = static_cast<int>(m_resultnums.size());
  for (int i = 0; i < icountresult; i++) {
    int ivalue = m_resultnums.at(i);
    gp_Pnt apoint = m_resultpoints.at(i);
    double dpercent = ivalue / (1.0 * itotalsize);
    gp_Rectangle arect(gp_Pnt(apoint.X(), apoint.Y(), 0),
                       gp_Pnt(apoint.X() + iw, apoint.Y() + ih, 0));
    std::ostringstream stream;
    stream << dpercent;
    std::string astr = stream.str();
    m_resultrects.addrect(arect, astr);
  }
}
void FastMatch::MatchSampleABMore(Image &image, gp_Path &pathA,
                                  gp_Path &pathB) {
  int ithre = m_imatchthre;
  int icurmodule = ImageManager::GetCurMode();
  Image *pimage = ImageManager::GetTransferImage(icurmodule);
  (void)pimage;
  ZeroPOS();
  gp_Rectangle arect1 = pathA.boundingRect();
  gp_Rectangle arect2 = pathB.boundingRect();
  (void)arect2;
  int ix = 0;
  int iy = 0;
  int iw = static_cast<int>(pathA.boundingRect().Width());
  int ih = static_cast<int>(pathA.boundingRect().Height());
  int ix0, iy0, ix1, iy1;

  int igapx = m_stepgapx;
  int igapy = m_stepgapy;
  if (m_resultrects.size() > 0) {
    int iresultnum = static_cast<int>(m_resultrects.size()) - 1;
    ix0 = static_cast<int>(m_resultrects.getrect(iresultnum).TopLeft().X() -
                           1.5 * igapx);
    iy0 = static_cast<int>(m_resultrects.getrect(iresultnum).TopLeft().Y() -
                           1.5 * igapy);
    ix1 = static_cast<int>(m_resultrects.getrect(iresultnum).BottomRight().X() +
                           1.5 * igapx);
    iy1 = static_cast<int>(m_resultrects.getrect(iresultnum).BottomRight().Y() +
                           1.5 * igapx);

    igapx = 1;
    igapy = 1;
  } else {
    ix0 = static_cast<int>(m_matchrect.TopLeft().X());
    iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
    ix1 = static_cast<int>(m_matchrect.BottomRight().X());
    iy1 = static_cast<int>(m_matchrect.BottomRight().Y());
  }

  if (image.getWidth() <= ix1 || image.getHeight() <= iy1)
    return;
  m_iminfindnum = -1;
  const int icount1 = static_cast<int>(pathA.ElementCount());
  const int icount2 = static_cast<int>(pathB.ElementCount());
  (void)icount2;

  cv::Vec3b pixel0, pixel1;
  int icalnum = 0;
  int icalng = 0;

  gp_Pnt aele;

  iy1 = iy1 - static_cast<int>(arect1.Height());
  ix1 = ix1 - static_cast<int>(arect1.Width());
  int itotalsize = static_cast<int>(pathA.ElementCount());

  m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
  const int iminfindngnum =
      static_cast<int>((1 - m_dminscore) * itotalsize / 2);
  const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);

  for (iy = iy0; iy < iy1;) {
    for (ix = ix0; ix < ix1;) {
      ++m_rawmatch_probe_count;
      int imovx = ix;
      int imovy = iy;

      icalnum = 0;
      icalng = 0;
      for (int i = 0; i < icount1 - 1; i++) {
        aele = pathA.ElementAt(i);
        pixel0 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));
        aele = pathB.ElementAt(i);
        pixel1 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));

        if (0 == m_iB2W) {
          int ir = Red(pixel0) - Red(pixel1);
          int ig = Green(pixel0) - Green(pixel1);
          int ib = Blue(pixel0) - Blue(pixel1);
          if (ir > ithre || ig > ithre || ib > ithre) {
            icalnum++;
          } else {
            icalng++;
            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        } else if (1 == m_iB2W) {
          int ir = Red(pixel1) - Red(pixel0);
          int ig = Green(pixel1) - Green(pixel0);
          int ib = Blue(pixel1) - Blue(pixel0);
          if (ir > ithre || ig > ithre || ib > ithre) {
            icalnum++;
          } else {
            icalng++;

            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        }
      }

    NextStep_1:
      ix += igapx;
      if (icalnum > m_iminfindnum && icalnum > iminfindoknum) {
        ++m_rawmatch_threshold_hit_count;
        gp_Pnt apoint(ix, iy, 0);
        resulttolist(apoint, icalnum);
      }
    }
    iy += igapy;
  }

  resultsort();
  m_resultrects.clear();
  const int icountresult = static_cast<int>(m_resultnums.size());
  for (int i = 0; i < icountresult; i++) {
    int ivalue = m_resultnums.at(i);
    gp_Pnt apoint = m_resultpoints.at(i);
    double dpercent = ivalue / (1.0 * itotalsize);
    gp_Rectangle arect(gp_Pnt(apoint.X(), apoint.Y(), 0),
                       gp_Pnt(apoint.X() + iw, apoint.Y() + ih, 0));
    std::ostringstream stream;
    stream << dpercent;
    std::string astr = stream.str();
    m_resultrects.addrect(arect, astr);
  }
}

int FastMatch::getrawmatchprobecount() const { return m_rawmatch_probe_count; }

int FastMatch::getrawmatchthresholdhitcount() const {
  return m_rawmatch_threshold_hit_count;
}

int FastMatch::getmatchcallcount() const { return m_match_call_count; }

int FastMatch::getmatchabcallcount() const { return m_matchab_call_count; }

int FastMatch::getmatchsampleabcallcount() const {
  return m_matchsampleab_call_count;
}

int FastMatch::getmatchlaststage() const { return m_match_last_stage; }

int FastMatch::getmatchimagewidth() const { return m_match_debug_image_width; }

int FastMatch::getmatchimageheight() const {
  return m_match_debug_image_height;
}

int FastMatch::getlearnrectx0() const { return m_learn_roi_x; }

int FastMatch::getlearnrecty0() const { return m_learn_roi_y; }

int FastMatch::getlearnrectx1() const {
  return m_learn_roi_x + FastMatchPositiveInt(m_learn_roi_w);
}

int FastMatch::getlearnrecty1() const {
  return m_learn_roi_y + FastMatchPositiveInt(m_learn_roi_h);
}

int FastMatch::getmatchrectx0() const { return m_search_roi_x; }

int FastMatch::getmatchrecty0() const { return m_search_roi_y; }

int FastMatch::getmatchrectx1() const {
  return m_search_roi_x + FastMatchPositiveInt(m_search_roi_w);
}

int FastMatch::getmatchrecty1() const {
  return m_search_roi_y + FastMatchPositiveInt(m_search_roi_h);
}

int FastMatch::getresulttolistcallcount() const {
  return m_resulttolist_call_count;
}

int FastMatch::getresultcandidateinsertcount() const {
  return m_resultcandidate_insert_count;
}

int FastMatch::getresultcandidatereplacecount() const {
  return m_resultcandidate_replace_count;
}

int FastMatch::getresultcandidaterejectcount() const {
  return m_resultcandidate_reject_count;
}

int FastMatch::getresultcandidatecount() {
  return static_cast<int>(std::min(m_resultnums.size(), m_resultpoints.size()));
}

int FastMatch::getresultbestindex() {
  const int candidate_count = getresultcandidatecount();
  if (candidate_count <= 0) {
    return -1;
  }
  return candidate_count - 1;
}

double FastMatch::getresultbestscore() {
  const int best_index = getresultbestindex();
  if (best_index < 0) {
    return 0.0;
  }
  return m_resultnums.at(best_index);
}

int FastMatch::getrawthresholdhitrecordcount() const {
  return static_cast<int>(m_rawthresholdhitpoints.size());
}

gp_Pnt FastMatch::getrawthresholdhitpoint(int inum) const {
  if (inum >= 0 && inum < static_cast<int>(m_rawthresholdhitpoints.size())) {
    return m_rawthresholdhitpoints.at(inum);
  }
  return gp_Pnt();
}

int FastMatch::getrawthresholdhitscore(int inum) const {
  if (inum >= 0 && inum < static_cast<int>(m_rawthresholdhitscores.size())) {
    return m_rawthresholdhitscores.at(inum);
  }
  return 0;
}

void FastMatch::MultiMatch(Image &image) {
  resultclear();
  const int isize = static_cast<int>(m_models_l12.size());
  for (int i = 0; i < isize; i++) {
    modelstocurrent_l12(i);
    gp_Path &path = FindLine::getpatternpath();
    MultiMatchSample(image, path);
  }
}
void FastMatch::multimatch(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  MultiMatch(*pgetimage);
}
void FastMatch::MultiMatchSample(Image &image, gp_Path &path) {
  int ithre = m_imatchthre;
  int icurmodule = ImageManager::GetCurMode();
  Image *pimage = ImageManager::GetTransferImage(icurmodule);
  (void)pimage;

  const int imatchnum = static_cast<int>(m_matchrects.size());
  for (int ik = 0; ik < imatchnum; ik++) {
    int ix0 = static_cast<int>(m_matchrects.getrect(ik).TopLeft().X());
    int iy0 = static_cast<int>(m_matchrects.getrect(ik).TopLeft().Y());
    int ix1 = static_cast<int>(m_matchrects.getrect(ik).TopLeft().X() +
                               m_matchrects.getrect(ik).Width());
    int iy1 = static_cast<int>(m_matchrects.getrect(ik).TopLeft().Y() +
                               m_matchrects.getrect(ik).Height());

    if (image.getWidth() < ix1 || image.getHeight() < iy1)
      return;

    m_iminfindnum = -1;
    const int icount = static_cast<int>(path.ElementCount());

    cv::Vec3b pixel0, pixel1;

    int icalnum = 0;
    int icalng = 0;
    gp_Pnt aele;

    int igapx = m_stepgapx;
    int igapy = m_stepgapy;
    gp_Rectangle arect1 = path.boundingRect();
    iy1 = iy1 - static_cast<int>(arect1.Height());
    ix1 = ix1 - static_cast<int>(arect1.Width());
    int ix = 0;
    int iy = 0;

    int iw = static_cast<int>(FindLine::patternboundingrect().Width());
    int ih = static_cast<int>(FindLine::patternboundingrect().Height());
    int itotalsize = static_cast<int>(FindLine::getpattern().size());

    m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
    const int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize);
    const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize);

    for (iy = iy0; iy < iy1;) {
      for (ix = ix0; ix < ix1;) {
        int imovx = ix;
        int imovy = iy;

        icalnum = 0;
        for (int i = 0; i < icount - 1; i++) {
          aele = path.ElementAt(i);
          pixel0 = image.pixel(static_cast<int>(aele.X() + imovx),
                               static_cast<int>(aele.Y() + imovy));
          i++;
          aele = path.ElementAt(i);
          pixel1 = image.pixel(static_cast<int>(aele.X() + imovx),
                               static_cast<int>(aele.Y() + imovy));

          if (0 == m_iB2W) {
            int ir = Red(pixel0) - Red(pixel1);
            int ig = Green(pixel0) - Green(pixel1);
            int ib = Blue(pixel0) - Blue(pixel1);
            if (ir > ithre || ig > ithre || ib > ithre) {
              icalnum++;
            } else {
              icalng++;
              if (icalng > iminfindngnum)
                goto NextStep_2;
            }
          } else if (1 == m_iB2W) {
            int ir = Red(pixel1) - Red(pixel0);
            int ig = Green(pixel1) - Green(pixel0);
            int ib = Blue(pixel1) - Blue(pixel0);
            if (ir > ithre || ig > ithre || ib > ithre) {
              icalnum++;
            } else {
              icalng++;
              if (icalng > iminfindngnum)
                goto NextStep_2;
            }
          }
        }
      NextStep_2:
        ix += igapx;

        if (icalnum > m_iminfindnum && icalnum > iminfindoknum) {
          gp_Pnt apoint(ix, iy, 0);
          resulttolist(apoint, icalnum);
        }
      }
      iy += igapy;
    }

    resultsort();
    m_resultrects.clear();

    const int icountresult = static_cast<int>(m_resultnums.size());
    for (int i = 0; i < icountresult; i++) {
      int ivalue = m_resultnums.at(i);
      gp_Pnt apoint = m_resultpoints.at(i);
      double dpercent = (2.0 * ivalue) / (1.0 * itotalsize);
      gp_Rectangle arect(gp_Pnt(apoint.X(), apoint.Y(), 0),
                         gp_Pnt(apoint.X() + iw, apoint.Y() + ih, 0));
      std::ostringstream stream;
      stream << dpercent;
      std::string astr = stream.str();
      m_resultrects.addrect(arect, astr);
    }
  }
}

void FastMatch::RotateMatchAB(Image &image) {
  m_matchimage = &image;
  resultclear();

  for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++) {
    m_rotateshaperesults[it].clear();
  }
  m_rotateshaperesults.clear();
  m_rotatereslutpoints.clear();
  m_rotateresults.clear();
  m_rotatereslutangles.clear();
  m_clusters.clear();

  int isize = static_cast<int>((m_dangle_add - m_dangle_mud) / m_danglegap);
  int icurangle = 0;

  if (m_models_rotate.size() <= 0)
    return;

  if (m_dangle_mud < 0 && m_dangle_add >= 0) {
    int ibeginangle = static_cast<int>(360 + m_dangle_mud);
    icurangle = ibeginangle;
    while (icurangle < 360) {
      resultclear();
      gp_Path &pathA = m_models_rotate[icurangle].getpathA();
      gp_Path &pathB = m_models_rotate[icurangle].getpathB();
      PointsShape &arectpoints = m_models_rotaterects[icurangle];
      RotateMatchSampleAB(image, pathA, pathB, arectpoints, icurangle);
      icurangle = static_cast<int>(icurangle + m_danglegap);
    }
    icurangle = 0;
    while (icurangle < m_dangle_add) {
      resultclear();
      gp_Path &pathA = m_models_rotate[icurangle].getpathA();
      gp_Path &pathB = m_models_rotate[icurangle].getpathB();
      PointsShape &arectpoints = m_models_rotaterects[icurangle];
      RotateMatchSampleAB(image, pathA, pathB, arectpoints, icurangle);
      icurangle = static_cast<int>(icurangle + m_danglegap);
    }
  } else if (m_dangle_mud < m_dangle_add && m_dangle_mud >= 0) {
    int ibeginangle = static_cast<int>(m_dangle_mud);
    icurangle = ibeginangle;
    for (int i = 0; i < isize; i++) {
      resultclear();
      gp_Path &pathA = m_models_rotate[icurangle].getpathA();
      gp_Path &pathB = m_models_rotate[icurangle].getpathB();
      PointsShape &arectpoints = m_models_rotaterects[icurangle];
      RotateMatchSampleAB(image, pathA, pathB, arectpoints, icurangle);
      icurangle = static_cast<int>(icurangle + m_danglegap);
    }
  }

  rotateresultsort();
  if (m_danglegap > 1.0 && !m_rotateresults.empty() &&
      m_iupgradeanglescale > 0) {
    setupgradenum(0);
    RotateMatchAB_upgrade(image);
  }
}
void FastMatch::setupgradenum(int iresultnum) { m_iupgradenum = iresultnum; }
void FastMatch::RotateMatchAB_upgrade(Image &image) {

  if (!HasRotateResultAt(m_iupgradenum, m_rotateresults, m_rotatereslutpoints,
                         m_rotatereslutangles, m_rotateshaperesults))
    return;

  PointsShape abestshape = m_rotateshaperesults[m_iupgradenum];
  (void)abestshape;
  gp_Pnt abestpoint = m_rotatereslutpoints[m_iupgradenum];
  double abestresult = m_rotateresults[m_iupgradenum];
  (void)abestresult;
  double abestreslutangle = m_rotatereslutangles[m_iupgradenum];
  m_matchimage = &image;
  resultclear();
  for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++) {
    m_rotateshaperesults[it].clear();
  }
  m_rotateshaperesults.clear();
  m_rotatereslutpoints.clear();
  m_rotateresults.clear();
  m_rotatereslutangles.clear();
  m_clusters.clear();

  if (m_iupgradeanglescale <= 6)
    m_iupgradeanglescale = 6;
  int isize = m_iupgradeanglescale;
  int ihfsize = m_iupgradeanglescale / 2;
  int icurangle = 0;
  if (m_models_rotate.size() <= 0)
    return;
  if (abestreslutangle + m_iupgradeanglescale > 360) {
    abestreslutangle = abestreslutangle - 360;
  }

  if (abestreslutangle - ihfsize < 0 && abestreslutangle + ihfsize >= 0) {
    int ibeginangle = static_cast<int>(360 + abestreslutangle - ihfsize);
    icurangle = ibeginangle;
    while (icurangle < 360) {
      resultclear();
      gp_Path &path = m_models_rotate[icurangle].getpath();
      PointsShape &arectpoints = m_models_rotaterects[icurangle];

      RotateMatchSample_upgrade(image, path, arectpoints, icurangle,
                                abestpoint);
      icurangle = icurangle + 1;
    }
    icurangle = 0;
    while (icurangle < abestreslutangle + ihfsize) {
      resultclear();
      gp_Path &path = m_models_rotate[icurangle].getpath();
      PointsShape &arectpoints = m_models_rotaterects[icurangle];
      RotateMatchSample_upgrade(image, path, arectpoints, icurangle,
                                abestpoint);
      icurangle = icurangle + 1;
    }
  } else if (abestreslutangle - ihfsize >= 0) {
    int ibeginangle = static_cast<int>(abestreslutangle - ihfsize);
    icurangle = ibeginangle;
    for (int i = 0; i < isize; i++) {
      resultclear();
      gp_Path &path = m_models_rotate[icurangle].getpath();
      PointsShape &arectpoints = m_models_rotaterects[icurangle];
      RotateMatchSample_upgrade(image, path, arectpoints, icurangle,
                                abestpoint);
      icurangle = icurangle + 1;
    }
  }

  rotateresultsort();
}
void FastMatch::RotateMatchAB05_upgrade(Image &image) {

  if (!HasRotateResultAt(m_iupgradenum, m_rotateresults, m_rotatereslutpoints,
                         m_rotatereslutangles, m_rotateshaperesults))
    return;

  PointsShape abestshape = m_rotateshaperesults[m_iupgradenum];
  (void)abestshape;
  gp_Pnt abestpoint = m_rotatereslutpoints[m_iupgradenum];
  double abestresult = m_rotateresults[m_iupgradenum];
  (void)abestresult;
  double abestreslutangle = m_rotatereslutangles[m_iupgradenum];
  m_matchimage = &image;
  resultclear();
  for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++) {
    m_rotateshaperesults[it].clear();
  }
  m_rotateshaperesults.clear();
  m_rotatereslutpoints.clear();
  m_rotateresults.clear();
  m_rotatereslutangles.clear();
  m_clusters.clear();

  int isize = 12;
  double dcurangle = 0;
  int ianglenum = 0;
  if (m_models05_rotate.size() <= 0)
    return;

  if (abestreslutangle + 6 > 360) {
    abestreslutangle = abestreslutangle - 360;
  }
  if (abestreslutangle - 6 < 0 && abestreslutangle + 6 >= 0) {
    int ibeginangle = static_cast<int>(360 + abestreslutangle - 6);
    dcurangle = ibeginangle;
    ianglenum = ibeginangle * 2;
    while (ianglenum < 720) {
      resultclear();
      gp_Path &path = m_models05_rotate[ianglenum].getpath();
      PointsShape &arectpoints = m_models05_rotaterects[ianglenum];
      RotateMatchSample_upgrade(image, path, arectpoints, dcurangle,
                                abestpoint);
      dcurangle = dcurangle + 0.5;
      ianglenum = ianglenum + 1;
    }
    dcurangle = 0;
    ianglenum = 0;
    while (dcurangle < abestreslutangle + 6) {
      resultclear();
      gp_Path &path = m_models05_rotate[ianglenum].getpath();
      PointsShape &arectpoints = m_models05_rotaterects[ianglenum];
      RotateMatchSample_upgrade(image, path, arectpoints, dcurangle,
                                abestpoint);
      dcurangle = dcurangle + 0.5;
      ianglenum = ianglenum + 1;
    }
  } else if (abestreslutangle - 6 >= 0) {
    int ibeginangle = static_cast<int>(abestreslutangle - 6);
    dcurangle = ibeginangle;
    ianglenum = ibeginangle * 2;
    for (int i = 0; i < isize; i++) {
      resultclear();
      gp_Path &path = m_models05_rotate[ianglenum].getpath();
      PointsShape &arectpoints = m_models05_rotaterects[ianglenum];
      RotateMatchSample_upgrade(image, path, arectpoints, dcurangle,
                                abestpoint);
      dcurangle = dcurangle + 0.5;
      ianglenum = ianglenum + 1;
    }
  }

  rotateresultsort();
}
void FastMatch::RotateMatchAB025_upgrade(Image &image) {
  if (!HasRotateResultAt(m_iupgradenum, m_rotateresults, m_rotatereslutpoints,
                         m_rotatereslutangles, m_rotateshaperesults))
    return;

  PointsShape abestshape = m_rotateshaperesults[m_iupgradenum];
  (void)abestshape;
  gp_Pnt abestpoint = m_rotatereslutpoints[m_iupgradenum];
  double abestresult = m_rotateresults[m_iupgradenum];
  (void)abestresult;
  double abestreslutangle = m_rotatereslutangles[m_iupgradenum];
  m_matchimage = &image;
  resultclear();
  for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++) {
    m_rotateshaperesults[it].clear();
  }
  m_rotateshaperesults.clear();
  m_rotatereslutpoints.clear();
  m_rotateresults.clear();
  m_rotatereslutangles.clear();
  m_clusters.clear();

  int isize = 24;
  double dcurangle = 0;
  int ianglenum = 0;
  if (m_models025_rotate.size() <= 0)
    return;

  if (abestreslutangle + 6 > 360) {
    abestreslutangle = abestreslutangle - 360;
  }

  if (abestreslutangle - 3 < 0 && abestreslutangle + 3 >= 0) {
    int ibeginangle = static_cast<int>(360 + abestreslutangle - 3);
    dcurangle = ibeginangle;
    ianglenum = ibeginangle * 4;
    while (ianglenum < 1440) {
      resultclear();
      gp_Path &path = m_models025_rotate[ianglenum].getpath();
      PointsShape &arectpoints = m_models025_rotaterects[ianglenum];
      RotateMatchSample_upgrade(image, path, arectpoints, dcurangle,
                                abestpoint);
      dcurangle = dcurangle + 0.25;
      ianglenum = ianglenum + 1;
    }
    dcurangle = 0;
    ianglenum = 0;
    while (dcurangle < abestreslutangle + 3) {
      resultclear();
      gp_Path &path = m_models025_rotate[ianglenum].getpath();
      PointsShape &arectpoints = m_models025_rotaterects[ianglenum];
      RotateMatchSample_upgrade(image, path, arectpoints, dcurangle,
                                abestpoint);
      dcurangle = dcurangle + 0.25;
      ianglenum = ianglenum + 1;
    }
  } else if (abestreslutangle - 3 >= 0) {
    int ibeginangle = static_cast<int>(abestreslutangle - 3);
    dcurangle = ibeginangle;
    ianglenum = ibeginangle * 4;
    for (int i = 0; i < isize; i++) {
      resultclear();
      gp_Path &path = m_models025_rotate[ianglenum].getpath();
      PointsShape &arectpoints = m_models025_rotaterects[ianglenum];
      RotateMatchSample_upgrade(image, path, arectpoints, dcurangle,
                                abestpoint);
      dcurangle = dcurangle + 0.25;
      ianglenum = ianglenum + 1;
    }
  }

  rotateresultsort();
}
void FastMatch::samplemodelAB(int inum) { FindLine::samplemodelAB(inum); }
void FastMatch::RotateMatch(Image &image) {
  m_matchimage = &image;
  resultclear();
  for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++) {
    m_rotateshaperesults[it].clear();
  }
  m_rotateshaperesults.clear();
  m_rotatereslutpoints.clear();
  m_rotateresults.clear();
  m_rotatereslutangles.clear();
  m_clusters.clear();

  int isize = static_cast<int>(360 / m_danglegap);
  int icurangle = 0;

  if (m_models_rotate.size() <= 0)
    return;

  for (int i = 0; i < isize; i++) {
    resultclear();

    gp_Path &path = m_models_rotate[icurangle].getpath();
    PointsShape &arectpoints = m_models_rotaterects[icurangle];
    RotateMatchSample(image, path, arectpoints, icurangle);

    icurangle = static_cast<int>(icurangle + m_danglegap);
  }

  resultcluster(m_ixclustergap, m_iyclustergap, m_iangleclustergap);
}
void FastMatch::setclustergap(int ixclustergap, int iyclustergap,
                              int iangleclustergap) {
  m_ixclustergap = FastMatchPositiveInt(ixclustergap);
  m_iyclustergap = FastMatchPositiveInt(iyclustergap);
  m_iangleclustergap = FastMatchPositiveInt(iangleclustergap);
}
void FastMatch::rotatematchAB(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  resetRotateBudget();
  RotateMatchAB(*pgetimage);
}

void FastMatch::Setupgradescale(int isx, int isy) {
  m_iupgradexscale = FastMatchPositiveInt(isx);
  m_iupgradeyscale = FastMatchPositiveInt(isy);
}
void FastMatch::Setupgradeanglescale(int iangle) {
  m_iupgradeanglescale = FastMatchPositiveInt(iangle);
}
void FastMatch::rotatematchAB_upgrade(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  resetRotateBudget();
  RotateMatchAB_upgrade(*pgetimage);
}

void FastMatch::rotatematchAB05_upgrade(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  resetRotateBudget();
  RotateMatchAB05_upgrade(*pgetimage);
}
void FastMatch::rotatematchAB025_upgrade(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  resetRotateBudget();
  RotateMatchAB025_upgrade(*pgetimage);
}

void FastMatch::rotatematch(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  resetRotateBudget();
  RotateMatch(*pgetimage);
}

void FastMatch::RotateMatchSample(Image &image, gp_Path &path,
                                  PointsShape &modelrect, double dangle) {
  if (!consumeRotateCandidate()) return;
  int ithre = m_imatchthre;
  int icurmodule = ImageManager::GetCurMode();
  Image *pimage = ImageManager::GetTransferImage(icurmodule);
  (void)pimage;
  (void)modelrect;
  (void)dangle;
  int ix0 = static_cast<int>(m_matchrect.TopLeft().X());
  int iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
  int ix1 = static_cast<int>(m_matchrect.TopLeft().X() + m_matchrect.Width());
  int iy1 = static_cast<int>(m_matchrect.TopLeft().Y() + m_matchrect.Height());

  if (image.getWidth() <= ix1 || image.getHeight() <= iy1)
    return;
  m_iminfindnum = -1;
  const int icount = static_cast<int>(path.ElementCount());
  cv::Vec3b pixel0, pixel1;
  int icalnum = 0;
  int icalng = 0;

  gp_Pnt aele;

  int igapx = m_stepgapx;
  int igapy = m_stepgapy;
  gp_Rectangle arect1 = path.boundingRect();
  iy1 = iy1 - static_cast<int>(arect1.Height());
  ix1 = ix1 - static_cast<int>(arect1.Width());
  int ix = 0;
  int iy = 0;
  int iw = static_cast<int>(FindLine::patternboundingrect().Width());
  int ih = static_cast<int>(FindLine::patternboundingrect().Height());
  int itotalsize = static_cast<int>(FindLine::getpattern().size());

  m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
  const int iminfindngnum =
      static_cast<int>((1 - m_dminscore) * itotalsize / 2);
  const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);

  for (iy = iy0; iy < iy1;) {
    for (ix = ix0; ix < ix1;) {
      if (!consumeRotateProbe(std::max(1, icount / 2)))
        goto RotateMatchSampleDone;
      int imovx = ix;
      int imovy = iy;

      icalnum = 0;
      icalng = 0;
      for (int i = 0; i < icount - 1; i++) {
        aele = path.ElementAt(i);
        pixel0 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));
        i++;
        aele = path.ElementAt(i);
        pixel1 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));

        if (0 == m_iB2W) {
#ifdef COLORMATCH
          int ir = Red(pixel0) - Red(pixel1);
          int ig = Green(pixel0) - Green(pixel1);
          int ib = Blue(pixel0) - Blue(pixel1);
          if (ir > ithre || ig > ithre || ib > ithre)
#else
          int ir = Red(pixel0) - Red(pixel1);
          if (ir > ithre)
#endif
          {
            icalnum++;
          } else {
            icalng++;
            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        } else if (1 == m_iB2W) {
#ifdef COLORMATCH
          int ir = Red(pixel1) - Red(pixel0);
          int ig = Green(pixel1) - Green(pixel0);
          int ib = Blue(pixel1) - Blue(pixel0);
          if (ir > ithre || ig > ithre || ib > ithre)
#else
          int ir = Red(pixel1) - Red(pixel0);
          if (ir > ithre)
#endif
          {
            icalnum++;
          } else {
            icalng++;

            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        }
      }

    NextStep_1:
      ix += igapx;
      if (icalnum > m_iminfindnum && icalnum > iminfindoknum) {
        gp_Pnt apoint(imovx, imovy, 0);
        if (m_rawthresholdhitpoints.size() < 32) {
          m_rawthresholdhitpoints.push_back(apoint);
          m_rawthresholdhitscores.push_back(icalnum);
        }
        resulttolist(apoint, icalnum);
      }
    }
    iy += igapy;
  }

RotateMatchSampleDone:
  resultsort();
  m_resultrects.clear();
  int icountresult = static_cast<int>(m_resultnums.size());
  for (int i = 0; i < icountresult; i++) {
    int ivalue = m_resultnums.at(i);
    gp_Pnt apoint = m_resultpoints.at(i);
    double dpercent = (2.0 * ivalue) / (1.0 * itotalsize);
    gp_Rectangle arect(gp_Pnt(apoint.X(), apoint.Y(), 0), static_cast<int>(iw),
                       static_cast<int>(ih));
    std::ostringstream stream;
    stream << dangle << "   " << dpercent;
    std::string astr = stream.str();
    m_resultrects.addrect(arect, astr);
    PointsShape bmodelrect = modelrect;
    bmodelrect.Move(static_cast<int>(apoint.X()), static_cast<int>(apoint.Y()));
    m_rotateshaperesults.push_back(bmodelrect);
    m_rotatereslutpoints.push_back(apoint);
    m_rotateresults.push_back(dpercent);
    m_rotatereslutangles.push_back(dangle);
  }
}

void FastMatch::RotateMatchSampleAB(Image &image, gp_Path &pathA,
                                    gp_Path &pathB, PointsShape &modelrect,
                                    double dangle) {
  if (!consumeRotateCandidate()) return;
  int ithre = m_imatchthre;
  int icurmodule = ImageManager::GetCurMode();
  Image *pimage = ImageManager::GetTransferImage(icurmodule);
  (void)pimage;
  int ix0 = static_cast<int>(m_matchrect.TopLeft().X());
  int iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
  int ix1 = static_cast<int>(m_matchrect.TopLeft().X() + m_matchrect.Width());
  int iy1 = static_cast<int>(m_matchrect.TopLeft().Y() + m_matchrect.Height());

  if (image.getWidth() <= ix1 || image.getHeight() <= iy1) {
    if (image.getWidth() <= ix1)
      ix1 = image.getWidth();
    if (image.getHeight() <= iy1)
      iy1 = image.getHeight();
  }
  m_iminfindnum = -1;
  int icount1 = static_cast<int>(pathA.ElementCount());
  int icount2 = static_cast<int>(pathB.ElementCount());
  (void)icount2;

  cv::Vec3b pixel0, pixel1;
  int icalnum = 0;
  int icalng = 0;

  gp_Pnt aele;

  int igapx = m_stepgapx;
  int igapy = m_stepgapy;
  gp_Rectangle arect1 = pathA.boundingRect();
  gp_Rectangle arect2 = pathB.boundingRect();
  (void)arect2;
  iy1 = static_cast<int>(iy1 - arect1.Height());
  ix1 = static_cast<int>(ix1 - arect1.Width());
  int ix = 0;
  int iy = 0;
  int iw = static_cast<int>(pathA.boundingRect().Width());
  int ih = static_cast<int>(pathA.boundingRect().Height());
  int itotalsize = static_cast<int>(pathA.ElementCount());

  m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
  int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize / 2);
  int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);

  for (iy = iy0; iy < iy1;) {
    for (ix = ix0; ix < ix1;) {
      if (!consumeRotateProbe(std::max(1, icount1)))
        goto RotateMatchSampleABDone;
      int imovx = ix;
      int imovy = iy;

      icalnum = 0;
      icalng = 0;
      if (!FastMatchCandidateMaskPass(m_matchmask, pathA, imovx, imovy)) {
        ix += igapx;
        continue;
      }
      if (!MatchSampleABAnchorPass(image, pathA, pathB, imovx, imovy, ithre,
                                   m_iB2W)) {
        ix += igapx;
        continue;
      }
      for (int i = 0; i < icount1 - 1; i++) {
        aele = pathA.ElementAt(i);
        pixel0 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));
        aele = pathB.ElementAt(i);
        pixel1 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));

        if (0 == m_iB2W) {
          int ir = Red(pixel0) - Red(pixel1);
          int ig = Green(pixel0) - Green(pixel1);
          int ib = Blue(pixel0) - Blue(pixel1);
          if (ir > ithre || ig > ithre || ib > ithre) {
            icalnum++;
          } else {
            icalng++;
            if (icalng > iminfindngnum)
              goto NextStep_1_R;
          }
        } else if (1 == m_iB2W) {
          int ir = Red(pixel1) - Red(pixel0);
          int ig = Green(pixel1) - Green(pixel0);
          int ib = Blue(pixel1) - Blue(pixel0);
          if (ir > ithre || ig > ithre || ib > ithre) {
            icalnum++;
          } else {
            icalng++;

            if (icalng > iminfindngnum)
              goto NextStep_1_R;
          }
        }
      }

    NextStep_1_R:
      ix += igapx;
      if (icalnum > m_iminfindnum && icalnum > iminfindoknum) {
        gp_Pnt refined_point(imovx, imovy, 0);
        int refined_score = icalnum;
        refined_point = RefineMatchSampleABPoint(
            image, pathA, pathB, refined_point, icalnum, ithre, m_iB2W,
            iminfindngnum, igapx, igapy, refined_score);
        resulttolist(refined_point, refined_score);
      }
    }
    iy += igapy;
  }

RotateMatchSampleABDone:
  resultsort();
  m_resultrects.clear();
  int icountresult = static_cast<int>(m_resultnums.size());
  for (int i = 0; i < icountresult; i++) {
    int ivalue = m_resultnums.at(i);
    gp_Pnt apoint = m_resultpoints.at(i);
    double dpercent = ivalue / (1.0 * itotalsize);
    gp_Rectangle arect(gp_Pnt(apoint.X(), apoint.Y(), 0),
                       gp_Pnt(apoint.X() + static_cast<int>(iw),
                              apoint.Y() + static_cast<int>(ih), 0));
    std::ostringstream stream;
    stream << dangle << "   " << dpercent;
    std::string astr = stream.str();
    m_resultrects.addrect(arect, astr);
    PointsShape bmodelrect = modelrect;
    bmodelrect.Move(static_cast<int>(apoint.X()), static_cast<int>(apoint.Y()));
    m_rotateshaperesults.push_back(bmodelrect);
    m_rotatereslutpoints.push_back(apoint);
    m_rotateresults.push_back(dpercent);
    m_rotatereslutangles.push_back(dangle);
  }
}

void FastMatch::RotateMatchSample_upgrade(Image &image, gp_Path &path,
                                          PointsShape &modelrect, double dangle,
                                          gp_Pnt &resultpoint) {
  if (!consumeRotateCandidate()) return;
  int ithre = m_imatchthre;
  int icurmodule = ImageManager::GetCurMode();
  Image *pimage = ImageManager::GetTransferImage(icurmodule);
  (void)pimage;
  (void)modelrect;
  (void)dangle;
  int ix0 = static_cast<int>(resultpoint.X() - m_iupgradexscale);
  int iy0 = static_cast<int>(resultpoint.Y() - m_iupgradeyscale);
  int ix1 = static_cast<int>(resultpoint.X() + m_iupgradexscale);
  int iy1 = static_cast<int>(resultpoint.Y() + m_iupgradeyscale);

  if (image.getWidth() <= ix1 || image.getHeight() <= iy1)
    return;
  m_iminfindnum = -1;
  int icount = static_cast<int>(path.ElementCount());
  cv::Vec3b pixel0, pixel1;
  int icalnum = 0;
  int icalng = 0;

  gp_Pnt aele;

  int igapx = 1;
  int igapy = 1;
  int ix = 0;
  int iy = 0;
  int iw = static_cast<int>(FindLine::patternboundingrect().Width());
  int ih = static_cast<int>(FindLine::patternboundingrect().Height());
  int itotalsize = static_cast<int>(FindLine::getpattern().size());

  m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
  int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize / 2);
  int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);

  for (iy = iy0; iy < iy1;) {
    for (ix = ix0; ix < ix1;) {
      if (!consumeRotateProbe(std::max(1, icount / 2)))
        return;
      int imovx = ix;
      int imovy = iy;
      icalnum = 0;
      icalng = 0;
      for (int i = 0; i < icount - 1; i++) {
        aele = path.ElementAt(i);
        pixel0 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));
        i++;
        aele = path.ElementAt(i);
        pixel1 = image.pixel(static_cast<int>(aele.X() + imovx),
                             static_cast<int>(aele.Y() + imovy));

        if (0 == m_iB2W) {
#ifdef COLORMATCH
          int ir = Red(pixel0) - Red(pixel1);
          int ig = Green(pixel0) - Green(pixel1);
          int ib = Blue(pixel0) - Blue(pixel1);
          if (ir > ithre || ig > ithre || ib > ithre)
#else
          int ir = Red(pixel0) - Red(pixel1);
          if (ir > ithre)
#endif
          {
            icalnum++;
          } else {
            icalng++;
            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        } else if (1 == m_iB2W) {
#ifdef COLORMATCH
          int ir = qRed(pixel1) - qRed(pixel0);
          int ig = qGreen(pixel1) - qGreen(pixel0);
          int ib = qBlue(pixel1) - qBlue(pixel0);
          if (ir > ithre || ig > ithre || ib > ithre)
#else
          int ir = Red(pixel1) - Red(pixel0);
          if (ir > ithre)
#endif
          {
            icalnum++;
          } else {
            icalng++;

            if (icalng > iminfindngnum)
              goto NextStep_1;
          }
        }
      }

    NextStep_1:
      ix += igapx;
      if (icalnum > m_iminfindnum && icalnum > iminfindoknum) {
        gp_Pnt apoint(ix, iy, 0);
        resulttolist(apoint, icalnum);
      }
    }
    iy += igapy;
  }

  resultsort();
  m_resultrects.clear();
  int icountresult = static_cast<int>(m_resultnums.size());
  for (int i = 0; i < icountresult; i++) {
    int ivalue = m_resultnums.at(i);
    gp_Pnt apoint = m_resultpoints.at(i);
    double dpercent = (2.0 * ivalue) / (1.0 * itotalsize);
    gp_Rectangle arect(gp_Pnt(apoint.X(), apoint.Y(), 0), static_cast<int>(iw),
                       static_cast<int>(ih));
    std::ostringstream stream;
    stream << dangle << "  " << dpercent;
    std::string astr = stream.str();
    m_resultrects.addrect(arect, astr);
    PointsShape bmodelrect = modelrect;
    bmodelrect.Move(static_cast<int>(apoint.X()), static_cast<int>(apoint.Y()));
    m_rotateshaperesults.push_back(bmodelrect);
    m_rotatereslutpoints.push_back(apoint);
    m_rotateresults.push_back(dpercent);
    m_rotatereslutangles.push_back(dangle);
  }
}

double FastMatch::getresultnum(int inum) {
  if (inum >= 0 && inum < static_cast<int>(m_resultnums.size())) {
    return m_resultnums.at(inum);
  }
  if (inum == -1 && !m_resultnums.empty()) {
    return m_resultnums.back();
  }
  return 0.0;
}
double FastMatch::getresultcentx(int inum) {
  int iw = FastMatchPositiveInt(m_imodelwith);
  if (iw <= 0)
    iw = FastMatchPositiveInt(
        static_cast<int>(FindLine::patternboundingrectAB().Width()));
  if (inum >= 0 && inum < static_cast<int>(m_resultpoints.size())) {
    return m_resultpoints.at(inum).X() + (iw / 2);
  }
  if (inum == -1 && !m_resultpoints.empty()) {
    return m_resultpoints.back().X() + (iw / 2);
  }
  return 0.0;
}
double FastMatch::getresultcenty(int inum) {
  int ih = FastMatchPositiveInt(m_imodelheigh);
  if (ih <= 0)
    ih = FastMatchPositiveInt(
        static_cast<int>(FindLine::patternboundingrectAB().Height()));
  if (inum >= 0 && inum < static_cast<int>(m_resultpoints.size())) {
    return m_resultpoints.at(inum).Y() + (ih / 2);
  }
  if (inum == -1 && !m_resultpoints.empty()) {
    return m_resultpoints.back().Y() + (ih / 2);
  }
  return 0.0;
}

double FastMatch::getresolvedresultcentx(int inum) {
  return getresultcentx(inum);
}

double FastMatch::getresolvedresultcenty(int inum) {
  if ((inum >= 0 && inum < static_cast<int>(m_resultpoints.size())) ||
      (inum == -1 && !m_resultpoints.empty())) {
    return getresultcenty(inum);
  }
  return 0.0;
}

int FastMatch::getrotateresultcentx(int inum) {
  if (!HasRotateResultAt(inum, m_rotateresults, m_rotatereslutpoints,
                         m_rotatereslutangles, m_rotateshaperesults)) {
    return -9999;
  }
  return static_cast<int>(m_rotateshaperesults[inum].getpointscent().X());
}
int FastMatch::getrotateresultcenty(int inum) {
  if (!HasRotateResultAt(inum, m_rotateresults, m_rotatereslutpoints,
                         m_rotatereslutangles, m_rotateshaperesults)) {
    return -9999;
  }
  return static_cast<int>(m_rotateshaperesults[inum].getpointscent().Y());
}

void FastMatch::getresultcentpoints(void *apoints) {
  PointsShape *points = (PointsShape *)apoints;
  if (nullptr == points)
    return;
  points->clear();
  const int isize = static_cast<int>(
      RotateResultSharedCount(m_rotateresults, m_rotatereslutpoints,
                              m_rotatereslutangles, m_rotateshaperesults));
  for (int i = 0; i < isize; i++) {
    double dx = m_rotateshaperesults[i].getpointscent().X();
    double dy = m_rotateshaperesults[i].getpointscent().Y();

    points->addpoint(dx, dy);
  }
}
void FastMatch::getrotateresultrectpoints(std::vector<cv::Point2f> &points) {
  points.clear();
  int isize = static_cast<int>(m_rotateshaperesults.size());
  if (isize > 0) {
    for (int i = 0; i < static_cast<int>(m_rotateshaperesults[0].size()); i++) {
      double dx = m_rotateshaperesults[0].getx(i);
      double dy = m_rotateshaperesults[0].gety(i);
      points.push_back(
          cv::Point2f(static_cast<float>(dx), static_cast<float>(dy)));
    }
  }
}
double FastMatch::getmaxresult() {
  if (m_resultnums.size() > 0) {
    const int total_size =
        static_cast<int>(FindLine::getpatternpathA().ElementCount());
    if (total_size <= 0) {
      return 0.0;
    }
    return m_resultnums.at(m_resultnums.size() - 1) /
           static_cast<double>(total_size);
  }

  return 0.0;
}
int FastMatch::geteasyobjectw() { return m_easyobject.s_iwobjnum; }
int FastMatch::geteasyobjectb() { return m_easyobject.s_ibobjnum; }

int FastMatch::getmodeleasyobjectw_l72(int i) {
  return EasyObjectWidthAt(m_easyobjectmodels_l72, i);
}
int FastMatch::getmodeleasyobjectb_l72(int i) {
  return EasyObjectBlackCountAt(m_easyobjectmodels_l72, i);
}

int FastMatch::getmodeleasyobjectw_l36(int i) {
  return EasyObjectWidthAt(m_easyobjectmodels_l36, i);
}
int FastMatch::getmodeleasyobjectb_l36(int i) {
  return EasyObjectBlackCountAt(m_easyobjectmodels_l36, i);
}

int FastMatch::getmodeleasyobjectw_l12(int i) {
  return EasyObjectWidthAt(m_easyobjectmodels_l12, i);
}
int FastMatch::getmodeleasyobjectb_l12(int i) {
  return EasyObjectBlackCountAt(m_easyobjectmodels_l12, i);
}

int FastMatch::getmodeleasyobjectw_l3(int i) {
  return EasyObjectWidthAt(m_easyobjectmodels_l3, i);
}
int FastMatch::getmodeleasyobjectb_l3(int i) {
  return EasyObjectBlackCountAt(m_easyobjectmodels_l3, i);
}
int FastMatch::getmodeleasyobjectw_l6(int i) {
  return EasyObjectWidthAt(m_easyobjectmodels_l6, i);
}
int FastMatch::getmodeleasyobjectb_l6(int i) {
  return EasyObjectBlackCountAt(m_easyobjectmodels_l6, i);
}

void FastMatch::setrelationrectfromresultnum(int inum) {
  m_irelationresultnum = inum;
}
void FastMatch::setrelationrectfrom_matchresult(void *pmatch) {
  m_prelationmatch = (FastMatch *)pmatch;
  if (0 != m_prelationmatch) {
    int inum = m_prelationmatch->m_resultrects.size();
    if (m_irelationresultnum >= 0 && m_irelationresultnum < inum) {
      m_irelationrect =
          m_prelationmatch->m_resultrects.getrect(m_irelationresultnum);
    }
  }
}
void FastMatch::setrelationxy(int iprex1, int iprey1, int iendx1, int iendy1) {
  (void)iprex1;
  (void)iprey1;
  (void)iendx1;
  (void)iendy1;
  /*   m_irelationrect.setLeft(m_irelationrect.left() + iprex1);
     m_irelationrect.setTop(m_irelationrect.top() + iprey1);
     m_irelationrect.setRight(m_irelationrect.right() + iendx1);
     m_irelationrect.setBottom(m_irelationrect.bottom() + iendy1);
     */
}
void FastMatch::setrelationzoom(double drelationzoomx, double drelationzoomy) {
  (void)drelationzoomx;
  (void)drelationzoomy;
  /*    m_irelationrect.setLeft((double)m_irelationrect.left() *
     drelationzoomx); m_irelationrect.setTop((double)m_irelationrect.top() *
     drelationzoomy); m_irelationrect.setRight((double)m_irelationrect.right() *
     drelationzoomx); m_irelationrect.setBottom((double)m_irelationrect.bottom()
     * drelationzoomy);*/
}
void FastMatch::setrelationtorect() {
  if (m_irelationrect.TopLeft().X() >= 0 &&
      m_irelationrect.TopLeft().Y() >= 0 && m_irelationrect.Width() > 0 &&
      m_irelationrect.Height() > 0)
    m_matchrect = m_irelationrect;
}
void FastMatch::shapesetroi(void *pshape) {
  if (pshape == nullptr)
    return;
  Shape::shapesetroi(pshape);
}

std::vector<cv::Point2f> FastMatch::getmodel() {
  std::vector<cv::Point2f> points;
  const int count = m_modelpoints_sample1.size();
  for (int i = 0; i < count; ++i) {
    points.push_back(
        cv::Point2f(static_cast<float>(m_modelpoints_sample1.getx(i)),
                    static_cast<float>(m_modelpoints_sample1.gety(i))));
  }
  if (points.empty())
    ABtoShape(points);
  return points;
}

int FastMatch::getmodelpointcount() {
  const int sample_count = m_modelpoints_sample1.size();
  if (sample_count > 0)
    return sample_count;
  return ABpatternsize();
}

int FastMatch::getlearnacount() { return m_fastmatch_learn_a_count; }

int FastMatch::getlearnbcount() { return m_fastmatch_learn_b_count; }

int FastMatch::getlearna2count() { return m_fastmatch_learn_a2_count; }

int FastMatch::getlearnb2count() { return m_fastmatch_learn_b2_count; }

int FastMatch::getpatternapointcount() const {
  return static_cast<int>(const_cast<FastMatch *>(this)
                              ->FindLine::getpatternpathA()
                              .ElementCount());
}

int FastMatch::getpatternbpointcount() const {
  return static_cast<int>(const_cast<FastMatch *>(this)
                              ->FindLine::getpatternpathB()
                              .ElementCount());
}

double FastMatch::getpatternax() const {
  return const_cast<FastMatch *>(this)
      ->FindLine::getpatternpathA()
      .boundingRect()
      .TopLeft()
      .X();
}

double FastMatch::getpatternay() const {
  return const_cast<FastMatch *>(this)
      ->FindLine::getpatternpathA()
      .boundingRect()
      .TopLeft()
      .Y();
}

double FastMatch::getpatternawidth() const {
  return const_cast<FastMatch *>(this)
      ->FindLine::getpatternpathA()
      .boundingRect()
      .Width();
}

double FastMatch::getpatternaheight() const {
  return const_cast<FastMatch *>(this)
      ->FindLine::getpatternpathA()
      .boundingRect()
      .Height();
}

double FastMatch::getpatternbx() const {
  return const_cast<FastMatch *>(this)
      ->FindLine::getpatternpathB()
      .boundingRect()
      .TopLeft()
      .X();
}

double FastMatch::getpatternby() const {
  return const_cast<FastMatch *>(this)
      ->FindLine::getpatternpathB()
      .boundingRect()
      .TopLeft()
      .Y();
}

double FastMatch::getpatternbwidth() const {
  return const_cast<FastMatch *>(this)
      ->FindLine::getpatternpathB()
      .boundingRect()
      .Width();
}

double FastMatch::getpatternbheight() const {
  return const_cast<FastMatch *>(this)
      ->FindLine::getpatternpathB()
      .boundingRect()
      .Height();
}

void FastMatch::PublishDisplayShapes(ICxShapeSink &sink,
                                     const std::string &owner_ref) {
  const double learn_x = static_cast<double>(m_learn_roi_x);
  PublishGeometryDisplayShapes(sink, owner_ref);

  const double learn_y = static_cast<double>(m_learn_roi_y);
  const double learn_w = static_cast<double>(m_learn_roi_w);
  const double learn_h = static_cast<double>(m_learn_roi_h);

  if (learn_w > 0 && learn_h > 0) {
    auto learn_roi_shape = std::make_unique<RectShape>();
    learn_roi_shape->setRect(learn_x, learn_y, learn_x + learn_w,
                             learn_y + learn_h);
    sink.UpsertShape(owner_ref + ".learn_roi", "FastMatch", owner_ref,
                     "learn_roi", "learn_roi", true, false,
                     std::move(learn_roi_shape));
  }

  const double search_x = static_cast<double>(m_search_roi_x);
  const double search_y = static_cast<double>(m_search_roi_y);
  const double search_w = static_cast<double>(m_search_roi_w);
  const double search_h = static_cast<double>(m_search_roi_h);

  if (search_w > 0 && search_h > 0) {
    auto search_roi_shape = std::make_unique<RectShape>();
    search_roi_shape->setRect(search_x, search_y, search_x + search_w,
                              search_y + search_h);
    sink.UpsertShape(owner_ref + ".search_roi", "FastMatch", owner_ref,
                     "search_roi", "search_roi", true, false,
                     std::move(search_roi_shape));
  }

  const double expected_w = m_expected_rect.Width();
  const double expected_h = m_expected_rect.Height();
  if (expected_w > 0 && expected_h > 0) {
    const double expected_x = m_expected_rect.TopLeft().X();
    const double expected_y = m_expected_rect.TopLeft().Y();
    auto expected_shape = std::make_unique<RectShape>();
    expected_shape->setRect(expected_x, expected_y, expected_x + expected_w,
                            expected_y + expected_h);
    sink.UpsertShape(owner_ref + ".expected_gt", "FastMatch", owner_ref,
                     "expected_gt", "expected_gt", false, false,
                     std::move(expected_shape));
  }

  // FastMatch normalizes the learned A/B template to a zero-origin matcher
  // coordinate frame. Publishing getmodel() as an Image View ShapeElement
  // therefore placed local template points directly into global image space.
  // That created a second, upper-left-shifted white trace beside the actual
  // Gauge/ANN/Dijkstra evidence. The Image View's explicit Learn Result layer
  // reconstructs A/B points with m_learn_model_origin_{x,y}; it is the only
  // valid global projection and remains user-controlled by its display check.
  // Intentionally do not publish owner_ref + ".model_points" here. The runtime
  // owner generation closes this stale element on the next publish.

  const int candidate_count = getresultcandidatecount();
  if (candidate_count > 0) {
    auto candidates_shape = std::make_unique<PointsShape>();
    for (int i = 0; i < candidate_count; ++i) {
      candidates_shape->addpoint(getresolvedresultcentx(i),
                                 getresolvedresultcenty(i));
    }
    sink.UpsertShape(owner_ref + ".candidate_centers", "FastMatch", owner_ref,
                     "", "measure_points", false, true,
                     std::move(candidates_shape));
  }

  const RectsShape *result_rects = getresultrects();
  if (result_rects != nullptr && result_rects->size() > 0) {
    for (int i = 0; i < result_rects->size(); ++i) {
      const gp_Rectangle r = getresolvedresultrect(i);
      auto result_shape = std::make_unique<RectShape>();
      result_shape->setRect(r.TopLeft().X(), r.TopLeft().Y(),
                            r.BottomRight().X(), r.BottomRight().Y());
      sink.UpsertShape(owner_ref + ".result_boxes." + std::to_string(i),
                       "FastMatch", owner_ref, "result", "result", false, true,
                       std::move(result_shape));
    }
  }

  if (candidate_count > 0) {
    const int best_index = getresultbestindex();
    if (best_index >= 0 && best_index < candidate_count) {
      auto best_shape = std::make_unique<PointsShape>();
      best_shape->addpoint(getresolvedresultcentx(best_index),
                           getresolvedresultcenty(best_index));
      sink.UpsertShape(owner_ref + ".best_center", "FastMatch", owner_ref, "",
                       "best_result", false, true, std::move(best_shape));
    }
  }
}

bool FastMatch::ApplyDisplayShapeEdit(const std::string &owner_binding,
                                      const std::string &semantic_role,
                                      double x0, double y0, double x1,
                                      double y1, std::string &reason) {
  if (owner_binding == "learn_roi") {
    const double w = std::abs(x1 - x0);
    const double h = std::abs(y1 - y0);
    const double x = std::min(x0, x1);
    const double y = std::min(y0, y1);

    if (w < 2.0 || h < 2.0) {
      reason = "learn ROI too small (min 2px)";
      return false;
    }

    setrectxywh(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w),
                static_cast<int>(h));
    reason = "learn ROI updated";
    return true;
  } else if (owner_binding == "search_roi") {
    const double w = std::abs(x1 - x0);
    const double h = std::abs(y1 - y0);
    const double x = std::min(x0, x1);
    const double y = std::min(y0, y1);

    if (w < 2.0 || h < 2.0) {
      reason = "search ROI too small (min 2px)";
      return false;
    }

    setmatchrectxywh(static_cast<int>(x), static_cast<int>(y),
                     static_cast<int>(w), static_cast<int>(h));
    reason = "search ROI updated";
    return true;
  } else if (owner_binding == "result") {
    reason = "result elements are not editable";
    return false;
  }

  reason = "unknown owner_binding: " + owner_binding;
  return false;
}

void FastMatch::PublishGeometryDisplayShapes(
    ICxShapeSink &sink, const std::string &owner_ref) const {
  if (m_template_geometry.available &&
      !m_template_geometry.normalized_boundary.empty()) {
    auto boundary = std::make_unique<PolylineShape>();
    for (const cv::Point2d &normalized :
         m_template_geometry.normalized_boundary) {
      boundary->addPoint(
          m_template_geometry.bbox_px.x +
              normalized.x * m_template_geometry.bbox_px.width,
          m_template_geometry.bbox_px.y +
              normalized.y * m_template_geometry.bbox_px.height);
    }
    boundary->close(true);
    sink.UpsertShape(owner_ref + ".template_geometry.boundary", "FastMatch",
                     owner_ref, "template_geometry", "template_boundary",
                     false, false, std::move(boundary));

    if (m_template_geometry.major_axis_length > 0.0) {
      const double angle =
          m_template_geometry.orientation_deg * CV_PI / 180.0;
      const double half_length =
          0.5 * m_template_geometry.major_axis_length;
      auto major_axis = std::make_unique<PolylineShape>();
      major_axis->addPoint(
          m_template_geometry.centroid_px.x - half_length * std::cos(angle),
          m_template_geometry.centroid_px.y - half_length * std::sin(angle));
      major_axis->addPoint(
          m_template_geometry.centroid_px.x + half_length * std::cos(angle),
          m_template_geometry.centroid_px.y + half_length * std::sin(angle));
      major_axis->close(false);
      sink.UpsertShape(owner_ref + ".template_geometry.major_axis",
                       "FastMatch", owner_ref, "template_geometry",
                       "major_axis", false, false, std::move(major_axis));
    }
  }

  for (const FastMatchPoseCandidateSnapshot &pose : m_pose_candidates) {
    const std::string suffix = std::to_string(pose.candidate_index);
    if (!pose.transformed_boundary.empty()) {
      auto boundary = std::make_unique<PolylineShape>();
      for (const cv::Point2d &point : pose.transformed_boundary)
        boundary->addPoint(point.x, point.y);
      boundary->close(true);
      sink.UpsertShape(owner_ref + ".pose_boundary." + suffix, "FastMatch",
                       owner_ref, "pose_geometry", "candidate_boundary",
                       false, true, std::move(boundary));
    }

    if (m_template_geometry.major_axis_length > 0.0) {
      const double angle = pose.angle_deg * CV_PI / 180.0;
      const double half_length =
          0.5 * m_template_geometry.major_axis_length * pose.scale_x;
      auto axis = std::make_unique<PolylineShape>();
      axis->addPoint(pose.center_px.x - half_length * std::cos(angle),
                     pose.center_px.y - half_length * std::sin(angle));
      axis->addPoint(pose.center_px.x + half_length * std::cos(angle),
                     pose.center_px.y + half_length * std::sin(angle));
      axis->close(false);
      sink.UpsertShape(owner_ref + ".pose_axis." + suffix, "FastMatch",
                       owner_ref, "pose_geometry", "candidate_axis", false,
                       true, std::move(axis));
    }
  }
}
