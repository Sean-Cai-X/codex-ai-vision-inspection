#include "CxSceneMapping.h"

namespace cxgeom
{
CxGeometrySceneRecord CxSceneMapping::MakeRecord(const CxGeometryItem& item)
{
  CxGeometrySceneRecord record;
  record.entity_id = item.EntityId();
  record.shape_kind = item.Payload().Kind();
  record.has_payload = !item.Payload().IsNull();
  record.has_presentation = item.Presentation().HasPresentation();
  record.visible = item.Style().visible;
  record.geometry_revision = item.Revision().geometry;
  return record;
}

CxGeometrySceneRecord CxSceneMapping::MakeRecord(const CxGeometryItem& item,
                                                 const CxGeometryOperationResult& operation)
{
  CxGeometrySceneRecord record = MakeRecord(item);
  record.geometry_revision = operation.revisions.geometry;
  return record;
}

bool CxSceneMapping::CanPublish(const CxGeometrySceneRecord& record)
{
  return record.entity_id > 0 && record.has_payload;
}
}
