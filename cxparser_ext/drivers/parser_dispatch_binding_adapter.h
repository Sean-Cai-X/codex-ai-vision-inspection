#ifndef CXPARSER_EXT_PARSER_DISPATCH_BINDING_ADAPTER_H
#define CXPARSER_EXT_PARSER_DISPATCH_BINDING_ADAPTER_H

#include "../meta/parser_pseudocode_types.h"
#include "../pipeline/parser_binding_builder.h"
#include "parser_dispatch_driver.h"

namespace cxparser_ext
{
namespace detail
{
inline PseudoClassSpec BuildImageProbePseudoClass()
{
  PseudoClassSpec pseudo_class;
  pseudo_class.module_name = "testdll_image_probe";
  pseudo_class.class_name = "ImageProbeWrapper";
  pseudo_class.parser_alias = "ImageProbe";

  PseudoMethodSpec load_method;
  load_method.name = "Load";
  load_method.param_types.push_back("const char*");
  load_method.return_type = "void";
  pseudo_class.methods.push_back(load_method);

  PseudoMethodSpec detect_method;
  detect_method.name = "Detect";
  detect_method.param_types.push_back("double");
  detect_method.return_type = "void";
  pseudo_class.methods.push_back(detect_method);

  PseudoMethodSpec score_method;
  score_method.name = "Score";
  score_method.return_type = "double";
  pseudo_class.methods.push_back(score_method);

  return pseudo_class;
}

inline PseudoClassSpec BuildGeometryContractPseudoClass()
{
  PseudoClassSpec pseudo_class;
  pseudo_class.module_name = "torch_geometry_contract";
  pseudo_class.class_name = "GeometryContractBridge";
  pseudo_class.parser_alias = "GeometryContract";
  return pseudo_class;
}
}

inline bool BuildBindingSpecForCase(const ParserDispatchCaseSpec &spec,
                                    ParserBindingSpec &binding_spec)
{
  binding_spec = ParserBindingSpec();
  if (spec.requires_image_probe_binding)
    return BuildBindingSpec(detail::BuildImageProbePseudoClass(), binding_spec);

  if (spec.requires_geometry_contract_binding)
    return BuildBindingSpec(detail::BuildGeometryContractPseudoClass(), binding_spec);

  return true;
}
}

#endif
