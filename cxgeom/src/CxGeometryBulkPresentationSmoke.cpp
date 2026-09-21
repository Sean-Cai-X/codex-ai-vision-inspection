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
    std::cerr << "[cxgeom_bulk_presentation_smoke] " << message << '\n';
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

  const Clock::time_point presentation_begin = Clock::now();
  for (int index = 0; index < count; ++index)
  {
    cxgeom::CxGeomRenderStyle style;
    style.red = 0.2 + (static_cast<double>(index % 5) * 0.1);
    style.green = 0.4;
    style.blue = 0.7;
    style.line_width = 1.0 + static_cast<double>(index % 3);
    presentations.push_back(convert.MakePresentation(payloads[static_cast<std::size_t>(index)], style));
  }
  const Clock::time_point presentation_end = Clock::now();

  int valid_payloads = 0;
  int valid_presentations = 0;
  for (const cxgeom::CxShapeHandle& payload : payloads)
  {
    if (!payload.IsNull())
    {
      ++valid_payloads;
    }
  }

  for (const cxgeom::CxGeomPresentation& presentation : presentations)
  {
    if (presentation.HasPresentation())
    {
      ++valid_presentations;
    }
  }

  if (!Check(static_cast<int>(payloads.size()) == count, "payload count mismatch"))
  {
    return false;
  }

  if (!Check(static_cast<int>(presentations.size()) == count, "presentation count mismatch"))
  {
    return false;
  }

  if (!Check(valid_payloads == count, "all payloads should be valid"))
  {
    return false;
  }

  if (!Check(valid_presentations == count, "all presentations should be valid"))
  {
    return false;
  }

  std::cout << "[cxgeom_bulk_presentation_smoke]"
            << " count=" << count
            << " payload_ms=" << MillisecondsBetween(payload_begin, payload_end)
            << " presentation_ms=" << MillisecondsBetween(presentation_begin, presentation_end)
            << " valid_payloads=" << valid_payloads
            << " valid_presentations=" << valid_presentations
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

  if (!RunCase(500))
  {
    return 1;
  }

  std::cout << "[cxgeom_bulk_presentation_smoke] ok" << '\n';
  return 0;
}
