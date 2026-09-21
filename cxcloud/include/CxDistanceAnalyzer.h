#pragma once

#include "CxCloudHandle.h"

namespace cxcloud
{
struct CxDistanceResult
{
  const char* metric_name;
  double value;
};

class CxDistanceAnalyzer
{
public:
  CxDistanceResult ComputeCloudDistance(const CxCloudHandle& lhs,
                                        const CxCloudHandle& rhs) const;
  CxDistanceResult ComputeCloudShapeDistance(const CxCloudHandle& cloud,
                                             int primitive_count) const;
};
}
