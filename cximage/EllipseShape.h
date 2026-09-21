#ifndef CXIMAGE_ELLIPSE_SHAPE_H
#define CXIMAGE_ELLIPSE_SHAPE_H

#include "shapebase.h"

class EllipseShape : public ShapeBase
{
public:
    EllipseShape();
    EllipseShape(double center_x, double center_y, double radius_x, double radius_y);
    EllipseShape(double center_x, double center_y, double radius_x, double radius_y, double angle_deg);

    CxShapeKind kind() const override { return CxShapeKind::Ellipse; }

    void setCenter(double cx, double cy);
    void setRadiusX(double rx);
    void setRadiusY(double ry);
    void setAngleDegrees(double angle_deg);
    void setInnerScalePercent(double percent);

    double cx() const { return m_cx; }
    double cy() const { return m_cy; }
    double radiusX() const { return m_rx; }
    double radiusY() const { return m_ry; }
    double angleDegrees() const { return m_angleDeg; }
    double innerScalePercent() const { return m_innerScalePercent; }
    double innerRadiusX() const { return m_rx * m_innerScalePercent * 0.01; }
    double innerRadiusY() const { return m_ry * m_innerScalePercent * 0.01; }

    CxShapeHit hitTest(double x, double y, double tolerance) const override;
    void enumerateHandles(std::vector<CxShapeHandle>& out) const override;
    void dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y) override;
    void translateBy(double dx, double dy) override;

    bool exportEllipse(CxShapePoint& center, double& radius_x, double& radius_y, double& angle) const override;
    void exportPoints(std::vector<CxShapePoint>& out) const override;
    bool snapshot(CxShapeGeometrySnapshot& out) const override;

    void drawshape(gp_Path& painter) override;

    void EnumerateBoundaryPoints(
        std::vector<CxShapePoint>& out,
        int segments = 96) const;

private:
    double m_cx = 0.0;
    double m_cy = 0.0;
    double m_rx = 50.0;
    double m_ry = 30.0;
    double m_angleDeg = 0.0;
    double m_innerScalePercent = 0.0;
};

#endif
