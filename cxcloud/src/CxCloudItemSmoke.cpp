#include "CxCloudItem.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxcloud_item_smoke] " << message << '\n';
    return false;
  }

  return true;
}

cxcloud::CxCloudHandle BuildCloud()
{
  cxcloud::CxCloudHandle cloud(21, "cloud", cxcloud::CxPointCloudData());
  cloud.Reserve(3);
  cloud.AddPoint(0.0, 0.0, 0.0);
  cloud.AddPoint(1.0, 0.0, 0.0);
  cloud.AddPoint(0.0, 1.0, 0.0);
  cloud.SyncFromNative();
  return cloud;
}
}

int main()
{
  cxcloud::CxCloudItem item(BuildCloud());
  if (!Check(item.EntityId() == 21, "cloud item should expose payload entity id"))
  {
    return 1;
  }

  if (!Check(item.RenderData().PointCount() == item.Payload().Data().PointCount(),
             "cloud item render data should mirror payload point count"))
  {
    return 1;
  }

  cxcloud::CxCloudRevision revision;
  revision.cloud = 9;
  item.SetRevision(revision);
  if (!Check(item.Revision().cloud == 9, "cloud item should store revision"))
  {
    return 1;
  }

  std::cout << "[cxcloud_item_smoke] ok" << '\n';
  return 0;
}
