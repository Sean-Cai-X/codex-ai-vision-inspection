#include "CxCloudItem.h"

#include <chrono>
#include <iostream>
#include <vector>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxcloud_bulk_create_smoke] " << message << '\n';
    return false;
  }

  return true;
}

using Clock = std::chrono::steady_clock;

double MillisecondsBetween(const Clock::time_point& begin, const Clock::time_point& end)
{
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

cxcloud::CxCloudHandle BuildCloud(int entity_id, int point_count)
{
  cxcloud::CxCloudHandle cloud(entity_id, "bulk_cloud", cxcloud::CxPointCloudData());
  cloud.Reserve(point_count);
  for (int point_index = 0; point_index < point_count; ++point_index)
  {
    cloud.AddPoint(static_cast<double>(point_index),
                   static_cast<double>(entity_id),
                   static_cast<double>(point_index % 7));
  }
  cloud.SyncFromNative();
  return cloud;
}

bool RunCase(int cloud_count, int point_count)
{
  std::vector<cxcloud::CxCloudHandle> payloads;
  std::vector<cxcloud::CxCloudItem> items;
  payloads.reserve(static_cast<std::size_t>(cloud_count));
  items.reserve(static_cast<std::size_t>(cloud_count));

  const Clock::time_point payload_begin = Clock::now();
  for (int index = 0; index < cloud_count; ++index)
  {
    payloads.push_back(BuildCloud(index + 1, point_count));
  }
  const Clock::time_point payload_end = Clock::now();

  const Clock::time_point item_begin = Clock::now();
  for (const cxcloud::CxCloudHandle& payload : payloads)
  {
    items.emplace_back(payload);
  }
  const Clock::time_point item_end = Clock::now();

  int valid_payloads = 0;
  for (const cxcloud::CxCloudHandle& payload : payloads)
  {
    if (payload.Data().PointCount() == point_count)
    {
      ++valid_payloads;
    }
  }

  if (!Check(static_cast<int>(payloads.size()) == cloud_count, "payload count mismatch"))
  {
    return false;
  }

  if (!Check(static_cast<int>(items.size()) == cloud_count, "item count mismatch"))
  {
    return false;
  }

  if (!Check(valid_payloads == cloud_count, "all payloads should contain expected point count"))
  {
    return false;
  }

  if (!Check(items.front().EntityId() == 1, "first entity id mismatch"))
  {
    return false;
  }

  if (!Check(items.back().EntityId() == cloud_count, "last entity id mismatch"))
  {
    return false;
  }

  if (!Check(items.front().RenderData().PointCount() == point_count, "render data point count mismatch"))
  {
    return false;
  }

  std::cout << "[cxcloud_bulk_create_smoke]"
            << " clouds=" << cloud_count
            << " points_per_cloud=" << point_count
            << " payload_ms=" << MillisecondsBetween(payload_begin, payload_end)
            << " item_ms=" << MillisecondsBetween(item_begin, item_end)
            << " valid_payloads=" << valid_payloads
            << '\n';
  return true;
}
}

int main()
{
  if (!RunCase(10, 100))
  {
    return 1;
  }

  if (!RunCase(50, 100))
  {
    return 1;
  }

  if (!RunCase(100, 1000))
  {
    return 1;
  }

  std::cout << "[cxcloud_bulk_create_smoke] ok" << '\n';
  return 0;
}
