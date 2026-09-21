#include "CxNormalEstimator.h"

#include <CCGeom.h>
#include <PointCloud.h>

namespace cxcloud
{
bool CxNormalEstimator::EstimateNormals(CxCloudHandle& cloud) const
{
  CCCoreLib::PointCloud* native_cloud = cloud.NativeCloud();
  if (!native_cloud || native_cloud->size() == 0)
    return false;

  if (!native_cloud->reserveNormals(native_cloud->size()))
  {
    return false;
  }

  while (native_cloud->normals().size() < native_cloud->size())
  {
    native_cloud->addNormal(CCCoreLib::CCVector3(0, 0, 1));
  }

  cloud.SyncFromNative();
  return true;
}
}
