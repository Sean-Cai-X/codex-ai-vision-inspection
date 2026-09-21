#include "pch.h"

#include "FormfitGauge.h"

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

double ClampPositive(double value, double fallback)
{
    return std::isfinite(value) && value > 0.0 ? value : fallback;
}

double ClampNonNegative(double value, double fallback = 0.0)
{
    return std::isfinite(value) && value >= 0.0 ? value : fallback;
}

double ClampConfidence(double value)
{
    return std::min(1.0, std::max(0.0, FiniteOr(value, 0.0)));
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

double EstimateLineAngleDegrees(const LineMeasurementOutput& line)
{
    if (line.measure_bounds.width >= line.measure_bounds.height)
    {
        return 0.0;
    }
    return 90.0;
}

double EstimateLineLength(const LineMeasurementOutput& line)
{
    return std::max(ClampPositive(line.measure_bounds.width, 0.0),
                    ClampPositive(line.measure_bounds.height, 0.0));
}

ResidualType RelationResidualType(GaugeRelationType relation_type)
{
    (void)relation_type;
    return ResidualType::Relation;
}

ResidualType ConstraintResidualType(GaugeConstraintType constraint_type)
{
    (void)constraint_type;
    return ResidualType::Constraint;
}

FitVariable MakeVariable(const char* name,
                         double value,
                         double lower_bound,
                         double upper_bound,
                         double weight = 1.0,
                         bool locked = false)
{
    FitVariable variable;
    variable.name = name ? name : "";
    variable.value = FiniteOr(value, 0.0);
    variable.lower_bound = FiniteOr(lower_bound, variable.value);
    variable.upper_bound = FiniteOr(upper_bound, variable.value);
    if (variable.lower_bound > variable.upper_bound)
        std::swap(variable.lower_bound, variable.upper_bound);
    variable.weight = ClampPositive(weight, 1.0);
    variable.locked = locked;
    return variable;
}
}

FormfitGauge MakeGauge(const char* gauge_id, const char* name)
{
    FormfitGauge gauge;
    gauge.gauge_id = gauge_id ? gauge_id : "";
    gauge.name = name ? name : "";
    gauge.overall_tolerance = 1.0;
    gauge.overall_weight = 1.0;
    gauge.learn_score = 0.0;
    return gauge;
}

GaugeElement MakeRectGaugeElement(const OutputRect& rect,
                                  const char* element_id,
                                  const char* source_entity_id,
                                  double confidence)
{
    GaugeElement element;
    element.element_id = element_id ? element_id : "rect";
    element.element_type = GaugeElementType::Rect;
    element.source_entity_id = source_entity_id ? source_entity_id : "";
    const OutputRect safe_rect = NormalizeRect(rect);
    element.confidence = ClampConfidence(confidence);
    element.bbox_x = safe_rect.x;
    element.bbox_y = safe_rect.y;
    element.bbox_width = safe_rect.width;
    element.bbox_height = safe_rect.height;
    element.variables.push_back(MakeVariable("rect.center_x", safe_rect.x + safe_rect.width * 0.5, safe_rect.x - safe_rect.width, safe_rect.x + safe_rect.width * 2.0));
    element.variables.push_back(MakeVariable("rect.center_y", safe_rect.y + safe_rect.height * 0.5, safe_rect.y - safe_rect.height, safe_rect.y + safe_rect.height * 2.0));
    element.variables.push_back(MakeVariable("rect.width", safe_rect.width, 1.0, std::max(1.0, safe_rect.width * 3.0)));
    element.variables.push_back(MakeVariable("rect.height", safe_rect.height, 1.0, std::max(1.0, safe_rect.height * 3.0)));
    element.variables.push_back(MakeVariable("rect.rotation", 0.0, -180.0, 180.0, 0.5));
    return element;
}

GaugeElement MakeLineGaugeElement(const LineMeasurementOutput& line,
                                  const char* element_id,
                                  const char* source_entity_id,
                                  double confidence)
{
    GaugeElement element;
    element.element_id = element_id ? element_id : "line";
    element.element_type = GaugeElementType::Line;
    element.source_entity_id = source_entity_id ? source_entity_id : "";
    const OutputRect safe_bounds = NormalizeRect(line.measure_bounds);
    element.confidence = ClampConfidence(confidence);
    element.bbox_x = safe_bounds.x;
    element.bbox_y = safe_bounds.y;
    element.bbox_width = safe_bounds.width;
    element.bbox_height = safe_bounds.height;

    const double center_x = safe_bounds.x + safe_bounds.width * 0.5;
    const double center_y = safe_bounds.y + safe_bounds.height * 0.5;
    const double length = ClampPositive(EstimateLineLength(line), 1.0);
    const double angle = EstimateLineAngleDegrees(line);

    element.variables.push_back(MakeVariable("line.center_x", center_x, center_x - length, center_x + length));
    element.variables.push_back(MakeVariable("line.center_y", center_y, center_y - length, center_y + length));
    element.variables.push_back(MakeVariable("line.length", length, 1.0, std::max(1.0, length * 3.0)));
    element.variables.push_back(MakeVariable("line.angle", angle, -180.0, 180.0, 0.75));
    return element;
}

GaugeElement MakeCircleGaugeElement(const CircleMeasurementOutput& circle,
                                    const char* element_id,
                                    const char* source_entity_id,
                                    double confidence)
{
    GaugeElement element;
    element.element_id = element_id ? element_id : "circle";
    element.element_type = GaugeElementType::Circle;
    element.source_entity_id = source_entity_id ? source_entity_id : "";
    const OutputRect safe_bounds = NormalizeRect(circle.measure_bounds);
    const double center_x = FiniteOr(circle.center.x, safe_bounds.x + safe_bounds.width * 0.5);
    const double center_y = FiniteOr(circle.center.y, safe_bounds.y + safe_bounds.height * 0.5);
    const double radius = ClampPositive(circle.radius, std::max(1.0, std::max(safe_bounds.width, safe_bounds.height) * 0.5));
    element.confidence = ClampConfidence(confidence);
    element.bbox_x = safe_bounds.x;
    element.bbox_y = safe_bounds.y;
    element.bbox_width = safe_bounds.width;
    element.bbox_height = safe_bounds.height;
    element.variables.push_back(MakeVariable("circle.center_x", center_x, center_x - radius * 2.0, center_x + radius * 2.0));
    element.variables.push_back(MakeVariable("circle.center_y", center_y, center_y - radius * 2.0, center_y + radius * 2.0));
    element.variables.push_back(MakeVariable("circle.radius", radius, 1.0, std::max(1.0, radius * 3.0)));
    return element;
}

GaugeElement MakeMatchRectGaugeElement(const MatchCandidateOutput& match_candidate,
                                       const char* element_id,
                                       const char* source_entity_id,
                                       double confidence)
{
    OutputRect rect;
    rect.x = match_candidate.bounds.x;
    rect.y = match_candidate.bounds.y;
    rect.width = match_candidate.bounds.width;
    rect.height = match_candidate.bounds.height;
    rect = NormalizeRect(rect);
    GaugeElement element = MakeRectGaugeElement(rect, element_id ? element_id : "match_rect", source_entity_id, confidence);
    return element;
}

FormfitGauge MakeRectCircleGauge(const OutputRect& rect,
                                 const CircleMeasurementOutput& circle,
                                 const char* gauge_id,
                                 const char* name)
{
    FormfitGauge gauge = MakeGauge(gauge_id, name);
    gauge.elements.push_back(MakeRectGaugeElement(rect, "rect_0", "rect", 1.0));
    gauge.elements.push_back(MakeCircleGaugeElement(circle, "circle_0", "circle", circle.has_direct_fit ? 1.0 : 0.75));

    GaugeRelation concentric_like;
    concentric_like.relation_id = "rect_circle_center_distance";
    concentric_like.lhs_element_id = "rect_0";
    concentric_like.rhs_element_id = "circle_0";
    concentric_like.relation_type = GaugeRelationType::Distance;
    concentric_like.target_value = 0.0;
    concentric_like.tolerance = std::max(1.0, circle.radius * 0.5);
    concentric_like.weight = 1.5;
    gauge.relations.push_back(concentric_like);

    GaugeConstraint rect_aspect;
    rect_aspect.constraint_id = "rect_aspect";
    rect_aspect.target_element_id = "rect_0";
    rect_aspect.constraint_type = GaugeConstraintType::AspectRatio;
    rect_aspect.target_value = ClampPositive(rect.height, 0.0) > 0.0 ? ClampNonNegative(rect.width) / ClampPositive(rect.height, 1.0) : 0.0;
    rect_aspect.tolerance = 0.25;
    rect_aspect.weight = 1.0;
    gauge.constraints.push_back(rect_aspect);

    GaugeConstraint circle_radius;
    circle_radius.constraint_id = "circle_radius";
    circle_radius.target_element_id = "circle_0";
    circle_radius.constraint_type = GaugeConstraintType::Radius;
    circle_radius.target_value = ClampPositive(circle.radius, 1.0);
    circle_radius.tolerance = std::max(1.0, circle_radius.target_value * 0.15);
    circle_radius.weight = 1.2;
    gauge.constraints.push_back(circle_radius);

    gauge.learn_score = circle.has_direct_fit ? 1.0 : 0.8;
    return gauge;
}

FormfitGauge MakeRectCircleLineGauge(const OutputRect& rect,
                                     const CircleMeasurementOutput& circle,
                                     const LineMeasurementOutput& line,
                                     const char* gauge_id,
                                     const char* name)
{
    FormfitGauge gauge = MakeRectCircleGauge(rect, circle, gauge_id, name);
    gauge.elements.push_back(MakeLineGaugeElement(line, "line_0", "line", 1.0));

    const double rect_center_x = rect.x + rect.width * 0.5;
    const double rect_center_y = rect.y + rect.height * 0.5;
    const double line_center_x = line.measure_bounds.x + line.measure_bounds.width * 0.5;
    const double line_center_y = line.measure_bounds.y + line.measure_bounds.height * 0.5;
    const double line_center_distance = std::hypot(rect_center_x - line_center_x, rect_center_y - line_center_y);

    GaugeRelation rect_line_distance;
    rect_line_distance.relation_id = "rect_line_center_distance";
    rect_line_distance.lhs_element_id = "rect_0";
    rect_line_distance.rhs_element_id = "line_0";
    rect_line_distance.relation_type = GaugeRelationType::Distance;
    rect_line_distance.target_value = line_center_distance;
    rect_line_distance.tolerance = std::max(2.0, std::max(rect.width, rect.height) * 0.2);
    rect_line_distance.weight = 1.0;
    gauge.relations.push_back(rect_line_distance);

    GaugeConstraint line_length;
    line_length.constraint_id = "line_length";
    line_length.target_element_id = "line_0";
    line_length.constraint_type = GaugeConstraintType::Length;
    line_length.target_value = EstimateLineLength(line);
    line_length.tolerance = std::max(2.0, line_length.target_value * 0.2);
    line_length.weight = 1.0;
    gauge.constraints.push_back(line_length);

    gauge.learn_score = (gauge.learn_score + 1.0) * 0.5;
    return gauge;
}

FormfitGauge MakeRectCircleLineMatchGauge(const OutputRect& rect,
                                          const CircleMeasurementOutput& circle,
                                          const LineMeasurementOutput& line,
                                          const MatchOutput& match,
                                          const char* gauge_id,
                                          const char* name)
{
    FormfitGauge gauge = MakeRectCircleLineGauge(rect, circle, line, gauge_id, name);
    if (match.candidates.empty())
    {
        return gauge;
    }

    const MatchCandidateOutput& best = match.candidates.front();
    const double match_confidence = std::max(0.25, ClampConfidence(best.score));
    gauge.elements.push_back(MakeMatchRectGaugeElement(best, "match_rect_0", "FastMatch", match_confidence));

    const double rect_center_x = rect.x + rect.width * 0.5;
    const double rect_center_y = rect.y + rect.height * 0.5;
    const double match_center_distance = std::hypot(rect_center_x - best.center.x, rect_center_y - best.center.y);

    GaugeRelation rect_match_distance;
    rect_match_distance.relation_id = "rect_match_center_distance";
    rect_match_distance.lhs_element_id = "rect_0";
    rect_match_distance.rhs_element_id = "match_rect_0";
    rect_match_distance.relation_type = GaugeRelationType::Distance;
    rect_match_distance.target_value = match_center_distance;
    rect_match_distance.tolerance = std::max(2.0, std::max(best.bounds.width, best.bounds.height) * 0.2);
    rect_match_distance.weight = 1.25;
    gauge.relations.push_back(rect_match_distance);

    GaugeConstraint match_width;
    match_width.constraint_id = "match_rect_width";
    match_width.target_element_id = "match_rect_0";
    match_width.constraint_type = GaugeConstraintType::Length;
    match_width.target_value = best.bounds.width;
    match_width.tolerance = std::max(2.0, best.bounds.width * 0.15);
    match_width.weight = 0.8;
    gauge.constraints.push_back(match_width);

    GaugeConstraint match_height;
    match_height.constraint_id = "match_rect_height";
    match_height.target_element_id = "match_rect_0";
    match_height.constraint_type = GaugeConstraintType::Length;
    match_height.target_value = best.bounds.height;
    match_height.tolerance = std::max(2.0, best.bounds.height * 0.15);
    match_height.weight = 0.8;
    gauge.constraints.push_back(match_height);

    gauge.learn_score = (gauge.learn_score + match_confidence) * 0.5;
    return gauge;
}

FormfitGauge MakeCircleRingGauge(
    const CircleMeasurementOutput& outer_circle,
    const CircleMeasurementOutput& inner_circle,
    const char* gauge_id,
    const char* name,
    double center_tolerance,
    double thickness_tolerance)
{
    FormfitGauge gauge = MakeGauge(gauge_id, name);

    GaugeElement outer =
        MakeCircleGaugeElement(
            outer_circle,
            "outer_circle",
            "outer_circle",
            outer_circle.has_direct_fit ? 1.0 : 0.75);

    GaugeElement inner =
        MakeCircleGaugeElement(
            inner_circle,
            "inner_circle",
            "inner_circle",
            inner_circle.has_direct_fit ? 1.0 : 0.75);

    gauge.elements.push_back(outer);
    gauge.elements.push_back(inner);

    const double outer_radius =
        ClampPositive(outer_circle.radius, 1.0);

    const double inner_radius =
        ClampPositive(inner_circle.radius, 1.0);

    const double thickness =
        std::max(0.0, outer_radius - inner_radius);

    GaugeRelation concentric;
    concentric.relation_id = "ring_concentric";
    concentric.lhs_element_id = "outer_circle";
    concentric.rhs_element_id = "inner_circle";
    concentric.relation_type = GaugeRelationType::Concentric;
    concentric.target_value = 0.0;
    concentric.tolerance = ClampPositive(center_tolerance, 3.0);
    concentric.weight = 2.0;
    gauge.relations.push_back(concentric);

    GaugeRelation inside;
    inside.relation_id = "ring_inner_inside_outer";
    inside.lhs_element_id = "inner_circle";
    inside.rhs_element_id = "outer_circle";
    inside.relation_type = GaugeRelationType::Inside;
    inside.target_value = 1.0;
    inside.tolerance = 0.0;
    inside.weight = 1.5;
    gauge.relations.push_back(inside);

    GaugeRelation radius_diff;
    radius_diff.relation_id = "ring_radius_difference";
    radius_diff.lhs_element_id = "outer_circle";
    radius_diff.rhs_element_id = "inner_circle";
    radius_diff.relation_type = GaugeRelationType::RadiusDifference;
    radius_diff.target_value = thickness;
    radius_diff.tolerance =
        ClampPositive(thickness_tolerance, std::max(1.0, thickness * 0.05));
    radius_diff.weight = 1.5;
    gauge.relations.push_back(radius_diff);

    GaugeConstraint outer_radius_constraint;
    outer_radius_constraint.constraint_id = "outer_radius";
    outer_radius_constraint.target_element_id = "outer_circle";
    outer_radius_constraint.constraint_type = GaugeConstraintType::Radius;
    outer_radius_constraint.target_value = outer_radius;
    outer_radius_constraint.tolerance = std::max(1.0, outer_radius * 0.05);
    outer_radius_constraint.weight = 1.0;
    gauge.constraints.push_back(outer_radius_constraint);

    GaugeConstraint inner_radius_constraint;
    inner_radius_constraint.constraint_id = "inner_radius";
    inner_radius_constraint.target_element_id = "inner_circle";
    inner_radius_constraint.constraint_type = GaugeConstraintType::Radius;
    inner_radius_constraint.target_value = inner_radius;
    inner_radius_constraint.tolerance = std::max(1.0, inner_radius * 0.05);
    inner_radius_constraint.weight = 1.0;
    gauge.constraints.push_back(inner_radius_constraint);

    const double center_distance =
        std::hypot(
            outer_circle.center.x - inner_circle.center.x,
            outer_circle.center.y - inner_circle.center.y);

    const bool center_ok =
        std::isfinite(center_distance) &&
        center_distance <= concentric.tolerance;

    const bool thickness_ok =
        std::isfinite(thickness) &&
        thickness > 0.0;

    const bool inner_inside_outer =
        std::isfinite(outer_radius) &&
        std::isfinite(inner_radius) &&
        inner_radius < outer_radius;

    double score = 0.0;
    score += center_ok ? 0.4 : 0.0;
    score += inner_inside_outer ? 0.3 : 0.0;
    score += thickness_ok ? 0.3 : 0.0;

    gauge.learn_score = score;

    return gauge;
}

FormfitGauge MakeCircleRingLineGauge(
    const CircleMeasurementOutput& outer_circle,
    const CircleMeasurementOutput& inner_circle,
    const LineMeasurementOutput& line,
    const char* gauge_id,
    const char* name,
    double center_tolerance,
    double thickness_tolerance)
{
    FormfitGauge gauge = MakeCircleRingGauge(
        outer_circle,
        inner_circle,
        gauge_id,
        name,
        center_tolerance,
        thickness_tolerance);

    gauge.elements.push_back(MakeLineGaugeElement(line, "line_0", "line", 1.0));

    const double outer_center_x = FiniteOr(outer_circle.center.x, outer_circle.measure_bounds.x + outer_circle.measure_bounds.width * 0.5);
    const double outer_center_y = FiniteOr(outer_circle.center.y, outer_circle.measure_bounds.y + outer_circle.measure_bounds.height * 0.5);
    const OutputRect line_bounds = NormalizeRect(line.measure_bounds);
    const double line_center_x = line_bounds.x + line_bounds.width * 0.5;
    const double line_center_y = line_bounds.y + line_bounds.height * 0.5;
    const double line_center_distance = std::hypot(outer_center_x - line_center_x, outer_center_y - line_center_y);

    GaugeRelation ring_line_distance;
    ring_line_distance.relation_id = "ring_line_center_distance";
    ring_line_distance.lhs_element_id = "outer_circle";
    ring_line_distance.rhs_element_id = "line_0";
    ring_line_distance.relation_type = GaugeRelationType::Distance;
    ring_line_distance.target_value = line_center_distance;
    ring_line_distance.tolerance = std::max(2.0, ClampPositive(outer_circle.radius, 1.0) * 0.20);
    ring_line_distance.weight = 1.0;
    gauge.relations.push_back(ring_line_distance);

    GaugeConstraint line_length;
    line_length.constraint_id = "line_length";
    line_length.target_element_id = "line_0";
    line_length.constraint_type = GaugeConstraintType::Length;
    line_length.target_value = EstimateLineLength(line);
    line_length.tolerance = std::max(2.0, line_length.target_value * 0.2);
    line_length.weight = 1.0;
    gauge.constraints.push_back(line_length);

    gauge.learn_score = (gauge.learn_score + 1.0) * 0.5;
    return gauge;
}

GaugeElement MakeFastMatchFormFitGaugeElement(
    const CxFastMatchFormFitResult& fit_result,
    const CxFastMatchShapeModel& observed_model,
    const char* element_id,
    const char* source_entity_id)
{
    GaugeElement element;
    element.element_id = element_id ? element_id : "fastmatch_form_fit";
    element.element_type = GaugeElementType::PointSet;
    element.source_entity_id = source_entity_id ? source_entity_id :
        observed_model.model_id;
    element.confidence = ClampConfidence(fit_result.score);
    element.bbox_x = FiniteOr(observed_model.bbox_x, 0.0);
    element.bbox_y = FiniteOr(observed_model.bbox_y, 0.0);
    element.bbox_width = ClampNonNegative(observed_model.bbox_width);
    element.bbox_height = ClampNonNegative(observed_model.bbox_height);
    const double span = std::max({1.0, element.bbox_width, element.bbox_height});
    element.variables.push_back(MakeVariable(
        "form_fit.translate_x", fit_result.translate_x, -span * 4.0, span * 4.0));
    element.variables.push_back(MakeVariable(
        "form_fit.translate_y", fit_result.translate_y, -span * 4.0, span * 4.0));
    element.variables.push_back(MakeVariable(
        "form_fit.angle_deg", fit_result.angle_deg, -180.0, 180.0));
    element.variables.push_back(MakeVariable(
        "form_fit.scale_x", fit_result.scale_x, 0.05, 20.0));
    element.variables.push_back(MakeVariable(
        "form_fit.scale_y", fit_result.scale_y, 0.05, 20.0));
    element.variables.push_back(MakeVariable(
        "form_fit.symmetric_residual_px",
        ClampNonNegative(fit_result.symmetric_residual_px), 0.0, span));
    element.variables.push_back(MakeVariable(
        "form_fit.normal_residual_deg",
        ClampNonNegative(fit_result.normal_residual_deg), 0.0, 180.0));
    element.variables.push_back(MakeVariable(
        "form_fit.mutual_coverage", fit_result.mutual_coverage, 0.0, 1.0));
    element.variables.push_back(MakeVariable(
        "form_fit.mutual_count", fit_result.dense_mutual_count, 0.0,
        std::max(1, fit_result.reference_dense_count), 1.0, true));
    return element;
}

FormfitGauge MakeFastMatchFormFitGauge(
    const CxFastMatchFormFitResult& fit_result,
    const CxFastMatchShapeModel& reference_model,
    const CxFastMatchShapeModel& observed_model,
    const char* gauge_id,
    const char* name)
{
    FormfitGauge gauge = MakeGauge(gauge_id, name);
    gauge.elements.push_back(MakeFastMatchFormFitGaugeElement(
        fit_result, observed_model, "fastmatch_form_fit", observed_model.model_id.c_str()));
    gauge.learn_score = ClampConfidence(fit_result.score);
    gauge.overall_tolerance = std::max(
        0.25, ClampNonNegative(fit_result.symmetric_residual_px, 1.0) * 2.0);

    GaugeConstraint closure;
    closure.constraint_id = "fastmatch_form_fit.closure";
    closure.target_element_id = "fastmatch_form_fit";
    closure.constraint_type = GaugeConstraintType::Closure;
    closure.target_value = 0.0;
    closure.tolerance = gauge.overall_tolerance;
    closure.weight = 1.0;
    gauge.constraints.push_back(closure);

    const bool has_symmetry_anchor = std::any_of(
        reference_model.anchors.begin(), reference_model.anchors.end(),
        [](const CxFastMatchStructuralAnchor& anchor) {
            return anchor.type == CxFastMatchStructuralAnchorType::SymmetryPair;
        });
    if (has_symmetry_anchor)
    {
        GaugeConstraint symmetry;
        symmetry.constraint_id = "fastmatch_form_fit.symmetry";
        symmetry.target_element_id = "fastmatch_form_fit";
        symmetry.constraint_type = GaugeConstraintType::Symmetry;
        symmetry.target_value = 0.0;
        symmetry.tolerance = std::max(1.0, gauge.overall_tolerance);
        symmetry.weight = 0.75;
        gauge.constraints.push_back(symmetry);
    }
    return gauge;
}

FitTaskSpec MakeTaskSpecFromGauge(const FormfitGauge& gauge, const char* task_id, FitTaskType task_type)
{
    FitTaskSpec task = MakeFitTaskSpec(task_id ? task_id : gauge.gauge_id.c_str(), task_type);
    task.gauge_ids.push_back(gauge.gauge_id);

    for (const GaugeElement& element : gauge.elements)
    {
        task.input_element_ids.push_back(element.element_id);
        task.output_target_ids.push_back(element.source_entity_id);
    }

    for (const GaugeConstraint& constraint : gauge.constraints)
    {
        task.constraint_ids.push_back(constraint.constraint_id);

        FitResidual residual;
        residual.name = constraint.constraint_id;
        residual.residual_type = ConstraintResidualType(constraint.constraint_type);
        residual.value = constraint.target_value;
        residual.weight = constraint.weight;
        residual.tolerance = constraint.tolerance;
        task.reference_residuals.push_back(residual);
    }

    for (const GaugeRelation& relation : gauge.relations)
    {
        task.relation_ids.push_back(relation.relation_id);
    }

    task.preferred_stage_ids.push_back("coarse");
    task.preferred_stage_ids.push_back("refine");
    task.preferred_stage_ids.push_back("final");

    return task;
}

FitProblem MakeFitProblemFromGauge(const FormfitGauge& gauge, const char* problem_id)
{
    FitProblem problem;
    problem.problem_id = problem_id ? problem_id : gauge.gauge_id;
    problem.global_tolerance_scale = gauge.overall_tolerance > 0.0 ? gauge.overall_tolerance : 1.0;
    problem.global_weight_scale = gauge.overall_weight > 0.0 ? gauge.overall_weight : 1.0;

    for (const GaugeElement& element : gauge.elements)
    {
        problem.element_ids.push_back(element.element_id);
        for (const FitVariable& variable : element.variables)
        {
            problem.variables.push_back(variable);
        }
    }

    for (const GaugeRelation& relation : gauge.relations)
    {
        problem.relation_ids.push_back(relation.relation_id);
        FitResidual residual;
        residual.name = relation.relation_id;
        residual.residual_type = RelationResidualType(relation.relation_type);
        residual.value = relation.target_value;
        residual.weight = relation.weight;
        residual.tolerance = relation.tolerance;
        problem.residuals.push_back(residual);
    }

    for (const GaugeConstraint& constraint : gauge.constraints)
    {
        problem.constraint_ids.push_back(constraint.constraint_id);
        FitResidual residual;
        residual.name = constraint.constraint_id;
        residual.residual_type = ConstraintResidualType(constraint.constraint_type);
        residual.value = constraint.target_value;
        residual.weight = constraint.weight;
        residual.tolerance = constraint.tolerance;
        problem.residuals.push_back(residual);
    }

    FitStage coarse = MakeFitStage("coarse", "coarse", FitMethodType::LeastSquares, 16);
    FitStage refine = MakeFitStage("refine", "refine", FitMethodType::WeightedLeastSquares, 32);
    FitStage final_stage = MakeFitStage("final", "final", FitMethodType::Constrained, 24);
    coarse.order_index = 0;
    coarse.stage_weight = 0.75;
    refine.order_index = 1;
    refine.stage_weight = 1.0;
    final_stage.order_index = 2;
    final_stage.stage_weight = 1.25;
    final_stage.tolerance_scale = 0.85;

    for (const GaugeElement& element : gauge.elements)
    {
        coarse.active_element_ids.push_back(element.element_id);
        refine.active_element_ids.push_back(element.element_id);
        final_stage.active_element_ids.push_back(element.element_id);
    }
    for (const GaugeRelation& relation : gauge.relations)
    {
        refine.active_relation_ids.push_back(relation.relation_id);
        final_stage.active_relation_ids.push_back(relation.relation_id);
    }
    for (const GaugeConstraint& constraint : gauge.constraints)
    {
        final_stage.active_constraint_ids.push_back(constraint.constraint_id);
    }

    problem.stages.push_back(coarse);
    problem.stages.push_back(refine);
    problem.stages.push_back(final_stage);
    return problem;
}

const char* GaugeElementTypeName(GaugeElementType element_type)
{
    switch (element_type)
    {
    case GaugeElementType::Line:
        return "line";
    case GaugeElementType::Circle:
        return "circle";
    case GaugeElementType::Rect:
        return "rect";
    case GaugeElementType::Arc:
        return "arc";
    case GaugeElementType::PointSet:
        return "pointset";
    case GaugeElementType::Unknown:
    default:
        return "unknown";
    }
}

const char* GaugeRelationTypeName(GaugeRelationType relation_type)
{
    switch (relation_type)
    {
    case GaugeRelationType::Parallel:
        return "parallel";
    case GaugeRelationType::Orthogonal:
        return "orthogonal";
    case GaugeRelationType::Tangent:
        return "tangent";
    case GaugeRelationType::Concentric:
        return "concentric";
    case GaugeRelationType::Inside:
        return "inside";
    case GaugeRelationType::Adjacent:
        return "adjacent";
    case GaugeRelationType::Distance:
        return "distance";
    case GaugeRelationType::RadiusDifference:
        return "radius_difference";
    case GaugeRelationType::Unknown:
    default:
        return "unknown";
    }
}

const char* GaugeConstraintTypeName(GaugeConstraintType constraint_type)
{
    switch (constraint_type)
    {
    case GaugeConstraintType::Length:
        return "length";
    case GaugeConstraintType::Radius:
        return "radius";
    case GaugeConstraintType::Angle:
        return "angle";
    case GaugeConstraintType::AspectRatio:
        return "aspect_ratio";
    case GaugeConstraintType::Closure:
        return "closure";
    case GaugeConstraintType::Symmetry:
        return "symmetry";
    case GaugeConstraintType::Unknown:
    default:
        return "unknown";
    }
}

} // namespace formfit
} // namespace cxcore
