#ifndef _GP_PATH_Header
#define _GP_PATH_Header

#include <AIS_InteractiveContext.hxx>
#include <AIS_ViewController.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom2d_Ellipse.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Handle.hxx>
#include <V3d_View.hxx>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "Image.h"
#include "View.h"
#include "occtinclude.h"
#include <cmath>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <stdexcept>

using namespace std;

class SLine {
public:
  SLine() : myStartPoint(gp_Pnt(0, 0, 0)), myEndPoint(gp_Pnt(0, 0, 0)) {}
  SLine(const gp_Pnt &startPoint, const gp_Pnt &endPoint)
      : myStartPoint(startPoint), myEndPoint(endPoint) {}

  gp_Pnt StartPoint() const { return myStartPoint; }
  gp_Pnt EndPoint() const { return myEndPoint; }
  double Length() const {
    gp_Vec vec(myStartPoint, myEndPoint);
    return vec.Magnitude();
  }
  gp_Pnt MidPoint() const {
    return gp_Pnt((myStartPoint.X() + myEndPoint.X()) / 2.0,
                  (myStartPoint.Y() + myEndPoint.Y()) / 2.0,
                  (myStartPoint.Z() + myEndPoint.Z()) / 2.0);
  }
  void SetPoints(const gp_Pnt &startPoint, const gp_Pnt &endPoint) {
    myStartPoint = startPoint;
    myEndPoint = endPoint;
  }

  void setLine(const gp_Pnt &startPoint, const gp_Pnt &endPoint) {
    myStartPoint = startPoint;
    myEndPoint = endPoint;
  }

  void setLine(int ix0, int iy0, int ix1, int iy1) {
    myStartPoint.SetX(ix0);
    myStartPoint.SetY(iy0);
    myStartPoint.SetZ(0);
    myEndPoint.SetX(ix1);
    myEndPoint.SetY(iy1);
    myEndPoint.SetZ(0);
  }
  Standard_Real x1() { return myStartPoint.X(); }
  Standard_Real y1() { return myStartPoint.Y(); }
  Standard_Real x2() { return myEndPoint.X(); }
  Standard_Real y2() { return myEndPoint.Y(); }

private:
  gp_Pnt myStartPoint;
  gp_Pnt myEndPoint;
};

class gp_Rectangle {
public:
  gp_Rectangle(const gp_Pnt &topLeft, const gp_Pnt &bottomRight)
      : myTopLeft(topLeft), myBottomRight(bottomRight) {}
  gp_Rectangle(const gp_Pnt &center, const double width, const double height) {
    double halfWidth = width / 2.0;
    double halfHeight = height / 2.0;
    myBottomRight.SetCoord(center.X() - halfWidth, center.Y() - halfHeight,
                           center.Z());
    myTopLeft.SetCoord(center.X() + halfWidth, center.Y() + halfHeight,
                       center.Z());
  }
  void setrect(const gp_Pnt &topLeft, const gp_Pnt &bottomRight) {
    myTopLeft = topLeft;
    myBottomRight = bottomRight;
  }
  gp_Pnt BottomRight() const { return myBottomRight; }
  gp_Pnt TopLeft() const { return myTopLeft; }
  double Width() const { return myBottomRight.X() - myTopLeft.X(); }
  double Height() const { return myBottomRight.Y() - myTopLeft.Y(); }
  void SetRectangleByCenterWidthHeight(const gp_Pnt &center, const double width,
                                       const double height) {
    double halfWidth = width / 2.0;
    double halfHeight = height / 2.0;
    myBottomRight.SetCoord(center.X() - halfWidth, center.Y() - halfHeight,
                           center.Z());
    myTopLeft.SetCoord(center.X() + halfWidth, center.Y() + halfHeight,
                       center.Z());
  }
  bool contains(const gp_Pnt &point) {
    if (point.X() > myTopLeft.X() && point.X() < myBottomRight.X())
      if (point.Y() > myTopLeft.Y() && point.Y() < myBottomRight.Y()) {
        return TRUE;
      }
    return FALSE;
  }
  void move(int ix, int iy) {
    const Standard_Real ix0 = myTopLeft.X();
    const Standard_Real iy0 = myTopLeft.Y();
    myTopLeft.SetX(ix0 + ix);
    myTopLeft.SetY(iy0 + iy);
    const Standard_Real ix1 = myBottomRight.X();
    const Standard_Real iy1 = myBottomRight.Y();
    myBottomRight.SetX(ix1 + ix);
    myBottomRight.SetY(iy1 + iy);
  }

private:
  gp_Pnt myTopLeft;
  gp_Pnt myBottomRight;
};

class gp_Path {
public:
  gp_Path() = default;

  void AddPoint(const gp_Pnt &point);
  gp_Pnt PointAtPercent(double percent) const;
  gp_Pnt ElementAt(size_t index) const;
  size_t ElementCount() const;
  void RotateAroundPoint(const gp_Pnt &rotationCenter, double angleDegrees,
                         const gp_Dir &rotationAxis = gp_Dir(0, 0, 1));
  void ScaleAroundPoint(const gp_Pnt &scaleCenter, double scaleFactorX,
                        double scaleFactorY, double scaleFactorZ = 1.0);
  void RotateAroundLine(const gp_Pnt &linePoint, const gp_Dir &lineDirection,
                        double angleDegrees);
  void Translate(const gp_Vec &translationVector);
  void TranslateAIShape(const gp_Vec &translationVector);
  void AddRectangularEllipse(const gp_Pnt &p1, const gp_Pnt &p2,
                             double drate = 0.5);
  void AddRectangularEllipse5p(const gp_Pnt *points, double drate);
  void AddLine(const gp_Pnt &start, const gp_Pnt &end);
  void AddArc(const gp_Pnt &center, double radius, double startAngle,
              double endAngle, int segments = 100);
  void AddPath(const gp_Path &otherPath);
  void CopyPath(const gp_Path &otherPath);
  void Clear();
  void SubtractPath(const gp_Path &otherPath, double tolerance = 1e-6);
  std::vector<gp_Pnt> IntersectPaths(const gp_Path &otherPath) const;
  void FindBestMatch(const gp_Path &otherPath, gp_Vec &bestTranslation,
                     double &bestRotation) const;

private:
  double CalculateError(const gp_Path &otherPath, double angle,
                        const gp_Vec &translation) const;

public:
  gp_Rectangle boundingRect() const;
  gp_Pnt centroid() const;
  gp_Pnt weightedCentroid(const std::vector<double> &weights) const;
  void AddCross(const gp_Pnt &center, double size);
  void AddCircle(const gp_Pnt &center, double radius, double drate = 0.5);
  void AddMCircle(const gp_Pnt &center, double radius, int segments = 100);
  void AddSquare(const gp_Pnt &center, double size);
  void AddTriangle(const gp_Pnt &center, double size);
  void AddRect(const gp_Rectangle &rect);
  void AddRect2(const gp_Pnt &p1, const gp_Pnt &p2, const gp_Pnt &p3,
                const gp_Pnt &p4);
  void SetContext(Handle(AIS_InteractiveContext) Context);
  void SetView(Handle(V3d_CustomView) customview);

  void SetVisibility(bool bshow) {
    if (!m_Context.IsNull())
      m_Context->Display(m_shape, bshow);
  }
  Handle(AIS_InteractiveContext) GetContext() { return m_Context; }
  void DrawRect(const gp_Rectangle &rect);
  void Draw();
  void DrawB();
  void DrawCircle(const gp_Pnt &center, double radius, int segments = 100);

  void MakeShape();
  void MakeEdgeShape();
  void MakePointShape();
  void MakeRectsShape();

  void PathShow(bool bshow);
  Handle(AIS_Shape) getshape();

  void setcolor(int ir, int ig, int ib);

  std::vector<gp_Pnt> &getpoints() { return points; }

private:
public:
  mutable double cachedTotalLength = -1.0;
  std::vector<gp_Pnt> points;

  Quantity_Color m_Color = Quantity_Color(Quantity_NOC_GREEN);
  ;

  static Handle(AIS_InteractiveContext) m_Context;
  static Handle(V3d_CustomView) m_CustomView;

  TopoDS_Shape m_pathFace;
  Handle(AIS_Shape) m_shape = nullptr;

  std::vector<Handle(AIS_Shape)> m_shapes;
  double CalculateTotalLength() const;

  gp_Pnt OBBCenterAngleSort();
};
#endif
