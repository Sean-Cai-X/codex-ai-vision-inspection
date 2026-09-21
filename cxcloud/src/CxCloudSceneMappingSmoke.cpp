#include "CxCloudHandle.h"
#include "CxCloudItem.h"
#include "CxCloudOperations.h"
#include "CxCloudSceneMapping.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxcloud_scene_mapping_smoke] " << message << '\n';
    return false;
  }

  return true;
}

cxcloud::CxCloudHandle BuildCloud()
{
  cxcloud::CxCloudHandle cloud(9, "scene_cloud", cxcloud::CxPointCloudData());
  cloud.Reserve(4);
  cloud.AddPoint(0.0, 0.0, 0.0);
  cloud.AddPoint(1.0, 0.0, 0.0);
  cloud.AddPoint(0.0, 1.0, 0.0);
  cloud.AddPoint(0.0, 0.0, 1.0);
  cloud.SyncFromNative();
  return cloud;
}
}

int main()
{
  cxcloud::CxCloudHandle cloud = BuildCloud();
  cxcloud::CxCloudItem item(cloud);

  cxcloud::CxCloudRenderStyle style;
  style.visible = true;
  item.SetStyle(style);

  const cxcloud::CxCloudOperationResult add_result =
    cxcloud::CxCloudOperations::AddCloud(item.Revision());
  item.SetRevision(add_result.revision);

  const cxcloud::CxCloudSceneRecord record =
    cxcloud::CxCloudSceneMapping::MakeRecord(item, add_result);

  if (!Check(record.entity_id == 9, "entity id mismatch"))
  {
    return 1;
  }

  if (!Check(record.has_payload, "record should expose payload"))
  {
    return 1;
  }

  if (!Check(record.has_render_data, "record should expose render data"))
  {
    return 1;
  }

  if (!Check(record.has_bounds, "record should expose bounds"))
  {
    return 1;
  }

  if (!Check(record.point_count == 4, "point count mismatch"))
  {
    return 1;
  }

  if (!Check(record.cloud_revision == 1, "cloud revision mismatch"))
  {
    return 1;
  }

  if (!Check(cxcloud::CxCloudSceneMapping::CanPublish(record), "record should be publishable"))
  {
    return 1;
  }

  std::cout << "[cxcloud_scene_mapping_smoke]"
            << " entity_id=" << record.entity_id
            << " has_payload=" << record.has_payload
            << " has_render_data=" << record.has_render_data
            << " has_bounds=" << record.has_bounds
            << " point_count=" << record.point_count
            << " cloud_revision=" << record.cloud_revision
            << '\n';
  std::cout << "[cxcloud_scene_mapping_smoke] ok" << '\n';
  return 0;
}
