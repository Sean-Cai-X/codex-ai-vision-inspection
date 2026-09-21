#pragma once

#include "CxRefreshDecision.h"

namespace cxgeom
{
enum class CxGeometryOperationKind
{
  Add = 0,
  UpdateStyle,
  ReplacePayload,
  Remove
};

struct CxGeometryOperationResult
{
  CxGeometryOperationKind kind = CxGeometryOperationKind::Add;
  CxSceneRevision revisions;
  std::uint32_t dirty_flags = CxDirtyNone;
  CxRefreshDecision decision;
};

class CxGeometryOperations
{
public:
  static CxGeometryOperationResult AddGeometry(const CxSceneRevision& previous);
  static CxGeometryOperationResult UpdateGeometryStyle(const CxSceneRevision& previous,
                                                       bool visibility_only = false);
  static CxGeometryOperationResult ReplaceGeometryPayload(const CxSceneRevision& previous);
  static CxGeometryOperationResult RemoveGeometry(const CxSceneRevision& previous);
};
}
