#ifndef CXIMAGE_LINE_GAUGE_SHAPE_H
#define CXIMAGE_LINE_GAUGE_SHAPE_H

#include "shapebase.h"

class LineGaugeShape : public ShapeBase
{
public:
    LineGaugeShape();
    LineGaugeShape(double x0, double y0, double x1, double y1, double half_width = 20.0);

    CxShapeKind kind() const override { return CxShapeKind::LineGauge; }

    void setLine(double x0, double y0, double x1, double y1);
    void setHalfWidth(double hw);

    double x0() const { return m_x0; }
    double y0() const { return m_y0; }
    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double halfWidth() const { return m_halfWidth; }

    CxShapeHit hitTest(double x, double y, double tolerance) const override;
    void enumerateHandles(std::vector<CxShapeHandle>& out) const override;
    void dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y) override;
    void translateBy(double dx, double dy) override;

    bool exportLine(CxShapePoint& p0, CxShapePoint& p1) const override;
    void exportPoints(std::vector<CxShapePoint>& out) const override;

    bool snapshot(CxShapeGeometrySnapshot& out) const override;

    void drawshape(gp_Path& painter) override;

private:
    double m_x0 = 0.0;
    double m_y0 = 0.0;
    double m_x1 = 100.0;
    double m_y1 = 100.0;
    double m_halfWidth = 20.0;
};

#endif