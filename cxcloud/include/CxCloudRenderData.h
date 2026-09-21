#pragma once

namespace cxcloud
{
class CxCloudHandle;
}

namespace cxcloud
{
class CxCloudRenderData
{
public:
  struct CxBounds
  {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
  };

  CxCloudRenderData();
  explicit CxCloudRenderData(const CxCloudHandle& cloud);

  int PointCount() const;
  void SetPointCount(int point_count);

  bool HasNormals() const;
  void SetHasNormals(bool has_normals);
  bool HasBounds() const;
  const CxBounds& Bounds() const;
  void SetBounds(const CxBounds& bounds);
  void SyncFromCloud(const CxCloudHandle& cloud);

private:
  int myPointCount;
  bool myHasNormals;
  bool myHasBounds;
  CxBounds myBounds;
};
}
