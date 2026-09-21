#ifndef CXCORE_CORE_FORMFITGAUGE_H
#define CXCORE_CORE_FORMFITGAUGE_H

#include "CxCoreBoundary.h"
#include "CxFastMatchShapeModel.h"
#include "FormfitFitMethod.h"

#include <string>
#include <vector>

namespace cxcore
{
namespace formfit
{

enum class GaugeElementType
{
    Unknown = 0,
    Line,
    Circle,
    Rect,
    Arc,
    PointSet
};

enum class GaugeRelationType
{
    Unknown = 0,
    Parallel,
    Orthogonal,
    Tangent,
    Concentric,
    Inside,
    Adjacent,
    Distance,

    RadiusDifference
};

enum class GaugeConstraintType
{
    Unknown = 0,
    Length,
    Radius,
    Angle,
    AspectRatio,
    Closure,
    Symmetry
};

struct GaugeElement
{
    std::string element_id;
    GaugeElementType element_type = GaugeElementType::Unknown;
    std::string source_entity_id;
    double confidence = 1.0;
    double bbox_x = 0.0;
    double bbox_y = 0.0;
    double bbox_width = 0.0;
    double bbox_height = 0.0;
    std::vector<FitVariable> variables;
};

struct GaugeRelation
{
    std::string relation_id;
    std::string lhs_element_id;
    std::string rhs_element_id;
    GaugeRelationType relation_type = GaugeRelationType::Unknown;
    double target_value = 0.0;
    double tolerance = 0.0;
    double weight = 1.0;
};

struct GaugeConstraint
{
    std::string constraint_id;
    std::string target_element_id;
    GaugeConstraintType constraint_type = GaugeConstraintType::Unknown;
    double target_value = 0.0;
    double tolerance = 0.0;
    double weight = 1.0;
};

struct FormfitGauge
{
    std::string gauge_id;
    std::string name;
    std::vector<GaugeElement> elements;
    std::vector<GaugeRelation> relations;
    std::vector<GaugeConstraint> constraints;
    double overall_tolerance = 1.0;
    double overall_weight = 1.0;
    double learn_score = 0.0;
};

FormfitGauge MakeGauge(const char* gauge_id, const char* name);
GaugeElement MakeRectGaugeElement(const OutputRect& rect,
                                  const char* element_id,
                                  const char* source_entity_id,
                                  double confidence = 1.0);
GaugeElement MakeLineGaugeElement(const LineMeasurementOutput& line,
                                  const char* element_id,
                                  const char* source_entity_id,
                                  double confidence = 1.0);
GaugeElement MakeCircleGaugeElement(const CircleMeasurementOutput& circle,
                                    const char* element_id,
                                    const char* source_entity_id,
                                    double confidence = 1.0);
GaugeElement MakeMatchRectGaugeElement(const MatchCandidateOutput& match_candidate,
                                       const char* element_id,
                                       const char* source_entity_id,
                                       double confidence = 1.0);
FormfitGauge MakeRectCircleGauge(const OutputRect& rect,
                                 const CircleMeasurementOutput& circle,
                                 const char* gauge_id,
                                 const char* name);
FormfitGauge MakeRectCircleLineGauge(const OutputRect& rect,
                                     const CircleMeasurementOutput& circle,
                                     const LineMeasurementOutput& line,
                                     const char* gauge_id,
                                     const char* name);
FormfitGauge MakeRectCircleLineMatchGauge(const OutputRect& rect,
                                          const CircleMeasurementOutput& circle,
                                          const LineMeasurementOutput& line,
                                          const MatchOutput& match,
                                          const char* gauge_id,
                                          const char* name);
FormfitGauge MakeCircleRingGauge(
    const CircleMeasurementOutput& outer_circle,
    const CircleMeasurementOutput& inner_circle,
    const char* gauge_id,
    const char* name,
    double center_tolerance = 3.0,
    double thickness_tolerance = 5.0);
FormfitGauge MakeCircleRingLineGauge(
    const CircleMeasurementOutput& outer_circle,
    const CircleMeasurementOutput& inner_circle,
    const LineMeasurementOutput& line,
    const char* gauge_id,
    const char* name,
    double center_tolerance = 3.0,
    double thickness_tolerance = 5.0);
// P6 bridge: FastMatch owns contour reconstruction and bidirectional form-fit.
// FormfitGauge receives only a value snapshot for business composition and
// constraint analysis; it never owns or reruns the matching algorithm.
GaugeElement MakeFastMatchFormFitGaugeElement(
    const CxFastMatchFormFitResult& fit_result,
    const CxFastMatchShapeModel& observed_model,
    const char* element_id,
    const char* source_entity_id);
FormfitGauge MakeFastMatchFormFitGauge(
    const CxFastMatchFormFitResult& fit_result,
    const CxFastMatchShapeModel& reference_model,
    const CxFastMatchShapeModel& observed_model,
    const char* gauge_id,
    const char* name);
FitTaskSpec MakeTaskSpecFromGauge(const FormfitGauge& gauge, const char* task_id, FitTaskType task_type);
FitProblem MakeFitProblemFromGauge(const FormfitGauge& gauge, const char* problem_id);

const char* GaugeElementTypeName(GaugeElementType element_type);
const char* GaugeRelationTypeName(GaugeRelationType relation_type);
const char* GaugeConstraintTypeName(GaugeConstraintType constraint_type);

} // namespace formfit
} // namespace cxcore

#endif
