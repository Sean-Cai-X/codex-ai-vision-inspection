#include "pch.h"
#include "metrology_analytics/CxAnalyticsObservationBridgeDraft.h"

#include "metrology_analytics/CxSyntheticSurfaceFactory.h"

#include <cmath>

namespace cxvision::metrology_analytics
{
namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;

bool BuildDraftSurface(
    const CxAnalyticsObservationBridgeDraft::ObservationId& id,
    CxSurfaceField& out)
{
    if (id == "draft.flat5")
    {
        out = CxSyntheticSurfaceFactory::flat(32, 32, 5.0);
        return true;
    }
    if (id == "draft.sine_A1_full_cycle")
    {
        out = CxSyntheticSurfaceFactory::sine1D(10000, 1, 1.0, 10000.0);
        return true;
    }
    if (id == "draft.plane_tilt")
    {
        CxPhysUnit unit;
        unit.x_scale_per_pixel = 0.01;
        unit.y_scale_per_pixel = 0.01;
        unit.z_scale_per_pixel = 1.0;
        out = CxSyntheticSurfaceFactory::plane(64, 48, 2.0, 3.0, 5.0, unit);
        return true;
    }
    return false;
}

CxProfile1D BuildXProfile(const CxSurfaceField& field)
{
    CxProfile1D profile;
    profile.delta_x_physical = 1.0;
    profile.z.reserve(static_cast<std::size_t>(field.xres()));
    for (int x = 0; x < field.xres(); ++x)
        profile.z.push_back(field.at(x, 0));
    return profile;
}
}

bool CxAnalyticsObservationBridgeDraft::TryQuerySurfaceBasicStats(
    const ObservationId& id,
    CxSurfaceBasicStats& out)
{
    CxSurfaceField field;
    if (!BuildDraftSurface(id, field))
        return false;
    out = computeSurfaceBasicStats(field);
    return true;
}

bool CxAnalyticsObservationBridgeDraft::TryQueryAreaResult(
    const ObservationId& id,
    CxSurfaceAreaResult& out)
{
    CxSurfaceField field;
    if (!BuildDraftSurface(id, field))
        return false;
    out = computeProjectedAndSurfaceArea(field);
    return true;
}

bool CxAnalyticsObservationBridgeDraft::TryQueryRoughness1D(
    const ObservationId& id,
    CxRoughness1DResult& out)
{
    CxSurfaceField field;
    if (!BuildDraftSurface(id, field))
        return false;
    out = computeProfileRoughness(BuildXProfile(field));
    return true;
}

std::vector<CxAnalyticsObservationBridgeDraft::ObservationId>
CxAnalyticsObservationBridgeDraft::SupportedObservationIds()
{
    return {
        "draft.flat5",
        "draft.sine_A1_full_cycle",
        "draft.plane_tilt",
    };
}

const char* CxAnalyticsObservationBridgeDraft::DraftStatus()
{
    return "S3_S4_BRIDGE_DRAFT_ONLY_PENDING_S4_REVIEW";
}

} // namespace cxvision::metrology_analytics
