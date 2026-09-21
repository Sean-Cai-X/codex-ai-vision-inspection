#pragma once

#include "CxShapeHandle.h"

#include <gp_Pnt.hxx>

#include <cstddef>
#include <string>
#include <vector>

namespace cxgeom
{
struct CxGeomBounds
{
  double min_x = 0.0;
  double min_y = 0.0;
  double min_z = 0.0;
  double max_x = 0.0;
  double max_y = 0.0;
  double max_z = 0.0;
  bool valid = false;
};

struct CxGeomElement
{
  int entity_id = 0;
  std::string entity_type = "geom";
  std::string name;
  std::string source_stage = "runtime";
  std::string status = "ok";
  CxShapeKind kind = CxShapeKind::Unknown;
  CxShapeHandle shape;
  CxShapeHandle measure_shape;
  CxGeomBounds bbox;
  double confidence = 0.0;
  bool success = false;
  bool visible = true;
};

struct CxCurveElement
{
  CxGeomElement base;
  std::string curve_type = "curve";
  gp_Pnt start_point;
  gp_Pnt end_point;
  gp_Pnt center_point;
  double length_hint = 0.0;
  bool closed = false;
};

struct CxSurfaceElement
{
  CxGeomElement base;
  std::string surface_type = "surface";
  std::vector<gp_Pnt> outer_boundary;
  double area_hint = 0.0;
};

struct CxGeomBatchElement
{
  int batch_id = 0;
  std::string element_type = "batch";
  std::string name;
  std::string source_stage = "runtime";
  std::string status = "ok";
  std::vector<CxShapeHandle> shapes;
  CxGeomBounds bbox;
  double confidence = 0.0;
  bool success = false;
  std::size_t source_count = 0;
  bool visible = true;
};

class CxGeomElementBody
{
public:
  static CxGeomElement MakeElement(const CxShapeHandle& shape, bool visible = true);
  static CxCurveElement MakeCurveElement(const CxShapeHandle& shape,
                                         double length_hint,
                                         bool closed,
                                         bool visible = true);
  static CxSurfaceElement MakeSurfaceElement(const CxShapeHandle& shape,
                                             double area_hint,
                                             bool visible = true);
  static CxGeomBatchElement MakeBatchElement(int batch_id,
                                             const char* name,
                                             const std::vector<CxShapeHandle>& shapes,
                                             bool visible = true);
};
}
