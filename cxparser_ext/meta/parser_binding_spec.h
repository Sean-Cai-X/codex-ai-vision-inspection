#ifndef CXPARSER_EXT_PARSER_BINDING_SPEC_H
#define CXPARSER_EXT_PARSER_BINDING_SPEC_H

#include "parser_meta_types.h"

namespace cxparser_ext
{
struct ParserModuleMeta
{
  std::string name;
  std::vector<ParserClassMeta> classes;
};

struct ParserBindingSpec
{
  std::vector<ParserModuleMeta> modules;
};
}

#endif
