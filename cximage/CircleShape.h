#ifndef CXIMAGE_CIRCLE_SHAPE_H
#define CXIMAGE_CIRCLE_SHAPE_H

#include "shapebase.h"

class CircleShape : public ShapeBase
{
public:
    CircleShape();
    CircleShape(double cx, double cy, double radius, double inner_radius = 0.0);

    CxShapeKind kind() const override { return CxShapeKind::Circle; }

    void setCenter(double cx, double cy);
    void setRadius(double r);
    void setInnerRadius(double r);
    // Optional FindCircle scan sector. It belongs to this same editable ROI
    // shape, so rendering, HitTest and Drag share one geometry owner.
    void setScanSector(bool enabled, double start_degrees, double end_degrees);

    double cx() const { return m_cx; }
    double cy() const { return m_cy; }
    double radius() const { return m_radius; }
    double innerRadius() const { return m_innerRadius; }
    bool hasScanSector() const { return m_hasScanSector; }
    double scanSectorStartDegrees() const { return m_scanSectorStartDegrees; }
    double scanSectorEndDegrees() const { return m_scanSectorEndDegrees; }
    // A0/A1 remain part of this CircleShape and lie on Rout.  The sector
    // boundary starts at Rin (or the centre when Rin is zero).
    CxShapePoint scanSectorInnerPoint(int endpoint_index) const;
    CxShapePoint scanSectorBoundaryPoint(int endpoint_index) const;
    CxShapePoint scanSectorHandlePoint(int endpoint_index) const;

    CxShapeHit hitTest(double x, double y, double tolerance) const override;
    void enumerateHandles(std::vector<CxShapeHandle>& out) const override;
    void dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y) override;
    void translateBy(double dx, double dy) override;

    bool exportCircle(CxShapePoint& center, double& radius, double& inner_radius) const override;
    void exportPoints(std::vector<CxShapePoint>& out) const override;
    bool snapshot(CxShapeGeometrySnapshot& out) const override;

    void drawshape(gp_Path& painter) override;

private:
    double m_cx = 0.0;
    double m_cy = 0.0;
    double m_radius = 50.0;
    double m_innerRadius = 0.0;
    bool m_hasScanSector = false;
    double m_scanSectorStartDegrees = 0.0;
    double m_scanSectorEndDegrees = 360.0;
};

#endif
