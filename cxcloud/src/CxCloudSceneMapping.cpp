#include "CxCloudSceneMapping.h"

namespace cxcloud
{
CxCloudSceneRecord CxCloudSceneMapping::MakeRecord(const CxCloudItem& item)
{
  CxCloudSceneRecord record;
  record.entity_id = item.EntityId();
  record.has_payload = item.Payload().Data().PointCount() > 0;
  record.has_render_data = item.RenderData().PointCount() > 0;
  record.has_bounds = item.RenderData().HasBounds();
  record.visible = item.Style().visible;
  record.point_count = item.RenderData().PointCount();
  record.cloud_revision = item.Revision().cloud;
  return record;
}

CxCloudSceneRecord CxCloudSceneMapping::MakeRecord(const CxCloudItem& item,
                                                   const CxCloudOperationResult& operation)
{
  CxCloudSceneRecord record = MakeRecord(item);
  record.cloud_revision = operation.revision.cloud;
  return record;
}

bool CxCloudSceneMapping::CanPublish(const CxCloudSceneRecord& record)
{
  return record.entity_id > 0 && record.has_payload && record.has_render_data;
}
}
