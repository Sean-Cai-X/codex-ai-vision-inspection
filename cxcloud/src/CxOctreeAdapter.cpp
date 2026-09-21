#include "CxOctreeAdapter.h"

#include <DgmOctree.h>
#include <PointCloud.h>

namespace cxcloud
{
CxOctreeAdapter::CxOctreeAdapter()
  : myBuilt(false)
  , myLevel(0)
  , myNativeOctree()
{
}

CxOctreeAdapter::~CxOctreeAdapter() = default;

bool CxOctreeAdapter::Build(const CxCloudHandle& cloud)
{
  const CCCoreLib::PointCloud* native_cloud = cloud.NativeCloud();
  if (!native_cloud || native_cloud->size() == 0)
  {
    myNativeOctree.reset();
    myBuilt = false;
    myLevel = 0;
    return false;
  }

  myNativeOctree = std::make_shared<CCCoreLib::DgmOctree>(const_cast<CCCoreLib::PointCloud*>(native_cloud));
  myBuilt = (myNativeOctree->build() >= 0);
  myLevel = myBuilt ? static_cast<int>(myNativeOctree->findBestLevelForAGivenPopulationPerCell(16)) : 0;
  return myBuilt;
}

bool CxOctreeAdapter::IsBuilt() const
{
  return myBuilt;
}

int CxOctreeAdapter::Level() const
{
  return myLevel;
}

CCCoreLib::DgmOctree* CxOctreeAdapter::NativeOctree()
{
  return myNativeOctree.get();
}

const CCCoreLib::DgmOctree* CxOctreeAdapter::NativeOctree() const
{
  return myNativeOctree.get();
}
}
