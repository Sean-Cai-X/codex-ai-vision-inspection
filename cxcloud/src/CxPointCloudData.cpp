#include "CxPointCloudData.h"

namespace cxcloud
{
CxPointCloudData::CxPointCloudData()
  : myPointCount(0)
  , myHasNormals(false)
{
}

CxPointCloudData::CxPointCloudData(int point_count)
  : myPointCount(point_count)
  , myHasNormals(false)
{
}

int CxPointCloudData::PointCount() const
{
  return myPointCount;
}

void CxPointCloudData::SetPointCount(int point_count)
{
  myPointCount = point_count;
}

bool CxPointCloudData::HasNormals() const
{
  return myHasNormals;
}

void CxPointCloudData::SetHasNormals(bool has_normals)
{
  myHasNormals = has_normals;
}
}
