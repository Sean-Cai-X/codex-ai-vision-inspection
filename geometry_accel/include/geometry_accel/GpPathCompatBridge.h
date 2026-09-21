#ifndef GEOMETRY_ACCEL_GPPATHCOMPATBRIDGE_H
#define GEOMETRY_ACCEL_GPPATHCOMPATBRIDGE_H

#include "GeometryPathContract.h"
#include "GpPathCompat.h"

#include <string>

namespace geometry_accel {

class GpPathCompatBridge
{
public:
  bool Execute(const GeometryPathCommand& command, std::string* error_message = nullptr);
  GeometryPathSnapshot Snapshot() const;

  gp_PathCompat& path() { return path_; }
  const gp_PathCompat& path() const { return path_; }

private:
  gp_PathCompat path_;
};

} // namespace geometry_accel

#endif
