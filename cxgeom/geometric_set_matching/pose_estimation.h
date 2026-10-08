#pragma once
#include "types.h"
#include <utility>
namespace cxgeom::gsm {
// Positive-scale, orientation-preserving least squares. No reflection/shear.
std::optional<Similarity2d> EstimateSimilarity(
    const std::vector<std::pair<Point2, Point2>>& pairs);
Point2 Transform(Point2 point, const Similarity2d& pose);
}
