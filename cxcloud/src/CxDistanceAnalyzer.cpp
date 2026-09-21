#include "CxDistanceAnalyzer.h"

#include <DgmOctree.h>
#include <DistanceComputationTools.h>
#include <PointCloud.h>
#include <ScalarField.h>

namespace cxcloud
{
CxDistanceResult CxDistanceAnalyzer::ComputeCloudDistance(const CxCloudHandle& lhs,
                                                          const CxCloudHandle& rhs) const
{
  CCCoreLib::PointCloud* compared_cloud = const_cast<CCCoreLib::PointCloud*>(lhs.NativeCloud());
  CCCoreLib::PointCloud* reference_cloud = const_cast<CCCoreLib::PointCloud*>(rhs.NativeCloud());
  if (!compared_cloud || !reference_cloud || compared_cloud->size() == 0 || reference_cloud->size() == 0)
  {
    return CxDistanceResult{"cloud_distance", 0.0};
  }

  CxCloudHandle& mutable_lhs = const_cast<CxCloudHandle&>(lhs);
  if (!mutable_lhs.EnsureScalarField("CxCloudDistance"))
  {
    return CxDistanceResult{"cloud_distance", 0.0};
  }

  CCCoreLib::DgmOctree compared_octree(compared_cloud);
  CCCoreLib::DgmOctree reference_octree(reference_cloud);
  if (compared_octree.build() < 0 || reference_octree.build() < 0)
  {
    return CxDistanceResult{"cloud_distance", 0.0};
  }

  CCCoreLib::DistanceComputationTools::Cloud2CloudDistancesComputationParams params;
  params.multiThread = false;
  params.octreeLevel = compared_octree.findBestLevelForComparisonWithOctree(&reference_octree);

  const int rc = CCCoreLib::DistanceComputationTools::computeCloud2CloudDistances(
    compared_cloud,
    reference_cloud,
    params,
    nullptr,
    &compared_octree,
    &reference_octree);

  if (rc < 0 || !compared_cloud->getCurrentOutScalarField())
  {
    return CxDistanceResult{"cloud_distance", 0.0};
  }

  CCCoreLib::ScalarType mean = 0;
  compared_cloud->getCurrentOutScalarField()->computeMeanAndVariance(mean, nullptr);
  return CxDistanceResult{"cloud_distance", static_cast<double>(mean)};
}

CxDistanceResult CxDistanceAnalyzer::ComputeCloudShapeDistance(const CxCloudHandle& cloud,
                                                               int primitive_count) const
{
  const CCCoreLib::PointCloud* native_cloud = cloud.NativeCloud();
  if (!native_cloud || native_cloud->size() == 0 || primitive_count <= 0)
  {
    return CxDistanceResult{"cloud_shape_distance", 0.0};
  }

  return CxDistanceResult{
    "cloud_shape_distance",
    static_cast<double>(native_cloud->size()) / static_cast<double>(primitive_count)};
}
}
