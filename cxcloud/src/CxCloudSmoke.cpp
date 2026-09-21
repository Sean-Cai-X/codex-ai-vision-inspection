#include "CxCloudHandle.h"
#include "CxCloudRenderData.h"
#include "CxDistanceAnalyzer.h"
#include "CxNormalEstimator.h"
#include "CxOctreeAdapter.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxcloud_smoke] " << message << '\n';
    return false;
  }

  return true;
}

cxcloud::CxCloudHandle BuildCloud(int entity_id, const char* name, double x_offset)
{
  cxcloud::CxCloudHandle cloud(entity_id, name ? name : "cloud", cxcloud::CxPointCloudData());
  cloud.Reserve(4);
  cloud.AddPoint(x_offset + 0.0, 0.0, 0.0);
  cloud.AddPoint(x_offset + 1.0, 0.0, 0.0);
  cloud.AddPoint(x_offset + 0.0, 1.0, 0.0);
  cloud.AddPoint(x_offset + 0.0, 0.0, 1.0);
  cloud.SyncFromNative();
  return cloud;
}
}

int main()
{
  cxcloud::CxCloudHandle lhs = BuildCloud(1, "lhs", 0.0);
  cxcloud::CxCloudHandle rhs = BuildCloud(2, "rhs", 0.25);

  if (!Check(lhs.Data().PointCount() == 4, "lhs point count mismatch"))
  {
    return 1;
  }

  if (!Check(rhs.Data().PointCount() == 4, "rhs point count mismatch"))
  {
    return 1;
  }

  cxcloud::CxOctreeAdapter octree;
  if (!Check(octree.Build(lhs), "octree build failed"))
  {
    return 1;
  }

  if (!Check(octree.IsBuilt(), "octree should report built"))
  {
    return 1;
  }

  if (!Check(octree.Level() > 0, "octree level should be positive"))
  {
    return 1;
  }

  cxcloud::CxNormalEstimator normals;
  if (!Check(normals.EstimateNormals(lhs), "normal estimation failed"))
  {
    return 1;
  }

  if (!Check(lhs.Data().HasNormals(), "cloud should report normals after estimation"))
  {
    return 1;
  }

  cxcloud::CxDistanceAnalyzer distance_analyzer;
  const cxcloud::CxDistanceResult distance = distance_analyzer.ComputeCloudDistance(lhs, rhs);
  if (!Check(distance.value >= 0.0, "cloud distance should be non-negative"))
  {
    return 1;
  }

  const cxcloud::CxDistanceResult shape_distance = distance_analyzer.ComputeCloudShapeDistance(lhs, 2);
  if (!Check(shape_distance.value > 0.0, "cloud-shape distance proxy should be positive"))
  {
    return 1;
  }

  cxcloud::CxCloudRenderData render_data(lhs);
  if (!Check(render_data.PointCount() == lhs.Data().PointCount(), "render data point count mismatch"))
  {
    return 1;
  }

  if (!Check(render_data.HasNormals() == lhs.Data().HasNormals(), "render data normal flag mismatch"))
  {
    return 1;
  }

  if (!Check(render_data.HasBounds(), "render data should expose bounds"))
  {
    return 1;
  }

  const cxcloud::CxCloudRenderData::CxBounds& bounds = render_data.Bounds();
  if (!Check(bounds.min_x <= bounds.max_x, "bounds x range should be valid"))
  {
    return 1;
  }

  if (!Check(bounds.min_y <= bounds.max_y, "bounds y range should be valid"))
  {
    return 1;
  }

  if (!Check(bounds.min_z <= bounds.max_z, "bounds z range should be valid"))
  {
    return 1;
  }

  std::cout << "[cxcloud_smoke] ok" << '\n';
  return 0;
}
