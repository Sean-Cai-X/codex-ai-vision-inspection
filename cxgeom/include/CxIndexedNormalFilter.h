#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace cxgeom {
// Indices identify entries in this invocation, never coordinate identities.
struct NormalFilterPoint { double x; double y; };
enum class NormalFilterDecision { Kept, InvalidPoint, NormalDistance };
struct NormalFilterEntry {
    std::size_t input_slot;
    int output_slot;
    NormalFilterDecision decision;
};
struct IndexedNormalFilterResult {
    bool evaluated = false;
    double median_normal = 0.0;
    std::vector<NormalFilterEntry> entries;
    int removed = 0;
};
inline bool IsNormalFilterPointValid(const NormalFilterPoint& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && p.x >= -90000.0 && p.y >= -90000.0;
}
// Preserves FindLine's existing upper-median and inclusive threshold semantics.
inline IndexedNormalFilterResult FilterIndexedNormalPoints(
    const std::vector<NormalFilterPoint>& points, double nx, double ny, double range) {
    IndexedNormalFilterResult result;
    if (points.size() < 3 || range <= 0.0) return result;
    std::vector<double> offsets;
    offsets.reserve(points.size());
    for (const auto& p : points)
        if (IsNormalFilterPointValid(p)) offsets.push_back(p.x * nx + p.y * ny);
    if (offsets.size() < 3) return result;
    std::nth_element(offsets.begin(), offsets.begin() + offsets.size() / 2, offsets.end());
    result.evaluated = true;
    result.median_normal = offsets[offsets.size() / 2];
    result.entries.reserve(points.size());
    int next = 0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        auto decision = NormalFilterDecision::InvalidPoint;
        if (IsNormalFilterPointValid(p))
            decision = std::abs(p.x * nx + p.y * ny - result.median_normal) <= range
                ? NormalFilterDecision::Kept : NormalFilterDecision::NormalDistance;
        const bool keep = decision == NormalFilterDecision::Kept;
        result.entries.push_back({i, keep ? next++ : -1, decision});
        if (!keep) ++result.removed;
    }
    return result;
}
} // namespace cxgeom
