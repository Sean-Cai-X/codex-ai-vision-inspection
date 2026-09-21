#include "CxRefreshTracker.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxgeom_refresh_smoke] " << message << '\n';
    return false;
  }

  return true;
}
}

int main()
{
  cxgeom::CxSceneRevision previous;
  cxgeom::CxSceneRevision current;

  current.camera = 1;
  cxgeom::CxRefreshDecision camera_only =
    cxgeom::CxRefreshTracker::Decide(previous, current, cxgeom::CxDirtyNone);
  if (!Check(camera_only.action == cxgeom::CxRefreshAction::RedrawOnly,
             "camera-only update should request redraw"))
  {
    return 1;
  }

  current = previous;
  current.geometry = 2;
  cxgeom::CxRefreshDecision geometry_only =
    cxgeom::CxRefreshTracker::Decide(previous, current, cxgeom::CxDirtyNone);
  if (!Check(geometry_only.action == cxgeom::CxRefreshAction::RebuildGeometryPresentation,
             "geometry update should rebuild geometry presentation"))
  {
    return 1;
  }

  current = previous;
  current.cloud = 3;
  cxgeom::CxRefreshDecision cloud_only =
    cxgeom::CxRefreshTracker::Decide(previous, current, cxgeom::CxDirtyNone);
  if (!Check(cloud_only.action == cxgeom::CxRefreshAction::RebuildCloudPresentation,
             "cloud update should rebuild cloud presentation"))
  {
    return 1;
  }

  current = previous;
  current.geometry = 4;
  current.cloud = 5;
  cxgeom::CxRefreshDecision full_rebuild =
    cxgeom::CxRefreshTracker::Decide(previous, current, cxgeom::CxDirtyNone);
  if (!Check(full_rebuild.action == cxgeom::CxRefreshAction::RebuildScene,
             "geometry plus cloud should rebuild the full scene"))
  {
    return 1;
  }

  current = previous;
  cxgeom::CxRefreshDecision visibility_only =
    cxgeom::CxRefreshTracker::Decide(previous, current, cxgeom::CxDirtyVisibility);
  if (!Check(visibility_only.action == cxgeom::CxRefreshAction::UpdatePresentation,
             "visibility-only update should refresh presentation"))
  {
    return 1;
  }

  std::cout << "[cxgeom_refresh_smoke] ok" << '\n';
  return 0;
}
