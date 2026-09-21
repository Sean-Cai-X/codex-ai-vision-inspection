#include "CxCloudHandle.h"
#include "CxCloudRenderData.h"

#include <chrono>
#include <iostream>
#include <vector>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxcloud_bulk_release_smoke] " << message << '\n';
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
                   static_cast<double>(point_index % 13));
  }

  cloud.SyncFromNative();
  return cloud;
}

bool RunCase(int cloud_count, int point_count)
{
  std::vector<cxcloud::CxCloudHandle> payloads;
  std::vector<cxcloud::CxCloudRenderData> render_data;
  payloads.reserve(static_cast<std::size_t>(cloud_count));
  render_data.reserve(static_cast<std::size_t>(cloud_count));

  for (int index = 0; index < cloud_count; ++index)
  {
    payloads.push_back(BuildCloud(index + 1, point_count));
  }

  for (const cxcloud::CxCloudHandle& payload : payloads)
  {
    render_data.emplace_back(payload);
  }

  if (!Check(static_cast<int>(payloads.size()) == cloud_count, "payload count mismatch before release"))
  {
    return false;
  }

  if (!Check(static_cast<int>(render_data.size()) == cloud_count, "render data count mismatch before release"))
  {
    return false;
  }

  const Clock::time_point release_begin = Clock::now();
  render_data.clear();
  payloads.clear();
  const Clock::time_point release_end = Clock::now();

  if (!Check(payloads.empty(), "payloads should be empty after release"))
  {
    return false;
  }

  if (!Check(render_data.empty(), "render data should be empty after release"))
  {
    return false;
  }

  std::cout << "[cxcloud_bulk_release_smoke]"
            << " clouds=" << cloud_count
            << " points_per_cloud=" << point_count
            << " release_ms=" << MillisecondsBetween(release_begin, release_end)
            << " remaining_payloads=" << payloads.size()
            << " remaining_render_data=" << render_data.size()
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

  std::cout << "[cxcloud_bulk_release_smoke] ok" << '\n';
  return 0;
}
