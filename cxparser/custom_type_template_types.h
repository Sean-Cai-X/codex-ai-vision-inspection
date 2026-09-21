

#ifndef CXPARSER_CUSTOM_TYPE_TEMPLATE_TYPES_H
#define CXPARSER_CUSTOM_TYPE_TEMPLATE_TYPES_H

#include <cstddef>

namespace cxparser_template
{
struct CustomTypeMethodSpec
{
  const char *method_name;
  const char *signature;
};

struct CustomTypeSpec
{
  const char *module_name;
  const char *class_name;
  const CustomTypeMethodSpec *methods;
  std::size_t method_count;
};
}

#endif
