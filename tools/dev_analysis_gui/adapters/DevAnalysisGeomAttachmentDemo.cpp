#include "DevAnalysisGeomAttachmentDemo.h"

#include "../../../cxgeom/include/CxGeomAnnotationBody.h"
#include "../../../cxgeom/include/CxGeomElementBody.h"

namespace
{
DevAnalysisDemoElement MakeDemoElement(const cxgeom::CxGeomElement& element,
                                       float x,
                                       float y,
                                       float width,
                                       float height)
{
  DevAnalysisDemoElement demo;
  demo.entity_id = element.entity_id;
  demo.name = element.name;
  demo.entity_type = element.entity_type;
  switch (element.kind)
  {
  case cxgeom::CxShapeKind::Curve:
    demo.kind_label = "Curve";
    break;
  case cxgeom::CxShapeKind::Wire:
    demo.kind_label = "Wire";
    break;
  case cxgeom::CxShapeKind::Face:
    demo.kind_label = "Face";
    break;
  case cxgeom::CxShapeKind::Solid:
    demo.kind_label = "Solid";
    break;
  default:
    demo.kind_label = "Unknown";
    break;
  }
  demo.source_stage = element.source_stage;
  demo.status = element.status;
  demo.x = x;
  demo.y = y;
  demo.width = width;
  demo.height = height;
  demo.visible = element.visible;
  demo.interactive = true;
  return demo;
}

DevAnalysisDemoAnnotation MakeDemoAnnotation(const cxgeom::CxGeomAnnotation& annotation,
                                             float label_dx,
                                             float label_dy,
                                             const char* layer_name)
{
  DevAnalysisDemoAnnotation demo;
  demo.annotation_id = annotation.annotation_id;
  demo.target_entity_id = annotation.target_entity_id;
  demo.annotation_type = annotation.annotation_type;
  demo.text = annotation.text;
  demo.anchor_x = static_cast<float>(annotation.anchor_x);
  demo.anchor_y = static_cast<float>(annotation.anchor_y);
  demo.label_dx = label_dx;
  demo.label_dy = label_dy;
  demo.layer_name = layer_name != nullptr ? layer_name : "label";
  demo.visible = annotation.visible;
  return demo;
}

DevAnalysisGeomAttachmentScene BuildEnsmallenScene()
{
  const cxgeom::CxShapeHandle circle_shape(101, "fit_circle", cxgeom::CxShapeKind::Curve);
  const cxgeom::CxShapeHandle roi_shape(102, "roi_window", cxgeom::CxShapeKind::Wire);
  const cxgeom::CxShapeHandle param_shape(103, "best_param_path", cxgeom::CxShapeKind::Curve);

  cxgeom::CxGeomElement circle = cxgeom::CxGeomElementBody::MakeElement(circle_shape);
  circle.source_stage = "postprocess";
  circle.status = "ok";

  cxgeom::CxGeomElement roi = cxgeom::CxGeomElementBody::MakeElement(roi_shape);
  roi.source_stage = "preprocess";
  roi.status = "ok";

  cxgeom::CxGeomElement param = cxgeom::CxGeomElementBody::MakeElement(param_shape);
  param.source_stage = "learning_input";
  param.status = "ok";

  DevAnalysisGeomAttachmentScene scene;
  scene.elements = {
    MakeDemoElement(circle, 0.14f, 0.18f, 0.26f, 0.26f),
    MakeDemoElement(roi, 0.08f, 0.10f, 0.46f, 0.48f),
    MakeDemoElement(param, 0.52f, 0.56f, 0.26f, 0.10f)
  };
  scene.elements[0].kind_label = "Circle";
  scene.elements[1].kind_label = "ROI";
  scene.elements[2].kind_label = "Line";

  const cxgeom::CxGeomAnnotation circle_annotation =
    cxgeom::CxGeomAnnotationBody::MakeAnnotation(1, circle.entity_id, 0, "circle score 0.91", 0.27, 0.31, 0.0);
  const cxgeom::CxGeomAnnotation roi_annotation =
    cxgeom::CxGeomAnnotationBody::MakeAnnotation(2, roi.entity_id, 0, "ROI anchor", 0.31, 0.15, 0.0);
  const cxgeom::CxGeomAnnotation param_annotation =
    cxgeom::CxGeomAnnotationBody::MakeAnnotation(3, param.entity_id, 0, "param delta 0.08", 0.65, 0.61, 0.0);

  scene.annotations = {
    MakeDemoAnnotation(circle_annotation, 82.0f, -34.0f, "label"),
    MakeDemoAnnotation(roi_annotation, 64.0f, -46.0f, "anchor"),
    MakeDemoAnnotation(param_annotation, 56.0f, 22.0f, "summary")
  };
  scene.attachment_rule = "Text labels attach to the element record by entity_id, while anchor points stay in the scene layer as lightweight view metadata.";
  scene.interaction_rule = "Hover and select should resolve entity first, then expose annotation and summary overlays without mutating the geometry payload.";
  return scene;
}

DevAnalysisGeomAttachmentScene BuildMlpackScene()
{
  const cxgeom::CxShapeHandle feature_shape(201, "feature_patch", cxgeom::CxShapeKind::Face);
  const cxgeom::CxShapeHandle vector_shape(202, "baseline_vector", cxgeom::CxShapeKind::Curve);
  const cxgeom::CxShapeHandle gate_shape(203, "class_gate", cxgeom::CxShapeKind::Wire);

  cxgeom::CxGeomElement feature = cxgeom::CxGeomElementBody::MakeElement(feature_shape);
  feature.source_stage = "postprocess";

  cxgeom::CxGeomElement vector = cxgeom::CxGeomElementBody::MakeElement(vector_shape);
  vector.source_stage = "learning_input";

  cxgeom::CxGeomElement gate = cxgeom::CxGeomElementBody::MakeElement(gate_shape);
  gate.source_stage = "train_or_infer_ready";

  DevAnalysisGeomAttachmentScene scene;
  scene.elements = {
    MakeDemoElement(feature, 0.12f, 0.24f, 0.24f, 0.20f),
    MakeDemoElement(vector, 0.44f, 0.32f, 0.36f, 0.10f),
    MakeDemoElement(gate, 0.66f, 0.54f, 0.18f, 0.18f)
  };
  scene.elements[0].kind_label = "Feature Patch";
  scene.elements[1].kind_label = "Vector Lane";
  scene.elements[2].kind_label = "Boundary";

  const cxgeom::CxGeomAnnotation feature_annotation =
    cxgeom::CxGeomAnnotationBody::MakeAnnotation(4, feature.entity_id, 0, "24 dims exported", 0.24, 0.30, 0.0);
  const cxgeom::CxGeomAnnotation vector_annotation =
    cxgeom::CxGeomAnnotationBody::MakeAnnotation(5, vector.entity_id, 0, "baseline logreg", 0.62, 0.37, 0.0);
  const cxgeom::CxGeomAnnotation gate_annotation =
    cxgeom::CxGeomAnnotationBody::MakeAnnotation(6, gate.entity_id, 0, "confidence 0.83", 0.75, 0.63, 0.0);

  scene.annotations = {
    MakeDemoAnnotation(feature_annotation, 70.0f, -30.0f, "label"),
    MakeDemoAnnotation(vector_annotation, 54.0f, -26.0f, "anchor"),
    MakeDemoAnnotation(gate_annotation, 50.0f, 24.0f, "summary")
  };
  scene.attachment_rule = "Feature and baseline summaries should attach above the mapped output lane, not become part of the mlpack feature payload itself.";
  scene.interaction_rule = "Selection should reveal feature provenance, baseline score, and case summary in parallel.";
  return scene;
}

DevAnalysisGeomAttachmentScene BuildTorchScene()
{
  const cxgeom::CxShapeHandle batch_shape(301, "sample_grid", cxgeom::CxShapeKind::Face);
  const cxgeom::CxShapeHandle bridge_shape(302, "label_bridge", cxgeom::CxShapeKind::Curve);
  const cxgeom::CxShapeHandle gate_shape(303, "model_gate", cxgeom::CxShapeKind::Solid);

  cxgeom::CxGeomElement batch = cxgeom::CxGeomElementBody::MakeElement(batch_shape);
  batch.source_stage = "preprocess";

  cxgeom::CxGeomElement bridge = cxgeom::CxGeomElementBody::MakeElement(bridge_shape);
  bridge.source_stage = "deep_model_input";

  cxgeom::CxGeomElement gate = cxgeom::CxGeomElementBody::MakeElement(gate_shape);
  gate.source_stage = "train_or_infer_ready";
  gate.status = "pending";

  DevAnalysisGeomAttachmentScene scene;
  scene.elements = {
    MakeDemoElement(batch, 0.10f, 0.16f, 0.32f, 0.28f),
    MakeDemoElement(bridge, 0.46f, 0.34f, 0.26f, 0.08f),
    MakeDemoElement(gate, 0.70f, 0.48f, 0.16f, 0.22f)
  };
  scene.elements[0].kind_label = "Image Batch";
  scene.elements[1].kind_label = "Contract Line";
  scene.elements[2].kind_label = "Model Gate";

  const cxgeom::CxGeomAnnotation batch_annotation =
    cxgeom::CxGeomAnnotationBody::MakeAnnotation(7, batch.entity_id, 0, "8 samples aligned", 0.26, 0.24, 0.0);
  const cxgeom::CxGeomAnnotation bridge_annotation =
    cxgeom::CxGeomAnnotationBody::MakeAnnotation(8, bridge.entity_id, 0, "label contract ok", 0.58, 0.38, 0.0);
  const cxgeom::CxGeomAnnotation gate_annotation =
    cxgeom::CxGeomAnnotationBody::MakeAnnotation(9, gate.entity_id, 0, "infer ready pending", 0.78, 0.59, 0.0);

  scene.annotations = {
    MakeDemoAnnotation(batch_annotation, 72.0f, -34.0f, "label"),
    MakeDemoAnnotation(bridge_annotation, 58.0f, -28.0f, "anchor"),
    MakeDemoAnnotation(gate_annotation, 46.0f, 24.0f, "summary")
  };
  scene.attachment_rule = "Deep-model labels should hang off the bridge and gate nodes so human review sees readiness without mixing GUI state into tensor state.";
  scene.interaction_rule = "When the gate is selected, the GUI should foreground handoff blockers before exposing train or infer actions.";
  return scene;
}
}

DevAnalysisGeomAttachmentScene BuildDevAnalysisGeomAttachmentScene(DevAnalysisChainId chain_id)
{
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    return BuildEnsmallenScene();
  case DevAnalysisChainId::CxcoreToMlpack:
    return BuildMlpackScene();
  case DevAnalysisChainId::CxcoreToTorch:
    return BuildTorchScene();
  }

  return {};
}
