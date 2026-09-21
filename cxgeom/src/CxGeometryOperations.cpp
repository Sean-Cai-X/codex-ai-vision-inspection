#include "CxGeometryOperations.h"

#include "CxRefreshTracker.h"

namespace cxgeom
{
namespace
{
CxGeometryOperationResult BuildResult(CxGeometryOperationKind kind,
                                      const CxSceneRevision& previous,
                                      const CxSceneRevision& current,
                                      std::uint32_t dirty_flags)
{
  CxGeometryOperationResult result;
  result.kind = kind;
  result.revisions = current;
  result.dirty_flags = dirty_flags;
  result.decision = CxRefreshTracker::Decide(previous, current, dirty_flags);
  return result;
}
}

CxGeometryOperationResult CxGeometryOperations::AddGeometry(const CxSceneRevision& previous)
{
  CxSceneRevision current = previous;
  ++current.geometry;
  return BuildResult(CxGeometryOperationKind::Add, previous, current, CxDirtyGeometry);
}

CxGeometryOperationResult CxGeometryOperations::UpdateGeometryStyle(const CxSceneRevision& previous,
                                                                    bool visibility_only)
{
  const std::uint32_t dirty_flags =
    visibility_only ? CxDirtyVisibility : CxDirtyGeometryPresentation;
  return BuildResult(CxGeometryOperationKind::UpdateStyle, previous, previous, dirty_flags);
}

CxGeometryOperationResult CxGeometryOperations::ReplaceGeometryPayload(const CxSceneRevision& previous)
{
  CxSceneRevision current = previous;
  ++current.geometry;
  return BuildResult(CxGeometryOperationKind::ReplacePayload,
                     previous,
                     current,
                     CxDirtyGeometry);
}

CxGeometryOperationResult CxGeometryOperations::RemoveGeometry(const CxSceneRevision& previous)
{
  CxSceneRevision current = previous;
  ++current.geometry;
  return BuildResult(CxGeometryOperationKind::Remove, previous, current, CxDirtyGeometry);
}
}
