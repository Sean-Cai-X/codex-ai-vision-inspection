#include "CxCloudItem.h"

namespace cxcloud
{
CxCloudItem::CxCloudItem()
  : myPayload()
  , myStyle()
  , myRenderData()
  , myRevision()
{
}

CxCloudItem::CxCloudItem(const CxCloudHandle& payload)
  : myPayload(payload)
  , myStyle()
  , myRenderData(payload)
  , myRevision()
{
}

int CxCloudItem::EntityId() const
{
  return myPayload.EntityId();
}

const CxCloudHandle& CxCloudItem::Payload() const
{
  return myPayload;
}

CxCloudHandle& CxCloudItem::Payload()
{
  return myPayload;
}

void CxCloudItem::SetPayload(const CxCloudHandle& payload)
{
  myPayload = payload;
}

const CxCloudRenderStyle& CxCloudItem::Style() const
{
  return myStyle;
}

void CxCloudItem::SetStyle(const CxCloudRenderStyle& style)
{
  myStyle = style;
}

const CxCloudRenderData& CxCloudItem::RenderData() const
{
  return myRenderData;
}

CxCloudRenderData& CxCloudItem::RenderData()
{
  return myRenderData;
}

void CxCloudItem::SetRenderData(const CxCloudRenderData& render_data)
{
  myRenderData = render_data;
}

const CxCloudRevision& CxCloudItem::Revision() const
{
  return myRevision;
}

void CxCloudItem::SetRevision(const CxCloudRevision& revision)
{
  myRevision = revision;
}
}
