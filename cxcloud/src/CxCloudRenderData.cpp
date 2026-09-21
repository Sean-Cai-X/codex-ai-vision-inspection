#include "CxCloudRenderData.h"

#include "CxCloudHandle.h"

#include <CCGeom.h>
#include <PointCloud.h>

namespace cxcloud
{
CxCloudRenderData::CxCloudRenderData()
  : myPointCount(0)
  , myHasNormals(false)
  , myHasBounds(false)
  , myBounds()
{
}

CxCloudRenderData::CxCloudRenderData(const CxCloudHandle& cloud)
  : myPointCount(0)
  , myHasNormals(false)
  , myHasBounds(false)
  , myBounds()
{
  SyncFromCloud(cloud);
}

int CxCloudRenderData::PointCount() const
{
  return myPointCount;
}

void CxCloudRenderData::SetPointCount(int point_count)
{
  myPointCount = point_count;
}

bool CxCloudRenderData::HasNormals() const
{
  return myHasNormals;
}

void CxCloudRenderData::SetHasNormals(bool has_normals)
{
  myHasNormals = has_normals;
}

bool CxCloudRenderData::HasBounds() const
{
  return myHasBounds;
}

const CxCloudRenderData::CxBounds& CxCloudRenderData::Bounds() const
{
  return myBounds;
}

void CxCloudRenderData::SetBounds(const CxBounds& bounds)
{
  myBounds = bounds;
  myHasBounds = true;
}

void CxCloudRenderData::SyncFromCloud(const CxCloudHandle& cloud)
{
  myPointCount = cloud.Data().PointCount();
  myHasNormals = cloud.Data().HasNormals();

  const CCCoreLib::PointCloud* native_cloud = cloud.NativeCloud();
  if (!native_cloud || native_cloud->size() == 0)
  {
    myHasBounds = false;
    myBounds = CxBounds();
    return;
  }

  CCCoreLib::CCVector3 bb_min;
  CCCoreLib::CCVector3 bb_max;
  const_cast<CCCoreLib::PointCloud*>(native_cloud)->getBoundingBox(bb_min, bb_max);

  myBounds.min_x = bb_min.x;
  myBounds.min_y = bb_min.y;
  myBounds.min_z = bb_min.z;
  myBounds.max_x = bb_max.x;
  myBounds.max_y = bb_max.y;
  myBounds.max_z = bb_max.z;
  myHasBounds = true;
}
}
