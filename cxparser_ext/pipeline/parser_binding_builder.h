#ifndef CXPARSER_EXT_PARSER_BINDING_BUILDER_H
#define CXPARSER_EXT_PARSER_BINDING_BUILDER_H

#include "../meta/parser_binding_spec.h"
#include "../meta/parser_pseudocode_types.h"

namespace cxparser_ext
{
bool BuildBindingSpec(const PseudoClassSpec &pseudo_class,
                      ParserBindingSpec &binding_spec);
}

#endif
