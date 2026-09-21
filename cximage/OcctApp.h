#pragma once

#include "Window.h"
#include "../../cxgeom/include/CxSetCircleBuild.h"
#include "../../cxgeom/include/CxSetCircleDisplay.h"
#include "../../cxgeom/include/CxSetLineBuild.h"
#include "../../cxgeom/include/CxSetLineDisplay.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_PointCloud.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include <string>
#include <vector>

struct CxOcctPointCloudPoint
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  bool has_normal = false;
  double nx = 0.0;
  double ny = 0.0;
  double nz = 1.0;
};

struct CxOcctPointCloudStyle
{
  double point_size = 2.0;
  bool visible = true;
  bool color_by_normals = false;
};

struct CxOcctPointCloudRequest
{
  int entity_id = 0;
  std::string name;
  std::vector<CxOcctPointCloudPoint> points;
  CxOcctPointCloudStyle style;
};

struct CxOcctPointCloudBody
{
  int entity_id = 0;
  std::string name;
  int point_count = 0;
  bool has_normals = false;
  bool visible = true;
  double point_size = 2.0;
};

struct CxOcctPointCloudAnnotation
{
  int annotation_id = 0;
  int target_entity_id = 0;
  std::string text;
  double anchor_x = 0.0;
  double anchor_y = 0.0;
  double anchor_z = 0.0;
  bool visible = true;
};

struct CxOcctPointCloudBatchRequest
{
  int batch_id = 0;
  std::string name;
  std::vector<CxOcctPointCloudRequest> clouds;
  CxOcctPointCloudStyle style;
};

struct CxOcctPointCloudBatchResult
{
  int batch_id = 0;
  int source_count = 0;
  int point_count = 0;
  bool has_normals = false;
  CxOcctPointCloudStyle style;
  Handle(AIS_PointCloud) presentation;
  bool success = false;
};

struct CxOcctPointCloudResult
{
  int entity_id = 0;
  int point_count = 0;
  bool has_normals = false;
  CxOcctPointCloudStyle style;
  Handle(AIS_PointCloud) presentation;
  bool success = false;
};

struct CxOcctPointCloudAnnotationDisplayRequest
{
  int layer_id = 0;
  std::vector<CxOcctPointCloudAnnotation> annotations;
  bool visible = true;
};

struct CxOcctPointCloudAnnotationDisplayResult
{
  int layer_id = 0;
  int annotation_count = 0;
  bool visible = true;
  std::vector<Handle(AIS_TextLabel)> labels;
  bool success = false;
};

class OcctApp
{
public:
  void Run();
  cxgeom::CxSetCircleBuildResult BuildSetCircleGeometry(const cxgeom::CxSetCircleRequest& request) const;
  cxgeom::CxSetCircleDisplayResult BuildSetCircleDisplayBatch(const cxgeom::CxSetCircleDisplayRequest& request) const;
  bool DisplaySetCircleBatch(const cxgeom::CxSetCircleDisplayResult& result);
  cxgeom::CxSetLineBuildResult BuildSetLineGeometry(const cxgeom::CxSetLineRequest& request) const;
  cxgeom::CxSetLineDisplayResult BuildSetLineDisplayBatch(const cxgeom::CxSetLineDisplayRequest& request) const;
  bool DisplaySetLineBatch(const cxgeom::CxSetLineDisplayResult& result);

  CxOcctPointCloudBody BuildPointCloudBody(const CxOcctPointCloudRequest& request) const;
  CxOcctPointCloudResult BuildPointCloudPresentation(const CxOcctPointCloudRequest& request) const;
  CxOcctPointCloudBatchResult BuildPointCloudBatchPresentation(const CxOcctPointCloudBatchRequest& request) const;
  bool DisplayPointCloud(const CxOcctPointCloudResult& result);
  bool DisplayPointCloudBatch(const CxOcctPointCloudBatchResult& result);
  CxOcctPointCloudAnnotationDisplayResult BuildPointCloudAnnotationDisplay(const CxOcctPointCloudAnnotationDisplayRequest& request) const;
  bool DisplayPointCloudAnnotations(const CxOcctPointCloudAnnotationDisplayResult& result);

private:
  static void OnFramebufferResize(GLFWwindow* theWindow, int theWidth, int theHeight);
  static void OnMouseButton(GLFWwindow* theWindow, int theButton, int theAction, int theMods);
  static void OnCursorPos(GLFWwindow* theWindow, double theX, double theY);
  static void OnScroll(GLFWwindow* theWindow, double theOffsetX, double theOffsetY);

  void InitWindow();
  void InitOcctViewer();
  void InitScene();
  void InitImGui();
  void Cleanup();
  void MainLoop();
  void DrawUi();
  void Resize(int theWidth, int theHeight);

  Handle(Window) myWindow;
  Handle(Aspect_DisplayConnection) myDisplayConnection;
  Handle(OpenGl_GraphicDriver) myGraphicDriver;
  Handle(V3d_Viewer) myViewer;
  Handle(V3d_View) myView;
  Handle(AIS_InteractiveContext) myContext;
  Handle(AIS_Shape) myBox;

  bool myLeftPressed = false;
  bool myRightPressed = false;
  double myLastX = 0.0;
  double myLastY = 0.0;
  int myViewportWidth = 1280;
  int myViewportHeight = 720;
};