#include "CxGeomAnnotationBody.h"

namespace cxgeom
{
CxGeomAnnotation CxGeomAnnotationBody::MakeAnnotation(int annotation_id,
                                                      int target_entity_id,
                                                      int target_batch_id,
                                                      const char* text,
                                                      double anchor_x,
                                                      double anchor_y,
                                                      double anchor_z,
                                                      bool visible)
{
  CxGeomAnnotation annotation;
  annotation.annotation_id = annotation_id;
  annotation.target_entity_id = target_entity_id;
  annotation.target_batch_id = target_batch_id;
  annotation.annotation_type = "text";
  annotation.status = "ok";
  annotation.text = text != nullptr ? text : "";
  annotation.anchor_x = anchor_x;
  annotation.anchor_y = anchor_y;
  annotation.anchor_z = anchor_z;
  annotation.visible = visible;
  return annotation;
}
}
