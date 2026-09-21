#pragma once

#include "CxCloudHandle.h"
#include "CxCloudOperations.h"
#include "CxCloudRenderData.h"
#include "CxCloudRenderStyle.h"

namespace cxcloud
{
class CxCloudItem
{
public:
  CxCloudItem();
  explicit CxCloudItem(const CxCloudHandle& payload);

  int EntityId() const;
  const CxCloudHandle& Payload() const;
  CxCloudHandle& Payload();
  void SetPayload(const CxCloudHandle& payload);

  const CxCloudRenderStyle& Style() const;
  void SetStyle(const CxCloudRenderStyle& style);

  const CxCloudRenderData& RenderData() const;
  CxCloudRenderData& RenderData();
  void SetRenderData(const CxCloudRenderData& render_data);

  const CxCloudRevision& Revision() const;
  void SetRevision(const CxCloudRevision& revision);

private:
  CxCloudHandle myPayload;
  CxCloudRenderStyle myStyle;
  CxCloudRenderData myRenderData;
  CxCloudRevision myRevision;
};
}
