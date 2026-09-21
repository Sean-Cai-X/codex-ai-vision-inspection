#include "CxCurveBuilder.h"
#include "CxGeomRenderStyle.h"
#include "CxOcctConvert.h"

#include <chrono>
#include <iostream>
#include <vector>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxgeom_bulk_release_smoke] " << message << '\n';
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
  cxgeom::CxOcctConvert convert;
  std::vector<cxgeom::CxShapeHandle> payloads;
  std::vector<cxgeom::CxGeomPresentation> presentations;
  payloads.reserve(static_cast<std::size_t>(count));
  presentations.reserve(static_cast<std::size_t>(count));

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

  for (int index = 0; index < count; ++index)
  {
    cxgeom::CxGeomRenderStyle style;
    style.red = 0.3;
    style.green = 0.6;
    style.blue = 0.8;
    presentations.push_back(convert.MakePresentation(payloads[static_cast<std::size_t>(index)], style));
  }

  if (!Check(static_cast<int>(payloads.size()) == count, "payload count mismatch before release"))
  {
    return false;
  }

  if (!Check(static_cast<int>(presentations.size()) == count, "presentation count mismatch before release"))
  {
    return false;
  }

  const Clock::time_point release_begin = Clock::now();
  presentations.clear();
  payloads.clear();
  const Clock::time_point release_end = Clock::now();

  if (!Check(payloads.empty(), "payloads should be empty after release"))
  {
    return false;
  }

  if (!Check(presentations.empty(), "presentations should be empty after release"))
  {
    return false;
  }

  std::cout << "[cxgeom_bulk_release_smoke]"
            << " count=" << count
            << " release_ms=" << MillisecondsBetween(release_begin, release_end)
            << " remaining_payloads=" << payloads.size()
            << " remaining_presentations=" << presentations.size()
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

  std::cout << "[cxgeom_bulk_release_smoke] ok" << '\n';
  return 0;
}
