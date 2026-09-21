#include "CxRefreshTracker.h"

namespace cxgeom
{
namespace
{
CxRefreshAction DecideAction(const CxRefreshDecision& decision)
{
  if (decision.reasons & (CxDirtyGeometry | CxDirtyGeometryPresentation))
  {
    return CxRefreshAction::RebuildGeometryPresentation;
  }

  if (decision.reasons & (CxDirtyCloud | CxDirtyCloudPresentation))
  {
    return CxRefreshAction::RebuildCloudPresentation;
  }

  if (decision.reasons & (CxDirtyVisibility | CxDirtySelection))
  {
    return CxRefreshAction::UpdatePresentation;
  }

  if (decision.reasons & CxDirtyCamera)
  {
    return CxRefreshAction::RedrawOnly;
  }

  return CxRefreshAction::None;
}
}

CxRefreshDecision CxRefreshTracker::Decide(const CxSceneRevision& previous,
                                           const CxSceneRevision& current,
                                           std::uint32_t dirty_flags)
{
  CxRefreshDecision decision;
  decision.geometry_changed = (previous.geometry != current.geometry);
  decision.cloud_changed = (previous.cloud != current.cloud);
  decision.camera_changed = (previous.camera != current.camera);
  decision.selection_changed = (previous.selection != current.selection);

  if (decision.geometry_changed)
  {
    decision.reasons |= CxDirtyGeometry;
  }

  if (decision.cloud_changed)
  {
    decision.reasons |= CxDirtyCloud;
  }

  if (decision.camera_changed)
  {
    decision.reasons |= CxDirtyCamera;
  }

  if (decision.selection_changed)
  {
    decision.reasons |= CxDirtySelection;
  }

  decision.reasons |= dirty_flags;
  decision.action = DecideAction(decision);

  if (decision.geometry_changed && decision.cloud_changed)
  {
    decision.action = CxRefreshAction::RebuildScene;
  }

  return decision;
}

bool CxRefreshTracker::HasAny(std::uint32_t dirty_flags, std::uint32_t mask)
{
  return (dirty_flags & mask) != 0;
}
}
