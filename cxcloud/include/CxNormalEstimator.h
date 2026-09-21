#pragma once

#include "CxCloudHandle.h"

namespace cxcloud
{
class CxNormalEstimator
{
public:
  bool EstimateNormals(CxCloudHandle& cloud) const;
};
}
