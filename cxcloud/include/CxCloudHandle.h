#pragma once

#include "CxPointCloudData.h"

#include <memory>
#include <string>

namespace CCCoreLib
{
class PointCloud;
}

namespace cxcloud
{
class CxCloudHandle
{
public:
  CxCloudHandle();
  CxCloudHandle(int entity_id, std::string name, CxPointCloudData cloud_data);
  ~CxCloudHandle();

  int EntityId() const;
  const std::string& Name() const;
  const CxPointCloudData& Data() const;
  CxPointCloudData& Data();

  bool Reserve(int point_count);
  bool AddPoint(double x, double y, double z);
  bool EnsureScalarField(const char* field_name);
  void SyncFromNative();

  CCCoreLib::PointCloud* NativeCloud();
  const CCCoreLib::PointCloud* NativeCloud() const;

private:
  int myEntityId;
  std::string myName;
  CxPointCloudData myData;
  std::shared_ptr<CCCoreLib::PointCloud> myNativeCloud;
};
}
