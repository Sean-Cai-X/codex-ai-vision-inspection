#include "CxCurveBuilder.h"
#include "CxGeomPresentation.h"
#include "CxGeomRenderStyle.h"
#include "CxOcctConvert.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxgeom_presentation_smoke] " << message << '\n';
    return false;
  }

  return true;
}
}

int main()
{
  cxgeom::CxCurveBuilder builder;
  const cxgeom::CxShapeHandle line =
    builder.BuildLine(1, "line", 0.0, 0.0, 0.0, 10.0, 0.0, 0.0);

  if (!Check(!line.IsNull(), "line shape should be created"))
  {
    return 1;
  }

  cxgeom::CxGeomRenderStyle style;
  style.red = 0.2;
  style.green = 0.7;
  style.blue = 0.3;
  style.line_width = 2.5;

  cxgeom::CxOcctConvert convert;
  cxgeom::CxGeomPresentation presentation = convert.MakePresentation(line, style);
  if (!Check(presentation.HasPresentation(), "presentation should wrap AIS object"))
  {
    return 1;
  }

  if (!Check(presentation.ApplyStyle(), "presentation style should apply"))
  {
    return 1;
  }

  std::cout << "[cxgeom_presentation_smoke] ok" << '\n';
  return 0;
}
