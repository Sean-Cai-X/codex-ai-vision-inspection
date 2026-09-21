#include "CxCurveBuilder.h"
#include "CxGeometryItem.h"
#include "CxOcctConvert.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxgeom_item_smoke] " << message << '\n';
    return false;
  }

  return true;
}
}

int main()
{
  cxgeom::CxCurveBuilder builder;
  const cxgeom::CxShapeHandle line =
    builder.BuildLine(11, "edge", 0.0, 0.0, 0.0, 5.0, 0.0, 0.0);

  cxgeom::CxGeometryItem item(line);
  if (!Check(item.EntityId() == 11, "geometry item should expose payload entity id"))
  {
    return 1;
  }

  cxgeom::CxOcctConvert convert;
  item.SetPresentation(convert.MakePresentation(item.Payload(), item.Style()));
  if (!Check(item.Presentation().HasPresentation(), "geometry item should hold presentation"))
  {
    return 1;
  }

  cxgeom::CxSceneRevision revision;
  revision.geometry = 7;
  item.SetRevision(revision);
  if (!Check(item.Revision().geometry == 7, "geometry item should store revision"))
  {
    return 1;
  }

  std::cout << "[cxgeom_item_smoke] ok" << '\n';
  return 0;
}
