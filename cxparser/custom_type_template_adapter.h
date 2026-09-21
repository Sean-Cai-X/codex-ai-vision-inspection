

#ifndef CXPARSER_CUSTOM_TYPE_TEMPLATE_ADAPTER_H
#define CXPARSER_CUSTOM_TYPE_TEMPLATE_ADAPTER_H

#include "custom_type_template_types.h"

namespace mu
{
class Parser;
}

namespace cxparser_template
{
class ICustomTypeBindingAdapter
{
public:
  virtual ~ICustomTypeBindingAdapter() {}

  virtual const CustomTypeSpec &GetTypeSpec() const = 0;
  virtual bool Register(mu::Parser &parser) const = 0;
};
}

#endif
