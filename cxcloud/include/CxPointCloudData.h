#pragma once

namespace cxcloud
{
class CxPointCloudData
{
public:
  CxPointCloudData();
  explicit CxPointCloudData(int point_count);

  int PointCount() const;
  void SetPointCount(int point_count);
  bool HasNormals() const;
  void SetHasNormals(bool has_normals);

private:
  int myPointCount;
  bool myHasNormals;
};
}
