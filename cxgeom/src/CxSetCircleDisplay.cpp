#include "CxSetCircleDisplay.h"

#include "CxOcctConvert.h"

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

namespace cxgeom
{
CxSetCircleDisplayResult CxSetCircleDisplay::MakeBatch(const CxSetCircleDisplayRequest& request) const
{
  CxSetCircleDisplayResult result;
  result.source_count = request.shapes.size();
  if (request.shapes.empty())
  {
    return result;
  }

  TopoDS_Compound compound;
  BRep_Builder builder;
  builder.MakeCompound(compound);

  for (const CxShapeHandle& shape : request.shapes)
  {
    if (!shape.IsNull())
    {
      builder.Add(compound, shape.NativeShape());
    }
  }

  result.batch_shape = CxShapeHandle(request.entity_id,
                                     request.batch_name,
                                     CxShapeKind::Unknown);
  result.batch_shape.SetNativeShape(compound);

  CxOcctConvert convert;
  result.presentation = convert.MakePresentation(result.batch_shape, request.style);
  result.success = !result.batch_shape.IsNull() && result.presentation.HasPresentation();
  return result;
}
}
