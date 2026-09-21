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
    std::cerr << "[cxcloud_bulk_render_smoke] " << message << '\n';
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
                   static_cast<double>(point_index % 11));
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

  const Clock::time_point payload_begin = Clock::now();
  for (int index = 0; index < cloud_count; ++index)
  {
    payloads.push_back(BuildCloud(index + 1, point_count));
  }
  const Clock::time_point payload_end = Clock::now();

  const Clock::time_point render_begin = Clock::now();
  for (const cxcloud::CxCloudHandle& payload : payloads)
  {
    render_data.emplace_back(payload);
  }
  const Clock::time_point render_end = Clock::now();

  int valid_payloads = 0;
  int valid_render_data = 0;
  int valid_bounds = 0;
  for (const cxcloud::CxCloudHandle& payload : payloads)
  {
    if (payload.Data().PointCount() == point_count)
    {
      ++valid_payloads;
    }
  }

  for (const cxcloud::CxCloudRenderData& data : render_data)
  {
    if (data.PointCount() == point_count)
    {
      ++valid_render_data;
    }

    if (data.HasBounds())
    {
      const cxcloud::CxCloudRenderData::CxBounds& bounds = data.Bounds();
      if (bounds.min_x <= bounds.max_x && bounds.min_y <= bounds.max_y && bounds.min_z <= bounds.max_z)
      {
        ++valid_bounds;
      }
    }
  }

  if (!Check(static_cast<int>(payloads.size()) == cloud_count, "payload count mismatch"))
  {
    return false;
  }

  if (!Check(static_cast<int>(render_data.size()) == cloud_count, "render data count mismatch"))
  {
    return false;
  }

  if (!Check(valid_payloads == cloud_count, "all payloads should contain expected point count"))
  {
    return false;
  }

  if (!Check(valid_render_data == cloud_count, "all render data should contain expected point count"))
  {
    return false;
  }

  if (!Check(valid_bounds == cloud_count, "all render data should contain valid bounds"))
  {
    return false;
  }

  std::cout << "[cxcloud_bulk_render_smoke]"
            << " clouds=" << cloud_count
            << " points_per_cloud=" << point_count
            << " payload_ms=" << MillisecondsBetween(payload_begin, payload_end)
            << " render_ms=" << MillisecondsBetween(render_begin, render_end)
            << " valid_payloads=" << valid_payloads
            << " valid_render_data=" << valid_render_data
            << " valid_bounds=" << valid_bounds
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

  std::cout << "[cxcloud_bulk_render_smoke] ok" << '\n';
  return 0;
}
