#pragma once

#include <cstdint>

namespace cxcloud
{
enum class CxCloudOperationKind
{
  Add = 0,
  UpdateStyle,
  ReplacePayload,
  Remove
};

enum class CxCloudRefreshHint
{
  None = 0,
  UpdatePresentation,
  RebuildCloudPresentation,
  RebuildScene
};

enum CxCloudDirtyFlags : std::uint32_t
{
  CxCloudDirtyNone = 0,
  CxCloudDirtyContent = 1u << 0,
  CxCloudDirtyPresentation = 1u << 1,
  CxCloudDirtyVisibility = 1u << 2
};

struct CxCloudRevision
{
  std::uint64_t cloud = 0;
};

struct CxCloudOperationResult
{
  CxCloudOperationKind kind = CxCloudOperationKind::Add;
  CxCloudRevision revision;
  std::uint32_t dirty_flags = CxCloudDirtyNone;
  CxCloudRefreshHint refresh_hint = CxCloudRefreshHint::None;
};

class CxCloudOperations
{
public:
  static CxCloudOperationResult AddCloud(const CxCloudRevision& previous);
  static CxCloudOperationResult UpdateCloudStyle(const CxCloudRevision& previous,
                                                 bool visibility_only = false);
  static CxCloudOperationResult ReplaceCloudPayload(const CxCloudRevision& previous);
  static CxCloudOperationResult RemoveCloud(const CxCloudRevision& previous);
};
}
