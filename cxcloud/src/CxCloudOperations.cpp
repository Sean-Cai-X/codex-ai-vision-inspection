#include "CxCloudOperations.h"

namespace cxcloud
{
namespace
{
CxCloudOperationResult BuildResult(CxCloudOperationKind kind,
                                   const CxCloudRevision& revision,
                                   std::uint32_t dirty_flags,
                                   CxCloudRefreshHint refresh_hint)
{
  CxCloudOperationResult result;
  result.kind = kind;
  result.revision = revision;
  result.dirty_flags = dirty_flags;
  result.refresh_hint = refresh_hint;
  return result;
}
}

CxCloudOperationResult CxCloudOperations::AddCloud(const CxCloudRevision& previous)
{
  CxCloudRevision current = previous;
  ++current.cloud;
  return BuildResult(CxCloudOperationKind::Add,
                     current,
                     CxCloudDirtyContent,
                     CxCloudRefreshHint::RebuildCloudPresentation);
}

CxCloudOperationResult CxCloudOperations::UpdateCloudStyle(const CxCloudRevision& previous,
                                                           bool visibility_only)
{
  return BuildResult(CxCloudOperationKind::UpdateStyle,
                     previous,
                     visibility_only ? CxCloudDirtyVisibility : CxCloudDirtyPresentation,
                     CxCloudRefreshHint::UpdatePresentation);
}

CxCloudOperationResult CxCloudOperations::ReplaceCloudPayload(const CxCloudRevision& previous)
{
  CxCloudRevision current = previous;
  ++current.cloud;
  return BuildResult(CxCloudOperationKind::ReplacePayload,
                     current,
                     CxCloudDirtyContent,
                     CxCloudRefreshHint::RebuildCloudPresentation);
}

CxCloudOperationResult CxCloudOperations::RemoveCloud(const CxCloudRevision& previous)
{
  CxCloudRevision current = previous;
  ++current.cloud;
  return BuildResult(CxCloudOperationKind::Remove,
                     current,
                     CxCloudDirtyContent,
                     CxCloudRefreshHint::RebuildCloudPresentation);
}
}
