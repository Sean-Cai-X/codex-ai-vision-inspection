#include "CxCloudHandle.h"

#include <CCGeom.h>
#include <PointCloud.h>

namespace cxcloud
{
CxCloudHandle::CxCloudHandle()
  : myEntityId(0)
  , myName()
  , myData()
  , myNativeCloud(std::make_shared<CCCoreLib::PointCloud>())
{
}

CxCloudHandle::CxCloudHandle(int entity_id, std::string name, CxPointCloudData cloud_data)
  : myEntityId(entity_id)
  , myName(name)
  , myData(cloud_data)
  , myNativeCloud(std::make_shared<CCCoreLib::PointCloud>())
{
  if (myData.PointCount() > 0)
  {
    Reserve(myData.PointCount());
  }
}

CxCloudHandle::~CxCloudHandle() = default;

bool CxCloudHandle::Reserve(int point_count)
{
  if (!myNativeCloud || point_count < 0)
  {
    return false;
  }

  const bool ok = myNativeCloud->reserve(static_cast<unsigned>(point_count));
  if (ok)
  {
    myData.SetPointCount(static_cast<int>(myNativeCloud->size()));
  }
  return ok;
}

bool CxCloudHandle::AddPoint(double x, double y, double z)
{
  if (!myNativeCloud)
  {
    return false;
  }

  myNativeCloud->addPoint(CCCoreLib::CCVector3(static_cast<PointCoordinateType>(x),
                                               static_cast<PointCoordinateType>(y),
                                               static_cast<PointCoordinateType>(z)));
  myData.SetPointCount(static_cast<int>(myNativeCloud->size()));
  return true;
}

bool CxCloudHandle::EnsureScalarField(const char* field_name)
{
  if (!myNativeCloud)
  {
    return false;
  }

  const std::string scalar_name = field_name ? field_name : "CxCloudScalar";
  int sf_index = myNativeCloud->getScalarFieldIndexByName(scalar_name);
  if (sf_index < 0)
  {
    sf_index = myNativeCloud->addScalarField(scalar_name);
  }

  if (sf_index < 0)
  {
    return false;
  }

  myNativeCloud->setCurrentScalarField(sf_index);
  return myNativeCloud->enableScalarField();
}

void CxCloudHandle::SyncFromNative()
{
  if (!myNativeCloud)
  {
    myData.SetPointCount(0);
    myData.SetHasNormals(false);
    return;
  }

  myData.SetPointCount(static_cast<int>(myNativeCloud->size()));
  myData.SetHasNormals(myNativeCloud->normalsAvailable());
}

int CxCloudHandle::EntityId() const
{
  return myEntityId;
}

const std::string& CxCloudHandle::Name() const
{
  return myName;
}

const CxPointCloudData& CxCloudHandle::Data() const
{
  return myData;
}

CxPointCloudData& CxCloudHandle::Data()
{
  return myData;
}

CCCoreLib::PointCloud* CxCloudHandle::NativeCloud()
{
  return myNativeCloud.get();
}

const CCCoreLib::PointCloud* CxCloudHandle::NativeCloud() const
{
  return myNativeCloud.get();
}
}
