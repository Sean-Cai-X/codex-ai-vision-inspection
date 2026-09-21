#include "pch.h"

#include "gp_path.h"

#include <vector>

Handle(AIS_InteractiveContext) gp_Path::m_Context = nullptr;

Handle(V3d_CustomView) gp_Path::m_CustomView = nullptr;

void gp_Path::AddPoint(const gp_Pnt &point) {
  if (!points.empty()) {
    const gp_Pnt &lastPoint = points.back();
    double tolerance = 1e-6;
    bool isEqual = std::fabs(point.X() - lastPoint.X()) < tolerance &&
                   std::fabs(point.Y() - lastPoint.Y()) < tolerance &&
                   std::fabs(point.Z() - lastPoint.Z()) < tolerance;
    if (isEqual) {
    }
  }
  points.push_back(point);
}

gp_Pnt gp_Path::PointAtPercent(double percent) const {
  if (percent < 0.0 || percent > 1.0) {
    throw std::out_of_range("Percent must be between 0 and 1.");
  }
  if (points.size() < 2) {
    throw std::runtime_error("Not enough points in the path.");
  }
  if (percent == 0.0)
    return points.front();
  if (percent == 1.0)
    return points.back();

  double totalLength = CalculateTotalLength();
  double targetLength = totalLength * percent;
  double accumulatedLength = 0.0;

  for (size_t i = 0; i < points.size() - 1; ++i) {
    gp_Pnt p1 = points[i];
    gp_Pnt p2 = points[i + 1];
    gp_Vec vec(p1, p2);
    double segmentLength = vec.Magnitude();

    if (accumulatedLength + segmentLength >= targetLength) {
      double localPercent = (targetLength - accumulatedLength) / segmentLength;
      return p1.XYZ() + vec.XYZ() * localPercent;
    }
    accumulatedLength += segmentLength;
  }

  return points.back();
}

gp_Pnt gp_Path::ElementAt(size_t index) const {
  if (index >= points.size()) {
    throw std::out_of_range("Index out of range.");
  }
  return points[index];
}
size_t gp_Path::ElementCount() const { return points.size(); }
void gp_Path::RotateAroundPoint(const gp_Pnt &rotationCenter,
                                double angleDegrees,
                                const gp_Dir &rotationAxis) {
  gp_Trsf trsf;
  trsf.SetRotation(gp_Ax1(rotationCenter, rotationAxis),
                   angleDegrees * M_PI / 180.0);

  for (auto &point : points) {
    point.Transform(trsf);
  }

  cachedTotalLength = -1.0;
}
void gp_Path::ScaleAroundPoint(const gp_Pnt &scaleCenter, double scaleFactorX,
                               double scaleFactorY, double scaleFactorZ) {
  for (auto &point : points) {
    gp_XYZ vectorToPoint = point.XYZ() - scaleCenter.XYZ();
    vectorToPoint.SetX(vectorToPoint.X() * scaleFactorX);
    vectorToPoint.SetY(vectorToPoint.Y() * scaleFactorY);
    vectorToPoint.SetZ(vectorToPoint.Z() * scaleFactorZ);
    point.SetXYZ(scaleCenter.XYZ() + vectorToPoint);
  }

  cachedTotalLength = -1.0;
}
void gp_Path::RotateAroundLine(const gp_Pnt &linePoint,
                               const gp_Dir &lineDirection,
                               double angleDegrees) {
  gp_Trsf trsf;
  trsf.SetRotation(gp_Ax1(linePoint, lineDirection),
                   angleDegrees * M_PI / 180.0);

  for (auto &point : points) {
    point.Transform(trsf);
  }

  cachedTotalLength = -1.0;
}

void gp_Path::Translate(const gp_Vec &translationVector) {
  for (auto &point : points) {
    point.Translate(translationVector);
  }
  TranslateAIShape(translationVector);
  cachedTotalLength = -1.0;
}

void gp_Path::TranslateAIShape(const gp_Vec &translationVector) {
  if (m_Context.IsNull()) {
    return;
  }
  if (m_shape.IsNull()) {
    return;
  }
  TopoDS_Shape originalShape = m_shape->Shape();

  gp_Trsf trsf;
  trsf.SetTranslation(translationVector);

  BRepBuilderAPI_Transform transform(originalShape, trsf);
  TopoDS_Shape transformedShape = transform.Shape();

  m_shape->Set(transformedShape);

  m_Context->Redisplay(m_shape, Standard_False);
  m_Context->UpdateCurrentViewer();
}

void gp_Path::AddLine(const gp_Pnt &start, const gp_Pnt &end) {
  GC_MakeSegment mkSeg(start, end);
  if (mkSeg.IsDone()) {
    Handle(Geom_TrimmedCurve) curve = mkSeg.Value();
    for (double t = 0.0; t <= 1.0; t += 0.01) {
      AddPoint(curve->Value(t));
    }
  }
}

void gp_Path::AddArc(const gp_Pnt &center, double radius, double startAngle,
                     double endAngle, int segments) {
  (void)segments;
  double width = radius * 2;
  double height = radius * 2;
  gp_Ax2 anAxis(center, gp_Dir(0, 0, 1));
  gp_Elips ellipse(anAxis, width / 2., height / 2.);
  Handle(Geom_TrimmedCurve) ellipseCurve =
      new Geom_TrimmedCurve(new Geom_Ellipse(ellipse), 0.0, 2 * M_PI);
  TopoDS_Edge wbe = BRepBuilderAPI_MakeEdge(ellipseCurve);
  TopoDS_Wire te = BRepBuilderAPI_MakeWire(wbe);
  BRepAdaptor_CompCurve compCurve(te);
  Standard_Real u1 = startAngle;
  Standard_Real u2 = endAngle;
  GCPnts_QuasiUniformDeflection algo(compCurve, 0.01, u1, u2);
  for (Standard_Integer i = 1; i <= algo.NbPoints(); ++i) {
    double parameter = algo.Parameter(i);
    gp_Pnt point = ellipseCurve->Value(parameter);
    AddPoint(point);
  }
}

void gp_Path::AddPath(const gp_Path &otherPath) {
  for (const auto &point : otherPath.points) {
    this->AddPoint(point);
  }
}
void gp_Path::CopyPath(const gp_Path &otherPath) {
  points.clear();
  for (const auto &point : otherPath.points) {
    this->AddPoint(point);
  }
}

void gp_Path::Clear() {
  points.clear();
  if (m_Context.IsNull()) {
    return;
  }
  if (!m_shape.IsNull())
    m_Context->Erase(m_shape, Standard_True);
  if (m_shapes.size() > 0) {
    for (int i = 0; i < m_shapes.size(); i++) {
      m_Context->Erase(m_shapes[i], Standard_True);
    }
    m_shapes.clear();
  }
}
void gp_Path::SubtractPath(const gp_Path &otherPath, double tolerance) {
  std::vector<gp_Pnt> newPoints;
  for (const auto &point : points) {
    bool found = false;
    for (const auto &otherPoint : otherPath.points) {
      if (point.Distance(otherPoint) < tolerance) {
        found = true;
        break;
      }
    }
    if (!found) {
      newPoints.push_back(point);
    }
  }
  points = newPoints;
}

std::vector<gp_Pnt> gp_Path::IntersectPaths(const gp_Path &otherPath) const {
  std::vector<gp_Pnt> intersectionPoints;
  if (points.size() <= 0 || otherPath.points.size() <= 0)
    return intersectionPoints;

  BRepBuilderAPI_MakePolygon mkPolygon1, mkPolygon2;
  for (const auto &point : points) {
    mkPolygon1.Add(point);
  }
  for (const auto &point : otherPath.points) {
    mkPolygon2.Add(point);
  }
  TopoDS_Wire wire1 = mkPolygon1.Wire();
  TopoDS_Wire wire2 = mkPolygon2.Wire();

  BRepAlgoAPI_Section section(wire1, wire2);
  section.Build();
  if (section.IsDone()) {
    TopExp_Explorer explorer(section.Shape(), TopAbs_VERTEX);
    for (; explorer.More(); explorer.Next()) {
      gp_Pnt vertexPoint = BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
      intersectionPoints.push_back(vertexPoint);
    }
  }

  return intersectionPoints;
}

void gp_Path::FindBestMatch(const gp_Path &otherPath, gp_Vec &bestTranslation,
                            double &bestRotation) const {
  double minError = std::numeric_limits<double>::max();
  for (double angle = 0; angle < 360; angle += 1.0) {
    for (const auto &point : points) {
      gp_Vec translation(point.X(), point.Y(), point.Z());
      double error = CalculateError(otherPath, angle, translation);
      if (error < minError) {
        minError = error;
        bestRotation = angle;
        bestTranslation = translation;
      }
    }
  }
}

double gp_Path::CalculateError(const gp_Path &otherPath, double angle,
                               const gp_Vec &translation) const {
  double error = 0.0;
  gp_Trsf trsf;
  trsf.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)),
                   angle * M_PI / 180.0);
  trsf.SetTranslation(translation);

  for (size_t i = 0; i < std::min(points.size(), otherPath.points.size());
       ++i) {
    gp_Pnt transformedPoint = points[i].Transformed(trsf);
    error += transformedPoint.Distance(otherPath.points[i]);
  }
  return error;
}

gp_Rectangle gp_Path::boundingRect() const {
  if (points.empty())
    return {gp_Pnt(), gp_Pnt()};

  double minX = std::numeric_limits<double>::max();
  double minY = std::numeric_limits<double>::max();
  double maxX = std::numeric_limits<double>::lowest();
  double maxY = std::numeric_limits<double>::lowest();

  for (const auto &point : points) {
    minX = std::min(minX, point.X());
    minY = std::min(minY, point.Y());
    maxX = std::max(maxX, point.X());
    maxY = std::max(maxY, point.Y());
  }

  return gp_Rectangle(gp_Pnt(minX, minY, 0), gp_Pnt(maxX, maxY, 0));
}

gp_Pnt gp_Path::centroid() const {
  if (points.empty())
    return gp_Pnt();

  double sumX = 0.0;
  double sumY = 0.0;

  for (const auto &point : points) {
    sumX += point.X();
    sumY += point.Y();
  }

  size_t n = points.size();
  return gp_Pnt(sumX / n, sumY / n, 0);
}

gp_Pnt gp_Path::weightedCentroid(const std::vector<double> &weights) const {
  if (points.size() != weights.size() || points.empty())
    return gp_Pnt();

  double sumWX = 0.0;
  double sumWY = 0.0;
  double totalWeight = 0.0;

  for (size_t i = 0; i < points.size(); ++i) {
    sumWX += weights[i] * points[i].X();
    sumWY += weights[i] * points[i].Y();
    totalWeight += weights[i];
  }

  return gp_Pnt(sumWX / totalWeight, sumWY / totalWeight, 0);
}

void gp_Path::AddCross(const gp_Pnt &center, double size) {
  const int halfSize = static_cast<int>(size / 2.0);
  for (int i = 0; i < 2; ++i) {
    for (int j = -halfSize; j <= halfSize; ++j) {
      int x_offset = (i == 0) ? j : -j;
      int y_offset = j;
      gp_Pnt point(center.X() + x_offset, center.Y() + y_offset, center.Z());
      points.push_back(point);
    }
    if (i == 0) {
      gp_Pnt centerPoint(center.X(), center.Y(), center.Z());
      points.push_back(centerPoint);
    }
  }
}

void gp_Path::AddCircle(const gp_Pnt &center, double radius, double drate) {

  gp_Ax2 axes(center, gp_Dir(0, 0, 1));
  gp_Ax2 axis(center, gp::DZ());
  double width = radius * 2;
  double height = radius * 2;
  gp_Ax2 anAxis(center, gp_Dir(0, 0, 1));
  gp_Elips ellipse(anAxis, width / 2., height / 2.);
  Handle(Geom_TrimmedCurve) ellipseCurve =
      new Geom_TrimmedCurve(new Geom_Ellipse(ellipse), 0.0, 2 * M_PI);
  TopoDS_Edge wbe = BRepBuilderAPI_MakeEdge(ellipseCurve);
  TopoDS_Wire te = BRepBuilderAPI_MakeWire(wbe);
  BRepAdaptor_CompCurve compCurve(te);
  Standard_Real u1 = ellipseCurve->FirstParameter();
  Standard_Real u2 = ellipseCurve->LastParameter();
  GCPnts_QuasiUniformDeflection algo(compCurve, drate, u1, u2);
  for (Standard_Integer i = 1; i <= algo.NbPoints(); ++i) {
    double parameter = algo.Parameter(i);
    gp_Pnt point = ellipseCurve->Value(parameter);
    AddPoint(point);
  }
}

void gp_Path::AddRectangularEllipse(const gp_Pnt &p1, const gp_Pnt &p2,
                                    double drate) {

  const double width = std::abs(p2.X() - p1.X());
  const double height = std::abs(p2.Y() - p1.Y());
  if (width <= 0.0 || height <= 0.0)
    return;

  // gp_Elips takes major/minor radii, while the public rectangular API is
  // axis-bound: X extent must remain X and Y extent must remain Y.  The old
  // implementation sorted width/height but left gp_Ax2's X direction fixed,
  // so every vertical ellipse was silently rotated to horizontal when a
  // CxScript replay rebuilt the path.
  const bool horizontalMajor = width >= height;
  const double majorRadius = (horizontalMajor ? width : height) * 0.5;
  const double minorRadius = (horizontalMajor ? height : width) * 0.5;
  const gp_Dir majorDirection =
      horizontalMajor ? gp_Dir(1.0, 0.0, 0.0) : gp_Dir(0.0, 1.0, 0.0);
  gp_Ax2 anAxis(gp_Pnt((p1.X() + p2.X()) * 0.5,
                       (p1.Y() + p2.Y()) * 0.5, 0.0),
                gp_Dir(0.0, 0.0, 1.0), majorDirection);
  gp_Elips elips(anAxis, majorRadius, minorRadius);

  Handle(Geom_TrimmedCurve) ellipseCurve =
      new Geom_TrimmedCurve(new Geom_Ellipse(elips), 0.0, 2 * M_PI);
  TopoDS_Edge wbe = BRepBuilderAPI_MakeEdge(ellipseCurve);
  TopoDS_Wire te = BRepBuilderAPI_MakeWire(wbe);
  BRepAdaptor_CompCurve compCurve(te);

  Standard_Real u1 = ellipseCurve->FirstParameter();
  Standard_Real u2 = ellipseCurve->LastParameter();
  GCPnts_QuasiUniformDeflection algo(compCurve, drate, u1, u2);

  for (Standard_Integer i = 1; i <= algo.NbPoints(); ++i) {
    double parameter = algo.Parameter(i);
    gp_Pnt point = ellipseCurve->Value(parameter);
    AddPoint(point);
  }
}

void calculateEllipseParameters(const gp_Pnt *ellipsePoints, gp_Pnt &center,
                                gp_Dir &dirMajorAxis, double &a, double &b) {
  double A = 0, B = 0, C = 0, D = 0, E = 0, F = 0;
  for (int i = 0; i < 5; ++i) {
    double x = ellipsePoints[i].X();
    double y = ellipsePoints[i].Y();
    double x2 = x * x;
    double y2 = y * y;

    A += x2 * y2;
    B += x2 * y;
    C += x2;
    D += x * y2;
    E += x * y;
    F += x2 + y2;
  }

  double det = 4 * A * C - B * B;
  if (fabs(det) < 1e-6) {
    std::cerr << "Points do not form an ellipse." << std::endl;
    return;
  }

  double xc = (B * E - 2 * C * D) / det;
  double yc = (B * D - 2 * A * E) / det;
  center.SetCoord(xc, yc, 0);

  double Ea = A - A * (xc * xc) - B * xc * yc - C * (yc * yc);
  double Eb = B * B - 4 * A * C;
  a = sqrt(fabs(2 * Ea / Eb));
  b = sqrt(fabs(2 * Ea / (Eb * (A / C))));

  double theta = 0.5 * atan2(B, A - C);
  dirMajorAxis = gp_Dir(cos(theta), sin(theta), 0);
}

void gp_Path::AddRectangularEllipse5p(const gp_Pnt *ellipsePoints,
                                      double drate) {
  gp_Pnt center;
  gp_Dir dirMajorAxis;
  double a;
  double b;
  calculateEllipseParameters(ellipsePoints, center, dirMajorAxis, a, b);
  double width = a >= b ? a : b;
  double height = a <= b ? a : b;
  gp_Ax2 anAxis(center, dirMajorAxis);
  gp_Elips elips(anAxis, width / 2., height / 2.);
  Handle(Geom_TrimmedCurve) ellipseCurve =
      new Geom_TrimmedCurve(new Geom_Ellipse(elips), 0.0, 2 * M_PI);
  TopoDS_Edge wbe = BRepBuilderAPI_MakeEdge(ellipseCurve);
  TopoDS_Wire te = BRepBuilderAPI_MakeWire(wbe);
  BRepAdaptor_CompCurve compCurve(te);

  Standard_Real u1 = ellipseCurve->FirstParameter();
  Standard_Real u2 = ellipseCurve->LastParameter();
  GCPnts_QuasiUniformDeflection algo(compCurve, drate, u1, u2);

  for (Standard_Integer i = 1; i <= algo.NbPoints(); ++i) {
    double parameter = algo.Parameter(i);
    gp_Pnt point = ellipseCurve->Value(parameter);
    AddPoint(point);
  }
}

void gp_Path::AddMCircle(const gp_Pnt &center, double radius, int segments) {
  for (int i = 0; i <= segments; ++i) {
    double angle = 2 * M_PI * i / segments;
    gp_Pnt point(center.X() + radius * cos(angle),
                 center.Y() + radius * sin(angle), center.Z());
    points.push_back(point);
  }
}

void gp_Path::AddSquare(const gp_Pnt &center, double size) {
  std::vector<gp_Pnt> squarePoints;
  for (double x = center.X() - size / 2; x <= center.X() + size / 2;
       x += 0.1 * size) {
    squarePoints.emplace_back(x, center.Y() - size / 2, center.Z());
  }
  for (double y = center.Y() - size / 2; y <= center.Y() + size / 2;
       y += 0.1 * size) {
    squarePoints.emplace_back(center.X() + size / 2, y, center.Z());
  }
  for (double x = center.X() + size / 2; x >= center.X() - size / 2;
       x -= 0.1 * size) {
    squarePoints.emplace_back(x, center.Y() + size / 2, center.Z());
  }
  for (double y = center.Y() + size / 2; y >= center.Y() - size / 2;
       y -= 0.1 * size) {
    squarePoints.emplace_back(center.X() - size / 2, y, center.Z());
  }
  points.insert(points.end(), squarePoints.begin(), squarePoints.end());
}

void gp_Path::AddTriangle(const gp_Pnt &center, double size) {
  std::vector<gp_Pnt> trianglePoints;
  for (double t = 0; t <= 1; t += 0.05) {
    double x = center.X() + (t - 0.5) * size;
    double y = center.Y() + (1 - fabs(t - 0.5)) * size / 2;
    trianglePoints.emplace_back(x, y, center.Z());
  }

  trianglePoints.emplace_back(trianglePoints.front());

  points.insert(points.end(), trianglePoints.begin(), trianglePoints.end());
}

void gp_Path::AddRect(const gp_Rectangle &rect) {
  gp_Pnt apoint0 = rect.TopLeft();
  double dh = rect.Height();
  double dw = rect.Width();
  gp_Pnt apoint1(rect.TopLeft().X() + dw, rect.TopLeft().Y(), 0);
  gp_Pnt apoint2(rect.TopLeft().X() + dw, rect.TopLeft().Y() + dh, 0);
  gp_Pnt apoint3(rect.TopLeft().X(), rect.TopLeft().Y() + dh, 0);

  points.push_back(apoint0);
  points.push_back(apoint1);
  points.push_back(apoint2);
  points.push_back(apoint3);
}
void gp_Path::AddRect2(const gp_Pnt &p1, const gp_Pnt &p2, const gp_Pnt &p3,
                       const gp_Pnt &p4) {
  points.push_back(p1);
  points.push_back(p2);
  points.push_back(p3);
  points.push_back(p4);
}
double gp_Path::CalculateTotalLength() const {
  if (cachedTotalLength < 0.0) {
    cachedTotalLength = 0.0;
    for (size_t i = 0; i < points.size() - 1; ++i) {
      gp_Vec vec(points[i], points[i + 1]);
      cachedTotalLength += vec.Magnitude();
    }
  }
  return cachedTotalLength;
}

void gp_Path::SetContext(Handle(AIS_InteractiveContext) Context) {
  if (!m_Context.IsNull())
    return;
  m_Context = Context;
}
void gp_Path::SetView(Handle(V3d_CustomView) customview) {
  if (!customview.IsNull())
    m_CustomView = customview;
}
void gp_Path::setcolor(int ir, int ig, int ib) {
  m_Color = Quantity_Color(ir / 255.0, ig / 255.0, ib / 255.0,
                           Quantity_TypeOfColor::Quantity_TOC_RGB);
}

void gp_Path::MakeEdgeShape() {
  if (m_Context.IsNull()) {
    return;
  }
  m_Context->Erase(m_shape, Standard_True);
  if (points.size() < 2)
    return;
  BRepBuilderAPI_MakePolygon polygonMaker;
  const int pointCount = static_cast<int>(points.size());
  for (int i = 0; i < pointCount; ++i) {
    polygonMaker.Add(points[i]);
  }
  for (int i = pointCount - 1; i >= 0; --i) {
    polygonMaker.Add(points[i]);
  }

  polygonMaker.Close();
  TopoDS_Shape pathFace = BRepBuilderAPI_MakeWire(polygonMaker.Wire());
  m_shape = new AIS_Shape(pathFace);
  m_shape->SetColor(m_Color);
  m_Context->Display(m_shape, AIS_Shaded, 0, false);
}
void gp_Path::MakeShape() {
  if (m_Context.IsNull()) {
    return;
  }
  m_Context->Erase(m_shape, Standard_True);
  if (points.size() < 2)
    return;
  BRepBuilderAPI_MakePolygon polygonMaker;
  for (int i = 0; i < points.size(); i++) {
    polygonMaker.Add(points[i]);
  }
  polygonMaker.Close();
  TopoDS_Shape pathFace = BRepBuilderAPI_MakeWire(polygonMaker.Wire());
  m_shape = new AIS_Shape(pathFace);
  m_shape->SetColor(m_Color);
  m_Context->Display(m_shape, AIS_Shaded, 0, false);
}
void gp_Path::MakePointShape() {
  if (m_Context.IsNull()) {
    return;
  }
  m_Context->Erase(m_shape, Standard_True);
  if (points.size() == 0)
    return;

  TopoDS_Compound compound;
  BRep_Builder builder;
  builder.MakeCompound(compound);
  for (int i = 0; i < points.size(); i++) {
    TopoDS_Shape vertex = BRepBuilderAPI_MakeVertex(points[i]);
    builder.Add(compound, vertex);
  }
  m_shape = new AIS_Shape(compound);
  m_shape->SetColor(m_Color);

  m_Context->Display(m_shape, AIS_Shaded, 0, false);
}
void gp_Path::MakeRectsShape() {
  if (m_Context.IsNull()) {
    return;
  }
  m_Context->Erase(m_shape, Standard_True);
  if (m_shapes.size() > 0) {
    for (int i = 0; i < m_shapes.size(); i++) {
      m_Context->Erase(m_shapes[i], Standard_True);
    }
    m_shapes.clear();
  }
  if (points.size() == 0)
    return;
  const int irectnum = static_cast<int>(points.size() / 4);
  int icurpoints = 0;
  for (int inum = 0; inum < irectnum; inum++) {
    BRepBuilderAPI_MakePolygon polygonMaker;
    for (int i = 0; i < 4; i++) {
      polygonMaker.Add(points[icurpoints]);
      icurpoints++;
    }
    polygonMaker.Close();
    TopoDS_Shape pathFace = BRepBuilderAPI_MakeWire(polygonMaker.Wire());

    Handle(AIS_Shape) a_shape = new AIS_Shape(pathFace);
    a_shape->SetColor(m_Color);
    m_shapes.push_back(a_shape);
    m_Context->Display(a_shape, AIS_Shaded, 0, false);
  }
}
void gp_Path::PathShow(bool bshow) {
  if (m_Context.IsNull() || m_CustomView.IsNull()) {
    return;
  }
  if (false == bshow) {
    if (!m_shape.IsNull())
      m_Context->SetViewAffinity(m_shape, m_CustomView, bshow);
  } else
    MakeShape();
}
Handle(AIS_Shape) gp_Path::getshape() { return m_shape; }
void gp_Path::DrawB() {
  int idx1 = 1;
  const int idx2 = static_cast<int>(points.size());
  vector<gp_Pnt> cloudLinePoints = points;

  TColgp_Array1OfPnt controlPoints(idx1, idx2);

  gp_Pnt *cloudPoints = cloudLinePoints.data();
  gp_Pnt center(0.0, 0.0, 0.0);
  const int numPoints = static_cast<int>(cloudLinePoints.size());
  for (int i = 0; i < numPoints; ++i) {
    center.SetX(center.X() + cloudLinePoints[i].X());
    center.SetY(center.Y() + cloudLinePoints[i].Y());
    center.SetZ(center.Z() + cloudLinePoints[i].Z());
    controlPoints.SetValue(i + 1, cloudPoints[i]);
  }

  center.SetX(center.X() / numPoints);
  center.SetY(center.Y() / numPoints);
  center.SetZ(center.Z() / numPoints);

  GeomAPI_PointsToBSpline outline(controlPoints, Approx_ChordLength);

  if (m_Context.IsNull()) {
    return;
  }
  m_Context->Erase(m_shape, Standard_True);

  if (outline.IsDone()) {
    Handle(Geom_BSplineCurve) bSplineCurve = outline.Curve();
    TopoDS_Edge anEdge = BRepBuilderAPI_MakeEdge(bSplineCurve);
    Handle(AIS_Shape) ais_shape = new AIS_Shape(anEdge);
    m_shape->SetColor(m_Color);
    m_shape = ais_shape;
    m_Context->Display(ais_shape, AIS_Shaded, 0, false);
  }
}

void gp_Path::Draw() {
  BRepBuilderAPI_MakePolygon polygonMaker;
  for (int i = 0; i < points.size(); i++) {
    polygonMaker.Add(points[i]);
  }
  polygonMaker.Close();

  if (m_Context.IsNull()) {
    return;
  }
  m_Context->Erase(m_shape, Standard_True);

  TopoDS_Shape pathFace = BRepBuilderAPI_MakeWire(polygonMaker.Wire());
  Handle(AIS_Shape) shape = new AIS_Shape(pathFace);
  m_shape = shape;

  shape->SetColor(m_Color);
  m_Context->Display(shape, AIS_Shaded, 0, false);
}

void gp_Path::DrawRect(const gp_Rectangle &rect) {
  Clear();
  AddRect(rect);
  BRepBuilderAPI_MakePolygon polygonMaker;
  for (int i = 0; i < points.size(); i++) {
    polygonMaker.Add(points[i]);
  }
  polygonMaker.Close();

  if (m_Context.IsNull()) {
    return;
  }
  m_Context->Erase(m_shape, Standard_True);

  TopoDS_Shape pathFace = BRepBuilderAPI_MakeWire(polygonMaker.Wire());
  Handle(AIS_Shape) shape = new AIS_Shape(pathFace);
  m_shape = shape;

  shape->SetColor(m_Color);
  m_Context->Display(shape, AIS_Shaded, 0, false);
}

void gp_Path::DrawCircle(const gp_Pnt &center, double radius, int segments) {
  AddMCircle(center, radius, segments);
  BRepBuilderAPI_MakePolygon polygonMaker;
  for (int i = 0; i < points.size(); i++) {
    polygonMaker.Add(points[i]);
  }
  polygonMaker.Close();

  if (m_Context.IsNull()) {
    return;
  }
  m_Context->Erase(m_shape, Standard_True);

  TopoDS_Shape pathFace = BRepBuilderAPI_MakeWire(polygonMaker.Wire());
  Handle(AIS_Shape) shape = new AIS_Shape(pathFace);
  m_shape = shape;

  shape->SetColor(m_Color);
  m_Context->Display(shape, AIS_Shaded, 0, false);
}

#include <BRepBuilderAPI_MakePolygon.hxx>
#include <Bnd_Box.hxx>
#include <Bnd_OBB.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <algorithm>
#include <gp_Ax3.hxx>

#include <BRepTools.hxx>
#include <cmath>
#include <gp_Pnt.hxx>
#include <iostream>

Handle(TColgp_HArray1OfPnt) VectorToHArray(const std::vector<gp_Pnt> &vec) {
  Handle(TColgp_HArray1OfPnt) array =
      new TColgp_HArray1OfPnt(1, static_cast<Standard_Integer>(vec.size()));
  for (size_t i = 0; i < vec.size(); ++i) {
    array->SetValue(static_cast<Standard_Integer>(i + 1), vec[i]);
  }
  return array;
}

gp_Pnt ComputeOBBCenter(const Handle(TColgp_HArray1OfPnt) & points) {
  Bnd_Box box;
  for (Standard_Integer i = 1; i <= points->Length(); ++i) {
    const gp_Pnt &p = points->Value(i);
    box.Add(p);
  }

  Bnd_OBB obb(box);

  gp_Ax3 ax = obb.Position();

  return ax.Location();
}

struct AngleCompare {
  gp_Pnt center;

  AngleCompare(const gp_Pnt &c) : center(c) {}

  bool operator()(const gp_Pnt &a, const gp_Pnt &b) const {
    double angleA = std::atan2(a.Y() - center.Y(), a.X() - center.X());
    double angleB = std::atan2(b.Y() - center.Y(), b.X() - center.X());

    if (std::fabs(angleA - angleB) < 1e-6) {
      double dxA = a.X() - center.X();
      double dyA = a.Y() - center.Y();
      double dxB = b.X() - center.X();
      double dyB = b.Y() - center.Y();
      return (dxA * dxA + dyA * dyA) < (dxB * dxB + dyB * dyB);
    }

    return angleA < angleB;
  }
};

#include <fstream>
#include <iostream>
using namespace std;

static ofstream log_test("log_test.txt");
gp_Pnt gp_Path::OBBCenterAngleSort() {
  log_test << "  OBBCenterAngleSort:" << std::endl;
  log_test << std::fixed << std::setprecision(3);
  gp_Pnt obbCenter;

  if (points.size() < 2)
    return obbCenter;
  std::vector<gp_Pnt> rawPoints;
  for (int i = 0; i < points.size(); i++) {
    rawPoints.push_back(points[i]);
  }
  try {
    BRepBuilderAPI_MakePolygon mkPoly;
    for (const auto &p : rawPoints) {
      mkPoly.Add(p);
    }
    mkPoly.Close();
    TopoDS_Wire wire = mkPoly.Wire();

    Handle(TColgp_HArray1OfPnt) occtPoints = VectorToHArray(rawPoints);

    obbCenter = ComputeOBBCenter(occtPoints);
    std::sort(rawPoints.begin(), rawPoints.end(), AngleCompare(obbCenter));

    points.clear();
    log_test << " Distance: ";
    for (const auto &p : rawPoints) {
      double rad = std::atan2(p.Y() - obbCenter.Y(), p.X() - obbCenter.X());
      double deg = rad * 180.0 / M_PI;
      if (deg < 0)
        deg = 360 + deg;
      double dist = obbCenter.Distance(p);
      points.push_back(gp_Pnt(p.X(), p.Y(), 0));

      log_test << dist << " ";
    }
    log_test << std::endl;
    log_test << " Angle   : ";
    for (const auto &p : rawPoints) {
      double rad = std::atan2(p.Y() - obbCenter.Y(), p.X() - obbCenter.X());
      double deg = rad * 180.0 / M_PI;
      if (deg < 0)
        deg = 360 + deg;

      log_test << deg << " ";
    }
    log_test << std::endl;

  } catch (const Standard_Failure &) {
  } catch (const std::exception &) {
  }

  return obbCenter;
}
