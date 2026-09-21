

#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <sys/stat.h>
#define CX_STAT _stat
#else
#include <sys/stat.h>
#include <sys/types.h>
#define CX_STAT stat
#endif

namespace
{
struct CxStatus
{
  bool ok;
  std::string message;
};

struct CxEntity
{
  int id;
  std::string name;
  std::string type;
};

struct CxFeature2D
{
  std::string kind;
  double confidence;
  std::vector<double> points;
};

struct CxShapeHandle
{
  int entity_id;
  std::string topology;
  int primitive_count;
};

struct CxPointCloud
{
  int point_count;
  bool has_normals;
};

struct CxMeasureResult
{
  std::string metric_name;
  double value;
};

bool NearlyEqual(double lhs, double rhs, double eps = 1e-9)
{
  return std::fabs(lhs - rhs) <= eps;
}

bool PathExists(const std::string &path)
{
  struct CX_STAT info;
  return CX_STAT(path.c_str(), &info) == 0;
}

std::string JoinPath(const std::string &lhs, const std::string &rhs)
{
  if (lhs.empty())
    return rhs;
  if (lhs == ".")
    return rhs;
  return lhs + "/" + rhs;
}

std::string FindWorkspacePath(const char *name)
{
  const char *roots[] = {".", "..", "../..", "../../..", "../../../..", "../../../../.."};
  for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i)
  {
    const std::string candidate = JoinPath(roots[i], name);
    if (PathExists(candidate))
      return candidate;
  }
  return std::string();
}

bool Check(bool condition, const char *message)
{
  if (!condition)
  {
    std::cerr << "[FAIL] " << message << std::endl;
    return false;
  }
  return true;
}

class MockCxGeom
{
public:
  explicit MockCxGeom(const std::string &occt_root)
    : occt_root_(occt_root)
    , build_curve_count_(0)
    , build_face_count_(0)
  {
  }

  CxStatus ValidateEnvironment() const
  {
    const bool ok = !occt_root_.empty();
    return CxStatus{ok, ok ? "occt-ready" : "occt-missing"};
  }

  CxShapeHandle BuildWireFromFeatures(const CxEntity &entity, const std::vector<CxFeature2D> &features)
  {
    ++build_curve_count_;
    return CxShapeHandle{entity.id, "wire", static_cast<int>(features.size())};
  }

  CxShapeHandle BuildFaceFromWire(const CxShapeHandle &wire)
  {
    ++build_face_count_;
    return CxShapeHandle{wire.entity_id, "face", wire.primitive_count};
  }

  int build_curve_count() const { return build_curve_count_; }
  int build_face_count() const { return build_face_count_; }

private:
  std::string occt_root_;
  int build_curve_count_;
  int build_face_count_;
};

class MockCxCloud
{
public:
  explicit MockCxCloud(const std::string &cloud_root)
    : cloud_root_(cloud_root)
    , build_octree_count_(0)
    , estimate_normals_count_(0)
    , compute_distance_count_(0)
  {
  }

  CxStatus ValidateEnvironment() const
  {
    const bool ok = !cloud_root_.empty();
    return CxStatus{ok, ok ? "cccorelib-ready" : "cccorelib-missing"};
  }

  CxMeasureResult BuildOctree(CxPointCloud &cloud)
  {
    ++build_octree_count_;
    return CxMeasureResult{"octree_level", cloud.point_count > 0 ? 1.0 : 0.0};
  }

  CxMeasureResult EstimateNormals(CxPointCloud &cloud)
  {
    ++estimate_normals_count_;
    cloud.has_normals = true;
    return CxMeasureResult{"normals_ready", cloud.has_normals ? 1.0 : 0.0};
  }

  CxMeasureResult ComputeDistance(const CxPointCloud &cloud, const CxShapeHandle &shape)
  {
    ++compute_distance_count_;
    return CxMeasureResult{"cloud_shape_distance", cloud.point_count + shape.primitive_count * 0.1};
  }

  int build_octree_count() const { return build_octree_count_; }
  int estimate_normals_count() const { return estimate_normals_count_; }
  int compute_distance_count() const { return compute_distance_count_; }

private:
  std::string cloud_root_;
  int build_octree_count_;
  int estimate_normals_count_;
  int compute_distance_count_;
};

int RunEnvironmentSmoke()
{
  std::cout << "[GROUP] environment" << std::endl;

  const std::string cxcore_root = FindWorkspacePath("cxcore");
  const std::string occt_root = FindWorkspacePath("opencascade-7.7.0");
  const std::string cccorelib_root = FindWorkspacePath("CCCoreLib");

  std::cout << "  [ROOT] cxcore=" << (cxcore_root.empty() ? "missing" : cxcore_root) << std::endl;
  std::cout << "  [ROOT] opencascade-7.7.0=" << (occt_root.empty() ? "missing" : occt_root) << std::endl;
  std::cout << "  [ROOT] CCCoreLib=" << (cccorelib_root.empty() ? "missing" : cccorelib_root) << std::endl;

  if (!Check(!cxcore_root.empty(), "cxcore root missing"))
    return 1;
  if (!Check(!occt_root.empty(), "opencascade root missing"))
    return 1;
  if (!Check(!cccorelib_root.empty(), "CCCoreLib root missing"))
    return 1;

  return 0;
}

int RunCxGeomContractSmoke()
{
  std::cout << "[GROUP] cxgeom_contract" << std::endl;

  MockCxGeom geom(FindWorkspacePath("opencascade-7.7.0"));
  const CxStatus env = geom.ValidateEnvironment();
  if (!Check(env.ok, "cxgeom environment validation failed"))
    return 1;

  CxEntity entity = {101, "feature_wire", "wire_candidate"};
  std::vector<CxFeature2D> features;
  features.push_back(CxFeature2D{"line", 0.98, std::vector<double>(4, 10.0)});
  features.push_back(CxFeature2D{"circle", 0.95, std::vector<double>(3, 20.0)});

  const CxShapeHandle wire = geom.BuildWireFromFeatures(entity, features);
  const CxShapeHandle face = geom.BuildFaceFromWire(wire);

  std::cout << "  [GEOM] wire_primitives=" << wire.primitive_count
            << " face_topology=" << face.topology << std::endl;

  if (!Check(wire.entity_id == entity.id, "wire entity id mismatch"))
    return 1;
  if (!Check(wire.topology == "wire", "wire topology mismatch"))
    return 1;
  if (!Check(wire.primitive_count == 2, "wire primitive count mismatch"))
    return 1;
  if (!Check(face.topology == "face", "face topology mismatch"))
    return 1;
  if (!Check(face.primitive_count == wire.primitive_count, "face primitive transfer mismatch"))
    return 1;
  if (!Check(geom.build_curve_count() == 1, "cxgeom build wire count mismatch"))
    return 1;
  if (!Check(geom.build_face_count() == 1, "cxgeom build face count mismatch"))
    return 1;

  return 0;
}

int RunCxCloudContractSmoke()
{
  std::cout << "[GROUP] cxcloud_contract" << std::endl;

  MockCxCloud cloud_module(FindWorkspacePath("CCCoreLib"));
  const CxStatus env = cloud_module.ValidateEnvironment();
  if (!Check(env.ok, "cxcloud environment validation failed"))
    return 1;

  CxPointCloud cloud = {128, false};
  const CxMeasureResult octree = cloud_module.BuildOctree(cloud);
  const CxMeasureResult normals = cloud_module.EstimateNormals(cloud);
  const CxShapeHandle probe = {301, "wire", 4};
  const CxMeasureResult distance = cloud_module.ComputeDistance(cloud, probe);

  std::cout << "  [CLOUD] octree=" << octree.value
            << " normals=" << normals.value
            << " distance=" << distance.value << std::endl;

  if (!Check(cloud.has_normals, "cloud normals flag mismatch"))
    return 1;
  if (!Check(NearlyEqual(octree.value, 1.0), "octree metric mismatch"))
    return 1;
  if (!Check(NearlyEqual(normals.value, 1.0), "normals metric mismatch"))
    return 1;
  if (!Check(NearlyEqual(distance.value, 128.4), "distance metric mismatch"))
    return 1;
  if (!Check(cloud_module.build_octree_count() == 1, "cxcloud build octree count mismatch"))
    return 1;
  if (!Check(cloud_module.estimate_normals_count() == 1, "cxcloud estimate normals count mismatch"))
    return 1;
  if (!Check(cloud_module.compute_distance_count() == 1, "cxcloud distance count mismatch"))
    return 1;

  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[CXGEOM-CXCLOUD-CONTRACT] start" << std::endl;

    int status = 0;
    status += RunEnvironmentSmoke();
    status += RunCxGeomContractSmoke();
    status += RunCxCloudContractSmoke();

    if (status != 0)
      return 1;

    std::cout << "[CXGEOM-CXCLOUD-CONTRACT] passed" << std::endl;
    return 0;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "[EXCEPTION] " << ex.what() << std::endl;
    return 2;
  }
}
