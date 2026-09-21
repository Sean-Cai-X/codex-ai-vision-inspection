#include "pch.h"

#include "CxFastMatchShapeModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

namespace
{
constexpr double kPi = 3.14159265358979323846;

struct AffineState
{
    double a = 1.0;
    double b = 0.0;
    double c = 0.0;
    double d = 1.0;
    double tx = 0.0;
    double ty = 0.0;
};

cv::Point2d Normalize(const cv::Point2d& value,
                      const cv::Point2d& fallback = cv::Point2d(1.0, 0.0))
{
    const double length = std::hypot(value.x, value.y);
    return length > 1e-12 ? value * (1.0 / length) : fallback;
}

double GrayAt(const cv::Mat& image, double x, double y)
{
    if (image.empty() || image.depth() != CV_8U ||
        x < 0.0 || y < 0.0 || x > image.cols - 1.001 ||
        y > image.rows - 1.001)
        return std::numeric_limits<double>::quiet_NaN();
    const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, image.cols - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, image.rows - 1);
    const int x1 = std::min(x0 + 1, image.cols - 1);
    const int y1 = std::min(y0 + 1, image.rows - 1);
    const double fx = x - x0;
    const double fy = y - y0;
    const auto pixel = [&image](int px, int py) {
        const unsigned char* row = image.ptr<unsigned char>(py);
        const int channels = image.channels();
        if (channels == 1)
            return static_cast<double>(row[px]);
        const unsigned char* p = row + px * channels;
        return 0.114 * p[0] + 0.587 * p[1] + 0.299 * p[2];
    };
    const double i00 = pixel(x0, y0);
    const double i10 = pixel(x1, y0);
    const double i01 = pixel(x0, y1);
    const double i11 = pixel(x1, y1);
    return (1.0 - fy) * ((1.0 - fx) * i00 + fx * i10) +
           fy * ((1.0 - fx) * i01 + fx * i11);
}

CxFastMatchShapePoint RefineAlongNormal(
    CxFastMatchShapePoint point, const cv::Mat& image,
    const CxFastMatchFormFitConfig& config)
{
    const double halfWidth = std::max(0.25,
        config.profile_half_width_milli_px / 1000.0);
    const double step = std::max(0.05,
        config.profile_step_milli_px / 1000.0);
    const cv::Point2d normal = Normalize(
        cv::Point2d(point.normal_x, point.normal_y), cv::Point2d(0.0, 1.0));
    std::vector<double> offsets;
    std::vector<double> response;
    for (double offset = -halfWidth; offset <= halfWidth + step * 0.25;
         offset += step)
    {
        const cv::Point2d center(point.x + normal.x * offset,
                                 point.y + normal.y * offset);
        const double before = GrayAt(image, center.x - normal.x * step,
                                     center.y - normal.y * step);
        const double after = GrayAt(image, center.x + normal.x * step,
                                    center.y + normal.y * step);
        offsets.push_back(offset);
        response.push_back(std::isfinite(before) && std::isfinite(after)
            ? std::abs(after - before) : -1.0);
    }
    if (response.size() < 3)
        return point;
    const auto bestIt = std::max_element(response.begin(), response.end());
    const std::size_t best = static_cast<std::size_t>(bestIt - response.begin());
    if (*bestIt < static_cast<double>(config.profile_min_gradient))
    {
        point.confidence = std::min(point.confidence, 0.25);
        return point;
    }
    double refinedOffset = offsets[best];
    if (best > 0 && best + 1 < response.size())
    {
        const double left = response[best - 1];
        const double middle = response[best];
        const double right = response[best + 1];
        const double denominator = left - 2.0 * middle + right;
        if (std::abs(denominator) > 1e-9)
        {
            const double fraction = std::clamp(
                0.5 * (left - right) / denominator, -1.0, 1.0);
            refinedOffset += fraction * step;
        }
    }
    point.x += normal.x * refinedOffset;
    point.y += normal.y * refinedOffset;
    point.gradient_magnitude = *bestIt;
    point.confidence = std::clamp(*bestIt / 64.0, 0.05, 1.0);
    return point;
}

std::vector<cv::Point2d> ResampleClosedPolyline(
    const std::vector<cv::Point2d>& input, double step)
{
    std::vector<cv::Point2d> points;
    for (const cv::Point2d& point : input)
    {
        if (points.empty() || cv::norm(points.back() - point) > 1e-9)
            points.push_back(point);
    }
    if (points.size() > 1 && cv::norm(points.front() - points.back()) < 1e-9)
        points.pop_back();
    if (points.size() < 3)
        return {};
    std::vector<cv::Point2d> output;
    step = std::max(0.1, step);
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const cv::Point2d from = points[index];
        const cv::Point2d to = points[(index + 1) % points.size()];
        const double length = cv::norm(to - from);
        const int count = std::max(1, static_cast<int>(std::ceil(length / step)));
        for (int sample = 0; sample < count; ++sample)
        {
            const double t = static_cast<double>(sample) / count;
            const cv::Point2d point = from + (to - from) * t;
            if (output.empty() || cv::norm(output.back() - point) > 1e-9)
                output.push_back(point);
        }
    }
    return output;
}

void UpdateBounds(CxFastMatchShapeModel& model)
{
    const std::vector<CxFastMatchShapePoint>& points =
        model.dense_points.empty() ? model.sparse_points : model.dense_points;
    if (points.empty())
        return;
    double minX = points.front().x;
    double minY = points.front().y;
    double maxX = points.front().x;
    double maxY = points.front().y;
    double sumX = 0.0;
    double sumY = 0.0;
    for (const auto& point : points)
    {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
        sumX += point.x;
        sumY += point.y;
    }
    model.centroid_x = sumX / points.size();
    model.centroid_y = sumY / points.size();
    model.bbox_x = minX;
    model.bbox_y = minY;
    model.bbox_width = maxX - minX;
    model.bbox_height = maxY - minY;
}

int NearestSparsePoint(const CxFastMatchShapeModel& model,
                       const cv::Point2d& point)
{
    int best = -1;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < model.sparse_points.size(); ++index)
    {
        const auto& candidate = model.sparse_points[index];
        const double distance = std::hypot(point.x - candidate.x,
                                           point.y - candidate.y);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = static_cast<int>(index);
        }
    }
    return best;
}

void ComputeDenseFrames(CxFastMatchShapeModel& model)
{
    if (model.dense_points.size() < 3)
        return;
    const std::size_t count = model.dense_points.size();
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto& previous = model.dense_points[(index + count - 1) % count];
        const auto& next = model.dense_points[(index + 1) % count];
        CxFastMatchShapePoint& point = model.dense_points[index];
        const cv::Point2d tangent = Normalize(
            cv::Point2d(next.x - previous.x, next.y - previous.y));
        cv::Point2d normal(-tangent.y, tangent.x);
        const int nearest = NearestSparsePoint(model, cv::Point2d(point.x, point.y));
        if (nearest >= 0)
        {
            const auto& source = model.sparse_points[static_cast<std::size_t>(nearest)];
            if (normal.dot(cv::Point2d(source.normal_x, source.normal_y)) < 0.0)
                normal *= -1.0;
            point.polarity = source.polarity;
            point.source_direction = source.source_direction;
            point.source_scan = source.source_scan;
            point.source_point_index = nearest;
        }
        point.tangent_x = tangent.x;
        point.tangent_y = tangent.y;
        point.normal_x = normal.x;
        point.normal_y = normal.y;
        const cv::Point2d beforeTangent = Normalize(
            cv::Point2d(point.x - previous.x, point.y - previous.y));
        const cv::Point2d afterTangent = Normalize(
            cv::Point2d(next.x - point.x, next.y - point.y));
        point.curvature = std::acos(std::clamp(
            beforeTangent.dot(afterTangent), -1.0, 1.0));
    }
}

cv::Point2d Apply(const AffineState& state, const cv::Point2d& point)
{
    return cv::Point2d(state.a * point.x + state.b * point.y + state.tx,
                       state.c * point.x + state.d * point.y + state.ty);
}

cv::Point2d ApplyNormal(const AffineState& state, const cv::Point2d& normal)
{
    // Points transform with A; covariant normals transform with A^-T.  Using
    // A directly is only correct for rigid/uniform-scale transforms and
    // biases the normal gate under non-uniform scale or shear.
    const double determinant = state.a * state.d - state.b * state.c;
    if (std::abs(determinant) <= 1e-12)
        return Normalize(normal, cv::Point2d(1.0, 0.0));
    return Normalize(
        cv::Point2d((state.d * normal.x - state.c * normal.y) / determinant,
                    (-state.b * normal.x + state.a * normal.y) / determinant),
        normal);
}

AffineState SeedState(const CxFastMatchShapeModel& reference,
                      const FastMatchTransform& seed)
{
    const double radians = seed.angle_deg * kPi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    AffineState state;
    state.a = cosine * seed.scale_x;
    state.b = cosine * seed.shear * seed.scale_y - sine * seed.scale_y;
    state.c = sine * seed.scale_x;
    state.d = sine * seed.shear * seed.scale_y + cosine * seed.scale_y;
    state.tx = seed.cx - state.a * reference.centroid_x -
               state.b * reference.centroid_y;
    state.ty = seed.cy - state.c * reference.centroid_x -
               state.d * reference.centroid_y;
    return state;
}

bool SolveAffine(const std::vector<CxFastMatchShapePoint>& reference,
                 const std::vector<CxFastMatchShapePoint>& observed,
                 const std::vector<CxFastMatchCorrespondence>& pairs,
                 bool affine, AffineState& state)
{
    if (pairs.size() < (affine ? 3u : 2u))
        return false;
    const int variableCount = affine ? 6 : 4;
    cv::Mat normal = cv::Mat::zeros(variableCount, variableCount, CV_64F);
    cv::Mat rhs = cv::Mat::zeros(variableCount, 1, CV_64F);
    const auto accumulate = [&normal, &rhs, variableCount](
        const std::vector<double>& row, double target, double weight) {
        for (int i = 0; i < variableCount; ++i)
        {
            rhs.at<double>(i, 0) += row[static_cast<std::size_t>(i)] * target * weight;
            for (int j = 0; j < variableCount; ++j)
                normal.at<double>(i, j) += row[static_cast<std::size_t>(i)] *
                                            row[static_cast<std::size_t>(j)] * weight;
        }
    };
    for (const auto& pair : pairs)
    {
        if (!pair.accepted)
            continue;
        const auto& source = reference[static_cast<std::size_t>(pair.reference_index)];
        const auto& target = observed[static_cast<std::size_t>(pair.observed_index)];
        const double weight = std::max(1e-6, pair.weight);
        if (affine)
        {
            accumulate({source.x, source.y, 0.0, 0.0, 1.0, 0.0}, target.x, weight);
            accumulate({0.0, 0.0, source.x, source.y, 0.0, 1.0}, target.y, weight);
        }
        else
        {
            accumulate({source.x, -source.y, 1.0, 0.0}, target.x, weight);
            accumulate({source.y, source.x, 0.0, 1.0}, target.y, weight);
        }
    }
    cv::Mat solved;
    if (!cv::solve(normal, rhs, solved, cv::DECOMP_SVD))
        return false;
    if (affine)
    {
        state.a = solved.at<double>(0);
        state.b = solved.at<double>(1);
        state.c = solved.at<double>(2);
        state.d = solved.at<double>(3);
        state.tx = solved.at<double>(4);
        state.ty = solved.at<double>(5);
    }
    else
    {
        state.a = solved.at<double>(0);
        state.b = -solved.at<double>(1);
        state.c = solved.at<double>(1);
        state.d = solved.at<double>(0);
        state.tx = solved.at<double>(2);
        state.ty = solved.at<double>(3);
    }
    return std::isfinite(state.a) && std::isfinite(state.b) &&
           std::isfinite(state.c) && std::isfinite(state.d) &&
           std::isfinite(state.tx) && std::isfinite(state.ty);
}

std::vector<CxFastMatchCorrespondence> MutualPairs(
    const std::vector<CxFastMatchShapePoint>& reference,
    const std::vector<CxFastMatchShapePoint>& observed,
    const AffineState& state, const CxFastMatchFormFitConfig& config,
    int& forwardCount, int& reverseCount, int& rejectedDistance,
    int& rejectedNormal)
{
    forwardCount = reverseCount = rejectedDistance = rejectedNormal = 0;
    if (reference.empty() || observed.empty())
        return {};
    const double radius = std::max(0.25,
        config.ann_search_radius_milli_px / 1000.0);
    const double normalLimit = std::clamp(
        static_cast<double>(config.ann_normal_tolerance_deg), 1.0, 90.0);
    std::vector<int> forward(reference.size(), -1);
    std::vector<double> forwardDistance(reference.size(), radius);
    std::vector<double> forwardNormal(reference.size(), 180.0);
    std::vector<cv::Point2d> transformed(reference.size());
    std::vector<cv::Point2d> transformedNormal(reference.size());
    for (std::size_t i = 0; i < reference.size(); ++i)
    {
        transformed[i] = Apply(state, cv::Point2d(reference[i].x, reference[i].y));
        transformedNormal[i] = ApplyNormal(
            state, cv::Point2d(reference[i].normal_x, reference[i].normal_y));
        double nearest = std::numeric_limits<double>::infinity();
        int nearestIndex = -1;
        for (std::size_t j = 0; j < observed.size(); ++j)
        {
            const double distance = std::hypot(transformed[i].x - observed[j].x,
                                               transformed[i].y - observed[j].y);
            if (distance < nearest)
            {
                nearest = distance;
                nearestIndex = static_cast<int>(j);
            }
        }
        if (nearestIndex < 0 || nearest > radius)
        {
            ++rejectedDistance;
            continue;
        }
        const auto& candidate = observed[static_cast<std::size_t>(nearestIndex)];
        const double normalDelta = std::acos(std::clamp(
            transformedNormal[i].dot(Normalize(
                cv::Point2d(candidate.normal_x, candidate.normal_y))), -1.0, 1.0)) *
            180.0 / kPi;
        if (normalDelta > normalLimit)
        {
            ++rejectedNormal;
            continue;
        }
        forward[i] = nearestIndex;
        forwardDistance[i] = nearest;
        forwardNormal[i] = normalDelta;
        ++forwardCount;
    }
    std::vector<int> reverse(observed.size(), -1);
    for (std::size_t j = 0; j < observed.size(); ++j)
    {
        double nearest = radius;
        for (std::size_t i = 0; i < transformed.size(); ++i)
        {
            const double distance = std::hypot(transformed[i].x - observed[j].x,
                                               transformed[i].y - observed[j].y);
            if (distance <= nearest)
            {
                nearest = distance;
                reverse[j] = static_cast<int>(i);
            }
        }
        if (reverse[j] >= 0)
            ++reverseCount;
    }
    std::vector<CxFastMatchCorrespondence> pairs;
    for (std::size_t i = 0; i < forward.size(); ++i)
    {
        const int j = forward[i];
        if (j < 0 || reverse[static_cast<std::size_t>(j)] != static_cast<int>(i))
            continue;
        CxFastMatchCorrespondence pair;
        pair.reference_index = static_cast<int>(i);
        pair.observed_index = j;
        pair.distance_px = forwardDistance[i];
        pair.normal_delta_deg = forwardNormal[i];
        pair.weight = std::max(0.01,
            std::min(reference[i].confidence,
                     observed[static_cast<std::size_t>(j)].confidence)) /
            (1.0 + pair.distance_px);
        pair.mutual = true;
        pair.accepted = true;
        pairs.push_back(pair);
    }
    if (pairs.size() > static_cast<std::size_t>(config.minimum_mutual_pairs))
    {
        std::vector<double> distances;
        for (const auto& pair : pairs)
            distances.push_back(pair.distance_px);
        std::sort(distances.begin(), distances.end());
        const int retainPercent = 100 - std::clamp(config.ann_trim_percent, 0, 80);
        const std::size_t retain = std::max<std::size_t>(
            static_cast<std::size_t>(config.minimum_mutual_pairs),
            (distances.size() * retainPercent + 99) / 100);
        const double threshold = distances[std::min(retain, distances.size()) - 1];
        for (auto& pair : pairs)
            pair.accepted = pair.distance_px <= threshold + 1e-12;
    }
    return pairs;
}

bool TimeExceeded(const std::chrono::steady_clock::time_point& start,
                  int maximumMilliseconds)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() >=
           std::max(1, maximumMilliseconds);
}
}

CxFastMatchShapeModel BuildFastMatchReferenceShapeModel(
    const std::vector<CxFastMatchSourceObservation>& sourcePoints,
    const std::vector<cv::Point2d>& derivedClosedTrace,
    const std::vector<cv::Point2d>& junctionPoints,
    const cv::Mat& image, const CxFastMatchFormFitConfig& config,
    const std::string& modelId)
{
    CxFastMatchShapeModel model;
    model.model_id = modelId;
    model.source_conclusion_count = static_cast<int>(sourcePoints.size());
    for (std::size_t index = 0; index < sourcePoints.size(); ++index)
    {
        const auto& source = sourcePoints[index];
        CxFastMatchShapePoint point;
        point.x = source.x;
        point.y = source.y;
        const cv::Point2d normal = Normalize(
            cv::Point2d(source.normal_x, source.normal_y), cv::Point2d(0.0, 1.0));
        point.normal_x = normal.x;
        point.normal_y = normal.y;
        point.tangent_x = -normal.y;
        point.tangent_y = normal.x;
        point.polarity = source.polarity;
        point.source_direction = source.source_direction;
        point.source_scan = source.source_scan;
        point.source_point_index = static_cast<int>(index);
        point.confidence = 1.0;
        point.derived = false;
        model.sparse_points.push_back(point);
    }
    const double denseStep = std::max(0.1,
        config.dense_sample_step_milli_px / 1000.0);
    for (const cv::Point2d& position :
         ResampleClosedPolyline(derivedClosedTrace, denseStep))
    {
        CxFastMatchShapePoint point;
        point.x = position.x;
        point.y = position.y;
        point.confidence = 0.5;
        point.derived = true;
        model.dense_points.push_back(point);
    }
    ComputeDenseFrames(model);
    for (auto& point : model.dense_points)
    {
        const double beforeX = point.x;
        const double beforeY = point.y;
        point = RefineAlongNormal(point, image, config);
        if (std::abs(point.x - std::round(point.x)) > 1e-6 ||
            std::abs(point.y - std::round(point.y)) > 1e-6 ||
            std::hypot(point.x - beforeX, point.y - beforeY) > 1e-6)
            ++model.subpixel_point_count;
    }
    ComputeDenseFrames(model);
    model.derived_point_count = static_cast<int>(model.dense_points.size());
    model.closed = model.dense_points.size() >= 3;
    model.dense_available = model.closed;
    UpdateBounds(model);

    for (const cv::Point2d& junction : junctionPoints)
    {
        int nearest = -1;
        double distance = std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; index < model.dense_points.size(); ++index)
        {
            const auto& point = model.dense_points[index];
            const double candidate = std::hypot(point.x - junction.x,
                                                point.y - junction.y);
            if (candidate < distance)
            {
                distance = candidate;
                nearest = static_cast<int>(index);
            }
        }
        CxFastMatchStructuralAnchor anchor;
        anchor.type = CxFastMatchStructuralAnchorType::DirectionJunction;
        anchor.point_index = nearest;
        anchor.x = junction.x;
        anchor.y = junction.y;
        anchor.strength = 1.0;
        anchor.source = "normal_trace_derived_junction";
        model.anchors.push_back(anchor);
    }
    const double curvatureThreshold =
        config.curvature_anchor_threshold_millideg / 1000.0 * kPi / 180.0;
    for (std::size_t index = 1;
         index + 1 < model.dense_points.size() &&
         model.anchors.size() < static_cast<std::size_t>(
             std::max(4, config.maximum_structural_anchors)); ++index)
    {
        const double curvature = model.dense_points[index].curvature;
        if (curvature < curvatureThreshold ||
            curvature < model.dense_points[index - 1].curvature ||
            curvature < model.dense_points[index + 1].curvature)
            continue;
        bool separated = true;
        for (const auto& anchor : model.anchors)
            if (std::hypot(anchor.x - model.dense_points[index].x,
                           anchor.y - model.dense_points[index].y) < denseStep * 4.0)
                separated = false;
        if (!separated)
            continue;
        CxFastMatchStructuralAnchor anchor;
        anchor.type = CxFastMatchStructuralAnchorType::CurvatureExtremum;
        anchor.point_index = static_cast<int>(index);
        anchor.x = model.dense_points[index].x;
        anchor.y = model.dense_points[index].y;
        anchor.strength = std::min(1.0, curvature / kPi);
        anchor.source = "dense_contour_curvature";
        model.anchors.push_back(anchor);
    }
    model.available = !model.sparse_points.empty() && model.dense_available;
    model.status = model.available ? "SHAPE_MODEL_READY" :
                                   "SHAPE_MODEL_INPUT_INSUFFICIENT";
    return model;
}

CxFastMatchShapeModel BuildFastMatchObservedShapeModel(
    const CxFastMatchShapeModel& reference, const cv::Mat& image,
    const FastMatchTransform& seed, const CxFastMatchFormFitConfig& config,
    const std::string& modelId)
{
    CxFastMatchShapeModel observed = reference;
    observed.model_id = modelId;
    observed.status = "OBSERVED_MODEL_BUILDING";
    observed.subpixel_point_count = 0;
    const auto transformPoint = [&reference, &seed](CxFastMatchShapePoint point,
                                                     bool refine,
                                                     const cv::Mat& sourceImage,
                                                     const CxFastMatchFormFitConfig& cfg) {
        double x = 0.0;
        double y = 0.0;
        seed.localToGlobal(point.x - reference.centroid_x,
                           point.y - reference.centroid_y, x, y);
        double nx = 0.0;
        double ny = 0.0;
        seed.localToGlobal(point.x - reference.centroid_x + point.normal_x,
                           point.y - reference.centroid_y + point.normal_y,
                           nx, ny);
        point.x = x;
        point.y = y;
        const cv::Point2d normal = Normalize(cv::Point2d(nx - x, ny - y));
        point.normal_x = normal.x;
        point.normal_y = normal.y;
        point.tangent_x = -normal.y;
        point.tangent_y = normal.x;
        point.derived = true;
        return refine ? RefineAlongNormal(point, sourceImage, cfg) : point;
    };
    for (auto& point : observed.sparse_points)
        point = transformPoint(point, true, image, config);
    for (auto& point : observed.dense_points)
    {
        point = transformPoint(point, true, image, config);
        if (std::abs(point.x - std::round(point.x)) > 1e-6 ||
            std::abs(point.y - std::round(point.y)) > 1e-6)
            ++observed.subpixel_point_count;
    }
    for (auto& anchor : observed.anchors)
    {
        double x = 0.0;
        double y = 0.0;
        seed.localToGlobal(anchor.x - reference.centroid_x,
                           anchor.y - reference.centroid_y, x, y);
        anchor.x = x;
        anchor.y = y;
    }
    ComputeDenseFrames(observed);
    UpdateBounds(observed);
    observed.available = reference.available && !image.empty();
    observed.status = observed.available ? "OBSERVED_MODEL_READY" :
                                           "OBSERVED_MODEL_INPUT_INSUFFICIENT";
    return observed;
}

CxFastMatchFormFitResult RunFastMatchBidirectionalFormFit(
    const CxFastMatchShapeModel& reference,
    const CxFastMatchShapeModel& observed,
    const FastMatchTransform& seed,
    const CxFastMatchFormFitConfig& config)
{
    CxFastMatchFormFitResult result;
    result.executed = true;
    result.reference_model_id = reference.model_id;
    result.observed_model_id = observed.model_id;
    result.reference_sparse_count = static_cast<int>(reference.sparse_points.size());
    result.observed_sparse_count = static_cast<int>(observed.sparse_points.size());
    result.reference_dense_count = static_cast<int>(reference.dense_points.size());
    result.observed_dense_count = static_cast<int>(observed.dense_points.size());
    result.structural_anchor_count = static_cast<int>(reference.anchors.size());
    if (!config.enabled)
    {
        result.status = "FORM_FIT_DISABLED";
        result.failure_stage = "disabled";
        return result;
    }
    if (!reference.available || !observed.available)
    {
        result.status = "FORM_FIT_MODEL_UNAVAILABLE";
        result.failure_stage = "shape_model_unavailable";
        return result;
    }
    const auto start = std::chrono::steady_clock::now();
    AffineState state = SeedState(reference, seed);
    int forward = 0;
    int reverse = 0;
    int rejectedDistance = 0;
    int rejectedNormal = 0;
    auto sparsePairs = MutualPairs(reference.sparse_points, observed.sparse_points,
                                   state, config, forward, reverse,
                                   rejectedDistance, rejectedNormal);
    result.sparse_mutual_count = static_cast<int>(std::count_if(
        sparsePairs.begin(), sparsePairs.end(),
        [](const auto& pair) { return pair.accepted; }));
    if (result.sparse_mutual_count >= config.minimum_mutual_pairs)
        SolveAffine(reference.sparse_points, observed.sparse_points, sparsePairs,
                    config.allow_nonuniform_affine, state);

    std::vector<CxFastMatchCorrespondence> pairs;
    for (int iteration = 0; iteration < std::max(1, config.maximum_iterations);
         ++iteration)
    {
        if (TimeExceeded(start, config.maximum_elapsed_ms))
        {
            result.budget_exceeded = true;
            break;
        }
        const AffineState before = state;
        pairs = MutualPairs(reference.dense_points, observed.dense_points,
                            state, config, forward, reverse,
                            rejectedDistance, rejectedNormal);
        const int accepted = static_cast<int>(std::count_if(
            pairs.begin(), pairs.end(),
            [](const auto& pair) { return pair.accepted; }));
        if (accepted < config.minimum_mutual_pairs ||
            !SolveAffine(reference.dense_points, observed.dense_points, pairs,
                         config.allow_nonuniform_affine, state))
            break;
        result.iterations = iteration + 1;
        const double delta = std::max({std::abs(state.a - before.a),
            std::abs(state.b - before.b), std::abs(state.c - before.c),
            std::abs(state.d - before.d), std::abs(state.tx - before.tx),
            std::abs(state.ty - before.ty)});
        if (delta < 1e-6)
            break;
    }
    pairs = MutualPairs(reference.dense_points, observed.dense_points,
                        state, config, forward, reverse,
                        rejectedDistance, rejectedNormal);
    result.correspondences = pairs;
    result.dense_forward_count = forward;
    result.dense_reverse_count = reverse;
    result.dense_mutual_count = static_cast<int>(std::count_if(
        pairs.begin(), pairs.end(),
        [](const auto& pair) { return pair.accepted; }));
    result.rejected_distance_count = rejectedDistance;
    result.rejected_normal_count = rejectedNormal;
    result.forward_coverage = reference.dense_points.empty() ? 0.0 :
        static_cast<double>(forward) / reference.dense_points.size();
    result.reverse_coverage = observed.dense_points.empty() ? 0.0 :
        static_cast<double>(reverse) / observed.dense_points.size();
    result.mutual_coverage = std::max(reference.dense_points.size(),
                                     observed.dense_points.size()) == 0 ? 0.0 :
        static_cast<double>(result.dense_mutual_count) /
        std::max(reference.dense_points.size(), observed.dense_points.size());
    double residualSum = 0.0;
    double normalSum = 0.0;
    result.max_residual_px = 0.0;
    for (const auto& pair : pairs)
    {
        if (!pair.accepted)
            continue;
        residualSum += pair.distance_px;
        normalSum += pair.normal_delta_deg;
        result.max_residual_px = std::max(result.max_residual_px,
                                          pair.distance_px);
    }
    if (result.dense_mutual_count > 0)
    {
        result.mean_residual_px = residualSum / result.dense_mutual_count;
        result.normal_residual_deg = normalSum / result.dense_mutual_count;
        result.symmetric_residual_px = result.mean_residual_px *
            (2.0 - 0.5 * (result.forward_coverage + result.reverse_coverage));
    }
    result.affine_a = state.a;
    result.affine_b = state.b;
    result.affine_c = state.c;
    result.affine_d = state.d;
    result.translate_x = state.tx;
    result.translate_y = state.ty;
    result.angle_deg = std::atan2(state.c, state.a) * 180.0 / kPi;
    result.scale_x = std::hypot(state.a, state.c);
    result.scale_y = std::hypot(state.b, state.d);
    const double radius = std::max(0.25,
        config.ann_search_radius_milli_px / 1000.0);
    result.score = result.dense_mutual_count > 0 ?
        std::clamp(0.5 * (result.forward_coverage + result.reverse_coverage) *
                   std::exp(-std::max(0.0, result.symmetric_residual_px) / radius) *
                   std::exp(-std::max(0.0, result.normal_residual_deg) / 90.0),
                   0.0, 1.0) : 0.0;
    result.elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count());
    result.succeeded = !result.budget_exceeded &&
        result.dense_mutual_count >= config.minimum_mutual_pairs &&
        result.forward_coverage >= 0.25 && result.reverse_coverage >= 0.25;
    result.status = result.succeeded ? "FORM_FIT_COMPLETE" :
        (result.budget_exceeded ? "FORM_FIT_BUDGET_EXCEEDED" :
                                  "FORM_FIT_CORRESPONDENCE_INSUFFICIENT");
    result.failure_stage = result.succeeded ? "complete" :
        (result.budget_exceeded ? "algorithm_budget_exceeded" :
                                  "bidirectional_ann_correspondence");
    return result;
}

const char* CxFastMatchStructuralAnchorTypeName(
    CxFastMatchStructuralAnchorType type)
{
    switch (type)
    {
    case CxFastMatchStructuralAnchorType::DirectionJunction:
        return "direction_junction";
    case CxFastMatchStructuralAnchorType::CurvatureExtremum:
        return "curvature_extremum";
    case CxFastMatchStructuralAnchorType::ContourExtremum:
        return "contour_extremum";
    case CxFastMatchStructuralAnchorType::SymmetryPair:
        return "symmetry_pair";
    case CxFastMatchStructuralAnchorType::OperatorConfirmed:
        return "operator_confirmed";
    case CxFastMatchStructuralAnchorType::Unknown:
    default:
        return "unknown";
    }
}
