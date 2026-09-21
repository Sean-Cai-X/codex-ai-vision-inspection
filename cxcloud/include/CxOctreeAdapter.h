#pragma once

#include "CxCloudHandle.h"

#include <memory>

namespace CCCoreLib
{
class DgmOctree;
}

namespace cxcloud
{
class CxOctreeAdapter
{
public:
  CxOctreeAdapter();
  ~CxOctreeAdapter();

  bool Build(const CxCloudHandle& cloud);
  bool IsBuilt() const;
  int Level() const;
  CCCoreLib::DgmOctree* NativeOctree();
  const CCCoreLib::DgmOctree* NativeOctree() const;

private:
  bool myBuilt;
  int myLevel;
  std::shared_ptr<CCCoreLib::DgmOctree> myNativeOctree;
};
}
