#include "CxGeometryOperations.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxgeom_operations_smoke] " << message << '\n';
    return false;
  }

  return true;
}
}

int main()
{
  cxgeom::CxSceneRevision baseline;

  const cxgeom::CxGeometryOperationResult add_result =
    cxgeom::CxGeometryOperations::AddGeometry(baseline);
  if (!Check(add_result.revisions.geometry == 1, "add should increment geometry revision"))
  {
    return 1;
  }
  if (!Check(add_result.decision.action == cxgeom::CxRefreshAction::RebuildGeometryPresentation,
             "add should rebuild geometry presentation"))
  {
    return 1;
  }

  const cxgeom::CxGeometryOperationResult style_result =
    cxgeom::CxGeometryOperations::UpdateGeometryStyle(add_result.revisions, false);
  if (!Check(style_result.revisions.geometry == add_result.revisions.geometry,
             "style update should not increment geometry revision"))
  {
    return 1;
  }
  if (!Check(style_result.decision.action == cxgeom::CxRefreshAction::UpdatePresentation,
             "style update should refresh presentation"))
  {
    return 1;
  }

  const cxgeom::CxGeometryOperationResult visibility_result =
    cxgeom::CxGeometryOperations::UpdateGeometryStyle(add_result.revisions, true);
  if (!Check(visibility_result.decision.action == cxgeom::CxRefreshAction::UpdatePresentation,
             "visibility update should refresh presentation"))
  {
    return 1;
  }

  const cxgeom::CxGeometryOperationResult replace_result =
    cxgeom::CxGeometryOperations::ReplaceGeometryPayload(add_result.revisions);
  if (!Check(replace_result.revisions.geometry == 2, "replace should increment geometry revision"))
  {
    return 1;
  }
  if (!Check(replace_result.decision.action == cxgeom::CxRefreshAction::RebuildGeometryPresentation,
             "replace should rebuild geometry presentation"))
  {
    return 1;
  }

  const cxgeom::CxGeometryOperationResult remove_result =
    cxgeom::CxGeometryOperations::RemoveGeometry(replace_result.revisions);
  if (!Check(remove_result.revisions.geometry == 3, "remove should increment geometry revision"))
  {
    return 1;
  }
  if (!Check(remove_result.decision.action == cxgeom::CxRefreshAction::RebuildGeometryPresentation,
             "remove should rebuild geometry presentation"))
  {
    return 1;
  }

  std::cout << "[cxgeom_operations_smoke] ok" << '\n';
  return 0;
}
