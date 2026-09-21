#include "pch.h"

#include "FormfitGaugeBuilder.h"

#include "CxCoreBoundary.h"

#include <algorithm>
#include <cmath>

namespace cxcore
{
namespace formfit
{

namespace
{
double FiniteOr(double value, double fallback)
{
    return std::isfinite(value) ? value : fallback;
}

OutputRect NormalizeRect(OutputRect rect)
{
    rect.x = FiniteOr(rect.x, 0.0);
    rect.y = FiniteOr(rect.y, 0.0);
    rect.width = FiniteOr(rect.width, 0.0);
    rect.height = FiniteOr(rect.height, 0.0);
    if (rect.width < 0.0)
    {
        rect.x += rect.width;
        rect.width = -rect.width;
    }
    if (rect.height < 0.0)
    {
        rect.y += rect.height;
        rect.height = -rect.height;
    }
    return rect;
}

bool HasPositiveFiniteBounds(const OutputRect& rect)
{
    return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.width) &&
        std::isfinite(rect.height) && rect.width > 0.0 && rect.height > 0.0;
}

double ClampPositiveScore(double score)
{
    return std::isfinite(score) && score > 0.0 ? score : 0.0;
}

int ClampCandidateLimit(int max_candidates)
{
    return std::max(0, max_candidates);
}
}

OutputRect GaugeBuilder::MakeRectOutput(const FindRect& rect, int index)
{
    OutputRect output;
    const gp_Rectangle result = rect.getresultrect(std::max(0, index));
    output.x = result.TopLeft().X();
    output.y = result.TopLeft().Y();
    output.width = result.Width();
    output.height = result.Height();
    return NormalizeRect(output);
}

LineMeasurementOutput GaugeBuilder::MakeLineOutput(FindLine& line)
{
    return ExportLineMeasurement(line);
}

CircleMeasurementOutput GaugeBuilder::MakeCircleOutput(FindCircle& circle)
{
    return ExportCircleMeasurement(circle);
}

MatchOutput GaugeBuilder::MakeMatchOutput(FastMatch& matcher, int max_candidates)
{
    MatchOutput output = ExportMatchOutput(matcher, ClampCandidateLimit(max_candidates));
    std::vector<MatchCandidateOutput> filtered;
    filtered.reserve(output.candidates.size());
    for (MatchCandidateOutput candidate : output.candidates)
    {
        candidate.bounds = NormalizeRect(candidate.bounds);
        candidate.center.x = FiniteOr(candidate.center.x, candidate.bounds.x + candidate.bounds.width * 0.5);
        candidate.center.y = FiniteOr(candidate.center.y, candidate.bounds.y + candidate.bounds.height * 0.5);
        candidate.score = ClampPositiveScore(candidate.score);
        if (HasPositiveFiniteBounds(candidate.bounds) && candidate.score > 0.0)
        {
            filtered.push_back(candidate);
        }
    }
    output.candidates = std::move(filtered);
    return output;
}

FitOperationConfig GaugeBuilder::MakeRectCircleCompositeConfig()
{
    return MakeRectCircleCompositeOperationConfig();
}

FitOperationConfig GaugeBuilder::MakeRectCircleLineCompositeConfig()
{
    return MakeRectCircleLineCompositeOperationConfig();
}

FitOperationConfig GaugeBuilder::MakeRectCircleLineMatchCompositeConfig()
{
    return MakeRectCircleLineMatchCompositeOperationConfig();
}

FitOperationConfig GaugeBuilder::MakeCircleRingCompositeConfig()
{
    return MakeCircleRingCompositeOperationConfig();
}
`n
PrototypeRunResult GaugeBuilder::BuildAndRunRectCirclePrototype(const FindRect& rect,
                                                                FindCircle& circle,
                                                                const FitOperationConfig& operation_config,
                                                                int rect_index)
{
    const OutputRect rect_output = MakeRectOutput(rect, rect_index);
    CircleMeasurementOutput circle_output = MakeCircleOutput(circle);
    return RunRectCirclePrototype(rect_output, circle_output, operation_config);
}

PrototypeRunResult GaugeBuilder::BuildAndRunRectCircleLinePrototype(const FindRect& rect,
                                                                    FindLine& line,
                                                                    FindCircle& circle,
                                                                    const FitOperationConfig& operation_config,
                                                                    int rect_index)
{
    const OutputRect rect_output = MakeRectOutput(rect, rect_index);
    LineMeasurementOutput line_output = MakeLineOutput(line);
    CircleMeasurementOutput circle_output = MakeCircleOutput(circle);
    return RunRectCircleLinePrototype(rect_output, circle_output, line_output, operation_config);
}

PrototypeRunResult GaugeBuilder::BuildAndRunRectCircleLineMatchPrototype(const FindRect& rect,
                                                                         FindLine& line,
                                                                         FindCircle& circle,
                                                                         FastMatch& matcher,
                                                                         const FitOperationConfig& operation_config,
                                                                         int rect_index,
                                                                         int max_candidates)
{
    const OutputRect rect_output = MakeRectOutput(rect, rect_index);
    LineMeasurementOutput line_output = MakeLineOutput(line);
    CircleMeasurementOutput circle_output = MakeCircleOutput(circle);
    MatchOutput match_output = MakeMatchOutput(matcher, max_candidates);
    return RunRectCircleLineMatchPrototype(rect_output, circle_output, line_output, match_output, operation_config);
}

PrototypeRunResult GaugeBuilder::BuildAndRunCircleRingPrototype(FindCircle& outer_circle,
                                                               FindCircle& inner_circle,
                                                               const FitOperationConfig& operation_config,
                                                               double center_tolerance,
                                                               double thickness_tolerance)
{
    CircleMeasurementOutput outer_output = MakeCircleOutput(outer_circle);
    CircleMeasurementOutput inner_output = MakeCircleOutput(inner_circle);

    return RunCircleRingPrototype(
        outer_output,
        inner_output,
        operation_config,
        center_tolerance,
        thickness_tolerance);
}

PrototypeRunResult GaugeBuilder::BuildAndRunCircleRingLinePrototype(FindCircle& outer_circle,
                                                                   FindCircle& inner_circle,
                                                                   FindLine& line,
                                                                   const FitOperationConfig& operation_config,
                                                                   double center_tolerance,
                                                                   double thickness_tolerance)
{
    CircleMeasurementOutput outer_output = MakeCircleOutput(outer_circle);
    CircleMeasurementOutput inner_output = MakeCircleOutput(inner_circle);
    LineMeasurementOutput line_output = MakeLineOutput(line);

    return RunCircleRingLinePrototype(
        outer_output,
        inner_output,
        line_output,
        operation_config,
        center_tolerance,
        thickness_tolerance);
}
} // namespace formfit
} // namespace cxcore
