#pragma once

#include "CxGeometryItem.h"
#include "CxGeometryOperations.h"

namespace cxgeom
{
struct CxGeometrySceneRecord
{
  int entity_id = 0;
  CxShapeKind shape_kind = CxShapeKind::Unknown;
  bool has_payload = false;
  bool has_presentation = false;
  bool visible = true;
  std::uint64_t geometry_revision = 0;
};

class CxSceneMapping
{
public:
  static CxGeometrySceneRecord MakeRecord(const CxGeometryItem& item);
  static CxGeometrySceneRecord MakeRecord(const CxGeometryItem& item,
                                          const CxGeometryOperationResult& operation);
  static bool CanPublish(const CxGeometrySceneRecord& record);
};
}
