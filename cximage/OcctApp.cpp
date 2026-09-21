#include "OcctApp.h"

#include <glad/glad.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <AIS_InteractiveContext.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <Message.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_PointAspect.hxx>
#include <Quantity_Color.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TColgp_HArray1OfDir.hxx>
#include <TColgp_HArray1OfPnt.hxx>

#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

namespace
{
bool HasPointNormals(const std::vector<CxOcctPointCloudPoint>& points, bool requested)
{
  if (!requested || points.empty())
  {
    return false;
  }

  for (const CxOcctPointCloudPoint& point : points)
  {
    if (!point.has_normal)
    {
      return false;
    }
  }
  return true;
}

Handle(AIS_PointCloud) MakePointCloudPresentation(
    const std::vector<CxOcctPointCloudPoint>& points,
    const CxOcctPointCloudStyle& style,
    const bool hasNormals)
{
  if (points.empty())
  {
    return Handle(AIS_PointCloud)();
  }

  const Standard_Integer pointCount = static_cast<Standard_Integer>(points.size());
  Handle(TColgp_HArray1OfPnt) coords = new TColgp_HArray1OfPnt(1, pointCount);
  Handle(TColgp_HArray1OfDir) normals;
  if (hasNormals)
  {
    normals = new TColgp_HArray1OfDir(1, pointCount);
  }

  for (Standard_Integer index = 1; index <= pointCount; ++index)
  {
    const CxOcctPointCloudPoint& point = points[static_cast<std::size_t>(index - 1)];
    coords->SetValue(index, gp_Pnt(point.x, point.y, point.z));
    if (!normals.IsNull())
    {
      normals->SetValue(index, gp_Dir(point.nx, point.ny, point.nz));
    }
  }

  Handle(AIS_PointCloud) cloud = new AIS_PointCloud();
  cloud->SetPoints(coords, Handle(Quantity_HArray1OfColor)(), normals);
  cloud->SetDisplayMode(AIS_PointCloud::DM_Points);

  const Standard_Real pointScale = style.point_size > 0.0 ? style.point_size : 1.0;
  Handle(Prs3d_PointAspect) pointAspect = new Prs3d_PointAspect(
      Aspect_TOM_POINT,
      Quantity_Color(Quantity_NOC_WHITE),
      pointScale);
  cloud->Attributes()->SetPointAspect(pointAspect);
  return cloud;
}
}

void OcctApp::Run()
{
  std::cout << "[occt_smoke] init_window.begin" << std::endl;
  InitWindow();
  std::cout << "[occt_smoke] init_window.done" << std::endl;
  std::cout << "[occt_smoke] init_occt.begin" << std::endl;
  InitOcctViewer();
  std::cout << "[occt_smoke] init_occt.done" << std::endl;
  std::cout << "[occt_smoke] init_scene.begin" << std::endl;
  InitScene();
  std::cout << "[occt_smoke] init_scene.done" << std::endl;
  std::cout << "[occt_smoke] init_imgui.begin" << std::endl;
  InitImGui();
  std::cout << "[occt_smoke] init_imgui.done" << std::endl;
  std::cout << "[occt_smoke] runtime_ready" << std::endl;
  Cleanup();
  std::cout << "[occt_smoke] cleanup.done" << std::endl;
}

cxgeom::CxSetCircleBuildResult OcctApp::BuildSetCircleGeometry(const cxgeom::CxSetCircleRequest& request) const
{
  cxgeom::CxSetCircleBuild builder;
  return builder.Build(request);
}

cxgeom::CxSetCircleDisplayResult OcctApp::BuildSetCircleDisplayBatch(const cxgeom::CxSetCircleDisplayRequest& request) const
{
  cxgeom::CxSetCircleDisplay displayBuilder;
  return displayBuilder.MakeBatch(request);
}

bool OcctApp::DisplaySetCircleBatch(const cxgeom::CxSetCircleDisplayResult& result)
{
  if (myContext.IsNull() || !result.success || !result.presentation.HasPresentation())
  {
    return false;
  }

  myContext->Display(result.presentation.NativePresentation(), Standard_True);
  return true;
}

cxgeom::CxSetLineBuildResult OcctApp::BuildSetLineGeometry(const cxgeom::CxSetLineRequest& request) const
{
  cxgeom::CxSetLineBuild builder;
  return builder.Build(request);
}

cxgeom::CxSetLineDisplayResult OcctApp::BuildSetLineDisplayBatch(const cxgeom::CxSetLineDisplayRequest& request) const
{
  cxgeom::CxSetLineDisplay displayBuilder;
  return displayBuilder.MakeBatch(request);
}

bool OcctApp::DisplaySetLineBatch(const cxgeom::CxSetLineDisplayResult& result)
{
  if (myContext.IsNull() || !result.success || !result.presentation.HasPresentation())
  {
    return false;
  }

  myContext->Display(result.presentation.NativePresentation(), Standard_True);
  return true;
}

CxOcctPointCloudBody OcctApp::BuildPointCloudBody(const CxOcctPointCloudRequest& request) const
{
  CxOcctPointCloudBody body;
  body.entity_id = request.entity_id;
  body.name = request.name;
  body.point_count = static_cast<int>(request.points.size());
  body.has_normals = HasPointNormals(request.points, true);
  body.visible = request.style.visible;
  body.point_size = request.style.point_size;
  return body;
}

CxOcctPointCloudResult OcctApp::BuildPointCloudPresentation(const CxOcctPointCloudRequest& request) const
{
  CxOcctPointCloudResult result;
  result.entity_id = request.entity_id;
  result.point_count = static_cast<int>(request.points.size());
  result.has_normals = HasPointNormals(request.points, request.style.color_by_normals);
  result.style = request.style;
  result.presentation = MakePointCloudPresentation(request.points, request.style, result.has_normals);
  result.success = !result.presentation.IsNull();
  return result;
}

CxOcctPointCloudBatchResult OcctApp::BuildPointCloudBatchPresentation(const CxOcctPointCloudBatchRequest& request) const
{
  CxOcctPointCloudBatchResult result;
  result.batch_id = request.batch_id;
  result.source_count = static_cast<int>(request.clouds.size());
  result.style = request.style;

  std::vector<CxOcctPointCloudPoint> mergedPoints;
  std::size_t totalPoints = 0;
  for (const CxOcctPointCloudRequest& cloud : request.clouds)
  {
    totalPoints += cloud.points.size();
  }
  mergedPoints.reserve(totalPoints);

  for (const CxOcctPointCloudRequest& cloud : request.clouds)
  {
    mergedPoints.insert(mergedPoints.end(), cloud.points.begin(), cloud.points.end());
  }

  result.point_count = static_cast<int>(mergedPoints.size());
  result.has_normals = HasPointNormals(mergedPoints, request.style.color_by_normals);
  result.presentation = MakePointCloudPresentation(mergedPoints, request.style, result.has_normals);
  result.success = !result.presentation.IsNull();
  return result;
}

bool OcctApp::DisplayPointCloud(const CxOcctPointCloudResult& result)
{
  if (myContext.IsNull() || !result.success || result.presentation.IsNull())
  {
    return false;
  }

  if (!result.style.visible)
  {
    myContext->Erase(result.presentation, Standard_True);
    return true;
  }

  myContext->Display(result.presentation, Standard_True);
  return true;
}

bool OcctApp::DisplayPointCloudBatch(const CxOcctPointCloudBatchResult& result)
{
  if (myContext.IsNull() || !result.success || result.presentation.IsNull())
  {
    return false;
  }

  if (!result.style.visible)
  {
    myContext->Erase(result.presentation, Standard_True);
    return true;
  }

  myContext->Display(result.presentation, Standard_True);
  return true;
}

CxOcctPointCloudAnnotationDisplayResult OcctApp::BuildPointCloudAnnotationDisplay(const CxOcctPointCloudAnnotationDisplayRequest& request) const
{
  CxOcctPointCloudAnnotationDisplayResult result;
  result.layer_id = request.layer_id;
  result.annotation_count = static_cast<int>(request.annotations.size());
  result.visible = request.visible;

  for (const CxOcctPointCloudAnnotation& annotation : request.annotations)
  {
    if (annotation.text.empty())
    {
      continue;
    }

    Handle(AIS_TextLabel) label = new AIS_TextLabel();
    label->SetText(TCollection_ExtendedString(annotation.text.c_str()));
    label->SetPosition(gp_Pnt(annotation.anchor_x, annotation.anchor_y, annotation.anchor_z));
    label->SetColor(Quantity_NOC_WHITE);
    result.labels.push_back(label);
  }

  result.success = true;
  return result;
}

bool OcctApp::DisplayPointCloudAnnotations(const CxOcctPointCloudAnnotationDisplayResult& result)
{
  if (myContext.IsNull() || !result.success)
  {
    return false;
  }

  bool changed = false;
  for (const Handle(AIS_TextLabel)& label : result.labels)
  {
    if (label.IsNull())
    {
      continue;
    }

    if (result.visible)
    {
      myContext->Display(label, Standard_False);
    }
    else
    {
      myContext->Erase(label, Standard_False);
    }
    changed = true;
  }

  if (changed)
  {
    myContext->UpdateCurrentViewer();
  }

  return changed;
}

void OcctApp::InitWindow()
{
  glfwSetErrorCallback(
      [](int theError, const char* theDescription)
      {
        Message::DefaultMessenger()->Send(
            TCollection_AsciiString("GLFW error ") + theError + ": " + theDescription,
            Message_Fail);
      });

  if (glfwInit() == GLFW_FALSE)
  {
    throw std::runtime_error("glfwInit() failed");
  }

 
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  

  myWindow = new Window(myViewportWidth, myViewportHeight, "cxcore");
  if (myWindow.IsNull() || myWindow->getGlfwWindow() == nullptr)
  {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }

  glfwMakeContextCurrent(myWindow->getGlfwWindow());
  glfwSwapInterval(1);

  if (gladLoadGL() == 0)
  {
    throw std::runtime_error("gladLoadGL() failed");
  }

  glfwSetWindowUserPointer(myWindow->getGlfwWindow(), this);
  glfwSetFramebufferSizeCallback(myWindow->getGlfwWindow(), &OcctApp::OnFramebufferResize);
  glfwSetMouseButtonCallback(myWindow->getGlfwWindow(), &OcctApp::OnMouseButton);
  glfwSetCursorPosCallback(myWindow->getGlfwWindow(), &OcctApp::OnCursorPos);
  glfwSetScrollCallback(myWindow->getGlfwWindow(), &OcctApp::OnScroll);
}

void OcctApp::InitOcctViewer()
{
  myDisplayConnection = new Aspect_DisplayConnection();
  myGraphicDriver = new OpenGl_GraphicDriver(myDisplayConnection, false);
  myViewer = new V3d_Viewer(myGraphicDriver);
  myViewer->SetDefaultLights();
  myViewer->SetLightOn();

  myContext = new AIS_InteractiveContext(myViewer);
  myView = myViewer->CreateView();
  myView->SetWindow(myWindow);
  if (!myWindow->IsMapped())
  {
    myWindow->Map();
  }

  myView->SetBackgroundColor(Quantity_NOC_BLACK);
  myView->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);
  myView->MustBeResized();
}

void OcctApp::InitScene()
{
  myBox = new AIS_Shape(BRepPrimAPI_MakeBox(120.0, 80.0, 60.0).Shape());
  myContext->Display(myBox, Standard_True);
  myView->FitAll(0.01, Standard_True);
}

void OcctApp::InitImGui()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(myWindow->getGlfwWindow(), true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void OcctApp::Cleanup()
{
  if (!myWindow.IsNull() && myWindow->getGlfwWindow() != nullptr)
  {
    glfwMakeContextCurrent(myWindow->getGlfwWindow());
    glfwSetWindowUserPointer(myWindow->getGlfwWindow(), nullptr);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  myBox.Nullify();
  myContext.Nullify();
  myView.Nullify();
  myViewer.Nullify();
  myGraphicDriver.Nullify();
  myDisplayConnection.Nullify();

  if (!myWindow.IsNull())
  {
    myWindow->Close();
    myWindow.Nullify();
  }

  glfwMakeContextCurrent(nullptr);
  glfwTerminate();
}

void OcctApp::MainLoop()
{
  glfwPollEvents();
}

void OcctApp::DrawUi()
{
  ImGui::Begin("cxcore");
  ImGui::Text("GLFW + OCCT + ImGui");
  ImGui::Separator();
  ImGui::Text("Viewport: %d x %d", myViewportWidth, myViewportHeight);
  ImGui::Text("Smoke target: OCCT only");

  if (ImGui::Button("Fit All"))
  {
    myView->FitAll(0.01, Standard_True);
  }

  ImGui::SameLine();
  if (ImGui::Button("Front"))
  {
    myView->SetProj(V3d_Yneg);
    myView->FitAll(0.01, Standard_True);
  }

  ImGui::SameLine();
  if (ImGui::Button("Iso"))
  {
    myView->SetProj(V3d_XposYnegZpos);
    myView->FitAll(0.01, Standard_True);
  }

  ImGui::Text("Mouse left: rotate");
  ImGui::Text("Mouse right: pan");
  ImGui::Text("Wheel: zoom");
  ImGui::Text("Esc: quit");
  ImGui::End();
}

void OcctApp::Resize(int theWidth, int theHeight)
{
  myViewportWidth = theWidth;
  myViewportHeight = theHeight;
  if (!myView.IsNull())
  {
    myView->MustBeResized();
  }
}

void OcctApp::OnFramebufferResize(GLFWwindow* theWindow, int theWidth, int theHeight)
{
  auto* anApp = static_cast<OcctApp*>(glfwGetWindowUserPointer(theWindow));
  if (anApp != nullptr)
  {
    anApp->Resize(theWidth, theHeight);
  }
}

void OcctApp::OnMouseButton(GLFWwindow* theWindow, int theButton, int theAction, int theMods)
{
  ImGui_ImplGlfw_MouseButtonCallback(theWindow, theButton, theAction, theMods);
  if (ImGui::GetIO().WantCaptureMouse)
  {
    return;
  }

  auto* anApp = static_cast<OcctApp*>(glfwGetWindowUserPointer(theWindow));
  if (anApp == nullptr || anApp->myView.IsNull())
  {
    return;
  }

  double aX = 0.0;
  double aY = 0.0;
  glfwGetCursorPos(theWindow, &aX, &aY);
  anApp->myLastX = aX;
  anApp->myLastY = aY;

  if (theAction == GLFW_PRESS)
  {
    if (theButton == GLFW_MOUSE_BUTTON_LEFT)
    {
      anApp->myLeftPressed = true;
      anApp->myView->StartRotation(static_cast<int>(aX), static_cast<int>(aY));
    }
    else if (theButton == GLFW_MOUSE_BUTTON_RIGHT)
    {
      anApp->myRightPressed = true;
    }

    anApp->myContext->MoveTo(static_cast<int>(aX), static_cast<int>(aY), anApp->myView, Standard_True);
    anApp->myContext->SelectDetected(AIS_SelectionScheme_Replace);
  }
  else if (theAction == GLFW_RELEASE)
  {
    if (theButton == GLFW_MOUSE_BUTTON_LEFT)
    {
      anApp->myLeftPressed = false;
    }
    else if (theButton == GLFW_MOUSE_BUTTON_RIGHT)
    {
      anApp->myRightPressed = false;
    }
  }
}

void OcctApp::OnCursorPos(GLFWwindow* theWindow, double theX, double theY)
{
  ImGui_ImplGlfw_CursorPosCallback(theWindow, theX, theY);
  if (ImGui::GetIO().WantCaptureMouse)
  {
    return;
  }

  auto* anApp = static_cast<OcctApp*>(glfwGetWindowUserPointer(theWindow));
  if (anApp == nullptr || anApp->myView.IsNull())
  {
    return;
  }

  if (anApp->myLeftPressed)
  {
    anApp->myView->Rotation(static_cast<int>(theX), static_cast<int>(theY));
  }
  else if (anApp->myRightPressed)
  {
    const int aDx = static_cast<int>(theX - anApp->myLastX);
    const int aDy = static_cast<int>(theY - anApp->myLastY);
    anApp->myView->Pan(aDx, -aDy);
  }
  else
  {
    anApp->myContext->MoveTo(static_cast<int>(theX), static_cast<int>(theY), anApp->myView, Standard_True);
  }

  anApp->myLastX = theX;
  anApp->myLastY = theY;
}

void OcctApp::OnScroll(GLFWwindow* theWindow, double theOffsetX, double theOffsetY)
{
  ImGui_ImplGlfw_ScrollCallback(theWindow, theOffsetX, theOffsetY);
  if (ImGui::GetIO().WantCaptureMouse)
  {
    return;
  }

  auto* anApp = static_cast<OcctApp*>(glfwGetWindowUserPointer(theWindow));
  if (anApp == nullptr || anApp->myView.IsNull())
  {
    return;
  }

  double aX = 0.0;
  double aY = 0.0;
  glfwGetCursorPos(theWindow, &aX, &aY);

  const Standard_Integer aDelta = theOffsetY > 0.0 ? 24 : -24;
  anApp->myView->StartZoomAtPoint(static_cast<int>(aX), static_cast<int>(aY));
  anApp->myView->ZoomAtPoint(static_cast<int>(aX), static_cast<int>(aY), static_cast<int>(aX), static_cast<int>(aY + aDelta));
}