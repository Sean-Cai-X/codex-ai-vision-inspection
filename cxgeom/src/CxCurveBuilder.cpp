#include "CxCurveBuilder.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <GC_MakeCircle.hxx>
#include <GC_MakeEllipse.hxx>
#include <GC_MakeSegment.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Pnt.hxx>
#include <gp.hxx>

namespace cxgeom
{
CxShapeHandle CxCurveBuilder::BuildLine(int entity_id, const char* name) const
{
  return BuildLine(entity_id, name, 0.0, 0.0, 0.0, 100.0, 0.0, 0.0);
}

CxShapeHandle CxCurveBuilder::BuildCircle(int entity_id, const char* name) const
{
  return BuildCircle(entity_id, name, 0.0, 0.0, 0.0, 25.0);
}

CxShapeHandle CxCurveBuilder::BuildEllipse(int entity_id, const char* name) const
{
  return BuildEllipse(entity_id, name, 0.0, 0.0, 0.0, 40.0, 20.0);
}

CxShapeHandle CxCurveBuilder::BuildLine(int entity_id,
                                        const char* name,
                                        double x0,
                                        double y0,
                                        double z0,
                                        double x1,
                                        double y1,
                                        double z1) const
{
  CxShapeHandle shape(entity_id, name ? name : "line", CxShapeKind::Curve);
  GC_MakeSegment make_segment(gp_Pnt(x0, y0, z0), gp_Pnt(x1, y1, z1));
  if (make_segment.IsDone())
  {
    shape.SetNativeShape(BRepBuilderAPI_MakeEdge(make_segment.Value()));
  }
  return shape;
}

CxShapeHandle CxCurveBuilder::BuildCircle(int entity_id,
                                          const char* name,
                                          double center_x,
                                          double center_y,
                                          double center_z,
                                          double radius) const
{
  CxShapeHandle shape(entity_id, name ? name : "circle", CxShapeKind::Curve);
  GC_MakeCircle make_circle(gp_Ax2(gp_Pnt(center_x, center_y, center_z), gp::DZ()), radius);
  if (make_circle.IsDone())
  {
    shape.SetNativeShape(BRepBuilderAPI_MakeEdge(make_circle.Value()));
  }
  return shape;
}

CxShapeHandle CxCurveBuilder::BuildEllipse(int entity_id,
                                           const char* name,
                                           double center_x,
                                           double center_y,
                                           double center_z,
                                           double major_radius,
                                           double minor_radius) const
{
  CxShapeHandle shape(entity_id, name ? name : "ellipse", CxShapeKind::Curve);
  GC_MakeEllipse make_ellipse(gp_Ax2(gp_Pnt(center_x, center_y, center_z), gp::DZ()),
                              major_radius,
                              minor_radius);
  if (make_ellipse.IsDone())
  {
    shape.SetNativeShape(BRepBuilderAPI_MakeEdge(make_ellipse.Value()));
  }
  return shape;
}
}
