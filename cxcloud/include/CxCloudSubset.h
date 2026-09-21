#pragma once

#include <memory>

namespace CCCoreLib
{
class ReferenceCloud;
}

namespace cxcloud
{
class CxCloudSubset
{
public:
  CxCloudSubset();
  explicit CxCloudSubset(int point_count);
  ~CxCloudSubset();

  int PointCount() const;
  bool BuildFromCloud(class CxCloudHandle& cloud);
  CCCoreLib::ReferenceCloud* NativeSubset();
  const CCCoreLib::ReferenceCloud* NativeSubset() const;

private:
  int myPointCount;
  std::shared_ptr<CCCoreLib::ReferenceCloud> myNativeSubset;
};
}
