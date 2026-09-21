#ifndef CXIMAGE_RECT_SHAPE_H
#define CXIMAGE_RECT_SHAPE_H

#include "shapebase.h"

class RectShape : public ShapeBase
{
public:
    RectShape();
    RectShape(double x0, double y0, double x1, double y1);

    CxShapeKind kind() const override { return CxShapeKind::Rect; }

    void setRect(double x0, double y0, double x1, double y1);

    double x0() const { return m_points[0].x; }
    double y0() const { return m_points[0].y; }
    double x1() const { return m_points[2].x; }
    double y1() const { return m_points[2].y; }

    CxShapeHit hitTest(double x, double y, double tolerance) const override;
    void enumerateHandles(std::vector<CxShapeHandle>& out) const override;
    void dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y) override;
    void translateBy(double dx, double dy) override;

    void exportPolyline(std::vector<CxShapePoint>& out, bool& closed) const override;
    void exportPoints(std::vector<CxShapePoint>& out) const override;

    void drawshape(gp_Path& painter) override;

private:
    CxShapePoint m_points[4];
};

#endif