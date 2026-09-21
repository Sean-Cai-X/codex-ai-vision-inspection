#pragma once

#include <cstdint>

namespace cxgeom
{
enum class CxRefreshAction
{
  None = 0,
  RedrawOnly,
  UpdatePresentation,
  RebuildGeometryPresentation,
  RebuildCloudPresentation,
  RebuildScene
};

enum CxDirtyFlags : std::uint32_t
{
  CxDirtyNone = 0,
  CxDirtyGeometry = 1u << 0,
  CxDirtyGeometryPresentation = 1u << 1,
  CxDirtyCloud = 1u << 2,
  CxDirtyCloudPresentation = 1u << 3,
  CxDirtyCamera = 1u << 4,
  CxDirtySelection = 1u << 5,
  CxDirtyVisibility = 1u << 6
};

struct CxSceneRevision
{
  std::uint64_t geometry = 0;
  std::uint64_t cloud = 0;
  std::uint64_t annotation = 0;
  std::uint64_t camera = 0;
  std::uint64_t selection = 0;
};

struct CxRefreshDecision
{
  CxRefreshAction action = CxRefreshAction::None;
  std::uint32_t reasons = CxDirtyNone;
  bool geometry_changed = false;
  bool cloud_changed = false;
  bool camera_changed = false;
  bool selection_changed = false;
};
}
