#include "CxCurveBuilder.h"
#include "CxGeometryItem.h"

#include <chrono>
#include <iostream>
#include <vector>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxgeom_bulk_create_smoke] " << message << '\n';
    return false;
  }

  return true;
}

using Clock = std::chrono::steady_clock;

double MillisecondsBetween(const Clock::time_point& begin, const Clock::time_point& end)
{
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

bool RunCase(int count)
{
  cxgeom::CxCurveBuilder builder;
  std::vector<cxgeom::CxShapeHandle> payloads;
  std::vector<cxgeom::CxGeometryItem> items;
  payloads.reserve(static_cast<std::size_t>(count));
  items.reserve(static_cast<std::size_t>(count));

  const Clock::time_point payload_begin = Clock::now();
  for (int index = 0; index < count; ++index)
  {
    payloads.push_back(builder.BuildLine(index + 1,
                                         "bulk_line",
                                         static_cast<double>(index),
                                         0.0,
                                         0.0,
                                         static_cast<double>(index) + 1.0,
                                         0.0,
                                         0.0));
  }
  const Clock::time_point payload_end = Clock::now();

  const Clock::time_point item_begin = Clock::now();
  for (const cxgeom::CxShapeHandle& payload : payloads)
  {
    items.emplace_back(payload);
  }
  const Clock::time_point item_end = Clock::now();

  int valid_payloads = 0;
  for (const cxgeom::CxShapeHandle& payload : payloads)
  {
    if (!payload.IsNull())
    {
      ++valid_payloads;
    }
  }

  if (!Check(static_cast<int>(payloads.size()) == count, "payload count mismatch"))
  {
    return false;
  }

  if (!Check(static_cast<int>(items.size()) == count, "item count mismatch"))
  {
    return false;
  }

  if (!Check(valid_payloads == count, "all payloads should be valid"))
  {
    return false;
  }

  if (!Check(items.front().EntityId() == 1, "first entity id mismatch"))
  {
    return false;
  }

  if (!Check(items.back().EntityId() == count, "last entity id mismatch"))
  {
    return false;
  }

  std::cout << "[cxgeom_bulk_create_smoke]"
            << " count=" << count
            << " payload_ms=" << MillisecondsBetween(payload_begin, payload_end)
            << " item_ms=" << MillisecondsBetween(item_begin, item_end)
            << " valid_payloads=" << valid_payloads
            << '\n';
  return true;
}
}

int main()
{
  if (!RunCase(10))
  {
    return 1;
  }

  if (!RunCase(100))
  {
    return 1;
  }

  if (!RunCase(1000))
  {
    return 1;
  }

  std::cout << "[cxgeom_bulk_create_smoke] ok" << '\n';
  return 0;
}
