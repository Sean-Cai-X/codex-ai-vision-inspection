#pragma once

#include "CxRefreshDecision.h"

namespace cxgeom
{
class CxRefreshTracker
{
public:
  static CxRefreshDecision Decide(const CxSceneRevision& previous,
                                  const CxSceneRevision& current,
                                  std::uint32_t dirty_flags);

  static bool HasAny(std::uint32_t dirty_flags, std::uint32_t mask);
};
}
