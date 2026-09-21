#pragma once

#include "CxCloudItem.h"

namespace cxcloud
{
struct CxCloudSceneRecord
{
  int entity_id = 0;
  bool has_payload = false;
  bool has_render_data = false;
  bool has_bounds = false;
  bool visible = true;
  int point_count = 0;
  std::uint64_t cloud_revision = 0;
};

class CxCloudSceneMapping
{
public:
  static CxCloudSceneRecord MakeRecord(const CxCloudItem& item);
  static CxCloudSceneRecord MakeRecord(const CxCloudItem& item,
                                       const CxCloudOperationResult& operation);
  static bool CanPublish(const CxCloudSceneRecord& record);
};
}
