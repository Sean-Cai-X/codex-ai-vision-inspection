#include "CxCloudSubset.h"

#include "CxCloudHandle.h"

#include <PointCloud.h>
#include <ReferenceCloud.h>

namespace cxcloud
{
CxCloudSubset::CxCloudSubset()
  : myPointCount(0)
  , myNativeSubset()
{
}

CxCloudSubset::CxCloudSubset(int point_count)
  : myPointCount(point_count)
  , myNativeSubset()
{
}

CxCloudSubset::~CxCloudSubset() = default;

int CxCloudSubset::PointCount() const
{
  return myPointCount;
}

bool CxCloudSubset::BuildFromCloud(CxCloudHandle& cloud)
{
  CCCoreLib::PointCloud* native_cloud = cloud.NativeCloud();
  if (!native_cloud)
  {
    myPointCount = 0;
    myNativeSubset.reset();
    return false;
  }

  myNativeSubset = std::make_shared<CCCoreLib::ReferenceCloud>(native_cloud);
  if (!myNativeSubset->reserve(native_cloud->size()))
  {
    myNativeSubset.reset();
    myPointCount = 0;
    return false;
  }

  if (native_cloud->size() > 0 && !myNativeSubset->addPointIndex(0, native_cloud->size()))
  {
    myNativeSubset.reset();
    myPointCount = 0;
    return false;
  }

  myPointCount = static_cast<int>(myNativeSubset->size());
  return true;
}

CCCoreLib::ReferenceCloud* CxCloudSubset::NativeSubset()
{
  return myNativeSubset.get();
}

const CCCoreLib::ReferenceCloud* CxCloudSubset::NativeSubset() const
{
  return myNativeSubset.get();
}
}
