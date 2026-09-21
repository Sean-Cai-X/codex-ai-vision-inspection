#pragma once

#include <string>

namespace cxgeom
{
struct CxGeomAnnotation
{
  int annotation_id = 0;
  int target_entity_id = 0;
  int target_batch_id = 0;
  std::string annotation_type = "text";
  std::string status = "ok";
  std::string text;
  double anchor_x = 0.0;
  double anchor_y = 0.0;
  double anchor_z = 0.0;
  bool visible = true;
};

class CxGeomAnnotationBody
{
public:
  static CxGeomAnnotation MakeAnnotation(int annotation_id,
                                         int target_entity_id,
                                         int target_batch_id,
                                         const char* text,
                                         double anchor_x,
                                         double anchor_y,
                                         double anchor_z,
                                         bool visible = true);
};
}
