#ifndef OCC_SEMANTIC_GEOMETRY_H
#define OCC_SEMANTIC_GEOMETRY_H

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "../cximage_v1/gp_path.h"

struct GeometryContext {
  int image_w = 0;
  int image_h = 0;
  int roi_x = 0;
  int roi_y = 0;
  int roi_w = 0;
  int roi_h = 0;
  int state_id = 0;
  int camera_id = 0;
  float template_score = 0.0f;
  float confidence_prior = 0.0f;
};

struct SemanticGeometryVector {
  std::vector<float> values;
  std::string source_id;
  std::string shape_type;
};

class ISemanticGeometryExtractor {
public:
  virtual ~ISemanticGeometryExtractor() = default;

  virtual SemanticGeometryVector
  build(const gp_Path &path, const GeometryContext &ctx,
        const std::string &source_id = "",
        const std::string &shape_type = "unknown") const = 0;
};

namespace occ_semantic_geometry {

inline float safe_div(float a, float b) {
  return std::fabs(b) < 1e-6f ? 0.0f : a / b;
}

inline float clamp01(float x) { return std::max(0.0f, std::min(1.0f, x)); }

inline std::vector<gp_Pnt> sample_path_uniform(const gp_Path &path, int n) {
  std::vector<gp_Pnt> out;
  out.reserve(n);

  const int count = static_cast<int>(path.ElementCount());
  if (count <= 0 || n <= 0) {
    return out;
  }
  if (count == 1) {
    out.assign(n, path.ElementAt(0));
    return out;
  }

  for (int i = 0; i < n; ++i) {
    const double t =
        (n == 1) ? 0.0 : static_cast<double>(i) / static_cast<double>(n - 1);
    out.push_back(path.PointAtPercent(t));
  }
  return out;
}

inline float estimate_path_length(const gp_Path &path) {
  float total = 0.0f;
  const int count = static_cast<int>(path.ElementCount());
  for (int i = 0; i + 1 < count; ++i) {
    gp_Vec v(path.ElementAt(i), path.ElementAt(i + 1));
    total += static_cast<float>(v.Magnitude());
  }
  return total;
}

inline std::vector<float> empty_descriptor(size_t dim = 96) {
  return std::vector<float>(dim, 0.0f);
}

} // namespace occ_semantic_geometry

class OccSemanticGeometryExtractor final : public ISemanticGeometryExtractor {
public:
  static constexpr size_t kDefaultDim = 96;

  SemanticGeometryVector
  build(const gp_Path &path, const GeometryContext &ctx,
        const std::string &source_id = "",
        const std::string &shape_type = "unknown") const override {

    SemanticGeometryVector out;
    out.values = occ_semantic_geometry::empty_descriptor(kDefaultDim);
    out.source_id = source_id;
    out.shape_type = shape_type;

    const int count = static_cast<int>(path.ElementCount());
    if (count <= 0) {
      return out;
    }

    const gp_Rectangle rect = path.boundingRect();
    const gp_Pnt center = path.centroid();
    const gp_Pnt first = path.ElementAt(0);
    const gp_Pnt last = path.ElementAt(count - 1);

    const float img_w = static_cast<float>(std::max(1, ctx.image_w));
    const float img_h = static_cast<float>(std::max(1, ctx.image_h));
    const float box_w = static_cast<float>(std::fabs(rect.Width()));
    const float box_h = static_cast<float>(std::fabs(rect.Height()));
    const float box_area = box_w * box_h;
    const float path_len = occ_semantic_geometry::estimate_path_length(path);
    const float start_end_dist = static_cast<float>(first.Distance(last));
    const float closed_flag =
        start_end_dist <= std::max(2.0f, path_len * 0.02f) ? 1.0f : 0.0f;

    std::vector<float> segment_lengths;
    segment_lengths.reserve(std::max(0, count - 1));
    for (int i = 0; i + 1 < count; ++i) {
      gp_Vec v(path.ElementAt(i), path.ElementAt(i + 1));
      segment_lengths.push_back(static_cast<float>(v.Magnitude()));
    }

    float mean_seg = 0.0f;
    float std_seg = 0.0f;
    float max_seg = 0.0f;
    float min_seg = segment_lengths.empty() ? 0.0f : segment_lengths.front();
    for (float x : segment_lengths) {
      mean_seg += x;
      max_seg = std::max(max_seg, x);
      min_seg = std::min(min_seg, x);
    }
    if (!segment_lengths.empty()) {
      mean_seg /= static_cast<float>(segment_lengths.size());
      for (float x : segment_lengths) {
        const float d = x - mean_seg;
        std_seg += d * d;
      }
      std_seg = std::sqrt(std_seg / static_cast<float>(segment_lengths.size()));
    }

    const float center_x = static_cast<float>(center.X());
    const float center_y = static_cast<float>(center.Y());
    const float aspect_ratio =
        occ_semantic_geometry::safe_div(box_w, std::max(box_h, 1e-6f));
    const float circularity = occ_semantic_geometry::safe_div(
        4.0f * 3.1415926f * box_area, std::max(path_len * path_len, 1e-6f));

    out.values[0] =
        occ_semantic_geometry::clamp01(static_cast<float>(count) / 512.0f);
    out.values[1] =
        occ_semantic_geometry::safe_div(path_len, std::max(img_w, img_h));
    out.values[2] = occ_semantic_geometry::safe_div(box_w, img_w);
    out.values[3] = occ_semantic_geometry::safe_div(box_h, img_h);
    out.values[4] = aspect_ratio;
    out.values[5] = occ_semantic_geometry::safe_div(box_area, img_w * img_h);
    out.values[6] =
        occ_semantic_geometry::safe_div(path_len, std::max(box_area, 1e-6f));
    out.values[7] = occ_semantic_geometry::safe_div(center_x, img_w);
    out.values[8] = occ_semantic_geometry::safe_div(center_y, img_h);
    out.values[9] = out.values[7];
    out.values[10] = out.values[8];
    out.values[11] =
        occ_semantic_geometry::safe_div(start_end_dist, std::max(img_w, img_h));
    out.values[12] = closed_flag;
    out.values[13] =
        occ_semantic_geometry::safe_div(mean_seg, std::max(img_w, img_h));
    out.values[14] =
        occ_semantic_geometry::safe_div(std_seg, std::max(img_w, img_h));
    out.values[15] =
        occ_semantic_geometry::safe_div(max_seg, std::max(img_w, img_h));
    out.values[16] =
        occ_semantic_geometry::safe_div(min_seg, std::max(img_w, img_h));
    out.values[17] = std::max(out.values[2], out.values[3]);
    out.values[18] = std::min(out.values[2], out.values[3]);
    out.values[19] = occ_semantic_geometry::safe_div(
        out.values[18], std::max(out.values[17], 1e-6f));

    const float orientation =
        std::atan2(static_cast<float>(last.Y() - first.Y()),
                   static_cast<float>(last.X() - first.X()));
    out.values[20] = std::sin(orientation);
    out.values[21] = std::cos(orientation);
    out.values[22] = 1.0f;
    out.values[23] = occ_semantic_geometry::clamp01(circularity);

    const auto sampled = occ_semantic_geometry::sample_path_uniform(path, 32);
    std::vector<float> turn_hist(8, 0.0f);
    std::vector<float> curv_hist(8, 0.0f);
    int corner_count = 0;

    for (int i = 1; i + 1 < static_cast<int>(sampled.size()); ++i) {
      gp_Vec a(sampled[i - 1], sampled[i]);
      gp_Vec b(sampled[i], sampled[i + 1]);
      const double len_a = a.Magnitude();
      const double len_b = b.Magnitude();
      if (len_a < 1e-6 || len_b < 1e-6) {
        continue;
      }

      double cos_theta = a.Dot(b) / (len_a * len_b);
      cos_theta = std::max(-1.0, std::min(1.0, cos_theta));
      const float angle = static_cast<float>(std::acos(cos_theta));
      const int turn_bin =
          std::min(7, static_cast<int>(angle / 3.1415926f * 8.0f));
      turn_hist[turn_bin] += 1.0f;

      const float curvature =
          angle / static_cast<float>(std::max((len_a + len_b) * 0.5, 1e-6));
      const int curv_bin =
          std::min(7, static_cast<int>(std::min(curvature, 1.0f) * 8.0f));
      curv_hist[curv_bin] += 1.0f;

      if (angle > 0.6f) {
        corner_count++;
      }
    }

    float turn_sum = 0.0f;
    float curv_sum = 0.0f;
    for (float x : turn_hist)
      turn_sum += x;
    for (float x : curv_hist)
      curv_sum += x;

    if (turn_sum > 0.0f) {
      for (int i = 0; i < 8; ++i) {
        out.values[24 + i] = turn_hist[i] / turn_sum;
      }
    }
    if (curv_sum > 0.0f) {
      for (int i = 0; i < 8; ++i) {
        out.values[32 + i] = curv_hist[i] / curv_sum;
      }
    }

    out.values[40] = occ_semantic_geometry::clamp01(
        static_cast<float>(corner_count) / 16.0f);
    out.values[42] = (aspect_ratio > 5.0f || aspect_ratio < 0.2f) ? 1.0f : 0.0f;
    out.values[43] =
        closed_flag > 0.5f ? occ_semantic_geometry::clamp01(circularity) : 0.0f;
    out.values[44] = closed_flag > 0.5f
                         ? occ_semantic_geometry::clamp01(
                               1.0f - std::fabs(out.values[19] - 0.7f))
                         : 0.0f;
    out.values[45] = closed_flag > 0.5f
                         ? occ_semantic_geometry::clamp01(
                               1.0f - std::fabs(aspect_ratio - 1.0f))
                         : 0.0f;

    out.values[80] =
        occ_semantic_geometry::safe_div(static_cast<float>(ctx.roi_x), img_w);
    out.values[81] =
        occ_semantic_geometry::safe_div(static_cast<float>(ctx.roi_y), img_h);
    out.values[82] =
        occ_semantic_geometry::safe_div(static_cast<float>(ctx.roi_w), img_w);
    out.values[83] =
        occ_semantic_geometry::safe_div(static_cast<float>(ctx.roi_h), img_h);
    out.values[84] =
        occ_semantic_geometry::safe_div(center_x - img_w * 0.5f, img_w);
    out.values[85] =
        occ_semantic_geometry::safe_div(center_y - img_h * 0.5f, img_h);
    out.values[86] = occ_semantic_geometry::clamp01(
        static_cast<float>(ctx.state_id) / 32.0f);
    out.values[87] = occ_semantic_geometry::clamp01(
        static_cast<float>(ctx.camera_id) / 16.0f);
    out.values[88] = occ_semantic_geometry::clamp01(ctx.template_score);
    out.values[93] = occ_semantic_geometry::clamp01(ctx.confidence_prior);

    return out;
  }
};

#endif
