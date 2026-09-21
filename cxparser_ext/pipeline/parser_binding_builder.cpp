#include "parser_binding_builder.h"

namespace cxparser_ext
{
namespace
{
ParserValueKind GuessValueKind(const std::string &type_name)
{
  if (type_name == "void")
    return pvk_void;
  if (type_name == "bool")
    return pvk_bool;
  if (type_name == "int")
    return pvk_int;
  if (type_name == "double" || type_name == "float")
    return pvk_double;
  if (type_name == "const char*" || type_name == "char*" || type_name == "std::string")
    return pvk_string;
  if (type_name.find('*') != std::string::npos)
    return pvk_pointer;
  return pvk_object;
}

ParserTypeMeta MakeTypeMeta(const std::string &type_name)
{
  ParserTypeMeta type_meta;
  type_meta.name = type_name;
  type_meta.qualified_name = type_name;
  type_meta.value_kind = GuessValueKind(type_name);
  type_meta.is_const = type_name.find("const ") == 0;
  type_meta.is_ref = type_name.find('&') != std::string::npos;
  type_meta.is_ptr = type_name.find('*') != std::string::npos;
  return type_meta;
}
}

bool BuildBindingSpec(const PseudoClassSpec &pseudo_class,
                      ParserBindingSpec &binding_spec)
{
  binding_spec = ParserBindingSpec();
  if (pseudo_class.module_name.empty() ||
      pseudo_class.class_name.empty() ||
      pseudo_class.parser_alias.empty())
    return false;

  ParserModuleMeta module;
  module.name = pseudo_class.module_name;

  ParserClassMeta parser_class;
  parser_class.name = pseudo_class.class_name;
  parser_class.qualified_name =
    pseudo_class.module_name.empty() ? pseudo_class.class_name
                                     : (pseudo_class.module_name + "::" + pseudo_class.class_name);
  parser_class.parser_alias = pseudo_class.parser_alias;
  parser_class.is_external = true;

  for (std::size_t i = 0; i < pseudo_class.methods.size(); ++i)
  {
    ParserMethodMeta method;
    method.name = pseudo_class.methods[i].name;
    method.qualified_name = parser_class.qualified_name + "::" + method.name;
    method.owner_class = parser_class.qualified_name;
    method.return_type = MakeTypeMeta(pseudo_class.methods[i].return_type);
    for (std::size_t p = 0; p < pseudo_class.methods[i].param_types.size(); ++p)
    {
      ParserParamMeta param;
      param.name = "arg" + std::to_string(p);
      param.type = MakeTypeMeta(pseudo_class.methods[i].param_types[p]);
      method.params.push_back(param);
    }
    parser_class.methods.push_back(method);
  }

  module.classes.push_back(parser_class);
  binding_spec.modules.push_back(module);
  return true;
}
}
