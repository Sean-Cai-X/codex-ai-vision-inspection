#pragma once

#include "metrology_analytics/CxRoughness1D.h"
#include "metrology_analytics/CxSurfaceAreas.h"
#include "metrology_analytics/CxSurfaceBasicStats.h"

#include <string>
#include <vector>

namespace cxvision::metrology_analytics
{

// S3 -> S4 draft bridge.
//
// This is intentionally value-only and draft-only.  It does not expose
// CxSurfaceField*, Parser objects, Find* objects, Image objects or Shape
// pointers.  Future S4 measurement semantics may consume these observations,
// but missing entries must be treated as PENDING_ANALYTICS_BINDING.
struct CxAnalyticsObservationBridgeDraft
{
    using ObservationId = std::string;

    static bool TryQuerySurfaceBasicStats(
        const ObservationId& id,
        CxSurfaceBasicStats& out);

    static bool TryQueryAreaResult(
        const ObservationId& id,
        CxSurfaceAreaResult& out);

    static bool TryQueryRoughness1D(
        const ObservationId& id,
        CxRoughness1DResult& out);

    static std::vector<ObservationId> SupportedObservationIds();
    static const char* DraftStatus();
};

} // namespace cxvision::metrology_analytics

