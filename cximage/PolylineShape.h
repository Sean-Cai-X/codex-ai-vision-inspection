#ifndef CXIMAGE_POLYLINE_SHAPE_H
#define CXIMAGE_POLYLINE_SHAPE_H

#include "shapebase.h"

class PolylineShape : public ShapeBase
{
public:
    PolylineShape();

    CxShapeKind kind() const override { return CxShapeKind::Polyline; }

    void addPoint(double x, double y);
    void insertPoint(int index, double x, double y);
    void removePoint(int index);
    void setPoint(int index, double x, double y);
    void clear();
    void close(bool closed);

    int pointCount() const { return static_cast<int>(m_points.size()); }
    const CxShapePoint& point(int index) const { return m_points[index]; }
    bool isClosed() const { return m_closed; }

    CxShapeHit hitTest(double x, double y, double tolerance) const override;
    void enumerateHandles(std::vector<CxShapeHandle>& out) const override;
    void dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y) override;
    void translateBy(double dx, double dy) override;

    void exportPolyline(std::vector<CxShapePoint>& out, bool& closed) const override;
    void exportPoints(std::vector<CxShapePoint>& out) const override;
    bool snapshot(CxShapeGeometrySnapshot& out) const override;

    void drawshape(gp_Path& painter) override;

private:
    std::vector<CxShapePoint> m_points;
    bool m_closed = false;
};

#endif
