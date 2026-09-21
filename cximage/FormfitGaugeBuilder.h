#ifndef CXCORE_CORE_FORMFITGAUGEBUILDER_H
#define CXCORE_CORE_FORMFITGAUGEBUILDER_H

#include "FastMatch.h"
#include "FindLine.h"
#include "FindRect.h"
#include "FindCircle.h"
#include "FormfitPrototype.h"

namespace cxcore
{
namespace formfit
{

class GaugeBuilder
{
public:
    static OutputRect MakeRectOutput(const FindRect& rect, int index);
    static LineMeasurementOutput MakeLineOutput(FindLine& line);
    static CircleMeasurementOutput MakeCircleOutput(FindCircle& circle);
    static MatchOutput MakeMatchOutput(FastMatch& matcher, int max_candidates = 1);
    static FitOperationConfig MakeRectCircleCompositeConfig();
    static FitOperationConfig MakeRectCircleLineCompositeConfig();
    static FitOperationConfig MakeRectCircleLineMatchCompositeConfig();
    static FitOperationConfig MakeCircleRingCompositeConfig();
    static PrototypeRunResult BuildAndRunRectCirclePrototype(const FindRect& rect,
                                                             FindCircle& circle,
                                                             const FitOperationConfig& operation_config,
                                                             int rect_index = 0);
    static PrototypeRunResult BuildAndRunRectCircleLinePrototype(const FindRect& rect,
                                                                 FindLine& line,
                                                                 FindCircle& circle,
                                                                 const FitOperationConfig& operation_config,
                                                                 int rect_index = 0);
    static PrototypeRunResult BuildAndRunRectCircleLineMatchPrototype(const FindRect& rect,
                                                                      FindLine& line,
                                                                      FindCircle& circle,
                                                                      FastMatch& matcher,
                                                                      const FitOperationConfig& operation_config,
                                                                      int rect_index = 0,
                                                                      int max_candidates = 1);
    static PrototypeRunResult BuildAndRunCircleRingPrototype(FindCircle& outer_circle,
                                                             FindCircle& inner_circle,
                                                             const FitOperationConfig& operation_config,
                                                             double center_tolerance = 3.0,
                                                             double thickness_tolerance = 5.0);
};

} // namespace formfit
} // namespace cxcore

#endif
