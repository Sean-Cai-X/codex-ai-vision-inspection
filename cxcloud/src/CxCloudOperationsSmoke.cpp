#include "CxCloudOperations.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxcloud_operations_smoke] " << message << '\n';
    return false;
  }

  return true;
}
}

int main()
{
  cxcloud::CxCloudRevision baseline;

  const cxcloud::CxCloudOperationResult add_result =
    cxcloud::CxCloudOperations::AddCloud(baseline);
  if (!Check(add_result.revision.cloud == 1, "add should increment cloud revision"))
  {
    return 1;
  }
  if (!Check(add_result.refresh_hint == cxcloud::CxCloudRefreshHint::RebuildCloudPresentation,
             "add should rebuild cloud presentation"))
  {
    return 1;
  }

  const cxcloud::CxCloudOperationResult style_result =
    cxcloud::CxCloudOperations::UpdateCloudStyle(add_result.revision, false);
  if (!Check(style_result.revision.cloud == add_result.revision.cloud,
             "style update should not increment cloud revision"))
  {
    return 1;
  }
  if (!Check(style_result.refresh_hint == cxcloud::CxCloudRefreshHint::UpdatePresentation,
             "style update should refresh presentation"))
  {
    return 1;
  }

  const cxcloud::CxCloudOperationResult visibility_result =
    cxcloud::CxCloudOperations::UpdateCloudStyle(add_result.revision, true);
  if (!Check(visibility_result.dirty_flags == cxcloud::CxCloudDirtyVisibility,
             "visibility update should set visibility dirty flag"))
  {
    return 1;
  }

  const cxcloud::CxCloudOperationResult replace_result =
    cxcloud::CxCloudOperations::ReplaceCloudPayload(add_result.revision);
  if (!Check(replace_result.revision.cloud == 2, "replace should increment cloud revision"))
  {
    return 1;
  }
  if (!Check(replace_result.refresh_hint == cxcloud::CxCloudRefreshHint::RebuildCloudPresentation,
             "replace should rebuild cloud presentation"))
  {
    return 1;
  }

  const cxcloud::CxCloudOperationResult remove_result =
    cxcloud::CxCloudOperations::RemoveCloud(replace_result.revision);
  if (!Check(remove_result.revision.cloud == 3, "remove should increment cloud revision"))
  {
    return 1;
  }
  if (!Check(remove_result.refresh_hint == cxcloud::CxCloudRefreshHint::RebuildCloudPresentation,
             "remove should rebuild cloud presentation"))
  {
    return 1;
  }

  std::cout << "[cxcloud_operations_smoke] ok" << '\n';
  return 0;
}
